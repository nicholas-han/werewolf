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

    def test_speak_never_leaks_unterminated_think(self):
        # Truncated reasoning (no closing </think>) must NOT be broadcast (§11).
        b = _brain("<think>我是狼人，应该悍跳预言家骗信任，刀掉真预言")
        reply, _ = b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak", "kind": "Statement"})
        self.assertNotIn("<think>", reply["text"])
        self.assertNotIn("我是狼人", reply["text"])  # reasoning stripped, not spoken
        self.assertEqual(reply["text"], "")           # nothing left to say -> 过

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

    def test_choose_bool_is_not_seat_one(self):
        # bool ⊂ int in Python: {"choice": true} must NOT be accepted as seat 1.
        b = _brain('{"choice": true}')
        ask = {"t": "ask", "id": 1, "seat": 6, "qtype": "choose", "kind": "Vote",
               "candidates": [{"seat": 1, "name": "P1"}, {"seat": 7, "name": "P7"}], "allowSkip": True}
        reply, tr = b.answer(ask)
        self.assertTrue(tr["fallback"])
        self.assertIn(reply["choice"], (1, 7))  # via fallback, never because True == 1

    def test_speak_extracts_from_fenced_json_with_meta(self):
        # god-script consistency: meta-prefix + ```json fence + "speak" key -> clean text
        out = '好的，作为P8……\n```json\n{"speak": "我跳预言家，验 P3 金水"}\n```'
        b = _brain(out)
        reply, _ = b.answer({"t": "ask", "id": 1, "seat": 8, "qtype": "speak", "kind": "Statement"})
        self.assertEqual(reply["text"], "我跳预言家，验 P3 金水")

    def test_situation_tracks_public_deaths_only(self):
        b = AgentBrain(seat=2, name="P2", llm=StubLlm(""), roster={1: "P1", 2: "P2", 3: "P3"})
        b.observe({"vis": "public", "etype": "narration", "text": "P1 出局（放逐）"})
        b.observe({"vis": "public", "etype": "speech", "text": "P3 出局后我们就赢了"})  # speech ≠ death
        self.assertEqual(b.dead, {1})  # only P1; the speech mentioning "P3 出局" must not count
        sit = b._situation()
        self.assertIn("已出局", sit)
        self.assertIn("P1", sit)


    def test_scratchpad_remembers_and_feeds_back(self):
        b = _brain("<think>我怀疑P5是狼，明天推他</think>我是预言家，查杀P5")
        b.answer({"t": "ask", "id": 1, "seat": 8, "qtype": "speak", "kind": "Statement",
                  "day": 1, "phase": "Day"})
        self.assertTrue(any("我怀疑P5是狼" in n["reasoning"] for n in b.notes))
        # a later turn's prompt carries the prior private reasoning (coherence)
        msgs = b._render({"t": "ask", "id": 2, "seat": 8, "qtype": "speak", "kind": "Statement",
                          "day": 2, "phase": "Day"})
        content = msgs[0]["content"]
        self.assertIn("此前的内心判断", content)
        self.assertIn("我怀疑P5是狼", content)


if __name__ == "__main__":
    unittest.main(verbosity=2)
