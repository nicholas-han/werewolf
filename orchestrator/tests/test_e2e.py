"""End-to-end: a full all-AI (FakeLlmClient) game over the real engine subprocess.

Verifies the game completes, both records are produced, and §11 fairness holds
(no brain ever sees another seat's private events / role).

Run: python3 orchestrator/tests/test_e2e.py   (or via pytest)
"""

from __future__ import annotations

import json
import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))  # repo root on path

from orchestrator.runner import Config, Orchestrator  # noqa: E402

ENGINE = pathlib.Path(__file__).resolve().parents[2] / "build" / "werewolf"


class E2ETest(unittest.TestCase):
    def setUp(self):
        if not ENGINE.exists():
            self.skipTest(f"engine not built at {ENGINE} (run cmake --build build)")

    def _run(self, board: int, seed: int, out_dir: str) -> Orchestrator:
        cfg = Config(board=board, seed=seed, human_seat=None, provider="fake",
                     engine_path=str(ENGINE), out_dir=out_dir)
        orch = Orchestrator(cfg)
        orch.run()
        return orch

    def test_full_fake_game_completes_and_records(self):
        with tempfile.TemporaryDirectory() as d:
            orch = self._run(1, 4242, d)
            self.assertIn(orch.result, ("TownWins", "WolfWins"))

            # each game writes into its own timestamped sub-dir (preserved, not overwritten)
            self.assertTrue(orch.run_dir.startswith(d))
            script = os.path.join(orch.run_dir, "god_script.md")
            trace = os.path.join(orch.run_dir, "trace.jsonl")
            self.assertTrue(os.path.exists(script))
            self.assertTrue(os.path.exists(trace))

            rows = [json.loads(l) for l in pathlib.Path(trace).read_text(encoding="utf-8").splitlines() if l.strip()]
            self.assertTrue(any(r.get("type") == "decision" for r in rows), "no decisions traced")
            results = [r for r in rows if r.get("type") == "result"]
            self.assertEqual(len(results), 1)
            self.assertIn(results[0]["result"], ("TownWins", "WolfWins"))

    def test_fairness_no_cross_seat_leak(self):
        with tempfile.TemporaryDirectory() as d:
            orch = self._run(1, 4242, d)
            self.assertTrue(orch.brains)
            for seat, brain in orch.brains.items():
                for ev in brain.view:
                    # a brain may only ever receive its OWN private events
                    if ev.get("vis") == "private":
                        self.assertEqual(ev.get("seat"), seat,
                                         f"P{seat} saw P{ev.get('seat')}'s private event")
                    # roles only ever arrive via a private deal — never publicly
                    if (ev.get("data") or {}).get("role") is not None:
                        self.assertEqual(ev.get("vis"), "private")
                        self.assertEqual(ev.get("seat"), seat)
                # the brain learned exactly one role: its own
                deals = [e for e in brain.view if e.get("etype") == "deal"]
                self.assertEqual(len(deals), 1)
                self.assertEqual(deals[0]["seat"], seat)

    def test_moderator_events_never_reach_brains(self):
        with tempfile.TemporaryDirectory() as d:
            orch = self._run(1, 99, d)
            for brain in orch.brains.values():
                self.assertFalse(any(e.get("vis") == "moderator" for e in brain.view))


    def test_each_game_preserved_in_own_dir(self):
        with tempfile.TemporaryDirectory() as d:
            o1 = self._run(1, 7, d)
            o2 = self._run(1, 7, d)  # same board+seed, run again
            self.assertNotEqual(o1.run_dir, o2.run_dir)  # not overwritten
            self.assertTrue(os.path.exists(os.path.join(o1.run_dir, "god_script.md")))
            self.assertTrue(os.path.exists(os.path.join(o2.run_dir, "god_script.md")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
