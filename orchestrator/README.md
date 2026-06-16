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

默认用本地 **Ollama `deepseek-r1:14b`**（须先 `ollama serve` 且 `ollama pull deepseek-r1:14b`）：

```bash
python3 -m orchestrator --board 1 --seed 12345                 # 全 AI（真模型，较慢）
python3 -m orchestrator --board 1 --seed 12345 --human-seat 1  # 你坐 1 号，其余 AI
python3 -m orchestrator --fake                                 # 确定性假模型，秒级（管线/调试）
```

> 真模型整局**很慢**（14b 推理约 17–35s/决策，整局数十分钟~数小时）；快速验证用 `--fake`。

产物：
- `games/god_script.md` — 上帝视角复盘（含所有 public / private / moderator 事件）。
- `games/trace.jsonl` — 每个 AI 决策一行（可见上下文 → reasoning → 行动 → 战局结果），供 post-training。

测试：

```bash
python3 orchestrator/tests/test_e2e.py     # 整局跑通 + 两份记录 + §11 公平性断言
```

## 模块

| 模块 | 职责 |
| --- | --- |
| `engine.py` | `EngineProcess`：启动 `werewolf --json`，逐行收发 JSON |
| `llm.py` | `LlmClient` 抽象 + `FakeLlmClient`（步骤 5 加 `OllamaClient`/远程） |
| `brain.py` | `AgentBrain`：累积 view、构造 prompt、解析/校验/回退、剥离 `<think>` |
| `recorder.py` | `Recorder`：写 `god_script.md` + `trace.jsonl` |
| `runner.py` | `Config` / `HumanTerminal` / `Orchestrator` 主循环 + 座位路由 |
| `__main__.py` | CLI 入口 |

## 本地模型（已接通）

`OllamaClient` 接本地 `deepseek-r1:14b`（R1 适配见设计文档 §7.5：system 折进 user；Ollama 分离的 `thinking` 包回 `<think>` 供切分；`num_ctx 8192 / temp 0.6 / num_predict 2048`；不强制 `format:json`）。换模型只改 `--model`（`deepseek-r1:1.5b` 更快、`deepseek-r1:32b` 更强）。接远程（API key）只需新增一个 `LlmClient` 后端，AgentBrain/协议不变。
