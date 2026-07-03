"""knowledge.py unit tests: anchor parsing, role/board/kind scoping, budget, stability."""

from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from orchestrator import knowledge  # noqa: E402

GLOSSARY = """# 术语表

> 人类前言：issue #20，不该被注入。

## 通用

- **金水**：通用词条。

## 板2专属
<!-- knowledge: board=2 -->

- **狼枪**：板2词条。

## 游离锚点区

正文第一行。
<!-- knowledge: role=Witch -->
正文第二行。
"""

STRATEGY = """# 策略

## 通用打法

- 通用策略。

### 女巫
<!-- knowledge: role=Witch -->

- 女巫专属策略。

## 狼人区
<!-- knowledge: role=Werewolf,WolfGun -->

- 狼队通用内容。

### 狼枪细则
<!-- knowledge: role=WolfGun -->

- 狼枪继承内容。

### 夜刀细则
<!-- knowledge: kind=NightKill -->

- 不该进 system 的 kind 材料。

## 角色区

### 守卫
<!-- knowledge: role=Guardian -->

- 守卫内容。
"""

ANTI = """# 负面清单

## 通用

- 通用错误。
"""

SPEECH = """# 话术

### 悍跳
<!-- knowledge: role=Werewolf -->

> 我是预言家，昨晚金水 P3。
"""

CHECKLISTS = """# 决策清单

### 刀人
<!-- knowledge: kind=NightKill role=Werewolf,WolfGun -->

1. 优先刀跳神位。

### 投票
<!-- knowledge: kind=Vote -->

1. 跟真神归票。
"""


class KnowledgeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        (self.dir / "glossary.md").write_text(GLOSSARY, encoding="utf-8")
        (self.dir / "strategy.md").write_text(STRATEGY, encoding="utf-8")
        (self.dir / "anti_patterns.md").write_text(ANTI, encoding="utf-8")
        (self.dir / "speech_examples.md").write_text(SPEECH, encoding="utf-8")
        (self.dir / "checklists.md").write_text(CHECKLISTS, encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def _load(self, role, board, mode="full"):
        return knowledge.load_domain_knowledge(role, board, mode, docs_dir=self.dir)

    def test_generic_sections_for_everyone(self):
        out = self._load("Seer", 1)
        self.assertIn("金水", out)
        self.assertIn("通用策略", out)
        self.assertIn("通用错误", out)
        self.assertNotIn("板2词条", out)
        self.assertNotIn("女巫专属", out)
        self.assertNotIn("狼队通用", out)
        self.assertNotIn("<!--", out)  # anchors never reach the model

    def test_h1_preamble_never_injected(self):
        out = self._load("Seer", 1)
        self.assertNotIn("人类前言", out)
        self.assertNotIn("issue #20", out)

    def test_stray_anchor_dropped_not_leaked(self):
        # A misplaced anchor (not right below a heading) loses its scope silently
        # but must never appear in the assembled prompt text.
        out = self._load("Seer", 1)
        self.assertIn("正文第一行", out)
        self.assertIn("正文第二行", out)  # scope NOT applied (Seer still sees it)
        self.assertNotIn("<!--", out)

    def test_role_filter(self):
        self.assertIn("女巫专属策略", self._load("Witch", 1))
        wolf = self._load("Werewolf", 1)
        self.assertIn("狼队通用内容", wolf)
        self.assertIn("我是预言家", wolf)  # speech example for Werewolf
        self.assertNotIn("女巫专属", wolf)

    def test_board_filter(self):
        self.assertIn("板2词条", self._load("Seer", 2))
        self.assertNotIn("板2词条", self._load("Seer", 1))

    def test_nested_anchor_and_combines(self):
        # 狼枪细则: Werewolf,WolfGun (parent) AND WolfGun (own) => WolfGun only
        self.assertIn("狼枪继承内容", self._load("WolfGun", 1))
        self.assertNotIn("狼枪继承内容", self._load("Werewolf", 1))

    def test_kind_sections_never_in_system(self):
        for role in ("Werewolf", "Seer", "WolfGun"):
            self.assertNotIn("不该进 system", self._load(role, 1))

    def test_dangling_parent_heading_dropped(self):
        # "## 角色区" has no body of its own; when its only child (守卫) is filtered
        # out, the bare heading must not survive as an empty section.
        seer = self._load("Seer", 1)
        self.assertNotIn("角色区", seer)
        guardian = self._load("Guardian", 1)
        self.assertIn("角色区", guardian)   # child survives → parent heading kept
        self.assertIn("守卫内容", guardian)

    def test_kind_checklist(self):
        got = knowledge.kind_checklist("NightKill", "Werewolf", 1, docs_dir=self.dir)
        self.assertIn("优先刀跳神位", got)
        self.assertEqual(knowledge.kind_checklist("NightKill", "Seer", 1, docs_dir=self.dir), "")
        self.assertIn("跟真神归票", knowledge.kind_checklist("Vote", "Seer", 1, docs_dir=self.dir))
        self.assertEqual(knowledge.kind_checklist("", "Seer", 1, docs_dir=self.dir), "")

    def test_off_and_missing_role(self):
        self.assertEqual(self._load("Seer", 1, mode="off"), "")
        self.assertEqual(self._load("", 1), "")

    def test_lite_budget_prefers_role_sections(self):
        big = "- 大词条：" + "术" * 3900 + "。\n"
        (self.dir / "glossary.md").write_text("# 术语表\n\n## 大节\n\n" + big, encoding="utf-8")
        old = knowledge.LITE_BUDGET
        knowledge.LITE_BUDGET = 500
        try:
            out = self._load("Witch", 1, mode="lite")
            self.assertIn("女巫专属策略", out)   # role-matched kept first
            self.assertIn("通用错误", out)       # anti-patterns highest priority
            self.assertNotIn("大词条", out)      # oversized glossary chunk dropped
            self.assertLessEqual(len(out), 600)
        finally:
            knowledge.LITE_BUDGET = old

    def test_deterministic(self):
        self.assertEqual(self._load("Witch", 2), self._load("Witch", 2))
        a = self._load("Werewolf", 3, mode="lite")
        b = self._load("Werewolf", 3, mode="lite")
        self.assertEqual(a, b)

    def test_missing_docs_dir_is_empty_not_error(self):
        out = knowledge.load_domain_knowledge("Seer", 1, docs_dir=self.dir / "nope")
        self.assertEqual(out, "")


class RealDocsSmokeTest(unittest.TestCase):
    """The repo docs must parse: non-empty per role/board, anchors stripped."""

    def test_all_roles_boards_full(self):
        cases = [("Seer", 1), ("Witch", 1), ("Hunter", 1), ("Civilian", 1), ("Werewolf", 1),
                 ("WolfGun", 2), ("Guardian", 2), ("Psychic", 3), ("MechanicWolf", 3)]
        for role, board in cases:
            out = knowledge.load_domain_knowledge(role, board)
            self.assertTrue(out, f"empty knowledge for {role}/board{board}")
            self.assertNotIn("<!-- knowledge:", out)
            self.assertIn("金水", out)  # glossary basics reach everyone

    def test_role_scoping_on_real_docs(self):
        seer = knowledge.load_domain_knowledge("Seer", 1)
        self.assertNotIn("### 2.3 女巫", seer)      # other roles' playbooks trimmed
        witch = knowledge.load_domain_knowledge("Witch", 1)
        self.assertIn("### 2.3 女巫", witch)
        self.assertNotIn("### 4.2", witch)          # other boards' quick-refs trimmed

    def test_real_checklists(self):
        self.assertTrue(knowledge.kind_checklist("Vote", "Civilian", 1))
        self.assertTrue(knowledge.kind_checklist("NightKill", "Werewolf", 1))
        self.assertEqual(knowledge.kind_checklist("NightKill", "Seer", 1), "")
        # no anchor/meta leakage in per-ask injections, across every engine kind
        kinds = ["NightKill", "SelfDestruct", "Inspect", "Guard", "WitchSave", "WitchPoison",
                 "HunterShot", "MechanicLearn", "MechanicBigKnife", "Vote", "RunoffVote",
                 "RunForSheriff", "Withdraw", "SheriffVote", "ConsolidateSingle",
                 "BallotTarget", "BadgeTransfer", "SpeechDirection", "Statement",
                 "LastWords", "WolfChat"]
        roles = ["Werewolf", "WolfGun", "MechanicWolf", "Seer", "Witch", "Hunter",
                 "Guardian", "Psychic", "Civilian"]
        for kind in kinds:
            for role in roles:
                for board in (1, 2, 3):
                    self.assertNotIn("<!--", knowledge.kind_checklist(kind, role, board))

    def test_lite_within_budget(self):
        for role, board in [("Werewolf", 1), ("Psychic", 3)]:
            out = knowledge.load_domain_knowledge(role, board, mode="lite")
            self.assertTrue(out)
            self.assertLessEqual(len(out), knowledge.LITE_BUDGET + 200)


if __name__ == "__main__":
    unittest.main()
