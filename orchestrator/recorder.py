"""Recorder — the two M15 artifacts (ai_agents_design.md §8).

- god_script.md : human-readable god-view replay (ALL events incl. moderator).
- trace.jsonl   : one line per AI decision (visible context -> reasoning ->
                  action -> outcome) for post-training; closing line = result.
"""

from __future__ import annotations

import json
from typing import Optional, TextIO


class Recorder:
    def __init__(self, script_path: str, trace_path: str, game_id: str):
        self.script: TextIO = open(script_path, "w", encoding="utf-8")
        self.trace: TextIO = open(trace_path, "w", encoding="utf-8")
        self.game_id = game_id
        self.names: dict[int, str] = {}
        self.roles: dict[int, str] = {}
        self.factions: dict[int, str] = {}
        self.died_day: dict[int, int] = {}
        self._section: Optional[tuple] = None
        self._wolfchat_seen: set[tuple] = set()

    # --- lifecycle ---
    def on_game_start(self, msg: dict) -> None:
        self.names = {s["seat"]: s["name"] for s in msg.get("seats", [])}
        self.script.write(f"# 上帝视角复盘 — {msg.get('board')} (seed={msg.get('seed')})\n\n")
        self.script.flush()

    def on_game_over(self, msg: dict) -> None:
        result = msg.get("result", "Ongoing")
        self.script.write(f"\n## 终局\n\n**{result}**\n")
        self.script.flush()
        win_faction = "Town" if result == "TownWins" else ("Wolf" if result == "WolfWins" else None)
        per_seat = {
            str(seat): {
                "role": self.roles.get(seat),
                "faction": self.factions.get(seat),
                "died_day": self.died_day.get(seat),
                "won": (self.factions.get(seat) == win_faction) if win_faction else None,
            }
            for seat in self.names
        }
        self.trace.write(json.dumps(
            {"game_id": self.game_id, "type": "result", "result": result, "per_seat": per_seat},
            ensure_ascii=False) + "\n")
        self.trace.flush()

    def close(self) -> None:
        for f in (self.script, self.trace):
            try:
                f.close()
            except Exception:
                pass

    # --- events ---
    def on_event(self, ev: dict) -> None:
        etype = ev.get("etype")
        data = ev.get("data", {}) or {}
        if etype == "deal":
            seat = ev.get("seat")
            if seat is not None:
                self.roles[seat] = data.get("role")
                self.factions[seat] = data.get("faction")
        # Death is announced as public narration ("<name> 出局…"), not a structured
        # event (protocol_v1.md §6.2), so track died_day by matching seat names —
        # name+space is unambiguous ("P1 出局" ⊄ "P12 出局"), same rule AgentBrain uses.
        if etype == "narration" and ev.get("vis") == "public":
            text = ev.get("text", "")
            for seat, name in self.names.items():
                if seat not in self.died_day and f"{name} 出局" in text:
                    self.died_day[seat] = ev.get("day")

        # Wolf-chat is emitted privately to each wolf; collapse to one script line.
        if etype == "speech" and data.get("kind") == "WolfChat":
            key = (ev.get("day"), data.get("speaker"), ev.get("text"))
            if key in self._wolfchat_seen:
                return
            self._wolfchat_seen.add(key)

        self._section_header(ev.get("day"), ev.get("phase"))
        self.script.write(self._format(ev) + "\n")
        self.script.flush()

    def on_decision(self, ask: dict, reply: dict, trace: dict) -> None:
        rec = {"game_id": self.game_id, "type": "decision", **trace}
        self.trace.write(json.dumps(rec, ensure_ascii=False) + "\n")
        self.trace.flush()

    # --- formatting ---
    def _section_header(self, day, phase) -> None:
        if day is None:
            return
        sec = (day, phase)
        if sec != self._section:
            self._section = sec
            label = {"Night": "夜", "Day": "白天"}.get(phase or "", phase or "")
            self.script.write(f"\n## 第 {day} 天 · {label}\n\n")

    def _format(self, ev: dict) -> str:
        vis = ev.get("vis")
        text = ev.get("text", "")
        if vis == "moderator":
            return f"- 〔上帝〕{text}"
        if vis == "private":
            seat = ev.get("seat")
            who = self.names.get(seat, f"P{seat}")
            return f"- 〔私·{who}〕{text}"
        return f"- {text}"
