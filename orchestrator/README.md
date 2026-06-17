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

默认用本地 **Ollama**（须先 `ollama serve`）。**不带 `--model` 时，启动会列出已装模型让你下拉选择**：

```bash
python3 -m orchestrator --human-seat 1            # 启动时下拉选模型，你坐 1 号
python3 -m orchestrator --model deepseek-r1:1.5b  # 指定模型、跳过下拉（1.5b 最快）
python3 -m orchestrator --fake                    # 确定性假模型，秒级（管线/调试）
```

> 真模型整局**较慢**（14b 约 17–35s/决策；1.5b 快很多但策略弱）；快速验证用 `--fake`。

### 云端大模型（API key，M16）——本地太慢时用它，**优先阿里百炼**

API key **只走环境变量**（绝不写进代码/配置/提交）。先 `export`，再 `--provider`：

```bash
export DASHSCOPE_API_KEY=sk-xxxx                      # 阿里百炼（DashScope，OpenAI 兼容）
python3 -m orchestrator --provider bailian --human-seat 1            # 默认 qwen-plus
python3 -m orchestrator --provider bailian --model qwen-max          # 指定模型（更强）
```

同一套机制通用覆盖其它「OpenAI 兼容」云厂商，每家只是一个**预设**（base_url + 取 key 的环境变量名 + 默认模型）：

| `--provider` | 厂商 | 环境变量 | 默认模型 |
| --- | --- | --- | --- |
| `bailian` / `dashscope` | 阿里百炼（**优先**） | `DASHSCOPE_API_KEY` | `qwen-plus` |
| `openai` | OpenAI | `OPENAI_API_KEY` | `gpt-4o-mini` |
| `deepseek` | DeepSeek | `DEEPSEEK_API_KEY` | `deepseek-chat` |
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
- 推理模型（`qwq-plus`/`deepseek-r1`/`deepseek-reasoner`…）的思维链在独立字段 `reasoning_content` 返回，已被包回 `<think>` 供 `AgentBrain` 切分——**绝不会漏进公开发言**（§11）。
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
