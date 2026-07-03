"""Domain-knowledge assembly for AI seats (issue #20).

docs/*.md hold werewolf domain knowledge (glossary, strategy, speech examples,
anti-patterns, per-decision checklists). A section is scoped by an HTML-comment
anchor on the line right below its heading:

    ### 2.3 女巫
    <!-- knowledge: role=Witch board=2,3 -->

Attributes (`role=` / `board=` / `kind=`) are optional; a missing attribute
means "no constraint". A section spans until the next heading of the same or
higher level; nested headings inherit (AND-combine) their ancestors'
constraints. Unanchored sections apply to everyone.

Two entry points:
- load_domain_knowledge(role, board, mode) — the static block for the system
  prompt. It MUST stay byte-stable for a whole game (same role/board/mode →
  same string), so prefix caches (Ollama KV / cloud context caching) keep
  working across asks.
- kind_checklist(kind, role, board) — the small per-ask checklist injected at
  the tail of the user message (kept out of the system prompt on purpose).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

DOCS_DIR = Path(__file__).resolve().parents[1] / "docs"

# System-knowledge docs in presentation order. checklists.md is NOT here — its
# sections are per-ask (kind=) material served by kind_checklist().
SYSTEM_DOCS = ("anti_patterns.md", "glossary.md", "strategy.md", "speech_examples.md")
CHECKLIST_DOC = "checklists.md"

# lite mode char budget (Chinese ≈ 1 token/char); "full" has no cap.
LITE_BUDGET = 4000

_ANCHOR = re.compile(r"^\s*<!--\s*knowledge:\s*(.*?)\s*-->\s*$")
_HEADING = re.compile(r"^(#{1,6})\s")


@dataclass
class Chunk:
    """One heading + its immediate body, with the effective (inherited) scope."""
    doc: str
    order: int
    level: int
    lines: list[str] = field(default_factory=list)
    role: Optional[frozenset] = None    # None = no constraint
    board: Optional[frozenset] = None
    kind: Optional[frozenset] = None

    @property
    def text(self) -> str:
        return "\n".join(self.lines).strip("\n")


def _parse_anchor(s: str) -> dict[str, frozenset]:
    out: dict[str, frozenset] = {}
    for token in s.split():
        if "=" not in token:
            continue
        key, _, val = token.partition("=")
        if key in ("role", "board", "kind") and val:
            out[key] = frozenset(v for v in val.split(",") if v)
    return out


def _merge(base: Optional[frozenset], new: Optional[frozenset]) -> Optional[frozenset]:
    if base is None:
        return new
    if new is None:
        return base
    return base & new  # nested anchors AND-combine


def _parse_doc(path: Path) -> list[Chunk]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return []
    chunks: list[Chunk] = []
    # stack of (level, own-anchor constraints) for subtree inheritance
    stack: list[tuple[int, dict[str, frozenset]]] = []
    cur = Chunk(doc=path.name, order=0, level=0)
    i = 0
    while i < len(lines):
        line = lines[i]
        m = _HEADING.match(line)
        if not m:
            # A stray anchor (not on the line right below a heading) is malformed —
            # drop it so it can never leak into a prompt (it just loses its scope).
            if not _ANCHOR.match(line):
                cur.lines.append(line)
            i += 1
            continue
        if cur.lines and cur.text:
            chunks.append(cur)
        level = len(m.group(1))
        while stack and stack[-1][0] >= level:
            stack.pop()
        own: dict[str, frozenset] = {}
        if i + 1 < len(lines):
            am = _ANCHOR.match(lines[i + 1])
            if am:
                own = _parse_anchor(am.group(1))
                i += 1  # the anchor line is consumed (never shown to the model)
        stack.append((level, own))
        eff: dict[str, Optional[frozenset]] = {"role": None, "board": None, "kind": None}
        for _, cons in stack:
            for key, vals in cons.items():
                eff[key] = _merge(eff[key], vals)
        cur = Chunk(doc=path.name, order=len(chunks) + 1, level=level, lines=[line],
                    role=eff["role"], board=eff["board"], kind=eff["kind"])
        i += 1
    if cur.lines and cur.text:
        chunks.append(cur)
    return chunks


def _matches(chunk: Chunk, role: str, board: int) -> bool:
    if chunk.role is not None and role not in chunk.role:
        return False
    if chunk.board is not None and str(board) not in chunk.board:
        return False
    return True


def _priority(chunk: Chunk) -> int:
    """Budget rank (lower = kept first). Role-specific material beats generic;
    glossary goes last (language understanding degrades most gracefully)."""
    if chunk.doc == "anti_patterns.md":
        return 0
    if chunk.role is not None:
        return 1
    if chunk.board is not None:
        return 2
    if chunk.doc == "strategy.md":
        return 3
    if chunk.doc == "speech_examples.md":
        return 4
    return 5


def load_domain_knowledge(role: str, board: int, mode: str = "full",
                          docs_dir: Optional[Path] = None) -> str:
    """Assemble the role/board-scoped knowledge block for the system prompt.

    mode: "full" (everything that matches) | "lite" (priority-greedy within
    LITE_BUDGET chars, for small local contexts) | "off" ("").
    Deterministic: same (role, board, mode, docs) → identical string.
    """
    if mode == "off" or not role:
        return ""
    base = docs_dir or DOCS_DIR
    picked: list[tuple[int, int, Chunk]] = []  # (doc_index, order, chunk)
    for di, name in enumerate(SYSTEM_DOCS):
        for ch in _parse_doc(base / name):
            # H1 preambles are for human readers (issue links, file cross-refs) —
            # meta like that must not reach the model; content lives in ##+ sections.
            if ch.level <= 1:
                continue
            # kind-scoped sections are per-ask material — never in the system block
            if ch.kind is not None:
                continue
            if _matches(ch, role, board):
                picked.append((di, ch.order, ch))
    if mode == "lite":
        kept: set[tuple[int, int]] = set()
        total = 0
        for di, order, ch in sorted(picked, key=lambda t: (_priority(t[2]), t[0], t[1])):
            size = len(ch.text) + 2
            if total + size > LITE_BUDGET:
                continue  # skip and keep trying smaller, lower-priority chunks
            kept.add((di, order))
            total += size
        picked = [(di, order, ch) for di, order, ch in picked if (di, order) in kept]
    ordered = [ch for _, _, ch in sorted(picked, key=lambda t: (t[0], t[1]))]
    return "\n\n".join(ch.text for ch in _drop_dangling_headings(ordered))


def _drop_dangling_headings(chunks: list[Chunk]) -> list[Chunk]:
    """Drop heading-only chunks whose children were all filtered/budget-dropped —
    an empty section header reads as broken structure to the model."""
    kept: list[Chunk] = []
    for ch in reversed(chunks):
        heading_only = (ch.lines and _HEADING.match(ch.lines[0])
                        and not "\n".join(ch.lines[1:]).strip())
        if heading_only and not (kept and kept[-1].level > ch.level):
            continue  # no surviving child follows → dangling
        kept.append(ch)
    kept.reverse()
    return kept


def kind_checklist(kind: str, role: str, board: int,
                   docs_dir: Optional[Path] = None) -> str:
    """Checklist sections matching this ask's kind (and the seat's role/board)."""
    if not kind or not role:
        return ""
    base = docs_dir or DOCS_DIR
    parts = [ch.text for ch in _parse_doc(base / CHECKLIST_DOC)
             if ch.kind is not None and kind in ch.kind and _matches(ch, role, board)]
    return "\n\n".join(parts)
