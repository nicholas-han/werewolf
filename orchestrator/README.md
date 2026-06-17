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
| `llm.py` | `LlmClient` 抽象 + `FakeLlmClient`（步骤 5 加 `OllamaClient`/远程） |
| `brain.py` | `AgentBrain`：累积 view、构造 prompt、解析/校验/回退、剥离 `<think>` |
| `recorder.py` | `Recorder`：写 `god_script.md` + `trace.jsonl` |
| `runner.py` | `Config` / `HumanTerminal` / `Orchestrator` 主循环 + 座位路由 |
| `__main__.py` | CLI 入口 |

## 本地模型（已接通）

`OllamaClient` 接本地 `deepseek-r1:14b`（R1 适配见设计文档 §7.5：system 折进 user；Ollama 分离的 `thinking` 包回 `<think>` 供切分；`num_ctx 8192 / temp 0.6 / num_predict 2048`；不强制 `format:json`）。换模型只改 `--model`（`deepseek-r1:1.5b` 更快、`deepseek-r1:32b` 更强）。接远程（API key）只需新增一个 `LlmClient` 后端，AgentBrain/协议不变。
