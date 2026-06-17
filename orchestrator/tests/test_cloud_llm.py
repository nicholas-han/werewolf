"""Cloud LLM backends — unit tests (no network).

Covers the M16 work: the provider-preset registry (Bailian-first), the
OpenAI-compatible client (request shape, auth header, reasoning_content split,
json_object gating, retry/error → legal fallback), the Anthropic client, the
factory, and the end-to-end §11 guarantee that a reasoning model's chain-of-thought
NEVER reaches a spoken line when it flows through AgentBrain.

Run: python3 -m unittest orchestrator.tests.test_cloud_llm
"""

from __future__ import annotations

import io
import json
import pathlib
import sys
import unittest
import urllib.error
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from orchestrator import cloud  # noqa: E402
from orchestrator.brain import AgentBrain  # noqa: E402
from orchestrator.cloud import (AnthropicClient, MissingApiKey,  # noqa: E402
                                OpenAICompatClient, ProviderPreset, build_client,
                                provider_choices)
from orchestrator.runner import Config, Orchestrator  # noqa: E402

_OPENAI_OK = {"choices": [{"message": {"content": "hello"}}],
              "usage": {"prompt_tokens": 11, "completion_tokens": 7}}


def _stub_request(client, payload, captured=None):
    """Replace client._request with one that records the call and returns payload."""
    def fake(url, headers, body):
        if captured is not None:
            captured.update(url=url, headers=headers, body=body)
        return payload
    client._request = fake  # type: ignore[assignment]
    return client


class PresetRegistryTest(unittest.TestCase):
    def test_bailian_is_priority_preset(self):
        p = cloud.PROVIDER_PRESETS["bailian"]
        self.assertEqual(p.base_url, "https://dashscope.aliyuncs.com/compatible-mode/v1")
        self.assertEqual(p.api_key_env, "DASHSCOPE_API_KEY")
        self.assertEqual(p.default_model, "qwen-plus")
        self.assertEqual(p.kind, "openai")

    def test_dashscope_is_alias_of_bailian(self):
        self.assertIs(cloud.PROVIDER_PRESETS["dashscope"], cloud.PROVIDER_PRESETS["bailian"])

    def test_choices_include_presets_and_generic(self):
        choices = provider_choices()
        for name in ("bailian", "openai", "deepseek", "moonshot", "zhipu", "anthropic"):
            self.assertIn(name, choices)
        self.assertIn("openai-compat", choices)

    def test_anthropic_preset_is_anthropic_kind(self):
        self.assertEqual(cloud.PROVIDER_PRESETS["anthropic"].kind, "anthropic")


class FactoryTest(unittest.TestCase):
    def test_bailian_builds_openai_compat_with_preset_defaults(self):
        c = build_client("bailian")
        self.assertIsInstance(c, OpenAICompatClient)
        self.assertEqual(c.base_url, "https://dashscope.aliyuncs.com/compatible-mode/v1")
        self.assertEqual(c.model, "qwen-plus")
        self.assertEqual(c.api_key_env, "DASHSCOPE_API_KEY")
        self.assertEqual(c.name, "bailian:qwen-plus")

    def test_overrides_take_effect(self):
        c = build_client("bailian", model="qwen-max", base_url="https://x/v1",
                         api_key_env="MY_KEY")
        self.assertEqual(c.model, "qwen-max")
        self.assertEqual(c.base_url, "https://x/v1")
        self.assertEqual(c.api_key_env, "MY_KEY")

    def test_anthropic_builds_anthropic_client(self):
        c = build_client("anthropic")
        self.assertIsInstance(c, AnthropicClient)
        self.assertEqual(c.api_key_env, "ANTHROPIC_API_KEY")

    def test_unknown_provider_raises(self):
        with self.assertRaises(ValueError):
            build_client("not-a-provider")

    def test_generic_requires_base_url_and_key_env(self):
        with self.assertRaises(ValueError):
            build_client("openai-compat", model="m")  # missing base_url/api_key_env
        c = build_client("openai-compat", model="m", base_url="https://e/v1",
                         api_key_env="MY_KEY")
        self.assertIsInstance(c, OpenAICompatClient)
        self.assertEqual(c.name, "openai-compat:m")


class OpenAICompatRequestTest(unittest.TestCase):
    def _client(self, **kw):
        return OpenAICompatClient(base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
                                  model="qwen-plus", api_key_env="DASHSCOPE_API_KEY", **kw)

    def test_request_shape_and_auth(self):
        c = self._client()
        captured: dict = {}
        _stub_request(c, _OPENAI_OK, captured)
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            comp = c.complete("SYS", [{"role": "user", "content": "U"}],
                              schema={"properties": {"choice": {}}})
        self.assertTrue(captured["url"].endswith("/chat/completions"))
        self.assertEqual(captured["headers"]["Authorization"], "Bearer sk-test")
        self.assertEqual(captured["body"]["model"], "qwen-plus")
        self.assertEqual(captured["body"]["messages"][0], {"role": "system", "content": "SYS"})
        self.assertEqual(captured["body"]["messages"][1]["content"], "U")
        self.assertEqual(comp.text, "hello")
        self.assertEqual(comp.usage, {"prompt_tokens": 11, "completion_tokens": 7})

    def test_reasoning_content_wrapped_as_think(self):
        c = self._client()
        payload = {"choices": [{"message": {"content": "我跳预言家。",
                                            "reasoning_content": "其实我是狼"}}]}
        _stub_request(c, payload)
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            comp = c.complete("SYS", [{"role": "user", "content": "U"}])
        self.assertEqual(comp.text, "<think>其实我是狼</think>我跳预言家。")

    def test_json_object_only_for_choose_confirm(self):
        c = self._client(json_object=True)
        captured: dict = {}
        _stub_request(c, _OPENAI_OK, captured)
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            c.complete("SYS", [{"role": "user", "content": "U"}],
                       schema={"properties": {"choice": {"enum": [1, None]}}})
            self.assertEqual(captured["body"]["response_format"], {"type": "json_object"})
            captured.clear()
            # speak carries a {"speech": ...} schema — must NOT be forced into JSON
            c.complete("SYS", [{"role": "user", "content": "U"}],
                       schema={"properties": {"speech": {"type": "string"}}})
            self.assertNotIn("response_format", captured["body"])

    def test_json_object_off_by_default(self):
        c = self._client()  # json_object defaults False
        captured: dict = {}
        _stub_request(c, _OPENAI_OK, captured)
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            c.complete("SYS", [{"role": "user", "content": "U"}],
                       schema={"properties": {"choice": {}}})
        self.assertNotIn("response_format", captured["body"])

    def test_max_tokens_takes_the_larger(self):
        c = self._client(max_tokens=2048)
        captured: dict = {}
        _stub_request(c, _OPENAI_OK, captured)
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            c.complete("SYS", [{"role": "user", "content": "U"}], max_tokens=1024)
        self.assertEqual(captured["body"]["max_tokens"], 2048)


class MissingKeyTest(unittest.TestCase):
    def test_health_reports_missing_env(self):
        c = build_client("bailian")
        with mock.patch.dict("os.environ", {}, clear=True):
            ok, msg = c.health()
        self.assertFalse(ok)
        self.assertIn("DASHSCOPE_API_KEY", msg)

    def test_complete_without_key_falls_back_to_empty(self):
        c = build_client("bailian")
        with mock.patch.dict("os.environ", {}, clear=True):
            comp = c.complete("SYS", [{"role": "user", "content": "U"}])
        self.assertEqual(comp.text, "")  # empty → AgentBrain applies legal default
        self.assertIn("missing", comp.raw.get("error", ""))

    def test_api_key_raises_when_unset(self):
        c = build_client("bailian")
        with mock.patch.dict("os.environ", {}, clear=True):
            with self.assertRaises(MissingApiKey):
                c._api_key()


class _FakeResp:
    def __init__(self, payload):
        self._b = json.dumps(payload).encode("utf-8")

    def read(self):
        return self._b

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class RetryAndErrorTest(unittest.TestCase):
    def setUp(self):
        # no real sleeping during retry tests
        patcher = mock.patch.object(cloud._HttpBackend, "_sleep_backoff",
                                    staticmethod(lambda attempt: None))
        patcher.start()
        self.addCleanup(patcher.stop)
        self.c = OpenAICompatClient(base_url="https://e/v1", model="m",
                                    api_key_env="K", max_retries=2)

    def _http_error(self, code, body=b"err"):
        return urllib.error.HTTPError("https://e/v1/chat/completions", code, "x", {},
                                      io.BytesIO(body))

    def test_retries_on_429_then_succeeds(self):
        calls = {"n": 0}

        def fake_urlopen(req, timeout=None):
            calls["n"] += 1
            if calls["n"] == 1:
                raise self._http_error(429, b"slow down")
            return _FakeResp(_OPENAI_OK)

        with mock.patch("orchestrator.cloud.urllib.request.urlopen", fake_urlopen):
            out = self.c._request("https://e/v1/chat/completions", {}, {"a": 1})
        self.assertEqual(calls["n"], 2)
        self.assertEqual(out["choices"][0]["message"]["content"], "hello")

    def test_non_retriable_4xx_raises_immediately(self):
        calls = {"n": 0}

        def fake_urlopen(req, timeout=None):
            calls["n"] += 1
            raise self._http_error(400, b"bad request")

        with mock.patch("orchestrator.cloud.urllib.request.urlopen", fake_urlopen):
            with self.assertRaises(cloud.ApiError) as cm:
                self.c._request("https://e/v1/chat/completions", {}, {"a": 1})
        self.assertFalse(cm.exception.retriable)
        self.assertEqual(calls["n"], 1)  # 400 is permanent — no retry

    def test_complete_maps_api_error_to_empty_completion(self):
        def fake_urlopen(req, timeout=None):
            raise self._http_error(500, b"boom")

        with mock.patch.dict("os.environ", {"K": "sk-test"}), \
                mock.patch("orchestrator.cloud.urllib.request.urlopen", fake_urlopen):
            comp = self.c.complete("SYS", [{"role": "user", "content": "U"}])
        self.assertEqual(comp.text, "")  # 5xx after retries → legal fallback, game never stalls


class AnthropicClientTest(unittest.TestCase):
    def test_request_shape_and_parse(self):
        c = build_client("anthropic", model="claude-x")
        captured: dict = {}
        payload = {"content": [{"type": "thinking", "text": "secret cot"},
                               {"type": "text", "text": "我跳预言家。"}],
                   "usage": {"input_tokens": 5, "output_tokens": 9}}
        _stub_request(c, payload, captured)
        with mock.patch.dict("os.environ", {"ANTHROPIC_API_KEY": "sk-ant"}):
            comp = c.complete("SYS", [{"role": "user", "content": "U"}])
        self.assertTrue(captured["url"].endswith("/messages"))
        self.assertEqual(captured["headers"]["x-api-key"], "sk-ant")
        self.assertIn("anthropic-version", captured["headers"])
        self.assertEqual(captured["body"]["system"], "SYS")  # system is a top-level param
        # only text blocks are spoken; thinking blocks are dropped
        self.assertEqual(comp.text, "我跳预言家。")
        self.assertEqual(comp.usage, {"prompt_tokens": 5, "completion_tokens": 9})


class FactoryWiringTest(unittest.TestCase):
    def test_orchestrator_make_llm_builds_cloud_client(self):
        cfg = Config(provider="bailian", model="qwen-max")
        orch = Orchestrator(cfg)
        client = orch._make_llm(seat=3)
        self.assertIsInstance(client, OpenAICompatClient)
        self.assertEqual(client.model, "qwen-max")


class BrainCloudIntegrationTest(unittest.TestCase):
    """The §11 guarantee end-to-end: reasoning_content must never be spoken."""

    def _brain_with(self, payload):
        c = OpenAICompatClient(base_url="https://x/v1", model="qwen-plus",
                               api_key_env="DASHSCOPE_API_KEY")
        _stub_request(c, payload)
        return AgentBrain(seat=6, name="P6", llm=c, role="Werewolf", faction="Wolf")

    def test_reasoning_content_never_leaks_into_speech(self):
        payload = {"choices": [{"message": {
            "content": "我跳预言家，昨晚验 P3 金水。",
            "reasoning_content": "其实我是狼人，悍跳骗信任，真预言在 P3"}}]}
        b = self._brain_with(payload)
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            reply, trace = b.answer({"t": "ask", "id": 1, "seat": 6, "qtype": "speak",
                                     "kind": "Statement", "day": 1, "phase": "Day"})
        self.assertEqual(reply["text"], "我跳预言家，昨晚验 P3 金水。")
        self.assertNotIn("狼人", reply["text"])           # reasoning stripped
        self.assertIn("其实我是狼人", trace["reasoning"])  # but kept for the trace

    def test_choose_parses_through_cloud(self):
        payload = {"choices": [{"message": {"content": '{"choice": 5}'}}]}
        b = self._brain_with(payload)
        ask = {"t": "ask", "id": 2, "seat": 6, "qtype": "choose", "kind": "Vote",
               "candidates": [{"seat": 5, "name": "P5"}, {"seat": 7, "name": "P7"}],
               "allowSkip": True}
        with mock.patch.dict("os.environ", {"DASHSCOPE_API_KEY": "sk-test"}):
            reply, tr = b.answer(ask)
        self.assertEqual(reply["choice"], 5)
        self.assertFalse(tr["fallback"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
