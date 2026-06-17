# AI Agent 玩家设计文档（M15 及后续）

> 状态：**设计稿（待开发）**。本文是 AI agent 方向的工程基准，配合 BRD（[`werewolf_business_requirement_document.md`](werewolf_business_requirement_document.md)）阅读。
> 范围：让「1 名真人 + 其余全部 AI」跑通一整局狼人杀（含发言、夜间技能、投票、狼队私聊），并产出上帝视角复盘 script 与可用于 post-training 的结构化 trace。
> 标识符/代码用英文，叙述用中文（沿用项目约定）。

---

## 0. 决策摘要（已拍板）

| 决策              | 选择                                                       | 理由                                                                |
| --------------- | -------------------------------------------------------- | ----------------------------------------------------------------- |
| 运行形态            | 进程外 sidecar                                              | C++ 引擎保持纯逻辑；agent 大脑用更适合 LLM/语音的语言写；即 BRD 预留的 `NetworkChannel` 终局 |
| 入口模型            | (B) 引擎当 JSON-stdio 服务，Python orchestrator 当入口            | 单一真相源、单一事件流；人类席与 AI 席在引擎眼里完全一致；最贴合「服务器跑引擎 + 瘦客户端」路线               |
| 首个模型后端          | 本地单模型优先（Ollama/llama-server），`LlmClient` 抽象层让远程 API 随后即插 | 内存友好：一份权重多 persona = 「一个 AI 的多 subagent」                          |
| 流程增量            | 加狼队夜间私聊（无机制影响）                                           | 协作更真实、训练数据更丰富                                                     |
| Orchestrator 语言 | Python（建议）                                               | 本地模型客户端、Whisper/TTS 语音、各家 LLM SDK 生态最成熟                           |

**已定运行参数（M15，详见对应小节）**：本地模型 **deepseek-r1:14b**（Ollama，§7.5）；**狼队私聊每晚刀前一轮**（§5.4）；**警长竞选候选人发言打开**（§5.6）；trace **全量记录**（token/延迟/非法输出/回退/`<think>` 推理，§8.2）；引擎 `ask` **可配软超时**（默认 600s，§4.7）。

**核心不变量：C++ 引擎的规则/结算/胜负逻辑一行都不改。** 本里程碑新增的全部是 I/O 层（一个走 JSON 协议的 `DecisionProvider` + 一个结构化事件发射器）、两处纯增量的流程步骤（狼队私聊、打开候选人发言）、以及 C++ 之外的 orchestrator。现有 104 个 GoogleTest 全程保持绿。

---

## 1. 目标与非目标

### 1.1 本期目标（M15）
1. 引擎以 JSON-lines 协议对外暴露**每个座位**的决策请求与信息事件。
2. Python orchestrator 启动引擎子进程，按座位把请求路由到「人类终端」或「AgentBrain」。
3. 每个 AgentBrain 依据**自己被允许知道的信息**（角色 + 收到的事件）发言与决策；能读懂他人（含真人）的发言。
4. 接入**本地单模型**（deepseek-r1:14b）：N 个 AI 席共享一个模型、各自 persona。`LlmClient` 抽象层让远程 API（带 API key）随后即插。
5. **每晚刀前**加一轮**狼队私聊**；并**打开警长竞选候选人发言**（引擎现状是跳过，打开后更贴合 BRD §7.2）。
6. 产出两份记录：**上帝视角 script**（人读、复盘）+ **post-training JSONL trace**（机读、训练）。
7. 跑通一整局 9 人板（1 人类 + 8 AI），并验证：没有任何 agent 能看到他人身份；游戏一定终止。

### 1.2 非目标（本期不做，但设计要为其留路）
- 多模态语音输入/输出（STT/TTS）。
- 真·联机（多设备/WebSocket/浏览器客户端）。
- 拍刀阶段 C「开卷最优」自动搜索（§4.4）。
- 多真人混局（引擎已支持多座位，但本期只 1 真人）。
- 远程模型先不接（抽象层留好，配置即可启用）。

---

## 2. 总体架构

```
┌────────────────────────────────────────────────────────────────
│  C++ 引擎进程（权威真相层 GameState；规则/结算/胜负逻辑不变）
│
│   Game ──→ DecisionProvider（JSON 实现）
│              ├─ 决策：emit "ask"  → 阻塞读 "reply"
│              └─ 信息：EventSink.emit(Event{vis, seat?, ...})
│                         │
│   stdout（引擎→orchestrator）：events + asks，逐行 JSON
│   stdin （orchestrator→引擎）：replies，逐行 JSON
│   stderr：引擎调试日志（非协议）
└───────────────┬────────────────────────────────────────────────
                │  JSON-lines 协议 v1（§4，耐久产物）
                ▼
┌────────────────────────────────────────────────────────────────
│  Python orchestrator（入口；拥有人类终端 + 所有 AI 大脑 + 模型）
│
│   EngineProcess（启动引擎、读写管道、按 id 关联 ask/reply）
│   EventRouter（把每条 event 派发：public→所有 brain；
│               private(seat)→该 brain；moderator→仅 script 写手）
│   SeatTable：seat → HumanTerminal | AgentBrain
│   AgentBrain × N：persona + view（重建的可见信息）+ 解析/校验/回退
│   LlmClient（抽象）→ OllamaClient（本地） / 远程后端（随后加）
│   Recorder：god_script.md（人读） + trace.jsonl（训练）
└────────────────────────────────────────────────────────────────
```

**关键点**：引擎不知道、也不关心某座位背后是真人还是 AI——它只发 `ask`、收 `reply`、发 `event`。「谁是人、谁用哪个模型」全部是 orchestrator 的配置。

---

## 3. 公平性与信息可见性（§11 如何落地）

这是整套设计正确性的根。BRD §11 要求引擎严格区分**真相层**与**玩家视图层**，绝不能把真相直接暴露给玩家。

进程外 sidecar 把这件事变得近乎免费：

- **AgentBrain 物理上拿不到 `GameState`。** 它跑在另一个进程里，只能看到引擎**主动发给它这个座位**的东西。
- 引擎发给某座位的信息只有两种来源：
  - `vis=public` 的事件（所有人可见，如死讯、发言、计票）；
  - `vis=private, seat=S` 的事件（仅座位 S 可见，如「你的身份」「查验结果」「你今晚被刀」）。
- `vis=moderator` 的事件（上帝视角，如身份状态面板、各座位的秘密决策记录）**只进 script 写手，永不进任何 brain**。

因此一个座位 brain 的合法 view = 「全部 public 事件」∪「seat 等于自己的 private 事件」。orchestrator 的 `EventRouter` 据此重建 view。**只要引擎侧的 public/private/moderator 口径是对的，brain 怎么写都作弊不了。**

引擎侧这套口径 M12 已经做对了（`notify` / `notifyPlayer` / `notifyModerator` 三分，见 [routing_decision_provider.cpp](../src/io/routing_decision_provider.cpp)）。本期只是把这三个出口从「写字符串到 ostream / 调 channel.tell」改成「发结构化 JSON 事件」，口径不变。

> **唯一的纪律点**：所有秘密信息必须经 `vis=private`（定向）或 `vis=moderator`（仅上帝）发出，绝不能误用 `vis=public`。测试要有一条「扫描每个 brain 的 view，断言其中不含他座位的角色/私密结果」（§9）。

---

## 4. 通信协议规范 v1

> **权威规范已抽到 [`protocol_v1.md`](protocol_v1.md)**；以下为同步摘要，**如有出入以 `protocol_v1.md` 为准**。

JSON-lines（每行一个 JSON 对象，UTF-8，`\n` 分隔）。引擎与 orchestrator 通过引擎子进程的 stdin/stdout 通信。**这是本项目最需要先冻结的耐久产物**——协议稳定，则引擎、orchestrator、未来的 NetworkChannel/客户端可独立演进。

### 4.1 进程编排与生命周期

1. orchestrator 启动引擎：`werewolf --json --board <1|2|3> --seed <N>`
   （board 决定板子，seed 决定发牌；二者都由 orchestrator 选定，保证可复现）。
2. 引擎建好 `GameState`、发牌，发出 `game_start`（携带座位花名册：seat + name，**不含角色**）。
3. 引擎对每个座位发私密 `deal` 事件（`vis=private`）：告知本人角色；狼人附队友。
4. 进入主循环：引擎按需发 `event`（单向）与 `ask`（需 `reply`）。
5. 终局：引擎发 `game_over`，然后退出。

引擎是**单线程同步**的：任一时刻至多一个 `ask` 在途，发出 `ask` 后阻塞读 stdin 直到拿到匹配 `id` 的 `reply`。事件是 fire-and-forget。stdout 是一条线性流，事件与 ask 天然有序，无竞态。

### 4.2 消息信封

每行一个对象，必有字段 `t`（消息类型）。三种类型：

| `t` | 方向 | 含义 | 是否需回复 |
| --- | --- | --- | --- |
| `event` | 引擎 → orch | 信息事件（叙述/发言/死讯/私密结果/上帝记录…） | 否 |
| `ask` | 引擎 → orch | 请求一个决策 | **是**（`reply`） |
| `reply` | orch → 引擎 | 对 `ask` 的应答（按 `id` 关联） | — |

外加两条控制消息：`game_start`、`game_over`（可视为特殊 event，单列以便 orchestrator 初始化/收尾）。

### 4.3 `game_start` / `game_over`

```jsonc
{ "t":"game_start", "protocol":"1.0", "board":"Board9_SeerWitchHunter",
  "seed":12345,
  "seats":[ {"seat":1,"name":"P1"}, {"seat":2,"name":"P2"}, /* ... */ {"seat":9,"name":"P9"} ] }

{ "t":"game_over", "result":"TownWins" }   // 或 "WolfWins"
```

`protocol` 字段做版本协商；orchestrator 校验主版本。

### 4.4 `ask` / `reply`

`ask` 有三种形态，由 `qtype` 区分；`kind` 给出机器可读意图，brain 据此分支而**不解析 `prompt`**（`prompt` 仅供人类终端/调试显示）。

```jsonc
// qtype=choose：选一个目标（对应 PlayerChannel::chooseAmong）
{ "t":"ask", "id":42, "seat":3, "qtype":"choose",
  "kind":"WitchPoison",                 // AskKind，见 player_channel.h
  "prompt":"是否使用毒药（毒谁）",
  "candidates":[ {"seat":1,"name":"P1"}, {"seat":5,"name":"P5"} ],
  "allowSkip":true }
// 回复：choice = 候选 seat，或 null（allowSkip 时表示跳过/弃权/空刀）
{ "t":"reply", "id":42, "choice":5, "reasoning":"P5 昨天踩我，先毒" }

// qtype=confirm：是/否（对应 confirm）
{ "t":"ask", "id":43, "seat":3, "qtype":"confirm",
  "kind":"WitchSave", "prompt":"今晚 P7 被刀，是否使用解药？" }
{ "t":"reply", "id":43, "decision":false, "reasoning":"留解药给关键神职" }

// qtype=speak：自由发言（对应 speak）
{ "t":"ask", "id":44, "seat":3, "qtype":"speak",
  "kind":"Statement", "prompt":"请发言" }       // kind 此处是 SpeechKind
{ "t":"reply", "id":44, "text":"我是预言家，昨晚验 P5 金水……", "reasoning":"打预言家位置" }
```

- `reasoning` 字段**可选**，仅用于 trace（§8）；引擎收到后忽略它对规则的影响，只取 `choice`/`decision`/`text`。
- `choose` 的 `choice` 必须 ∈ `candidates` 的 seat 集合，或在 `allowSkip` 时为 `null`。引擎对非法回复的处理见 §4.7。

#### AskKind（choose/confirm 的 `kind`，来自 [player_channel.h:27](../src/io/player_channel.h)）

`NightKill`(team) · `SelfDestruct` · `Vote` · `RunoffVote` · `Inspect` · `Guard` · `WitchSave`(confirm) · `WitchPoison` · `HunterShot` · `MechanicLearn` · `MechanicBigKnife` · `RunForSheriff`(confirm) · `Withdraw`(confirm) · `SheriffVote` · `ConsolidateSingle`(confirm) · `BallotTarget` · `BadgeTransfer` · `SpeechDirection`(confirm)

> 团队决策（狼刀 `NightKill`）按现有 routing 口径只发给**狼队代表**（最小座号的睁眼狼，机械狼除外），其余睁眼狼通过 private 事件获知结果。详见 §5.3。

#### SpeechKind（speak 的 `kind`）

`Statement`（白天发言，竞选发言也复用它）· `LastWords`（遗言）· **`WolfChat`（狼队私聊，新增，§5.4）**

### 4.5 `event`

```jsonc
{ "t":"event",
  "vis":"public" | "private" | "moderator",
  "seat":3,                 // 仅 vis=private 时必填（接收者）
  "etype":"...",            // 见下表
  "text":"渲染好的中文文案", // 引擎用 messages.h 渲染，orchestrator 直接显示
  "day":2, "phase":"Night", // 上下文
  "data":{ /* 机器可读补充字段，按 etype 不同 */ } }
```

`text` 由引擎用 `core/messages.h`（中文文案单一来源）渲染，orchestrator/终端直接打印——**不在 orchestrator 里重做文案**。`data` 给机器可读补充，让 brain 不必从中文里抠信息。

| `etype` | 典型 `vis` | 含义 | `data` 例 |
| --- | --- | --- | --- |
| `deal` | private | 告知本人角色（狼附队友） | `{role:"Witch", teammates:[2,6]}` |
| `phase` | public | 阶段横幅（第 N 夜/天） | `{day:2, phase:"Day"}` |
| `narration` | public | 法官叙述、睁眼/闭眼、流程提示 | — |
| `speech` | public/private | 某座位的发言（WolfChat 为 private 狼队） | `{speaker:3, kind:"Statement"}` |
| `death` | public | 公布死讯 | `{dead:[7], causes:{7:["Killed"]}}` |
| `vote` | public | 计票结果 | `{tally:{5:3,7:2}, exiled:5}` |
| `result_private` | private | 私密技能结果（查验/通灵/验枪/被刀通知） | `{kind:"Inspect", target:5, isWolf:false}` |
| `decision` | moderator | 上帝记录：某座位的秘密决策 | `{seat:4, kind:"NightKill", target:7}` |
| `status` | moderator | 法官状态面板（全员身份/存活/药剂） | `{...}` |

**发言要广播**：玩家发言经 `speak` 的 ask/reply 产生后，引擎额外发一条 `vis=public` 的 `speech` 事件，使其他 brain 的 view 收到它（狼队私聊则发 `vis=private` 给各狼）。这样上帝 script 与各 brain 看到的发言一致，协议自洽；发言者本人也借此把"自己的发言"留进自己的 view（§7.3）。

### 4.6 版本与前向兼容
- `protocol` 主版本不匹配 → orchestrator 报错退出。
- orchestrator 遇未知 `etype`：记录并按 `vis` 当作普通文本事件处理（不崩）。
- 引擎遇 `reply` 里的未知字段：忽略。
- 新增 AskKind/etype 属**小版本**演进；删除/改语义属**大版本**。

### 4.7 错误、超时、非法回复（引擎侧裁决）
引擎是权威方，必须对坏输入鲁棒：
- `reply.choice` 不在候选集 / 类型错误 / JSON 解析失败 → 引擎回退到**合法默认**（`allowSkip` 则跳过；否则投票取第一个非自己、其余取首候选），并发一条 `vis=moderator` 的 `narration` 记录这次回退。
- orchestrator 侧的超时/模型故障由 orchestrator 兜底（AgentBrain 自带重试+回退，§7.4），最终总会给引擎一个**合法 reply**，使引擎不必等待无限期。
- 引擎对 `ask` 设**可配置软超时**（`--ask-timeout`，默认 **600s**）仅作防挂死；超时则回退到合法默认并记一条 `vis=moderator` 事件。**注意**：本地 deepseek-r1:14b 是推理模型，单次决策可能耗时数十秒~数分钟（§7.5），故默认放宽到 600s，并用 `num_predict` 收紧输出上限来兜住最坏时延；正常路径不触发。

---

## 5. 引擎侧设计（C++）

### 5.1 改动总览（最小、可测、纯增量）

**改：**
- 新增 `src/io/json_decision_provider.*`：实现 `DecisionProvider`，把每个决策方法变成「发 `ask` → 阻塞读 `reply`」，把信息出口变成发 `event`。
- 新增 `src/io/event_sink.h`：`EventSink` 抽象 + `JsonEventSink`（写 stdout）。把「说什么 + 给谁看(vis)」与「怎么传输(json/console/test)」解耦。
- `src/core/enums.h`：`SpeechKind` 增 `WolfChat`。
- `src/flow/game.cpp`：`runNight` 增一处**狼队私聊**步骤（§5.4），及发言后广播 `speech` 事件（§4.5）；`runSheriffElection` **打开候选人发言**（§5.6）。
- `src/app/main.cpp`：增 `--json` 运行模式 + 解析 `--board/--seed`。

**不改：**
- `flow/settlement.*`、`flow/win_condition.*`、`core/roles/*`、`core/abilities/*`、所有规则/结算/胜负逻辑。
- `RoutingDecisionProvider`、`PassAndPlay`、`Console` 等既有 provider（继续服务单屏/传递游玩）。

### 5.2 EventSink 与既有出口的映射

`DecisionProvider` 现有三个信息出口（[decision_provider.h](../src/io/decision_provider.h)）直接映射到 `vis`：

| 现有出口 | 事件 |
| --- | --- |
| `notify(msg)` | `event vis=public etype=narration`（或 death/vote/phase，按调用点细分） |
| `notifyPlayer(seat,msg)` | `event vis=private seat=S` |
| `notifyModerator(msg)` | `event vis=moderator etype=status/...` |
| 定向结果 `onInspectResult` / `onPsychicResult` / `onHunterGunCheck` / `onMechanicLearnResult` | `event vis=private seat=S etype=result_private`（带 `data`） |

实现上 `JsonDecisionProvider` 持有一个 `EventSink*`；上述出口都改成 `sink_->emit(...)`。秘密决策（如狼刀目标、女巫毒谁）在引擎拿到 `reply` 后，额外 `emit(vis=moderator, etype=decision, data=...)`，供上帝 script 还原暗线。

> 实现选择：可以新写一个独立 `JsonDecisionProvider`，也可以把 `RoutingDecisionProvider` 泛化成「持 `EventSink` + 每座位一个 `JsonChannel`」。推荐**前者**——直接实现 `DecisionProvider` 更短，且 routing 的「狼代表/定向通知」逻辑可作为内部 helper 复用。最终类拆分留到开发阶段定，但**协议与 EventSink 抽象先冻结**。

### 5.3 团队决策（狼刀）

沿用现有口径（[routing_decision_provider.cpp:42](../src/io/routing_decision_provider.cpp)）：
- 狼刀 `ask(NightKill)` 只发给**狼队代表**（最小座号的睁眼狼，机械狼除外）。
- 拿到结果后，向其他睁眼狼发 `vis=private etype=narration`：「今晚刀：Px / 空刀」。
- 机械狼不参与（不与狼队见面）。
- v1 由代表在私聊后拍板；「全狼投票定刀」留作后续可选。

### 5.4 狼队私聊（新增，无机制影响）

目的：让 AI 狼协作更真实，并产出协作训练数据。**与白天发言一样，对结算/胜负零影响。**

- `enums.h`：`SpeechKind::WolfChat`。
- `game.cpp::runNight`：在狼人睁眼、做刀决策**之前**，插入一轮私聊：
  - 参与者 = 当前**存活的睁眼狼**（座位升序）；机械狼不参与。
  - 对每个参与者发 `ask(qtype=speak, kind=WolfChat)`；拿到 `text` 后，向**其他每个睁眼狼**发 `event vis=private etype=speech data={speaker, kind:"WolfChat"}`，并发 `vis=moderator` 入上帝 script。
  - **每晚刀前都进行**；v1 固定 **1 轮**（顺序发言）。多轮/自由往复留后续。
  - 仅当存活睁眼狼 ≥ 1 时进行；只剩机械狼则跳过。
  - **自爆狼的当夜私聊（BRD §2）**：前一白天自爆的狼，理论上当夜仍可参与一次私聊向队友传递最后情报，随后离场——**仅讨论、无机制影响**。**例外**：若它是**最后一名睁眼狼**（无其他睁眼狼可聊），则不参与，按 BRD §2「不点刀、入夜后才直接离场」处理（避免他人据离场时机反推狼队余员）。**v1 实现先只让存活睁眼狼参与**；「自爆狼再聊一晚」作为忠实度精修后补（因其无机制影响，可安全延后）。
  - ⚠️ **不要混淆**：「最后一名**睁眼**狼自爆」**不等于游戏结束**——若机械狼/隐狼等**不睁眼狼**仍存活，引擎按阵营计数仍判「狼未灭」，游戏继续、由不睁眼狼接手（如机械狼独立刀人，见 BRD §2/§4.1、[win_condition.cpp](../src/flow/win_condition.cpp) 按 `Faction::Wolf` 计数）。只有**全场狼皆出局**才判好人胜。
- 记录：写入 `GameState.speeches`（`kind=WolfChat`），`formatTranscript` 可在上帝复盘中展示；brain 侧因 `vis=private` 仅狼可见，不泄露给好人。

> 实现位置：狼私聊只是「收集发言 + 定向转发」，无机制效果，可作为 `runNight` 开头（重置 tonight 标记之后、夜间行动循环之前）的一个独立 helper，narration cue 用「狼人请睁眼，先内部交流」。这样不侵入既有按 `nightOrder` 排序的夜间行动循环。

### 5.5 `--json` 模式（main.cpp）

```
werewolf --json --board 1 --seed 12345
```
- 解析 board/seed；`randomDeal(board, seed)` 发牌（可复现）。
- 装配 `JsonEventSink(stdout)` + `JsonDecisionProvider(stdin, sink)`，构造 `Game` 并 `run()`。
- 全程不向 stdout 打任何非协议文本（调试信息走 stderr）。

### 5.6 警长竞选候选人发言（打开）

BRD §7.2-2 要求「候选人依次发言拉票」，但当前引擎跳过了（[game.cpp](../src/flow/game.cpp) 内注释 "Candidate speeches are cosmetic and skipped"）。AI 局**必须打开**——这既是 BRD 的本意，也是 AI 拉票/站边的关键信息源。
- 在 `runSheriffElection` 报名确定候选人后、退水/自爆窗口前，按候选人顺序发 `ask(qtype=speak, kind=Statement)`（如需在 script 里区分竞选发言，可加 `SpeechKind::CampaignSpeech`），每条作为 `vis=public speech` 事件广播。
- 顺延到第二天的竞选**无发言**（BRD §7.5），保持不变。
- 纯增量、无机制影响，与白天发言同构；打开后引擎反而**更贴合 BRD**。

---

## 6. Orchestrator 侧设计（Python）

### 6.1 组件
- **EngineProcess**：`subprocess` 启动引擎，逐行读 stdout，写 stdin；维护 `id → Future` 关联 ask/reply。
- **EventRouter**：每条 `event` 派发——`public` → 所有 AgentBrain 的 view + script；`private(seat)` → 该 brain 的 view + script；`moderator` → 仅 script。
- **SeatTable**：`seat → HumanTerminal | AgentBrain`，从配置读（§7.2）。
- **AgentBrain**（§7.3）：持有 persona/system prompt、累积 view、`LlmClient`，把 `ask` 变成模型调用并解析成合法回复。
- **HumanTerminal**：把 `ask` 渲染到终端、读用户输入、回 reply；把发给该真人座位的 public/private 事件打印出来（真人也只看自己的 view，不看 moderator）。
- **Recorder**（§8）：写 `god_script.md` + `trace.jsonl`。

### 6.2 主循环（伪码）

```python
proc = EngineProcess(["werewolf", "--json", "--board", board, "--seed", seed])
gs = proc.read()                       # game_start
seats = setup_seats(gs, config)        # 谁是人、谁用哪个模型/persona
for msg in proc:                       # 逐行
    if msg.t == "event":
        router.dispatch(msg)           # 喂 view + 写 script
    elif msg.t == "ask":
        seat = seats[msg.seat]
        reply = seat.answer(msg)       # HumanTerminal 或 AgentBrain（可能慢）
        recorder.log_decision(msg, reply)   # trace
        proc.write(reply)
    elif msg.t == "game_over":
        recorder.finalize(msg.result); break
```

引擎同步、一次一个 ask，所以 orchestrator 也顺序应答即可；本地单模型下请求本就串行。远程多模型的并发优化留后续（§11）。

---

## 7. LlmClient 抽象与多模型接入（扩展性命脉）

这是你后续主要发力、反复迭代的部分，设计目标：**加一家模型 = 加一个后端类 + 改配置，不动 AgentBrain 与协议。**

### 7.1 抽象接口

```python
@dataclass
class Completion:
    text: str
    raw: dict          # 原始响应，存入 trace
    usage: dict | None # token 计数（可选）

class LlmClient(Protocol):
    def complete(self, system: str, messages: list[dict],
                 *, schema: dict | None = None,
                 temperature: float = 0.6,   # 本地 deepseek-r1 建议 0.6，见 §7.5
                 max_tokens: int = 1024) -> Completion: ...
```

`schema` 非空时要求模型产出匹配该 JSON Schema 的结构化输出（用于 choose/confirm）；为空时自由文本（用于 speak）。各后端用各自最佳手段实现「结构化输出」，对上统一。

### 7.2 后端与配置

后端（实现同一 `LlmClient`）：
- `OllamaClient(model, host="http://localhost:11434")` — **本期默认**。choose/confirm 把 `AgentBrain` 的 JSON schema 作为 Ollama **`format`** 下发——**只约束 `content` 为合法 JSON（choice 的 `enum` 还保证合法候选），思考仍在独立 `thinking` 字段、不被抑制** → 决策几乎不再回退。speak 不带 `format`（自由发言）。对 R1 做适配（§7.5）。
- `OpenAICompatClient(base_url, api_key_env, model)` — 覆盖 OpenAI 及大量兼容服务；结构化用 `response_format=json_schema` / function calling。
- `AnthropicClient(api_key_env, model)` — Anthropic Messages API；结构化用 tool use。
- 后续：其它家照此添加。

工厂按配置里的 `provider` 字段构造。配置示例（orchestrator 读，**不进引擎**）：

```yaml
board: 1
seed: 12345
human_seat: 1
default_ai:                          # 不写 seats 的 AI 席都用它
  provider: ollama
  model: "deepseek-r1:14b"           # 本期本地默认（§7.5）
  options: { num_ctx: 8192, temperature: 0.6, top_p: 0.95, num_predict: 2048 }
seats:                               # 覆盖个别席位
  4: { persona: aggressive }                                  # 仍用 default_ai 的模型
  7: { provider: anthropic, model: "claude-...", api_key_env: ANTHROPIC_API_KEY, persona: cautious }  # 远程示例（随后启用）
personas:                            # persona → 追加到提示的策略口吻
  aggressive: "你打法激进，敢起跳、敢对跳、敢踩人。"
  cautious:   "你打法稳健，重逻辑、慎站边、留余地。"
```

- **API key 只走环境变量**（`ANTHROPIC_API_KEY` / `OPENAI_API_KEY` …），配置文件只写 `api_key_env` 指向哪个变量名，**绝不把密钥写进文件或提交**。
- 「**一个 AI 的多 subagent**」= 多个座位 → 同一 provider+model，不同 persona（共享一份本地权重）。
- 「**多个独立 AI**」= 不同座位指不同 provider/model。
- 两者都只是配置差异，AgentBrain 代码相同。

### 7.3 AgentBrain

```
AgentBrain(seat, role_info, persona, llm: LlmClient):
  system = identity_preamble       # 「你正在玩狼人杀（9 人局），你是玩家 Pn；目标=帮你的阵营获胜」
         + rules_summary           # 该板规则要点（精简版规则书）
         + role_block(role_info)   # 你的角色/技能/胜利条件/队友(若狼)
         + persona                 # 策略口吻
         + output_contract         # speak: 只输出"要当众说的话"；choose/confirm: 思考后只输出一行 JSON

  # render_ask 对 speak 类要把任务讲清楚：「现在轮到你公开发言，请说出你要对全场讲的话」
  # （白天发言/竞选发言/遗言/狼队私聊各有不同措辞，但都明确"这是要被别人听到的发言"）
  view: list[Event]                # EventRouter 持续追加（public + 自己的 private）

  answer(ask) ->
    messages = render_view(view) + render_ask(ask)   # 把可见历史 + 本次决策喂模型
    schema   = schema_for(ask.qtype, ask.candidates, ask.allowSkip)
    comp     = llm.complete(system, messages, schema=schema)   # speak 时 schema=None
    parsed   = parse_and_validate(comp.text, ask)              # 见 7.4
    return Reply(parsed, reasoning=parsed.reasoning)
```

- **每个 AI 的完整记忆（view 必须涵盖）**——这是本期重点（你强调的「每个 AI 记录下自己获取的全部信息」）：
  1. **自己的角色 / 技能 / 胜利条件**，以及（若狼）**队友**——来自开局 private `deal` 事件；
  2. **自己历次发言**（白天发言、竞选发言、遗言）——自己的发言经 public `speech` 事件回流，天然进 view；
  3. **所有能听到的他人发言**（含真人）——来自 public `speech` 事件；
  4. **狼队夜间私聊**（仅当自己是狼）——来自 private `speech kind=WolfChat` 事件；
  5. **自己的私密技能结果**：查验/通灵/验枪/被刀通知/学习结果——来自 private `result_private` 事件；
  6. **公共事件**：死讯、计票、警长流程、阶段横幅——来自 public 事件。
  即 §3 的「全部 public 事件 ∪ seat==自己的 private 事件」，**全程累积、不丢**。
- **心里话 vs 公开发言分离**：`reasoning`（含 R1 的 `<think>`，§7.5）只进 trace + 自己的记忆，**绝不广播**；只有 `speak` 的 `text` 才经引擎广播。这正是狼人杀「内心/明面」的天然分裂，也是最值钱的训练信号。
- **上下文预算（重要）**：deepseek-r1:14b 的 `num_ctx` 受内存限制只能开到 ~8192（§7.5），而中后期 view + R1 的 `<think>` 很容易超。渲染 view 时要**分级保真**：角色/私密结果/各人发言**逐字保留**，较旧的阶段横幅/法官叙述**可压缩或摘要**。长局滚动摘要作为**必做项**（非"可选优化"）。

### 7.4 解析 / 校验 / 回退（保证游戏永不卡死）

```
parse_and_validate(out, ask):
  if R1: out = strip_think(out)          # 先切掉 <think>…</think>，§7.5
  try: obj = json.loads(out)             # speak 直接取自由文本
  except: obj = salvage(out)             # 容错：抽取首个 JSON / 关键字段
  if qtype==choose:
     if obj.choice in ask.candidate_seats: ok
     elif obj.choice is None and ask.allowSkip: ok(skip)
     else: 重试一次（追加「必须从候选中选」）→ 仍失败 → 合法默认
  if qtype==confirm: 取 bool；缺失→默认 False（保守：不救/不上警/不自爆）
  if qtype==speak:   取 text；缺失→ ""（视为过）
```

合法默认与 `BotChannel` 一致（[bot_channel.cpp](../src/io/bot_channel.cpp)）：投票投首个非自己，其余可跳过则跳过。**无论模型多离谱，orchestrator 总返回一个合法 reply。**

### 7.5 本地模型：deepseek-r1:14b（默认；32b 为质量档）

本期实跑 **deepseek-r1:14b**（用户为速度选它；32b 更强但在 Mac Air 上更慢，留作质量档）。deepseek-r1 是**推理模型**（产出 `<think>` 思考），对参数与提示方式有特定要求。

**Ollama 参数（偏稳、保证能跑）：**

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `num_ctx` | **8192**（紧张则 4096） | 上下文窗口。权重 Q4 约 20GB，32GB 机器留给 KV cache 的余量有限；8192 较安全，若见到换页/明显变慢就降到 4096 并加强 view 摘要（§7.3）。 |
| `temperature` | **0.6** | DeepSeek 官方对 R1 的推荐（0.5–0.7，0.6 最佳）；过低反而退化。 |
| `top_p` | **0.95** | 官方推荐。 |
| `num_predict` | **2048** | 输出上限。**必须同时覆盖 thinking + 最终答案**——给太小（如 1024）会在长 prompt 下被思考吃光、`content` 返回空 → 触发回退。 |
| `seed` | 可配 | 设定后利于复现 trace（发牌已由引擎 `--seed` 固定）。 |

**R1 适配（务必照做，否则效果差）：**
1. **不要用 system role**：DeepSeek 建议 R1 把所有指令放进 **user** 消息。`OllamaClient` 对 R1 做适配——把 `LlmClient.complete` 的 `system` **折叠进首条 user 消息**，而非走 system 字段。
2. **解析 `<think>`，绝不把思考当发言**：模型先输出 `<think>…</think>` 再给答案。解析器**先切出 `<think>` 内容当作 `reasoning`**（存 trace、进自己记忆，**绝不广播**）。
   - 对 **speak**：**只把 `</think>` 之后的正文当作要广播的发言**——`<think>` 里的推理（怀疑谁、要不要悍跳）**永远不进** `speak.text`。若模型只吐了 think、没给 think 外的正文，按 §7.4 重试/回退（绝不把思考块直接发出去）。
   - 对 **choose/confirm**：`</think>` 之后取那行 JSON。
   - 这天然喂给「心里话 vs 明面」的分离（§7.3）。
3. **结构化输出用 Ollama `format`**：把候选 `enum` 的 JSON schema 作为 `format` 下发，**强制 `content` 为合法 JSON 且 choice ∈ 合法候选**；因 Ollama 把思考放在独立 `thinking` 字段，**`format` 不抑制思考**。这从根本上消除了「R1 只思考不吐 JSON → 回退」。§7.4 的宽松解析 + 合法默认仍作最终兜底。

**实测（deepseek-r1:14b @ Ollama，已接通验证）：**
- **结构化输出已解决回退**：choose/confirm 用 `format`=候选 enum 的 schema → content 必为合法 JSON 且 choice 合法、thinking 照常（实测预言家会真验人、上警是真决策，`fallback` 基本消失）。不加 `format` 时 R1 常把答案留在思考里 → 回退、神职白白跳过。
- Ollama 把 **thinking 与 content 分字段**返回；`OllamaClient` 把 `thinking` 包回 `<think>…</think>` 拼在 content 前，供 `AgentBrain` 统一切分（reasoning 进 trace、**绝不广播**）。
- `think:false` 在本机**不可靠**（choose 仍思考、speak 直接拒答），故**默认保持 think 开**。
- 已验证可用：choose 返回**模型真实选择**（非回退）、speak 为干净的第一人称发言、`<think>` 不外泄。
- **时延：单次决策约 17–35s**（M 芯片 Air，14b）。整局 8 AI、多天多阶段会**很慢**（约数十分钟~数小时）——14b 推理在该硬件上的现实。**管线检查/调试用 `--fake`（秒级）**；真模型整局挑时间跑。
- 想更快可换更小模型（`--model deepseek-r1:1.5b`/`7b`），但 R1 越小策略越弱。

---

## 8. 记录产物

两份，皆由 orchestrator 写（它是事件流的唯一消费者，且握有模型 I/O）。

### 8.1 上帝视角 script（`god_script.md`，人读/复盘）
- 消费**全部**事件（含 `vis=moderator`），按天/阶段分组，时间序渲染：
  法官叙述、私聊/发言、夜间秘密决策（谁刀谁、谁验谁、女巫用药）、死讯、计票、技能结算、警长流程、终局。
- 形如一份「完整牌局回放」。C++ 侧 `formatTranscript`（[transcript.h](../src/flow/transcript.h)）可作为「仅发言」的补充视图。

### 8.2 post-training trace（`trace.jsonl`，机读/训练）
每个 AI 决策一行（**按你的要求，token/延迟/非法输出/回退/`<think>` 全记**）：
```jsonc
{ "game_id":"...", "seat":3, "role":"Witch", "day":2, "phase":"Night",
  "qtype":"choose", "kind":"WitchPoison",
  "visible_context":[ /* 决策时该 brain 的 view 快照 */ ],
  "prompt_sent":[ /* 实际发给模型的 messages */ ],
  "model":"deepseek-r1:14b",
  "raw_output":"<think>…</think>{\"choice\":5}",   // 含原始 think
  "reasoning":"…",            // 从 <think> 切出（R1，§7.5）
  "parsed":{"choice":5},
  "fallback":false,           // 是否走了合法默认
  "attempts":[ {"raw":"…非法输出…","error":"choice 不在候选"} ],  // 重试/被回退的非法输出，全留
  "latency_ms":48213,
  "usage":{ "prompt_tokens":1820, "completion_tokens":640 } }
```
末尾一行收尾：
```jsonc
{ "game_id":"...", "result":"WolfWins",
  "per_seat":{ "3":{"role":"Witch","died_day":3,"won":false}, ... } }
```
即 `(可见上下文 → 心里话 → 行动 → 战局结果)`，可直接喂 post-training（SFT/偏好/RL 皆可）。

> 复现性：要可比的 trace，可配 `temperature`/`seed`（评测/采数据时固定）；引擎 `--seed` 已保证发牌可复现。模型本身的非确定性单独标注。

---

## 9. 测试策略

- **引擎协议测试（C++/GoogleTest）**：给 `JsonDecisionProvider` 喂内存 istream（预设 reply 行）、捕获 ostream（断言 ask/event 序列与字段），跑完整局——和现有 `ScriptedDecisionProvider` 测试同构。
- **狼队私聊流程测试（C++）**：断言私聊只对狼可见、不影响结算/胜负、机械狼不参与。
- **候选人发言测试（C++）**：断言竞选阶段候选人发言被收集并以 public `speech` 广播；顺延竞选（第二天）无发言。
- **既有 104 用例保持绿**：引擎规则零改动，新增皆为 io/流程增量。
- **AgentBrain（Python）**：用 `FakeLlmClient`（返回预设 JSON / 故意返回垃圾 / 模拟 R1 的 `<think>` 包裹）断言：合法时正确走子；垃圾时重试后回退到合法默认；`<think>` 被正确切出当 reasoning。
- **公平性断言（关键）**：跑一局后扫描每个 brain 收到的 view，断言其中**不含任何他座位的角色或私密结果**；扫描发给真人座位的内容，断言不含 `vis=moderator`。
- **端到端（Python）**：启动真引擎子进程 + FakeLlmClient brains，断言整局完成、`god_script.md` 与 `trace.jsonl` 正确产出。

---

## 10. M15 里程碑拆解与构建顺序

> 每步可独立编译/测试通过再继续（项目约定：小步推进）。

1. **冻结协议 v1**（本文 §4）。先把 schema 定死，后续都照它写。
2. **引擎：EventSink + JsonDecisionProvider + `--json` 模式**（§5.1/5.2/5.5）。配协议测试。引擎规则不动，104 用例保持绿。
3. **引擎：狼队私聊（§5.4）+ 候选人发言（§5.6）+ 发言广播 `speech` 事件**。配流程测试。
4. **Python orchestrator 骨架**：EngineProcess + EventRouter + HumanTerminal + Recorder。先用 `FakeLlmClient` 跑通端到端（全 AI 走假大脑），验证整局完成 + 两份记录产出 + 公平性断言。
5. ✅ **LlmClient + OllamaClient(R1 适配) + AgentBrain**（§7）：接本地 deepseek-r1:14b——per-decision 已验证（真实选择 + 干净第一人称发言 + reasoning 入 trace、不外泄）。真模型整局较慢（§7.5），挑时间跑；管线/调试用 `--fake`。
6. **打磨**：persona 多样化、prompt 模板、`<think>` 解析鲁棒性、view 滚动摘要、trace 字段完善。

里程碑达成判据：**1 真人 + 8 本地 AI 跑完一局 9 人板（含狼私聊、候选人发言、发言、夜间技能、投票、警长），产出完整上帝 script 与 trace，且公平性测试通过。**

---

## 11. 远期演进（设计已为其留路）

- **远程多模型**：加 `OpenAICompatClient` / `AnthropicClient`，配置即用；API key 走 env。每席可不同模型，便于「模型对抗」与自对弈采数据。
- **并发**：远程后端下，相互独立的夜间决策可并发；需引擎支持「批量发 ask」或 orchestrator 侧推测式调度（当前同步模型先不做）。
- **多模态语音**（你的明确方向）：
  - 输入：STT（如 Whisper）把真人语音转成 `speak` 的 `text`——**协议/引擎不变**，只是 HumanTerminal 多一个输入源。
  - 输出：TTS 把 `speech` 事件念出来——orchestrator 侧旁挂，引擎不变。
  - 文本优先的设计就是为了让多模态变成「换 orchestrator 的 I/O」，而非动引擎。
- **真·联机**：把 stdio 传输换成 WebSocket，协议照旧；多真人 = 多个 HumanTerminal/浏览器客户端连到 orchestrator；引擎仍是唯一权威。
- **狼私聊增强**：多轮、自由往复、全狼投票定刀、自爆狼当夜再聊一晚（§5.4）。

---

## 12. 已定决策（原开放问题）

| # | 问题 | 决定 |
| --- | --- | --- |
| 1 | 本地模型选型与参数 | **deepseek-r1:14b**（Ollama）；num_ctx 8192 / temp 0.6 / top_p 0.95 / num_predict 2048；R1 适配见 **§7.5**。想快可换更小模型，仅改配置。 |
| 2 | 狼私聊轮数 | **每晚刀前 1 轮**（顺序发言）；多轮/自由往复留后续（§5.4）。 |
| 3 | 真人 UX | **朴素逐行打字**即可；重点是 **AgentBrain 完整记忆**（§7.3）。TUI 留后续。 |
| 4 | 警长竞选候选人发言 | **打开**（§5.6）。 |
| 5 | trace 粒度 | **全记**：token/延迟/非法输出/回退/`<think>` 推理（§8.2）。 |
| 6 | 引擎 ask 软超时 | **可配，默认 600s**（§4.7），因本地推理模型慢而放宽。 |

**仍待实测/微调（开发中定）：**
- deepseek-r1:14b 在你机器上的实际时延 / num_ctx 上限 / 是否需降到 14b（§7.5）。
- view 滚动摘要的触发阈值与保真策略（§7.3）。
- 「自爆狼再聊一晚」的忠实度精修是否要做（§5.4，无机制影响，可后补）。

---

## 附录 A：一回合协议流水示例（节选，9 人板）

```jsonc
{"t":"game_start","protocol":"1.0","board":"Board9_SeerWitchHunter","seed":12345,
 "seats":[{"seat":1,"name":"P1"},/*...*/{"seat":9,"name":"P9"}]}
{"t":"event","vis":"private","seat":2,"etype":"deal","data":{"role":"Werewolf","teammates":[2,4,6]},"text":"你的身份是：狼人；队友 P4 P6"}
{"t":"event","vis":"private","seat":3,"etype":"deal","data":{"role":"Seer"},"text":"你的身份是：预言家"}
/* ... 其余座位 deal ... */
{"t":"event","vis":"public","etype":"phase","day":1,"phase":"Night","text":"第 1 夜，天黑请闭眼"}
{"t":"event","vis":"public","etype":"narration","text":"狼人请睁眼，先内部交流"}
{"t":"ask","id":1,"seat":2,"qtype":"speak","kind":"WolfChat","prompt":"狼队私聊，请发言"}
{"t":"reply","id":1,"text":"今晚刀 P3 试预言家位","reasoning":"P3 像首验位"}
{"t":"event","vis":"private","seat":4,"etype":"speech","data":{"speaker":2,"kind":"WolfChat"},"text":"P2（狼队）：今晚刀 P3 试预言家位"}
{"t":"event","vis":"private","seat":6,"etype":"speech","data":{"speaker":2,"kind":"WolfChat"},"text":"P2（狼队）：今晚刀 P3 试预言家位"}
/* ... P4 P6 私聊 ... */
{"t":"ask","id":4,"seat":2,"qtype":"choose","kind":"NightKill","prompt":"狼队请选择今晚的刀杀目标",
 "candidates":[{"seat":1,"name":"P1"},/*...*/],"allowSkip":true}
{"t":"reply","id":4,"choice":3,"reasoning":"按讨论刀 P3"}
{"t":"event","vis":"moderator","etype":"decision","data":{"seat":2,"kind":"NightKill","target":3},"text":"【上帝】狼队刀 P3"}
{"t":"event","vis":"private","seat":4,"etype":"narration","text":"【狼队】今晚刀：P3"}
{"t":"ask","id":5,"seat":3,"qtype":"choose","kind":"Inspect","prompt":"请查验一名玩家","candidates":[/*...*/],"allowSkip":true}
{"t":"reply","id":5,"choice":2}
{"t":"event","vis":"private","seat":3,"etype":"result_private","data":{"kind":"Inspect","target":2,"isWolf":true},"text":"【查验结果】P2 是 狼人（查杀）"}
/* ... 女巫被刀通知（private 给女巫）、用药 ask、猎人验枪 private ... */
{"t":"event","vis":"public","etype":"phase","day":1,"phase":"Day","text":"第 1 天，天亮了"}
{"t":"event","vis":"public","etype":"death","data":{"dead":[3],"causes":{"3":["Killed"]}},"text":"昨夜 P3 出局"}
/* ... 警长竞选：候选人发言（public speech）→ 投票；白天发言（public speech）；放逐投票（vote）... */
{"t":"event","vis":"public","etype":"vote","data":{"tally":{"2":5,"5":2},"exiled":2},"text":"投票结果：P2=5，P5=2，P2 被放逐"}
/* ... 直到 ... */
{"t":"game_over","result":"TownWins"}
```
