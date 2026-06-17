# 狼人杀 Werewolf

一个用 C++ 写的**狼人杀规则引擎 + 法官控制台**。目标有两个：做一个能主持整局的工具，并借此提炼一套清晰的「规则引擎」架构（规则数据化、流程显式化、I/O 与逻辑解耦）。

完整的规则与架构基准见 [`docs/werewolf_business_requirement_document.md`](docs/werewolf_business_requirement_document.md)（项目 BRD）。

## 当前能力

- **板子**：9 人预女猎、12 人预女猎守 + 狼枪、12 人通灵机械狼（含通灵师、机械狼）。数据驱动、可扩展多板；启动时可选板子。
- **完整规则**：夜晚（狼刀/查验/解救毒）、白天（公布死讯/遗言/发言/放逐投票）、警长竞选与警徽移交、胜负判定（屠边/屠城 + 狼人绑票胜利 + §4.2 顺序结算）。
- **法官控制台**：开局发牌（随机/手动/默认）→ 全程中文主持词（天黑请闭眼、各角色睁眼闭眼、公布死讯、遗言停顿、发言顺序、放逐票数）→ 引擎自动判定。
- **三种玩法**：`1` 单屏法官（一人主持）/ `2` 传递游玩（一台设备多人，清屏 + 私密交接，每人只看自己信息，无需网络）/ `3` AI 自动对战（多座位 bot 演示，无需真人）。
- **按玩家路由地基**：`PlayerChannel` + `RoutingDecisionProvider` 把引擎的单一决策接口拆成「每玩家一条通道」，公共/私密/法官全知三类信息分流（§11）。这是真·联机与 AI agent 玩家的共同地基——各自只是不同的通道实现。
- **发言记录与复盘**（可选开关）：按发言顺序记录每人白天发言 + 出局者遗言，结束后按天分组打印复盘记录。
- **拍刀沙盒**（§4.4，进行中）：可回滚 `GameState` + 指定线推演（阶段 A/B 完成；好人最优自动搜索阶段 C 暂缓）。

## 构建与运行

需要 CMake ≥ 3.20 与支持 C++20 的编译器（首次构建会用 `FetchContent` 拉取 GoogleTest）。

```bash
cmake -B build && cmake --build build      # 构建
./build/werewolf                           # 启动法官控制台
ctest --test-dir build --output-on-failure # 跑测试（131 个用例）
```

## 怎么用（法官视角）

1. 启动后选择**板子** → **发牌方式**（`1` 随机 / `2` 手动 / `3` 默认顺序）→ **玩法**（`1` 单屏法官 / `2` 传递游玩）→ 是否**记录发言**（复盘用，默认否）。
2. 单屏法官会打印**法官视角**的座位→身份表（全知）；传递游玩则逐人私密发牌（清屏交接，狼队互见、机械狼不见队友）。
3. 之后按提示主持每一步：程序会念出「天黑请闭眼 / 狼人请睁眼 …」，问你**玩家做了什么**（狼刀谁、预言家验谁、女巫救/毒、投票放逐谁……），到关键处停下等你按回车（公布死讯、遗言）。开启发言记录时，会在发言/遗言环节让你录入文本。
4. 引擎自动结算死亡、连锁触发（猎人开枪）、警长竞选与移交、胜负判定，最后给出**游戏结束：好人/狼人胜利**；若开了发言记录，再打印**复盘：发言记录**。

> **信息可见性**（§11）：单屏法官把信息打在同屏（法官本就全知）；**传递游玩**用清屏 + 私密交接，在一台设备上做到「每人只看自己信息」。多设备**真·联机**仍属后续形态（见 [`docs/werewolf_business_requirement_document.md`](docs/werewolf_business_requirement_document.md) 路线图）。

## 架构一览

引擎为纯逻辑，I/O 全部经接口解耦：

- `core/` — `Player` / `Role`（轻量元数据 + 组合能力 `Ability`）/ `GameState` / `Board` / 枚举 / `messages.h`（中文文案单一来源）。
- `flow/` — `Game`（夜/昼/竞选编排）、`Settlement`（死亡结算核心，真实对局与沙盒共用）、`WinCondition`、`transcript`（发言复盘）、`paidao`（拍刀沙盒）等。
- `io/` — `DecisionProvider` 接口 + `Scripted`（测试）/ `Console`（真人）/ `PassAndPlay`（单设备多人）实现；以及按玩家路由：`PlayerChannel`（每玩家通道）+ `RoutingDecisionProvider`，通道实现 `Scripted`/`Bot`（真·联机的 `Network`、AI 的 `Agent` 留待后续）。
- `app/` — `main.cpp` 命令行入口。

设计要点：**角色用「浅继承 + 能力组合」**（加角色≈挑选/新写能力再拼装）；**胜负按阵营全局计数**；**信息可见性**严格区分「真相层 GameState」与「玩家视图」。详见 [`docs/werewolf_business_requirement_document.md`](docs/werewolf_business_requirement_document.md) 第二部分。

## 路线图（摘要）

法官单机工具（已完成）→ 随机发牌（已完成）→ 单设备传递游玩（已完成）→ 发言记录与复盘（已完成）→ 按玩家路由地基 + AI 自动对战演示（已完成）→ **AI agent 玩家（M15：进程外 sidecar ＝ 引擎 JSON 协议 + Python orchestrator + 本地模型；M16：通过 API key 接入云端大模型，优先阿里百炼，并通用覆盖 OpenAI/DeepSeek/Moonshot/智谱/Anthropic 等；见 [`docs/ai_agents_design.md`](docs/ai_agents_design.md) 与 [`orchestrator/`](orchestrator/)）** → 拍刀阶段 C 自动最优搜索 → 真·联机（`NetworkChannel` + 真人客户端）。

## 开发约定

- 标识符与代码注释用英文；与用户沟通用中文。
- 用 `ScriptedDecisionProvider` 对整局做确定性端到端测试。
- 小步推进：每步可独立编译 + 测试通过再继续。
