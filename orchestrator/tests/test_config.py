"""config.py unit tests: .env loading + werewolf.toml parsing (no engine, no model)."""

from __future__ import annotations

import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from orchestrator.config import load_config, load_dotenv  # noqa: E402


def _write(dirpath: str, name: str, content: str) -> pathlib.Path:
    p = pathlib.Path(dirpath) / name
    p.write_text(content, encoding="utf-8")
    return p


class DotenvTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def _load(self, content: str) -> list[str]:
        return load_dotenv(_write(self.tmp.name, ".env", content))

    def test_loads_keys_with_export_comments_and_quotes(self):
        for k in ("WW_TEST_A", "WW_TEST_B", "WW_TEST_C"):
            os.environ.pop(k, None)
            self.addCleanup(os.environ.pop, k, None)
        loaded = self._load(
            "# comment\n"
            "WW_TEST_A=plain\n"
            "export WW_TEST_B='quoted value'\n"
            "WW_TEST_C = \"spaced\" \n"
            "\n"
            "NOT_AN_ASSIGNMENT\n"
        )
        self.assertEqual(loaded, ["WW_TEST_A", "WW_TEST_B", "WW_TEST_C"])
        self.assertEqual(os.environ["WW_TEST_A"], "plain")
        self.assertEqual(os.environ["WW_TEST_B"], "quoted value")
        self.assertEqual(os.environ["WW_TEST_C"], "spaced")

    def test_real_environment_wins(self):
        os.environ["WW_TEST_KEEP"] = "from-shell"
        self.addCleanup(os.environ.pop, "WW_TEST_KEEP", None)
        loaded = self._load("WW_TEST_KEEP=from-dotenv\n")
        self.assertEqual(loaded, [])
        self.assertEqual(os.environ["WW_TEST_KEEP"], "from-shell")

    def test_missing_file_is_quietly_empty(self):
        self.assertEqual(load_dotenv(pathlib.Path(self.tmp.name) / "absent"), [])


class TomlConfigTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def _load(self, content: str) -> dict:
        return load_config(_write(self.tmp.name, "werewolf.toml", content))

    def test_flattens_known_sections(self):
        conf = self._load(
            "[game]\nboard = 2\nhuman_seat = 0\ninterrupts = true\n"
            "[llm]\nprovider = 'bailian'\nmodel = 'qwen-max'\ntimeout = 90\n"
            "[paths]\nout_dir = 'runs'\n"
        )
        self.assertEqual(conf["board"], 2)
        self.assertEqual(conf["human_seat"], 0)
        self.assertTrue(conf["interrupts"])
        self.assertEqual(conf["provider"], "bailian")
        self.assertEqual(conf["model"], "qwen-max")
        self.assertEqual(conf["timeout"], 90)
        self.assertEqual(conf["out_dir"], "runs")

    def test_personas_keys_become_int_seats(self):
        conf = self._load("[personas]\n1 = '谨慎逻辑流'\n5 = '激进带节奏'\n")
        self.assertEqual(conf["personas"], {1: "谨慎逻辑流", 5: "激进带节奏"})

    def test_unknown_and_mistyped_entries_are_skipped_not_fatal(self):
        conf = self._load(
            "[game]\nboard = 'two'\nnope = 1\n"      # wrong type / unknown key
            "[mystery]\nx = 1\n"                      # unknown section
            "[llm]\nprovider = 'deepseek'\njson_object = 3\n"  # int is not bool
        )
        self.assertNotIn("board", conf)
        self.assertNotIn("nope", conf)
        self.assertNotIn("x", conf)
        self.assertNotIn("json_object", conf)
        self.assertEqual(conf["provider"], "deepseek")  # valid entries survive

    def test_bool_does_not_satisfy_int(self):
        conf = self._load("[game]\nseed = true\n")
        self.assertNotIn("seed", conf)

    def test_default_path_missing_returns_empty(self):
        # load_config(None) reads the repo-root werewolf.toml; absent must mean {}.
        # (Guard: if a developer actually has one locally, skip instead of failing.)
        from orchestrator.config import CONFIG_PATH
        if CONFIG_PATH.is_file():
            self.skipTest("repo-root werewolf.toml exists locally")
        self.assertEqual(load_config(None), {})

    def test_explicit_missing_path_exits(self):
        with self.assertRaises(SystemExit):
            load_config(pathlib.Path(self.tmp.name) / "absent.toml")

    def test_broken_toml_exits(self):
        p = _write(self.tmp.name, "werewolf.toml", "[game\nboard = 1\n")
        with self.assertRaises(SystemExit):
            load_config(p)


if __name__ == "__main__":
    unittest.main()
