"""Orchestrator — the main loop + seat routing (ai_agents_design.md §6).

Single consumer of the engine's stdout: routes events to per-seat views, asks to
the right seat (HumanTerminal or AgentBrain), records the god-script + trace.
The engine never learns who is human vs AI — that lives entirely here.
"""

from __future__ import annotations

import datetime
import os
import pathlib
import sys
from dataclasses import dataclass, field
from typing import Callable, Optional

from orchestrator.brain import AgentBrain
from orchestrator.engine import EngineProcess
from orchestrator.llm import FakeLlmClient, LlmClient, OllamaClient
from orchestrator.recorder import Recorder

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]

# Night/secret actions: naming the seat would leak a hidden role to a human player
# (§11). Heartbeat stays generic for these; public actions show seat + a label.
_SECRET_KINDS = {"NightKill", "SelfDestruct", "Inspect", "Guard", "WitchSave",
                 "WitchPoison", "HunterShot", "MechanicLearn", "MechanicBigKnife"}
_PUBLIC_LABELS = {
    "Vote": "投票放逐", "RunoffVote": "决胜投票", "RunForSheriff": "决定是否上警",
    "Withdraw": "决定是否退水", "SheriffVote": "投票选警长", "ConsolidateSingle": "警长归票",
    "BallotTarget": "警长投票", "BadgeTransfer": "移交警徽", "SpeechDirection": "定发言方向",
}


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
    # AI backend: "ollama" (local deepseek-r1) or "fake" (deterministic, for tests).
    provider: str = "ollama"
    model: str = "deepseek-r1:14b"
    ollama_host: str = "http://localhost:11434"
    request_timeout: int = 600
    num_ctx: int = 8192
    wolf_chat_rounds: int = 2  # §5.4: max wolf night-chat rounds (natural early-stop)
    interrupts: bool = False   # §2/§7.2: per-speech 自爆/退水 interrupts (slow; default off)
    # Optional explicit per-seat factory; overrides provider/model when set (tests).
    llm_factory: Optional[Callable[[int], LlmClient]] = None


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
        self.run_dir: Optional[str] = None  # per-game output dir (set at game_start)

    def _make_llm(self, seat: int) -> LlmClient:
        if self.cfg.llm_factory is not None:
            return self.cfg.llm_factory(seat)
        if self.cfg.provider == "fake":
            return FakeLlmClient()
        return OllamaClient(self.cfg.model, self.cfg.ollama_host,
                            num_ctx=self.cfg.num_ctx, timeout=self.cfg.request_timeout)

    def _setup_seats(self, msg: dict) -> None:
        # Each game gets its own timestamped run dir, so past scripts are preserved
        # (a new script per game instead of overwriting one fixed file).
        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        game_id = f"{stamp}_board{self.cfg.board}_seed{self.cfg.seed}"
        self.run_dir = os.path.join(self.cfg.out_dir, game_id)
        os.makedirs(self.run_dir, exist_ok=True)
        self.recorder = Recorder(
            os.path.join(self.run_dir, "god_script.md"),
            os.path.join(self.run_dir, "trace.jsonl"),
            game_id,
        )
        self.recorder.on_game_start(msg)
        roster = {s["seat"]: s["name"] for s in msg.get("seats", [])}
        for s in msg.get("seats", []):
            seat, name = s["seat"], s["name"]
            if seat == self.cfg.human_seat:
                self.human = HumanTerminal(seat, name)
            else:
                self.brains[seat] = AgentBrain(
                    seat=seat, name=name, llm=self._make_llm(seat),
                    persona=self.cfg.personas.get(seat, ""), roster=roster,
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

    def _heartbeat(self, ask: dict) -> None:
        """Progress line for an AI decision. Public actions show seat + a label;
        secret night actions stay generic so a human player learns no hidden role."""
        kind = ask.get("kind", "")
        seat = ask["seat"]
        if ask.get("qtype") == "speak":
            if kind == "WolfChat":
                print("· 有玩家在行动…（请稍候）", file=sys.stderr, flush=True)
            else:
                label = "留遗言" if kind == "LastWords" else "发言"
                print(f"· P{seat} {label}中…", file=sys.stderr, flush=True)
            return
        if kind in _SECRET_KINDS:
            print("· 有玩家在行动…（请稍候）", file=sys.stderr, flush=True)
        else:
            print(f"· P{seat} {_PUBLIC_LABELS.get(kind, '思考')}中…", file=sys.stderr, flush=True)

    def _answer(self, ask: dict) -> dict:
        seat = ask["seat"]
        if seat in self.brains:
            self._heartbeat(ask)  # so silent AI stretches don't look frozen (§11-safe)
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
                               self.cfg.ask_timeout, self.cfg.wolf_chat_rounds,
                               self.cfg.interrupts)
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
