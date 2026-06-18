# Werewolf AI-agent orchestrator (M15)

Python sidecar that drives the C++ engine over the JSON-lines protocol
([../docs/protocol_v1.md](../docs/protocol_v1.md)). The engine is the rule
authority; this process owns the human terminal, the AI brains, the model
backend, and the records. Design: [../docs/ai_agents_design.md](../docs/ai_agents_design.md).

## 运行

先构建引擎：

```bash
cmake -B build && cmake --build build      # 产出 build/werewolf
```

**默认用云端 DeepSeek**（需 `DEEPSEEK_API_KEY`，见下「云端大模型」）：

```bash
export DEEPSEEK_API_KEY=sk-xxxx                    # 先设好 key（绝不写进代码/配置）
python3 -m orchestrator --human-seat 1            # 默认 provider=deepseek、model=deepseek-v4-flash
python3 -m orchestrator --model deepseek-v4-pro   # 更强的模型
python3 -m orchestrator --fake                    # 确定性假模型，秒级（管线/调试，无需 key）
```

本地 **Ollama**（须先 `ollama serve`）仍可用，加 `--provider ollama`；不带 `--model` 会列出已装模型下拉选择：

```bash
python3 -m orchestrator --provider ollama --human-seat 1   # 启动时下拉选本地模型
python3 -m orchestrator --provider ollama --model deepseek-r1:1.5b
```

> 本地真模型整局**较慢**（14b 约 17–35s/决策）；云端快很多；快速验证用 `--fake`。

### 云端大模型（API key，M16）

API key **只走环境变量**（绝不写进代码/配置/提交）。先 `export`，再（默认即 deepseek，其它家用 `--provider`）：

```bash
export DEEPSEEK_API_KEY=sk-xxxx                                      # DeepSeek（默认）
python3 -m orchestrator --human-seat 1                              # 默认 deepseek-v4-flash
python3 -m orchestrator --model deepseek-v4-pro                     # 更强

export DASHSCOPE_API_KEY=sk-xxxx                                    # 阿里百炼（DashScope）
python3 -m orchestrator --provider bailian --model qwen-max        # 换厂商只改 --provider
```

同一套机制通用覆盖其它「OpenAI 兼容」云厂商，每家只是一个**预设**（base_url + 取 key 的环境变量名 + 默认模型）：

| `--provider` | 厂商 | 环境变量 | 默认模型 |
| --- | --- | --- | --- |
| `deepseek` | DeepSeek（**默认**） | `DEEPSEEK_API_KEY` | `deepseek-v4-flash` |
| `bailian` / `dashscope` | 阿里百炼（DashScope） | `DASHSCOPE_API_KEY` | `qwen-plus` |
| `openai` | OpenAI | `OPENAI_API_KEY` | `gpt-4o-mini` |
| `moonshot` | Moonshot / Kimi | `MOONSHOT_API_KEY` | `moonshot-v1-8k` |
| `zhipu` | 智谱 GLM | `ZHIPU_API_KEY` | `glm-4-flash` |
| `anthropic` | Anthropic Claude | `ANTHROPIC_API_KEY` | `claude-haiku-4-5-20251001` |

任意其它 OpenAI 兼容端点用 `openai-compat`（显式给端点与环境变量名）：

```bash
export MY_KEY=sk-xxxx
python3 -m orchestrator --provider openai-compat \
    --base-url https://your-endpoint/v1 --api-key-env MY_KEY --model your-model
```

要点：
- 默认每次模型调用软超时 **云端 60s / 本地 600s**（`--timeout` 覆盖）；429/5xx/网络错带退避重试，最终失败回退合法默认，**绝不卡死游戏**。
- 推理模型（DeepSeek thinking 模式 / 百炼 `deepseek-r1`…）的思维链在独立字段 `reasoning_content` 返回，已被包回 `<think>` 供 `AgentBrain` 切分——**绝不会漏进公开发言**（§11）。
- 暂不支持**仅流式**的模型（百炼 QwQ 系 `qwq-*`）与 OpenAI **o 系**（`o4-mini` 等需 `max_completion_tokens`/不收 `temperature`）——本客户端发非流式、统一 `temperature`+`max_tokens`，这两类会被拒（HTTP 400 → 回退合法默认）；故未在下拉菜单列出，待后续按模型族适配 / 实现 SSE 流式后再开。
- 结构化输出默认靠提示词约定 + 宽松解析 + 合法回退；弱模型可加 `--json-object`（仅对 choose/confirm 下发 `response_format=json_object`，speak 不强制）。
- 海外节点：`--base-url https://dashscope-intl.aliyuncs.com/compatible-mode/v1`（新加坡）等。

产物：
**每局生成独立目录**（带时间戳，旧局保留、不覆盖）：`games/<时间戳>_board<N>_seed<S>/`，内含：
- `god_script.md` — 上帝视角复盘（含所有 public / private / moderator 事件）。
- `trace.jsonl` — 每个 AI 决策一行（可见上下文 → reasoning → 行动 → 战局结果），供 post-training。

（运行结束后控制台会打印本局目录路径。）

测试：

```bash
python3 orchestrator/tests/test_e2e.py     # 整局跑通 + 两份记录 + §11 公平性断言
```

## 模块

| 模块 | 职责 |
| --- | --- |
| `engine.py` | `EngineProcess`：启动 `werewolf --json`，逐行收发 JSON |
| `llm.py` | `LlmClient` 抽象 + `FakeLlmClient` + 本地 `OllamaClient` |
| `cloud.py` | 云端后端（M16）：`OpenAICompatClient`/`AnthropicClient` + `PROVIDER_PRESETS`（百炼优先）+ `build_client` 工厂 |
| `brain.py` | `AgentBrain`：累积 view、构造 prompt、解析/校验/回退、剥离 `<think>` |
| `recorder.py` | `Recorder`：写 `god_script.md` + `trace.jsonl` |
| `runner.py` | `Config` / `HumanTerminal` / `Orchestrator` 主循环 + 座位路由 |
| `__main__.py` | CLI 入口 |

## 本地模型（已接通）

`OllamaClient` 接本地 `deepseek-r1:14b`（R1 适配见设计文档 §7.5：system 折进 user；Ollama 分离的 `thinking` 包回 `<think>` 供切分；`num_ctx 8192 / temp 0.6 / num_predict 2048`；不强制 `format:json`）。换模型只改 `--model`（`deepseek-r1:1.5b` 更快、`deepseek-r1:32b` 更强）。

云端（API key）已接通（M16，见上节「云端大模型」）：新增 `cloud.py` 一个后端文件即覆盖所有 OpenAI 兼容厂商 + Anthropic，`AgentBrain`/协议完全不变——印证了 `LlmClient` 这层缝的通用性。
