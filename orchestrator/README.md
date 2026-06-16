# Werewolf AI-agent orchestrator (M15)

Python sidecar that drives the C++ engine over the JSON-lines protocol
([../docs/protocol_v1.md](../docs/protocol_v1.md)). The engine is the rule
authority; this process owns the human terminal, the AI brains, the model
backend, and the records. Design: [../docs/ai_agents_design.md](../docs/ai_agents_design.md).

## 运行（步骤 4：FakeLlmClient，无需真模型）

先构建引擎：

```bash
cmake -B build && cmake --build build      # 产出 build/werewolf
```

跑一局（全部 AI 由 FakeLlmClient 驱动，确定性、可跑通端到端）：

```bash
python3 -m orchestrator --board 1 --seed 12345 --out-dir ./games
# 1 真人 + 其余 AI：加 --human-seat <座位号>
python3 -m orchestrator --board 1 --seed 12345 --human-seat 1
```

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

## 下一步（步骤 5）

接本地 `deepseek-r1:32b`（Ollama）：新增 `OllamaClient(LlmClient)`（R1 适配：system 折进 user、解析 `<think>`、`num_ctx 8192 / temp 0.6`，见设计文档 §7.5），并让 `Config.llm_factory` 按配置选后端。AgentBrain 与协议不变。
