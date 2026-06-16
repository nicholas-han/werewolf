"""LlmClient abstraction + a deterministic FakeLlmClient (ai_agents_design.md §7).

Adding a model backend = a new LlmClient subclass; AgentBrain and the protocol
are untouched. The local Ollama (deepseek-r1:32b) and remote API backends arrive
in step 5; FakeLlmClient drives the end-to-end skeleton without a real model.
"""

from __future__ import annotations

import json
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
