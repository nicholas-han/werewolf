"""AgentBrain parsing/fallback unit tests (no engine, no model)."""

from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from orchestrator.brain import AgentBrain  # noqa: E402
from orchestrator.llm import Completion, LlmClient  # noqa: E402


class StubLlm(LlmClient):
    name = "stub"

    def __init__(self, out: str):
        self.out = out

    def complete(self, system, messages, *, schema=None, temperature=0.6, max_tokens=1024):
        return Completion(text=self.out)


def _brain(out: str, role="Werewolf", faction="Wolf") -> AgentBrain:
    return AgentBrain(seat=6, name="P6", llm=StubLlm(out), role=role, faction=faction)


class BrainParseTest(unittest.TestCase):
    def test_speak_plain_text(self):
        b = _brain("我跳预言家，昨晚验 P3 查杀。")
        reply, _ = b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak", "kind": "WolfChat"})
        self.assertEqual(reply["text"], "我跳预言家，昨晚验 P3 查杀。")

    def test_speak_unwraps_json(self):
        b = _brain('{"choice":"speak","message":"我是狼人，刀7号"}')
        reply, _ = b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak", "kind": "WolfChat"})
        self.assertEqual(reply["text"], "我是狼人，刀7号")

    def test_speak_strips_think(self):
        b = _brain("<think>该悍跳</think>我跳预言家！")
        reply, _ = b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak", "kind": "Statement"})
        self.assertEqual(reply["text"], "我跳预言家！")
        self.assertNotIn("<think>", reply["text"])

    def test_choose_valid(self):
        b = _brain('<think>…</think>{"choice": 5}')
        ask = {"t": "ask", "id": 1, "seat": 6, "qtype": "choose", "kind": "Vote",
               "candidates": [{"seat": 5, "name": "P5"}, {"seat": 7, "name": "P7"}], "allowSkip": True}
        reply, tr = b.answer(ask)
        self.assertEqual(reply["choice"], 5)
        self.assertFalse(tr["fallback"])

    def test_choose_illegal_vote_falls_back(self):
        b = _brain('{"choice": 99}')  # not a candidate
        ask = {"t": "ask", "id": 1, "seat": 6, "qtype": "choose", "kind": "Vote",
               "candidates": [{"seat": 5, "name": "P5"}, {"seat": 7, "name": "P7"}], "allowSkip": True}
        reply, tr = b.answer(ask)
        self.assertIn(reply["choice"], (5, 7))  # first non-self
        self.assertTrue(tr["fallback"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
