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

_ROLE_HINTS = {
    "Werewolf": "藏好狼身份、配合队友误导好人；可悍跳神职、对跳、栽赃带节奏。",
    "WolfGun": "狼（带枪）：藏身份配合狼队；自爆或被票出时可开枪带人。",
    "MechanicWolf": "机械狼：独立行动、不与狼队见面，藏好身份。",
    "Seer": "预言家：每晚验人；白天敢起跳，公布查杀/金水带队找狼。",
    "Witch": "女巫：有解药/毒药；谨慎用药与跳身份，关键时毒杀。",
    "Hunter": "猎人：出局可开枪带走一人；发言有威慑，必要时亮枪。",
    "Guardian": "守卫：每晚守护一人（不可连守），隐蔽身份保关键神职。",
    "Psychic": "通灵师：每晚查具体身份；带队精准找狼。",
    "Civilian": "平民：靠逻辑找狼、保神职、理性投票。",
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
    roster: dict[int, str] = field(default_factory=dict)  # seat -> name (for 局势摘要)
    dead: set[int] = field(default_factory=set)           # seats observed out (public)
    notes: list[dict] = field(default_factory=list)       # own past <think> (scratchpad)

    # --- view (memory) ---
    def observe(self, event: dict) -> None:
        self.view.append(event)
        if event.get("etype") == "deal":
            data = event.get("data", {})
            self.role = data.get("role", self.role)
            self.faction = data.get("faction", self.faction)
        elif event.get("etype") == "narration" and event.get("vis") == "public":
            # Track who's out from the public "<name> 出局" announcements (name+space is
            # unambiguous: "P1 出局" ⊄ "P12 出局"); only scan narration, not speeches.
            text = event.get("text", "")
            for seat, name in self.roster.items():
                if seat not in self.dead and f"{name} 出局" in text:
                    self.dead.add(seat)

    def _notes_section(self) -> str:
        if not self.notes:
            return ""
        lines = ["【你此前的内心判断（仅你自己知道；保持前后一致、别自相矛盾）】"]
        for n in self.notes[-4:]:  # recent few, tail-truncated (conclusion usually at end)
            r = n.get("reasoning", "")
            if len(r) > 260:
                r = "…" + r[-260:]
            lines.append(f"- 第{n.get('day')}天·{n.get('kind')}：{r}")
        return "\n".join(lines)

    def _situation(self) -> str:
        if not self.roster:
            return ""
        alive = [self.roster[s] for s in sorted(self.roster) if s not in self.dead]
        out = [self.roster[s] for s in sorted(self.roster) if s in self.dead]
        line = "【局势】存活：" + "、".join(alive)
        if out:
            line += "｜已出局：" + "、".join(out)
        return line

    # --- decision ---
    def answer(self, ask: dict) -> tuple[dict, dict]:
        """Return (reply, trace_record) for an `ask`."""
        qtype = ask["qtype"]
        schema = self._schema(ask)
        messages = self._render(ask)
        system = self._system_prompt()

        attempts: list[dict] = []
        t0 = time.monotonic()
        comp = self.llm.complete(system, messages, schema=schema,
                                 temperature=0.5 if qtype == "speak" else 0.6)
        latency_ms = int((time.monotonic() - t0) * 1000)
        reasoning, answer = _split_think(comp.text)
        parsed, fallback = self._parse(ask, answer)

        reply: dict = {"t": "reply", "id": ask["id"], **parsed}
        if reasoning:
            reply["reasoning"] = reasoning
            # Scratchpad: remember my own private reasoning so later turns stay coherent
            # (fed back into my prompt; never broadcast). Cap memory; render recent only.
            self.notes.append({"day": ask.get("day"), "kind": ask.get("kind"),
                               "reasoning": reasoning.strip()})
            self.notes = self.notes[-12:]

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
            f"你正在玩狼人杀。你是玩家 P{self.seat}，身份：{self.role or '（待告知）'}。",
            f"目标：{goal}",
        ]
        hint = _ROLE_HINTS.get(self.role or "")
        if hint:
            lines.append(f"打法要点：{hint}")
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
        # Day/phase-segmented log so the model tracks the timeline (not a flat blob)
        # and can tell what's NEW today vs. what it already said.
        log: list[str] = []
        cur = None
        for e in self.view:
            day, phase = e.get("day"), e.get("phase")
            if day and (day, phase) != cur:
                cur = (day, phase)
                log.append(f"—— 第 {day} 天 · {'夜晚' if phase == 'Night' else '白天'} ——")
            txt = e.get("text", "")
            if txt:
                tag = "[私密] " if e.get("vis") == "private" else ""
                log.append(tag + txt)

        day = ask.get("day")
        phase = "夜晚" if ask.get("phase") == "Night" else "白天"
        body = ["【你已知的信息（按时间顺序）】"] + (log or ["（暂无）"])
        notes = self._notes_section()
        if notes:
            body += ["", notes]
        sit = self._situation()
        if sit:
            body += ["", sit]
        body += ["", f"【现在：第 {day} 天 · {phase}】需要你：{ask.get('prompt', '')}"]

        qtype = ask["qtype"]
        if qtype == "choose":
            opts = "、".join(f'P{c["seat"]}' for c in ask.get("candidates", []))
            body.append("可选：" + opts + ("；也可不选（跳过/弃权）。" if ask.get("allowSkip") else "。"))
        elif qtype == "speak":
            body.append(self._speak_hint(ask))
        return [{"role": "user", "content": "\n".join(body)}]

    def _speak_hint(self, ask: dict) -> str:
        kind = ask.get("kind", "")
        seat, day = self.seat, ask.get("day")
        if kind == "WolfChat":
            return (f"（**这是狼队密谋，只有你的狼队友能看到，不是公开发言。** 以 P{seat} 的身份"
                    f"和队友商量配合：今晚刀谁、谁去悍跳预言家/女巫、怎么对跳站边、怎么甩锅给好人。"
                    f"直接说人话，别装好人、别复述规则、别念客套话。）")
        if kind == "LastWords":
            return f"（这是你的**遗言**，全场可见。以 P{seat} 的身份留下你的判断与带队建议。）"
        # public day / campaign speech
        return (f"（**公开发言**，全场可见。现在是第 {day} 天，请**基于目前最新的信息**"
                f"（今天的死讯、此前的发言/查杀/站边）说出**新的**分析或行动建议，"
                f"**不要重复你之前说过的话**。以 P{seat} 的第一人称说，别复述规则、别加前缀。）")

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
        # speak: force {"speech": "<words>"} so the model can't drift into choose-style
        # JSON (e.g. {"choice": null}); AgentBrain extracts the string. A backend that
        # ignores schema (Fake) just returns plain text, also handled by _parse.
        return {"type": "object", "properties": {"speech": {"type": "string"}},
                "required": ["speech"]}

    def _parse(self, ask: dict, answer: str) -> tuple[dict, bool]:
        """Parse the model answer into a legal reply field; return (fields, fallback)."""
        qtype = ask["qtype"]
        if qtype == "speak":
            # Recover the speech from ANY JSON wrapping (```fences / meta-prefix /
            # "speak" or "speech" key) so the god-script is always clean, uniform text.
            obj = _loads_lenient(answer)
            if isinstance(obj, dict):
                for k in ("speech", "speak", "message", "text", "content", "say"):
                    v = obj.get(k)
                    if isinstance(v, str) and v.strip():
                        return {"text": v.strip()}, False
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
