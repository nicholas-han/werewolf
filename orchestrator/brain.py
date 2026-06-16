"""AgentBrain — one AI seat (ai_agents_design.md §7.3/§7.4).

Holds the seat's accumulated view (public events + its own private events), turns
each `ask` into a model call, and parses the output into a LEGAL reply (with a
deterministic fallback so the game never stalls). The model's private reasoning
(incl. an R1 `<think>` block) is kept for the trace and NEVER broadcast.
"""

from __future__ import annotations

import json
import re
import time
from dataclasses import dataclass, field
from typing import Optional

from orchestrator.llm import LlmClient

_THINK = re.compile(r"<think>(.*?)</think>", re.DOTALL)

_FACTION_GOAL = {
    "Wolf": "狼人阵营：刀杀/误导，淘汰所有神或所有民。",
    "Town": "好人阵营：找出并票出所有狼人。",
}


def _split_think(text: str) -> tuple[str, str]:
    """Return (reasoning, answer): strip a leading <think>…</think> (R1)."""
    m = _THINK.search(text)
    if not m:
        return "", text.strip()
    reasoning = m.group(1).strip()
    answer = (text[: m.start()] + text[m.end():]).strip()
    return reasoning, answer


@dataclass
class AgentBrain:
    seat: int
    name: str
    llm: LlmClient
    persona: str = ""
    role: Optional[str] = None      # set from the private deal event
    faction: Optional[str] = None
    view: list[dict] = field(default_factory=list)   # events visible to this seat

    # --- view (memory) ---
    def observe(self, event: dict) -> None:
        self.view.append(event)
        if event.get("etype") == "deal":
            data = event.get("data", {})
            self.role = data.get("role", self.role)
            self.faction = data.get("faction", self.faction)

    # --- decision ---
    def answer(self, ask: dict) -> tuple[dict, dict]:
        """Return (reply, trace_record) for an `ask`."""
        qtype = ask["qtype"]
        schema = self._schema(ask)
        messages = self._render(ask)
        system = self._system_prompt()

        attempts: list[dict] = []
        t0 = time.monotonic()
        comp = self.llm.complete(system, messages, schema=schema)
        latency_ms = int((time.monotonic() - t0) * 1000)
        reasoning, answer = _split_think(comp.text)
        parsed, fallback = self._parse(ask, answer)

        reply: dict = {"t": "reply", "id": ask["id"], **parsed}
        if reasoning:
            reply["reasoning"] = reasoning

        trace = {
            "attempts": attempts,
            "seat": self.seat, "role": self.role, "day": ask.get("day"),
            "phase": ask.get("phase"), "qtype": qtype, "kind": ask.get("kind"),
            "visible_context": [
                {"vis": e.get("vis"), "etype": e.get("etype"), "text": e.get("text", "")}
                for e in self.view
            ],
            "prompt_sent": messages,
            "model": self.llm.name,
            "raw_output": comp.text,
            "reasoning": reasoning,
            "parsed": parsed,
            "fallback": fallback,
            "latency_ms": latency_ms,
            "usage": comp.usage,
        }
        return reply, trace

    # --- helpers ---
    def _system_prompt(self) -> str:
        goal = _FACTION_GOAL.get(self.faction or "", "帮你所在的阵营获胜。")
        lines = [
            f"你正在玩狼人杀（9 人局）。你是玩家 P{self.seat}，身份：{self.role or '（待告知）'}。",
            f"目标：{goal}",
        ]
        if self.persona:
            lines.append(f"风格：{self.persona}")
        lines += [
            "你可以先在 <think></think> 里思考；思考结束后再给出结果。输出约定（务必遵守）：",
            '- 选择类(choose)：思考后，最后必须单独输出一行 JSON，如 {"choice": 座位号} 或 {"choice": null}（跳过/弃权/空刀）。',
            '- 是非类(confirm)：思考后，最后必须单独输出一行 JSON：{"decision": true} 或 {"decision": false}。',
            "- 发言类(speak)：直接以第一人称说出你要当众讲的话，像在牌桌上发言；不要解释、不要复述规则、不要加“发言内容：”之类前缀。",
        ]
        return "\n".join(lines)

    def _render(self, ask: dict) -> list[dict]:
        log = []
        for e in self.view:
            tag = ""
            if e.get("vis") == "private":
                tag = "[私] "
            txt = e.get("text", "")
            if txt:
                log.append(tag + txt)
        body = ["【你已知的信息】"] + (log or ["（暂无）"]) + ["", "【当前需要你决定】", ask.get("prompt", "")]
        if ask["qtype"] == "choose":
            opts = "、".join(f'P{c["seat"]}' for c in ask.get("candidates", []))
            body.append(f"可选：{opts}" + ("；也可不选（跳过/弃权）。" if ask.get("allowSkip") else "。"))
        elif ask["qtype"] == "speak":
            body.append(f"（请直接以 P{self.seat} 的第一人称说出这句话，不要解释、不要加前缀。）")
        return [{"role": "user", "content": "\n".join(body)}]

    @staticmethod
    def _schema(ask: dict) -> Optional[dict]:
        qtype = ask["qtype"]
        if qtype == "choose":
            seats = [c["seat"] for c in ask.get("candidates", [])]
            enum = seats + ([None] if ask.get("allowSkip") else [])
            return {"type": "object", "properties": {"choice": {"enum": enum}}, "required": ["choice"]}
        if qtype == "confirm":
            return {"type": "object", "properties": {"decision": {"type": "boolean"}},
                    "required": ["decision"]}
        return None  # speak: free text

    def _parse(self, ask: dict, answer: str) -> tuple[dict, bool]:
        """Parse the model answer into a legal reply field; return (fields, fallback)."""
        qtype = ask["qtype"]
        if qtype == "speak":
            return {"text": answer.strip()}, False

        obj = _loads_lenient(answer)
        if qtype == "confirm":
            if isinstance(obj, dict) and isinstance(obj.get("decision"), bool):
                return {"decision": obj["decision"]}, False
            return {"decision": False}, True

        # choose
        seats = [c["seat"] for c in ask.get("candidates", [])]
        allow_skip = bool(ask.get("allowSkip"))
        vote_like = ask.get("kind") in ("Vote", "RunoffVote", "SheriffVote", "BallotTarget")

        def fallback() -> dict:
            if not seats:
                return {"choice": None}
            if vote_like:
                return {"choice": next((s for s in seats if s != self.seat), seats[0])}
            if allow_skip:
                return {"choice": None}
            return {"choice": next((s for s in seats if s != self.seat), seats[0])}

        if isinstance(obj, dict) and "choice" in obj:
            ch = obj["choice"]
            if ch is None:
                return ({"choice": None}, False) if allow_skip else (fallback(), True)
            if isinstance(ch, int) and ch in seats:
                return {"choice": ch}, False
        return fallback(), True


def _loads_lenient(s: str):
    """Best-effort: parse JSON, else extract the first {...} object."""
    s = s.strip()
    try:
        return json.loads(s)
    except Exception:
        m = re.search(r"\{.*\}", s, re.DOTALL)
        if m:
            try:
                return json.loads(m.group(0))
            except Exception:
                return None
    return None
