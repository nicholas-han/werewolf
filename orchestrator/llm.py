"""LlmClient abstraction + a deterministic FakeLlmClient (ai_agents_design.md §7).

Adding a model backend = a new LlmClient subclass; AgentBrain and the protocol
are untouched. The local Ollama (deepseek-r1:32b) and remote API backends arrive
in step 5; FakeLlmClient drives the end-to-end skeleton without a real model.
"""

from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Completion:
    text: str
    raw: dict = field(default_factory=dict)
    usage: Optional[dict] = None


class LlmClient(ABC):
    name: str = "abstract"

    @abstractmethod
    def complete(self, system: str, messages: list[dict], *, schema: Optional[dict] = None,
                 temperature: float = 0.6, max_tokens: int = 1024) -> Completion:
        ...


class FakeLlmClient(LlmClient):
    """Deterministic test double: follows the schema to emit a legal move.

    - choose: first non-null option in the schema's `choice.enum`.
    - confirm: always False (conservative).
    - speak: a canned line.
    Good enough to drive a whole game and exercise the full pipeline + records.
    """

    name = "fake"

    def complete(self, system: str, messages: list[dict], *, schema: Optional[dict] = None,
                 temperature: float = 0.6, max_tokens: int = 1024) -> Completion:
        props = (schema or {}).get("properties", {})
        if "choice" in props:
            enum = props["choice"].get("enum", [])
            pick = next((x for x in enum if x is not None), None)
            return Completion(text=json.dumps({"choice": pick}))
        if "decision" in props:
            return Completion(text=json.dumps({"decision": False}))
        return Completion(text="我先听听各位的发言，再做判断。")


class OllamaClient(LlmClient):
    """Local Ollama backend (ai_agents_design.md §7.5).

    R1 adaptation: DeepSeek-R1 wants instructions in the USER turn (no system
    role), and emits a <think>…</think> preamble. We fold `system` into the first
    user message and leave the <think> inline (or re-wrap Ollama's separate
    `thinking` field) so AgentBrain can split it into private reasoning. We do NOT
    force JSON output (that would suppress thinking); AgentBrain parses leniently
    and falls back, so a stray non-JSON answer never stalls the game.
    """

    def __init__(self, model: str = "deepseek-r1:14b", host: str = "http://localhost:11434",
                 num_ctx: int = 8192, top_p: float = 0.95, timeout: int = 600,
                 num_predict: int = 2048, think: bool = True):
        self.model = model
        self.host = host.rstrip("/")
        self.num_ctx = num_ctx
        self.top_p = top_p
        self.timeout = timeout
        # R1 spends ~500-1500 tokens thinking; the budget must also leave room for
        # the final answer, else `content` comes back empty (truncated mid-think).
        self.num_predict = num_predict
        self.think = think  # Ollama may still reason internally even if False
        self.name = f"ollama:{model}"
        self._warned = False

    def complete(self, system: str, messages: list[dict], *, schema: Optional[dict] = None,
                 temperature: float = 0.6, max_tokens: int = 1024) -> Completion:
        body = {
            "model": self.model,
            "messages": self._fold_system(system, messages),
            "stream": False,
            "think": self.think,
            "options": {"num_ctx": self.num_ctx, "temperature": temperature,
                        "top_p": self.top_p, "num_predict": max(max_tokens, self.num_predict)},
        }
        try:
            obj = self._post("/api/chat", body, timeout=self.timeout)
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            if not self._warned:
                print(f"[orchestrator] Ollama 调用失败，本回合回退合法默认：{e}", file=sys.stderr)
                self._warned = True
            return Completion(text="", raw={"error": str(e)})
        msg = obj.get("message", {}) or {}
        content = msg.get("content", "") or ""
        thinking = msg.get("thinking")
        if thinking:  # newer Ollama splits reasoning out; re-wrap for AgentBrain
            content = f"<think>{thinking}</think>{content}"
        usage = {"prompt_tokens": obj.get("prompt_eval_count"),
                 "completion_tokens": obj.get("eval_count")}
        return Completion(text=content, raw=obj, usage=usage)

    def health(self) -> tuple[bool, str]:
        """Verify Ollama is up and the model is installed (clear message if not)."""
        try:
            with urllib.request.urlopen(self.host + "/api/tags", timeout=10) as r:
                tags = json.loads(r.read().decode("utf-8"))
        except Exception as e:  # noqa: BLE001
            return False, f"Ollama 不可达（{self.host}）：{e}。请先 `ollama serve`。"
        models = [m.get("name", "") for m in tags.get("models", [])]
        if self.model in models or any(m.split(":")[0] == self.model.split(":")[0] for m in models):
            return True, "ok"
        return False, f"模型 {self.model} 未安装。已装：{models}。请 `ollama pull {self.model}`。"

    @staticmethod
    def _fold_system(system: str, messages: list[dict]) -> list[dict]:
        # R1: no system role. Prepend `system` to the first user message.
        if messages and messages[0].get("role") == "user":
            head = {"role": "user", "content": system + "\n\n" + messages[0].get("content", "")}
            return [head] + list(messages[1:])
        return [{"role": "user", "content": system}] + list(messages)

    def _post(self, path: str, body: dict, timeout: int) -> dict:
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        req = urllib.request.Request(self.host + path, data=data,
                                     headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode("utf-8"))
