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


class KnowledgeAndSaidTest(unittest.TestCase):
    """issue #20: domain-knowledge injection + own-speech (口径) memory."""

    def test_said_records_own_speech_and_wolfchat(self):
        # WolfChat is NOT echoed back by the engine — `said` is the only record.
        b = _brain("今晚刀 P3，我明天悍跳预言家给 P4 发查杀")
        b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak", "kind": "WolfChat",
                  "day": 1, "phase": "Night"})
        self.assertEqual(len(b.said), 1)
        msgs = b._render({"t": "ask", "id": 2, "seat": 6, "qtype": "speak", "kind": "Statement",
                          "day": 1, "phase": "Day"})
        content = msgs[0]["content"]
        self.assertIn("你自己此前的发言记录", content)
        self.assertIn("狼队夜聊", content)
        self.assertIn("悍跳预言家给 P4 发查杀", content)

    def test_said_ignores_choose_and_empty(self):
        b = _brain('{"choice": 3}')
        b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "choose", "kind": "Vote",
                  "candidates": [{"seat": 3, "name": "P3"}], "day": 1, "phase": "Day"})
        self.assertEqual(b.said, [])
        # empty speech (e.g. pure-<think> output) must not pollute the ledger
        b2 = _brain("<think>还没想好</think>")
        b2.answer({"t": "ask", "id": 2, "seat": 6, "qtype": "speak", "kind": "Statement",
                   "day": 1, "phase": "Day"})
        self.assertEqual(b2.said, [])

    def test_said_skips_wolfchat_pass(self):
        # the engine discards pass-like wolf-chat lines (isWolfChatPass) — they are
        # no commitment and must not enter the 口径 ledger either
        b = _brain("过")
        b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak", "kind": "WolfChat",
                  "day": 2, "phase": "Night"})
        self.assertEqual(b.said, [])
        # but "过" as a PUBLIC statement is a real (if lazy) speech — still recorded
        b.answer({"t": "ask", "id": 2, "seat": 6, "qtype": "speak", "kind": "Statement",
                  "day": 2, "phase": "Day"})
        self.assertEqual(len(b.said), 1)

    def test_system_prompt_gets_knowledge_and_is_stable(self):
        b = AgentBrain(seat=3, name="P3", llm=StubLlm("ok"), role="Seer", faction="Town",
                       board=1, roster={3: "P3", 5: "P5"})
        self.assertTrue(b.knowledge)
        sp1 = b._system_prompt()
        self.assertIn("【领域知识", sp1)
        self.assertIn("警徽流", sp1)          # seer knowledge includes badge-flow play
        # byte-stability must survive a whole game's worth of events and answers —
        # anything ask-dependent leaking into system would shatter the prefix cache
        b.observe({"vis": "public", "etype": "phase", "day": 1, "phase": "Day",
                   "text": "第 1 天，天亮了"})
        b.observe({"vis": "public", "etype": "narration", "text": "P5 出局（放逐）"})
        b.answer({"t": "ask", "id": 1, "seat": 3, "qtype": "speak", "kind": "Statement",
                  "day": 1, "phase": "Day"})
        b.answer({"t": "ask", "id": 2, "seat": 3, "qtype": "choose", "kind": "Inspect",
                  "candidates": [{"seat": 5, "name": "P5"}], "allowSkip": True,
                  "day": 1, "phase": "Night"})
        self.assertEqual(sp1, b._system_prompt())

    def test_knowledge_off_mode(self):
        b = AgentBrain(seat=3, name="P3", llm=StubLlm("ok"), role="Seer", faction="Town",
                       board=1, knowledge_mode="off")
        self.assertEqual(b.knowledge, "")
        self.assertNotIn("【领域知识", b._system_prompt())

    def test_knowledge_built_on_deal_event(self):
        b = AgentBrain(seat=5, name="P5", llm=StubLlm("ok"), board=2)
        self.assertEqual(b.knowledge, "")
        b.observe({"vis": "private", "seat": 5, "etype": "deal",
                   "data": {"role": "WolfGun", "faction": "Wolf"}, "text": "你的身份是：狼枪"})
        self.assertTrue(b.knowledge)
        self.assertIn("狼枪", b.knowledge)

    def test_checklist_injected_by_kind(self):
        b = AgentBrain(seat=6, name="P6", llm=StubLlm("ok"), role="Werewolf", faction="Wolf",
                       board=1)
        msgs = b._render({"t": "ask", "id": 9, "seat": 6, "qtype": "choose", "kind": "NightKill",
                          "candidates": [{"seat": 3, "name": "P3"}], "allowSkip": True,
                          "day": 1, "phase": "Night"})
        self.assertIn("【本次决策要点】", msgs[0]["content"])
        # a seer never sees wolf checklists
        s = AgentBrain(seat=3, name="P3", llm=StubLlm("ok"), role="Seer", faction="Town", board=1)
        msgs = s._render({"t": "ask", "id": 9, "seat": 3, "qtype": "choose", "kind": "NightKill",
                          "candidates": [{"seat": 6, "name": "P6"}], "day": 1, "phase": "Night"})
        self.assertNotIn("【本次决策要点】", msgs[0]["content"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
