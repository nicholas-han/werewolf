"""Cloud LLM backends (ai_agents_design.md §7.2): OpenAI-compatible + Anthropic.

本地推理模型（Ollama deepseek-r1）整局太慢，M16 增加「通过 API key 连云端大模型」。

一个 `OpenAICompatClient` 就覆盖了 OpenAI 以及一大票 **OpenAI 兼容** 的云厂商——
**阿里云百炼（DashScope）**、DeepSeek、Moonshot/Kimi、智谱 GLM……它们都说同一套
`POST {base_url}/chat/completions` + `Authorization: Bearer <key>`。每一家只是
`PROVIDER_PRESETS` 里的一个**预设**（base_url + 装 key 的环境变量名 + 默认模型）。
`"bailian"` 是优先预设。`AnthropicClient` 覆盖 Claude 形态不同的 Messages API，
用来证明 `LlmClient` 这层缝足够通用；其它非兼容厂商照此再加一个子类即可。

接口对上层（AgentBrain / 协议）完全不变：仍是 `complete(...)->Completion`。换后端
= 改 `--provider`，不碰 brain、不碰协议。

安全：**API key 只来自环境变量**（`api_key_env` 只记“去哪个环境变量取”），key
绝不写进配置、绝不打印日志、绝不进 trace。
"""

from __future__ import annotations

import json
import os
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass

from orchestrator.llm import Completion, LlmClient


# ---------------------------------------------------------------------------
# 预设：每个云厂商 = base_url + 装 key 的环境变量名 + 默认模型 + 备选模型清单
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class ProviderPreset:
    label: str           # 人读名字（startup / 交互选单展示）
    base_url: str        # OpenAI：{base_url}/chat/completions；Anthropic：{base_url}/messages
    api_key_env: str     # 去哪个环境变量取 key（绝不存 key 本身）
    default_model: str   # 不指定 --model 时用它
    kind: str = "openai"            # "openai"（兼容）| "anthropic"
    models: tuple[str, ...] = ()    # 交互选单里的备选模型（可空）
    note: str = ""                  # 额外提示（如海外节点）


# 阿里云百炼（DashScope）—— 用户的**优先**接入方式。OpenAI 兼容模式：
# base_url=.../compatible-mode/v1，key 走 DASHSCOPE_API_KEY。
_BAILIAN = ProviderPreset(
    label="阿里云百炼 DashScope（OpenAI 兼容）",
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
    api_key_env="DASHSCOPE_API_KEY",
    default_model="qwen-plus",
    kind="openai",
    # 常用：qwen-plus 均衡 / qwen-max 最强 / qwen-turbo·qwen-flash 便宜快 /
    # deepseek-r1·deepseek-v3 为推理/强模型（带思维链，会更慢更贵）。
    # 不列 QwQ 系（qwq-plus 等）：百炼上它们**仅支持流式输出**，本客户端发 stream:false
    # 会被拒（HTTP 400）——待实现 SSE 流式后再开（见 build_client/complete 注释）。
    models=("qwen-plus", "qwen-max", "qwen-turbo", "qwen-flash",
            "deepseek-r1", "deepseek-v3"),
    note="海外/新加坡节点改 --base-url https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
)

PROVIDER_PRESETS: dict[str, ProviderPreset] = {
    "bailian": _BAILIAN,
    "dashscope": _BAILIAN,  # 别名：两个名字都指百炼
    "openai": ProviderPreset(
        # 不列 o 系（o4-mini 等）：它们不收 temperature、且要 max_completion_tokens 而非
        # max_tokens，本客户端的请求体会被拒（HTTP 400）；待按模型族适配后再开。
        "OpenAI", "https://api.openai.com/v1", "OPENAI_API_KEY", "gpt-4o-mini",
        models=("gpt-4o-mini", "gpt-4o")),
    "deepseek": ProviderPreset(
        "DeepSeek", "https://api.deepseek.com/v1", "DEEPSEEK_API_KEY", "deepseek-chat",
        models=("deepseek-chat", "deepseek-reasoner")),
    "moonshot": ProviderPreset(
        "Moonshot / Kimi", "https://api.moonshot.cn/v1", "MOONSHOT_API_KEY", "moonshot-v1-8k",
        models=("moonshot-v1-8k", "moonshot-v1-32k", "kimi-k2-0711-preview")),
    "zhipu": ProviderPreset(
        "智谱 GLM", "https://open.bigmodel.cn/api/paas/v4", "ZHIPU_API_KEY", "glm-4-flash",
        models=("glm-4-flash", "glm-4-plus", "glm-4-air")),
    "anthropic": ProviderPreset(
        "Anthropic Claude", "https://api.anthropic.com/v1", "ANTHROPIC_API_KEY",
        "claude-haiku-4-5-20251001", kind="anthropic",
        models=("claude-haiku-4-5-20251001", "claude-sonnet-4-6", "claude-opus-4-8")),
}

# provider 名字 = 预设名 + 这俩特殊值（用于 CLI choices / 校验）。
GENERIC_PROVIDER = "openai-compat"  # 任意 OpenAI 兼容端点（须显式给 --base-url/--api-key-env）


def provider_choices() -> list[str]:
    """All accepted `--provider` values (presets + generic), stable order."""
    return list(PROVIDER_PRESETS) + [GENERIC_PROVIDER]


# ---------------------------------------------------------------------------
# 错误类型
# ---------------------------------------------------------------------------
class MissingApiKey(Exception):
    """The configured api_key_env is unset/empty (clear, actionable message)."""

    def __init__(self, env: str):
        self.env = env
        super().__init__(env)


class ApiError(Exception):
    """An HTTP-level error from the provider; `retriable` => worth a backoff retry."""

    def __init__(self, message: str, *, retriable: bool = False):
        self.retriable = retriable
        super().__init__(message)


# ---------------------------------------------------------------------------
# 共享 HTTP 后端：取 key、带退避重试的 POST、HTTP 状态码→可重试判断、一次性告警
# ---------------------------------------------------------------------------
class _HttpBackend(LlmClient):
    def __init__(self, *, api_key_env: str, timeout: int, max_retries: int):
        self.api_key_env = api_key_env
        self.timeout = timeout
        self.max_retries = max_retries
        self._warned = False

    def _api_key(self) -> str:
        key = os.environ.get(self.api_key_env, "").strip()
        if not key:
            raise MissingApiKey(self.api_key_env)
        return key

    def health(self) -> tuple[bool, str]:
        """Cheap startup check: just verify the key env var is set (no network call,
        so we don't spend a token / add latency; a bad key surfaces on first use)."""
        try:
            self._api_key()
        except MissingApiKey as e:
            return False, (f"环境变量 {e.env} 未设置，无法连云端模型。"
                           f"请先在终端执行：export {e.env}=<你的API Key>")
        return True, "ok"

    def _warn_once(self, msg: str) -> None:
        if not self._warned:
            print(f"[orchestrator] {msg}", file=sys.stderr)
            self._warned = True

    @staticmethod
    def _sleep_backoff(attempt: int) -> None:
        time.sleep(min(2 ** attempt, 8))  # 1s, 2s, 4s, …（封顶 8s）

    def _request(self, url: str, headers: dict, body: dict) -> dict:
        """POST JSON, parse JSON; retry on 429/5xx/network with capped backoff.

        Never logs `headers` (they carry the bearer key). Raises ApiError (HTTP 4xx/5xx
        OR a 2xx whose body isn't valid JSON/UTF-8) or a network exception (URLError/
        OSError/TimeoutError); `_complete` maps all of them to an empty Completion → 合法回退.
        """
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        last_exc: Exception | None = None
        for attempt in range(self.max_retries + 1):
            try:
                req = urllib.request.Request(url, data=data, headers=headers, method="POST")
                with urllib.request.urlopen(req, timeout=self.timeout) as r:
                    raw = r.read()
                try:
                    return json.loads(raw.decode("utf-8"))
                except (ValueError, UnicodeDecodeError) as e:
                    # 2xx but body isn't valid JSON/UTF-8 (proxy interstitial, truncated
                    # gateway response, text/plain error…): non-retriable, surfaced as
                    # ApiError so _complete's fallback path catches it (not a raw ValueError
                    # that would escape complete() and skip the warn + trace record).
                    raise ApiError(f"响应体非合法 JSON：{e}", retriable=False) from None
            except urllib.error.HTTPError as e:
                detail = ""
                try:
                    detail = e.read().decode("utf-8", "replace")[:300]
                except Exception:  # noqa: BLE001
                    pass
                finally:
                    try:
                        e.close()  # release the connection (avoid ResourceWarning/leak)
                    except Exception:  # noqa: BLE001
                        pass
                retriable = e.code == 429 or 500 <= e.code < 600
                last_exc = ApiError(f"HTTP {e.code}: {detail}", retriable=retriable)
                if not retriable or attempt == self.max_retries:
                    raise last_exc from None
            except (urllib.error.URLError, OSError, TimeoutError) as e:
                last_exc = e
                if attempt == self.max_retries:
                    raise
            self._sleep_backoff(attempt)
        assert last_exc is not None
        raise last_exc

    def _complete(self, *, url: str, headers_for, body: dict, parse) -> Completion:
        """Shared key→POST→parse with the single fallback contract both backends rely on:
        missing key OR any request failure → empty Completion (+ one-time warn), so
        AgentBrain always lands on a legal default and the game never stalls (protocol §8).
        Lives in ONE place so the OpenAI and Anthropic paths can't drift apart."""
        try:
            key = self._api_key()
        except MissingApiKey as e:
            self._warn_once(f"环境变量 {e.env} 未设置，本回合回退合法默认。"
                            f"请先 export {e.env}=<你的API Key>。")
            return Completion(text="", raw={"error": f"missing {e.env}"})
        try:
            obj = self._request(url, headers_for(key), body)
        except (urllib.error.URLError, OSError, TimeoutError, ApiError) as e:
            self._warn_once(f"云端模型调用失败，本回合回退合法默认：{e}")
            return Completion(text="", raw={"error": str(e)})
        return parse(obj)


# ---------------------------------------------------------------------------
# OpenAI 兼容后端（覆盖百炼 / OpenAI / DeepSeek / Moonshot / 智谱 / 任意兼容端点）
# ---------------------------------------------------------------------------
class OpenAICompatClient(_HttpBackend):
    """POST {base_url}/chat/completions, `Authorization: Bearer <key>`.

    - system 走真正的 system role（云端聊天模型支持，不必像本地 R1 折进 user）。
    - 推理模型（deepseek-r1 / qwq / qwen3-thinking / deepseek-reasoner）把思维链放在
      **独立字段** `reasoning_content`；这里把它包回 `<think>…</think>` 拼到正文前，
      让 AgentBrain 统一切分进 private reasoning——**思维链绝不会漏进公开发言**
      （protocol_v1 §6/§11，与 OllamaClient 同构）。
    - 结构化输出：默认**不**强制（靠 brain 的文字输出约定 + 宽松解析 + 合法回退，
      Qwen 等都能稳定吐那行 JSON）。`json_object=True` 时对 choose/confirm 下发
      `response_format={"type":"json_object"}` 作为加强（system 提示词里已含 “JSON”
      字样，满足这类接口“消息须提及 json”的要求）；speak 永不强制 JSON。
    """

    def __init__(self, *, base_url: str, model: str, api_key_env: str,
                 provider_label: str = GENERIC_PROVIDER, timeout: int = 60,
                 max_tokens: int = 2048, max_retries: int = 2, json_object: bool = False,
                 extra_headers: dict | None = None):
        super().__init__(api_key_env=api_key_env, timeout=timeout, max_retries=max_retries)
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.max_tokens = max_tokens
        self.json_object = json_object
        self.extra_headers = dict(extra_headers or {})
        self.name = f"{provider_label}:{model}"

    def complete(self, system: str, messages: list[dict], *, schema=None,
                 temperature: float = 0.6, max_tokens: int = 1024) -> Completion:
        body: dict = {
            "model": self.model,
            "messages": [{"role": "system", "content": system}] + list(messages),
            "temperature": temperature,
            "max_tokens": max(max_tokens, self.max_tokens),
            "stream": False,  # 同步逐 ask 应答；故流式-only 模型（如百炼 QwQ 系）暂不支持，见预设注释
        }
        # 只对 choose/confirm 启用 json_object；speak 也带 schema({"speech":..})但绝不强制
        # 成 JSON（json_object 只约束“是合法 JSON”、不约束形状，会逼模型把发言裹成乱键）。
        props = (schema or {}).get("properties", {})
        if self.json_object and ("choice" in props or "decision" in props):
            body["response_format"] = {"type": "json_object"}
        return self._complete(
            url=self.base_url + "/chat/completions",
            headers_for=lambda key: {"Content-Type": "application/json",
                                     "Authorization": f"Bearer {key}", **self.extra_headers},
            body=body, parse=self._parse)

    @staticmethod
    def _parse(obj) -> Completion:
        if not isinstance(obj, dict):  # 异常响应形态：稳妥回退（绝不抛进 AgentBrain）
            return Completion(text="", raw={"error": "意外响应形态"})
        choices = obj.get("choices") or []
        first = choices[0] if (choices and isinstance(choices[0], dict)) else {}
        msg = first.get("message")
        msg = msg if isinstance(msg, dict) else {}
        content = msg.get("content") or ""
        reasoning = msg.get("reasoning_content") or msg.get("reasoning")
        if reasoning:  # 推理模型：把思维链包回 <think> 供 brain 切分（绝不广播）
            content = f"<think>{reasoning}</think>{content}"
        u = obj.get("usage") if isinstance(obj.get("usage"), dict) else {}
        usage = {"prompt_tokens": u.get("prompt_tokens"),
                 "completion_tokens": u.get("completion_tokens")}
        return Completion(text=content, raw=obj, usage=usage)


# ---------------------------------------------------------------------------
# Anthropic Claude 后端（Messages API：system 是顶层参数，auth 走 x-api-key）
# ---------------------------------------------------------------------------
class AnthropicClient(_HttpBackend):
    """POST {base_url}/messages；header 用 `x-api-key` + `anthropic-version`。

    结构化输出沿用 brain 已下发的文字约定 + 宽松解析（不必上 tool-use）；若开启
    扩展思考，思维块同样不会进发言（brain 只取 text，再切 <think>）。
    """

    def __init__(self, *, model: str, api_key_env: str = "ANTHROPIC_API_KEY",
                 base_url: str = "https://api.anthropic.com/v1", timeout: int = 60,
                 max_tokens: int = 2048, max_retries: int = 2,
                 anthropic_version: str = "2023-06-01"):
        super().__init__(api_key_env=api_key_env, timeout=timeout, max_retries=max_retries)
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.max_tokens = max_tokens
        self.anthropic_version = anthropic_version
        self.name = f"anthropic:{model}"

    def complete(self, system: str, messages: list[dict], *, schema=None,
                 temperature: float = 0.6, max_tokens: int = 1024) -> Completion:
        body = {
            "model": self.model,
            "system": system,
            "messages": [{"role": m["role"], "content": m["content"]} for m in messages],
            "max_tokens": max(max_tokens, self.max_tokens),  # Anthropic：max_tokens 必填
            "temperature": temperature,
        }
        return self._complete(
            url=self.base_url + "/messages",
            headers_for=lambda key: {"Content-Type": "application/json", "x-api-key": key,
                                     "anthropic-version": self.anthropic_version},
            body=body, parse=self._parse)

    @staticmethod
    def _parse(obj) -> Completion:
        if not isinstance(obj, dict):  # 异常响应形态：稳妥回退（绝不抛进 AgentBrain）
            return Completion(text="", raw={"error": "意外响应形态"})
        # content 是 block 列表；拼接所有 text block（thinking block 不取 → 不广播）
        blocks = obj.get("content") or []
        text = "".join(b.get("text", "") for b in blocks
                       if isinstance(b, dict) and b.get("type") == "text")
        u = obj.get("usage") if isinstance(obj.get("usage"), dict) else {}
        usage = {"prompt_tokens": u.get("input_tokens"),
                 "completion_tokens": u.get("output_tokens")}
        return Completion(text=text, raw=obj, usage=usage)


# ---------------------------------------------------------------------------
# 工厂：provider 名字 → 具体后端
# ---------------------------------------------------------------------------
def build_client(provider: str, *, model: str | None = None, base_url: str | None = None,
                 api_key_env: str | None = None, timeout: int = 60, max_tokens: int = 2048,
                 json_object: bool = False) -> LlmClient:
    """Build a cloud LlmClient for a preset name (or the generic 'openai-compat').

    model/base_url/api_key_env 为 None 时取预设默认；显式传入则覆盖。未知 provider 抛
    ValueError（带可用清单）。'openai-compat' 必须显式给 base_url + api_key_env。
    """
    if provider == GENERIC_PROVIDER:
        if not base_url or not api_key_env:
            raise ValueError(
                f"provider '{GENERIC_PROVIDER}' 需要显式 --base-url 和 --api-key-env "
                "（指向 OpenAI 兼容端点与装 key 的环境变量名）")
        return OpenAICompatClient(base_url=base_url, model=model or "default",
                                  api_key_env=api_key_env, provider_label=GENERIC_PROVIDER,
                                  timeout=timeout, max_tokens=max_tokens, json_object=json_object)

    preset = PROVIDER_PRESETS.get(provider)
    if preset is None:
        raise ValueError(
            f"未知 provider：{provider!r}。可用：{', '.join(provider_choices())}")

    eff_model = model or preset.default_model
    eff_key_env = api_key_env or preset.api_key_env
    eff_base = base_url or preset.base_url
    if preset.kind == "anthropic":
        return AnthropicClient(model=eff_model, api_key_env=eff_key_env, base_url=eff_base,
                               timeout=timeout, max_tokens=max_tokens)
    return OpenAICompatClient(base_url=eff_base, model=eff_model, api_key_env=eff_key_env,
                              provider_label=provider, timeout=timeout, max_tokens=max_tokens,
                              json_object=json_object)
