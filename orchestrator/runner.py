"""Orchestrator — the main loop + seat routing (ai_agents_design.md §6).

Single consumer of the engine's stdout: routes events to per-seat views, asks to
the right seat (HumanTerminal or AgentBrain), records the god-script + trace.
The engine never learns who is human vs AI — that lives entirely here.
"""

from __future__ import annotations

import os
import pathlib
from dataclasses import dataclass, field
from typing import Callable, Optional

from orchestrator.brain import AgentBrain
from orchestrator.engine import EngineProcess
from orchestrator.llm import FakeLlmClient, LlmClient
from orchestrator.recorder import Recorder

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


def _default_engine_path() -> str:
    return str(_REPO_ROOT / "build" / "werewolf")


@dataclass
class Config:
    board: int = 1
    seed: int = 12345
    human_seat: Optional[int] = None
    engine_path: str = field(default_factory=_default_engine_path)
    ask_timeout: int = 600
    out_dir: str = field(default_factory=lambda: str(_REPO_ROOT / "games"))
    personas: dict[int, str] = field(default_factory=dict)
    # seat -> LlmClient factory; default = FakeLlmClient for every AI seat.
    llm_factory: Callable[[int], LlmClient] = field(default=lambda seat: FakeLlmClient())


class HumanTerminal:
    """A human seat: prints its visible events, prompts for each decision."""

    def __init__(self, seat: int, name: str):
        self.seat = seat
        self.name = name
        self.role: Optional[str] = None

    def observe(self, event: dict) -> None:
        if event.get("etype") == "deal":
            self.role = (event.get("data") or {}).get("role")
        text = event.get("text", "")
        if text:
            tag = "（私密）" if event.get("vis") == "private" else ""
            print(f"{tag}{text}")

    def answer(self, ask: dict) -> dict:
        rid = ask["id"]
        print(f"\n>>> 轮到你（P{self.seat}）：{ask.get('prompt','')}")
        qtype = ask["qtype"]
        if qtype == "choose":
            opts = ", ".join(f'{c["seat"]}={c["name"]}' for c in ask.get("candidates", []))
            tail = "（直接回车=跳过/弃权）" if ask.get("allowSkip") else ""
            print(f"    可选: {opts} {tail}")
            s = input("    输入座位号> ").strip()
            if s == "" and ask.get("allowSkip"):
                return {"t": "reply", "id": rid, "choice": None}
            try:
                return {"t": "reply", "id": rid, "choice": int(s)}
            except ValueError:
                return {"t": "reply", "id": rid, "choice": None}
        if qtype == "confirm":
            s = input("    (y/n)> ").strip().lower()
            return {"t": "reply", "id": rid, "decision": s.startswith("y")}
        return {"t": "reply", "id": rid, "text": input("    发言> ")}


class Orchestrator:
    def __init__(self, cfg: Config):
        self.cfg = cfg
        self.brains: dict[int, AgentBrain] = {}
        self.human: Optional[HumanTerminal] = None
        self.recorder: Optional[Recorder] = None
        self.result: Optional[str] = None

    def _setup_seats(self, msg: dict) -> None:
        os.makedirs(self.cfg.out_dir, exist_ok=True)
        game_id = f"board{self.cfg.board}-seed{self.cfg.seed}"
        self.recorder = Recorder(
            os.path.join(self.cfg.out_dir, "god_script.md"),
            os.path.join(self.cfg.out_dir, "trace.jsonl"),
            game_id,
        )
        self.recorder.on_game_start(msg)
        for s in msg.get("seats", []):
            seat, name = s["seat"], s["name"]
            if seat == self.cfg.human_seat:
                self.human = HumanTerminal(seat, name)
            else:
                self.brains[seat] = AgentBrain(
                    seat=seat, name=name, llm=self.cfg.llm_factory(seat),
                    persona=self.cfg.personas.get(seat, ""),
                )

    def _route_event(self, ev: dict) -> None:
        vis = ev.get("vis")
        if vis == "public":
            for b in self.brains.values():
                b.observe(ev)
            if self.human:
                self.human.observe(ev)
        elif vis == "private":
            seat = ev.get("seat")
            if seat in self.brains:
                self.brains[seat].observe(ev)
            elif self.human and self.human.seat == seat:
                self.human.observe(ev)
        # moderator: recorder only (never a player view)

    def _answer(self, ask: dict) -> dict:
        seat = ask["seat"]
        if seat in self.brains:
            reply, trace = self.brains[seat].answer(ask)
            assert self.recorder is not None
            self.recorder.on_decision(ask, reply, trace)
            return reply
        if self.human and self.human.seat == seat:
            return self.human.answer(ask)
        # Unknown seat: safe default so the engine never stalls.
        rid = ask["id"]
        if ask["qtype"] == "choose":
            return {"t": "reply", "id": rid, "choice": None}
        if ask["qtype"] == "confirm":
            return {"t": "reply", "id": rid, "decision": False}
        return {"t": "reply", "id": rid, "text": ""}

    def run(self) -> str:
        engine = EngineProcess(self.cfg.engine_path, self.cfg.board, self.cfg.seed,
                               self.cfg.ask_timeout)
        try:
            for msg in engine.messages():
                t = msg.get("t")
                if t == "game_start":
                    self._setup_seats(msg)
                elif t == "event":
                    if self.recorder:
                        self.recorder.on_event(msg)
                    self._route_event(msg)
                elif t == "ask":
                    engine.send(self._answer(msg))
                elif t == "game_over":
                    if self.recorder:
                        self.recorder.on_game_over(msg)
                    self.result = msg.get("result")
                    break
        finally:
            if self.recorder:
                self.recorder.close()
            engine.close()
        return self.result or "Ongoing"


def run_game(cfg: Config) -> Orchestrator:
    """Run one game to completion; returns the Orchestrator (holds result + brains)."""
    orch = Orchestrator(cfg)
    orch.run()
    return orch
