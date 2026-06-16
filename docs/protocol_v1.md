# Werewolf Engine ⇄ Orchestrator 协议 v1

> 状态：**v1（冻结中）**。这是引擎（C++，权威真相层）与 orchestrator（Python，拥有人类终端 + AI 大脑）之间的线缆协议，是 AI agent 方向最核心的耐久产物。
> 配套：总体设计见 [`ai_agents_design.md`](ai_agents_design.md)；规则见 [`werewolf_business_requirement_document.md`](werewolf_business_requirement_document.md)。
> 协议稳定，则引擎 / orchestrator / 未来的 `NetworkChannel`（WebSocket）/ 浏览器客户端可各自独立演进。

---

## 1. 传输与分帧

- **传输**：orchestrator 以子进程方式启动引擎；
  - 引擎 **stdout** → orchestrator：`event` / `ask` / `game_start` / `game_over`；
  - orchestrator → 引擎 **stdin**：`reply`；
  - 引擎 **stderr**：人读调试日志，**非协议**，orchestrator 可转存或忽略。
- **分帧**：JSON-lines —— 每行**恰好一个** JSON 对象，UTF-8，以 `\n` 结束。对象内部不得含裸 `\n`（字符串里的换行按 JSON 规则转义为 `\n`）。
- **必含字段**：每个对象都有 `t`（消息类型，见 §3）。
- **未来传输**：把 stdin/stdout 换成 socket/WebSocket 时，**逐行 JSON 的语义不变**——本协议与具体传输解耦。

---

## 2. 生命周期与握手

```
orchestrator                              engine
     │   spawn: werewolf --json --board B --seed S
     │ ───────────────────────────────────────────▶
     │                                    建 GameState、发牌
     │   ◀──────  game_start（花名册，不含角色）
     │   ◀──────  event(deal, private)  × 每座位   （私密告知本人角色）
     │
     │   ◀──────  event(...)            （叙述/发言/死讯/私密结果/上帝记录…）
     │   ◀──────  ask(...)              （需要一个决策）
     │   ──────▶  reply(...)            （应答，按 id 关联）
     │              …循环…
     │   ◀──────  game_over             然后引擎退出
```

- 引擎**单线程同步**：任一时刻至多一个 `ask` 在途；发出 `ask` 后**阻塞读 stdin**直到收到匹配 `id` 的 `reply`。
- `event` 是单向 fire-and-forget，不需应答。
- stdout 是一条**线性有序**流：某个 `ask` 之前出现的 `event` 即该决策的上下文，无并发、无竞态。

### 启动参数（引擎）
```
werewolf --json --board <1|2|3> --seed <uint>
```
- `--board`：1=9 人预女猎，2=12 人预女猎守+狼枪，3=12 人通灵机械狼。
- `--seed`：发牌随机种子（`randomDeal`），保证整局可复现。
- `--ask-timeout <秒>`（可选，默认 600）：见 §8。
- orchestrator 拥有全部配置（板子、种子、谁是人类、各 AI 用哪个模型）；引擎只认 `--board/--seed/--ask-timeout`。

---

## 3. 消息类型总览

| `t` | 方向 | 需回复 | 说明 |
| --- | --- | --- | --- |
| `game_start` | 引擎→orch | 否 | 开局：协议版本 + 板子 + 花名册（**不含角色**） |
| `event` | 引擎→orch | 否 | 信息事件（叙述/发言/死讯/私密结果/上帝记录…），见 §6 |
| `ask` | 引擎→orch | **是** | 请求一个决策，见 §5 |
| `reply` | orch→引擎 | — | 对某个 `ask` 的应答（按 `id`），见 §5 |
| `game_over` | 引擎→orch | 否 | 终局结果；其后引擎退出 |

---

## 4. `game_start` / `game_over`

```jsonc
{ "t":"game_start",
  "protocol":"1.0",
  "board":"Board9_SeerWitchHunter",   // 引擎内部板名
  "seed":12345,
  "seats":[ {"seat":1,"name":"P1"}, {"seat":2,"name":"P2"}, … {"seat":9,"name":"P9"} ] }
```
- `seats` 按座位升序；`seat` 是 1..N 的座位号（本协议统一用 `seat` 作为玩家标识；引擎内部 `id==seat`）。
- **绝不含角色**——角色经 `deal` 私密事件下发（§6）。

```jsonc
{ "t":"game_over", "result":"TownWins" }   // 或 "WolfWins"
```

---

## 5. `ask` / `reply`

`ask` 由 `qtype` 分三形态。`kind` 给机器可读意图（见 §5.4），客户端据此分支，**不解析 `prompt`**（`prompt` 仅供人类终端/调试显示）。

### 5.1 公共字段（所有 ask）
| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `t` | `"ask"` | |
| `id` | int | 单调递增，`reply` 按此关联 |
| `seat` | int | 被询问的座位 |
| `qtype` | `"choose"`/`"confirm"`/`"speak"` | 形态 |
| `kind` | string | 意图：choose/confirm 用 `AskKind`，speak 用 `SpeechKind` |
| `prompt` | string | 人读提示（中文，引擎渲染） |
| `day` | int | 第几天（夜与其后的白天同号） |
| `phase` | `"Night"`/`"Day"` | 阶段 |

### 5.2 choose
```jsonc
{ "t":"ask", "id":42, "seat":3, "qtype":"choose", "kind":"WitchPoison",
  "prompt":"是否使用毒药（毒谁）", "day":1, "phase":"Night",
  "candidates":[ {"seat":1,"name":"P1"}, {"seat":5,"name":"P5"} ],
  "allowSkip":true }
```
- `candidates`：合法目标（座位+名字）。
- `allowSkip`：true 表示可跳过/弃权/空刀（reply `choice=null`）。
- **reply**：
```jsonc
{ "t":"reply", "id":42, "choice":5, "reasoning":"P5 昨天踩我，先毒" }
{ "t":"reply", "id":42, "choice":null }     // 跳过（仅当 allowSkip）
```
  `choice` 必须 ∈ candidates 的 seat，或 `null`（allowSkip 时）。否则按 §8 回退。

### 5.3 confirm / speak
```jsonc
// confirm（是/否）
{ "t":"ask", "id":43, "seat":3, "qtype":"confirm", "kind":"WitchSave",
  "prompt":"今晚 P7 被刀，是否使用解药？", "day":1, "phase":"Night" }
{ "t":"reply", "id":43, "decision":false, "reasoning":"留解药" }

// speak（自由发言）；kind 此处是 SpeechKind
{ "t":"ask", "id":44, "seat":3, "qtype":"speak", "kind":"Statement",
  "prompt":"请发言", "day":1, "phase":"Day" }
{ "t":"reply", "id":44, "text":"我是预言家，昨晚验 P5 金水……", "reasoning":"打预言家位置" }
```
- `decision`：bool；缺失/非法默认 `false`（保守：不救/不上警/不自爆/归多人）。
- `text`：要**当众说出的话**（`""` = 过）。注意：客户端**绝不能把模型的内部思考（如 R1 的 `<think>`）放进 `text`**（见 `ai_agents_design.md` §7.5）。
- `reasoning`：**可选**，仅用于 trace；引擎收到后忽略其对规则的影响。

### 5.4 AskKind / SpeechKind 参考

choose/confirm 的 `kind`（来自 `src/io/player_channel.h`）：

| kind | qtype | 触发者 | candidate / 语义 |
| --- | --- | --- | --- |
| `NightKill` | choose | 狼队代表（§7） | 存活玩家（含自刀）；`null`=空刀 |
| `SelfDestruct` | confirm | 每个可自爆狼 | 是否自爆 |
| `Inspect` | choose | 预言家 / 学通机械狼 | 存活玩家；`null`=不验 |
| `Guard` | choose | 守卫 / 学守机械狼 | 存活玩家（已排除上一晚目标，除非板子允许连守）；`null`=空守 |
| `WitchSave` | confirm | 女巫（解药未用且今晚有刀） | 是否对被刀者用解药 |
| `WitchPoison` | choose | 女巫 | 存活玩家（含自毒）；`null`=不毒 |
| `HunterShot` | choose | 猎人/狼枪出局且可开枪 | 存活玩家；`null`=不开枪 |
| `MechanicLearn` | choose | 机械狼（全局一次） | 存活玩家；`null`=今晚不学 |
| `MechanicBigKnife` | choose | 机械狼（已成独狼、学过狼） | 存活玩家；`null`=留待后用 |
| `Vote` | choose | 放逐投票（首轮非警长） | 存活玩家；`null`=弃票 |
| `RunoffVote` | choose | 放逐决胜轮 | 并列候选；`null`=弃票 |
| `RunForSheriff` | confirm | 每个存活玩家 | 是否上警 |
| `Withdraw` | confirm | 每个候选人 | 是否退水 |
| `SheriffVote` | choose | 竞选投票（未上警者） | 候选人；`null`=弃票 |
| `ConsolidateSingle` | confirm | 警长（放逐归票） | 归单人(1.5)？否=归多人PK(1.0) |
| `BallotTarget` | choose | 警长（放逐归票） | 存活玩家；归单人时不可空 |
| `BadgeTransfer` | choose | 出局的警长 | 存活玩家；`null`=撕毁警徽 |
| `SpeechDirection` | confirm | 警长 | 向左（座位号增大）？否=向右 |

speak 的 `kind`（`SpeechKind`）：`Statement`（白天发言；竞选发言复用）· `LastWords`（遗言）· `WolfChat`（狼队夜间私聊，private）。

> **团队决策（`NightKill`）**：只向**狼队代表**（最小座号的睁眼狼，机械狼除外）发 ask；其余睁眼狼通过 private `narration` 事件获知结果。

---

## 6. `event`

```jsonc
{ "t":"event",
  "vis":"public" | "private" | "moderator",
  "seat":3,                  // 仅 vis=private 必填（接收座位）
  "etype":"…",               // 见下表
  "text":"渲染好的中文文案",  // 引擎用 messages.h 渲染，客户端直接显示
  "day":2, "phase":"Night",
  "data":{ … } }             // 机器可读补充，按 etype 不同；可缺省
```

### 6.1 可见性 `vis`（§11 信息隔离的落地）
- `public`：所有座位可见（→ 进每个 brain 的 view + 上帝 script）。
- `private` + `seat=S`：仅座位 S 可见（→ 仅进 S 的 view + 上帝 script）。
- `moderator`：仅上帝/观战（→ **只进 script，永不进任何 brain，也不进人类玩家终端**）。

> **铁律**：任何秘密信息必须用 `private` 或 `moderator`，**绝不能误用 `public`**。某 brain 的合法 view = 所有 `public` 事件 ∪ `seat==自己` 的 `private` 事件。

### 6.2 etype 与 data

| etype | 典型 vis | 含义 | data 字段 |
| --- | --- | --- | --- |
| `deal` | private | 告知本人角色 | `{role, faction, subKind, teammates?:[seat]}`（狼附 `teammates`；机械狼无队友） |
| `phase` | public | 阶段横幅 | `{day, phase}` |
| `narration` | public/private | 法官叙述、睁眼/闭眼、流程提示、狼队刀讯（private 给狼） | 视情况；可空 |
| `speech` | public/private | 某座位的发言（WolfChat → private 给各狼） | `{speaker:seat, kind:SpeechKind}` |
| `death` | public | 公布死讯 | `{dead:[seat], causes:{seat:[DeathCause]}}` |
| `vote` | public | 计票结果 | `{round, tally:{seat:weight}, exiled?:seat, sheriff?:seat}` |
| `result_private` | private | 私密技能结果 | 见 §6.3 |
| `decision` | moderator | 上帝记录：某座位的秘密决策 | `{seat, kind:AskKind, target?:seat, value?}` |
| `status` | moderator | 法官状态面板（全员身份/存活/药剂） | `{players:[{seat,role,alive,sheriff}], witch:{antidote,poison}}` |

### 6.3 `result_private` 的 data（按来源）
```jsonc
{ "kind":"Inspect",  "target":5, "isWolf":false }                 // 预言家查验
{ "kind":"Psychic",  "target":5, "shownRole":"Witch" }            // 通灵（机械狼按伪装）
{ "kind":"GunCheck", "canShoot":true }                            // 猎人每晚验枪
{ "kind":"Learn",    "target":5, "learnedRole":"Guardian" }       // 机械狼学习结果
{ "kind":"Knifed" }                                               // 女巫「你今晚被刀」（解药未用时）
```

### 6.4 发言的广播规则
玩家发言经 `speak` 的 ask/reply 产生后，引擎**额外**发一条 `speech` 事件，让别人（及上帝 script）也收到：
- 白天发言/竞选发言/遗言 → `vis=public`；
- 狼队私聊 → `vis=private`，对**每个其他睁眼狼**各发一条（`seat` = 接收狼）。
- 发言者本人也借 public 回流把"自己的发言"留进自己的 view。

---

## 7. 团队决策与狼队私聊（协议视角）

- **狼刀**：引擎只向狼队代表发 `ask(NightKill)`；代表回 `reply` 后，引擎向其余睁眼狼发 `event(private, narration, seat=该狼, text="【狼队】今晚刀：Px")`，并发 `event(moderator, decision, {kind:"NightKill",target})` 入 script。
- **狼队私聊**（每晚刀前一轮，无机制影响）：对每个存活睁眼狼依次发 `ask(speak, kind=WolfChat)`；每条回复经 §6.4 转发给其他睁眼狼（private）+ 入 script。机械狼不参与。

---

## 8. 错误、非法回复、超时（引擎裁决，权威方必须鲁棒）

- `reply.choice` 不在候选 / 类型错 / 该行 JSON 解析失败 / EOF → 引擎回退到**合法默认**；回退后的结果体现在该决策的 `decision`（moderator）事件里。规则：
  - choose：
    - **显式 `null` 且 `allowSkip`** → 尊重弃权/空刀（`null`）。
    - **EOF / 缺 `choice` / 非法值（类型错或不在候选）** → 回退：**投票类**（`Vote`/`RunoffVote`/`SheriffVote`/`BallotTarget`）取「首个非自己」**以保证游戏推进**；**其余**可跳过则跳过、不可跳过取「首个非自己」（与 `BotChannel` 一致）。
  - confirm：默认 `false`（保守：不救/不上警/不退水/不自爆/归多人）。
  - speak：默认 `""`（视为过）。
- **软超时** `--ask-timeout`（默认 **600s**）：超时同样回退到合法默认并记 moderator 事件。仅作防挂死——正常路径由 orchestrator 端的 AgentBrain 重试/兜底保证及时回复（见 `ai_agents_design.md` §7.4）。本地推理模型慢，故默认放宽。
- orchestrator 必须保证**对每个 `ask` 最终都回一个结构合法的 `reply`**；模型故障/超时由其本地兜底，不要让引擎空等。

---

## 9. 版本与前向兼容

- `game_start.protocol` 形如 `"MAJOR.MINOR"`。orchestrator 校验 **MAJOR**；不符 → 报错退出。
- 新增 `AskKind` / `etype` / `data` 字段 = **MINOR** 演进；删除或改变既有字段语义 = **MAJOR**。
- 兼容约定：
  - orchestrator 遇未知 `etype` → 记录并按 `vis` 当作普通文本事件（不崩）。
  - 引擎遇 `reply` 中未知字段 → 忽略。
  - 双方遇未知 `t` → 记录并跳过（不崩）。

---

## 10. 完整示例（节选，9 人板）

```jsonc
{"t":"game_start","protocol":"1.0","board":"Board9_SeerWitchHunter","seed":12345,
 "seats":[{"seat":1,"name":"P1"},{"seat":2,"name":"P2"},{"seat":3,"name":"P3"},{"seat":4,"name":"P4"},
          {"seat":5,"name":"P5"},{"seat":6,"name":"P6"},{"seat":7,"name":"P7"},{"seat":8,"name":"P8"},
          {"seat":9,"name":"P9"}]}
{"t":"event","vis":"private","seat":2,"etype":"deal","data":{"role":"Werewolf","faction":"Wolf","teammates":[4,6]},"text":"你的身份是：狼人；队友 P4 P6"}
{"t":"event","vis":"private","seat":3,"etype":"deal","data":{"role":"Seer","faction":"Town"},"text":"你的身份是：预言家"}
/* …其余座位 deal… */
{"t":"event","vis":"public","etype":"phase","day":1,"phase":"Night","data":{"day":1,"phase":"Night"},"text":"第 1 夜，天黑请闭眼"}
{"t":"event","vis":"public","etype":"narration","text":"狼人请睁眼，先内部交流"}
{"t":"ask","id":1,"seat":2,"qtype":"speak","kind":"WolfChat","prompt":"狼队私聊，请发言","day":1,"phase":"Night"}
{"t":"reply","id":1,"text":"今晚刀 P3 试预言家位","reasoning":"P3 像首验位"}
{"t":"event","vis":"private","seat":4,"etype":"speech","data":{"speaker":2,"kind":"WolfChat"},"text":"P2（狼队）：今晚刀 P3 试预言家位"}
{"t":"event","vis":"private","seat":6,"etype":"speech","data":{"speaker":2,"kind":"WolfChat"},"text":"P2（狼队）：今晚刀 P3 试预言家位"}
/* …P4 P6 私聊… */
{"t":"ask","id":4,"seat":2,"qtype":"choose","kind":"NightKill","prompt":"狼队请选择今晚的刀杀目标","day":1,"phase":"Night",
 "candidates":[{"seat":1,"name":"P1"},{"seat":3,"name":"P3"}],"allowSkip":true}
{"t":"reply","id":4,"choice":3,"reasoning":"按讨论刀 P3"}
{"t":"event","vis":"moderator","etype":"decision","data":{"seat":2,"kind":"NightKill","target":3},"text":"【上帝】狼队刀 P3"}
{"t":"event","vis":"private","seat":4,"etype":"narration","text":"【狼队】今晚刀：P3"}
{"t":"ask","id":5,"seat":3,"qtype":"choose","kind":"Inspect","prompt":"请查验一名玩家","day":1,"phase":"Night","candidates":[/*…*/],"allowSkip":true}
{"t":"reply","id":5,"choice":2}
{"t":"event","vis":"private","seat":3,"etype":"result_private","data":{"kind":"Inspect","target":2,"isWolf":true},"text":"【查验结果】P2 是 狼人（查杀）"}
/* …女巫被刀通知（private）、用药 ask、猎人验枪 private… */
{"t":"event","vis":"public","etype":"phase","day":1,"phase":"Day","data":{"day":1,"phase":"Day"},"text":"第 1 天，天亮了"}
{"t":"event","vis":"public","etype":"death","data":{"dead":[3],"causes":{"3":["Killed"]}},"text":"昨夜 P3 出局"}
/* …警长竞选：候选人发言(public speech)→投票(vote)；白天发言(public speech)；放逐投票(vote)… */
{"t":"event","vis":"public","etype":"vote","data":{"round":1,"tally":{"2":5,"5":2},"exiled":2},"text":"投票结果：P2=5，P5=2，P2 被放逐"}
/* …直到… */
{"t":"game_over","result":"TownWins"}
```

---

## 11. 实现备注

- **引擎侧**：`text` 一律用 `core/messages.h`（中文文案单一来源）渲染后塞进事件；`data` 给机器字段。三个信息出口 `notify/notifyPlayer/notifyModerator` + 定向结果 `onInspectResult/onPsychicResult/onHunterGunCheck/onMechanicLearnResult` → 对应 `public/private` 事件；秘密决策在拿到 `reply` 后补发 `moderator` 的 `decision` 事件。
- **orchestrator 侧**：单点消费引擎 stdout；`event` 按 `vis` 派发（public→全 brain；private→对应 brain；moderator→仅 script）；`ask` 路由到该座位的 HumanTerminal/AgentBrain，回 `reply`。每个 brain 的 view 即「public ∪ 自己 private」，全程累积。
- **协议测试（引擎）**：用内存 in/out 流喂预设 `reply` 行、捕获 `ask`/`event` 行，断言序列与字段，跑完整局（与现有 `ScriptedDecisionProvider` 测试同构）。
