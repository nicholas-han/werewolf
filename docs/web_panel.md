# Web 面板与 DSH 插件设计

> 状态：**已实现**。本文记录「狼人杀 DSH 插件」的边界与实现取舍，配合
> [`protocol_v1.md`](protocol_v1.md) 与 [`ai_agents_design.md`](ai_agents_design.md) 阅读。

## 1. 目标与非目标

目标：让仓库现有的 C++ 规则引擎以 **DSH Web 面板**的形态被真人玩到——入座、看夜间私密信息、发言、投票、复盘——其余座位由 AI 接管；同时让 DSH 智能体可以通过工具读牌桌、代打或让 AI 接管。

非目标（本期不做）：

- 多设备真·联机（`NetworkChannel` 仍留给后续）。
- 多真人同局（引擎支持，但面板本期只暴露一个真人座位）。
- 把规则移植到 JavaScript。规则仍只有一份：C++ 引擎。
- 用云端 LLM 驱动 AI 玩家。本期 AI 是进程内启发式 brain；LLM brain 可以复用同一套 `ask/reply` 接口后续接入。

## 2. 进程与模块

```
┌─────────────────────────────────────────────────────────────┐
│ DSH Web GUI（浏览器）                                        │
│   client.js：大厅 / 圆形牌桌 / 身份牌 / 操作卡 / 对局记录       │
│        │  fetch /werewolf/state, /werewolf/action            │
│        ▼                                                     │
│ DSH host（Node，Electron main）                              │
│   index.js：路由 + 生命周期 + werewolf 工具注册                │
│   lib/runtime.js：单局生命周期、设置持久化、面板/工具共用快照     │
│   lib/game.js   ：JSON-lines 编排器                            │
│        │        ├─ EventRouter：public→所有 brain；private→本人 │
│        │        ├─ Human seat：pending ask，等待 UI reply      │
│        │        └─ AI seats：lib/brain.js 逐座位决策            │
│        ▼                                                     │
│ C++ werewolf --json（权威 GameState / 规则 / 结算 / 胜负）      │
└─────────────────────────────────────────────────────────────┘
```

- 引擎是唯一规则真相层。插件只实现 I/O、编排、AI 与展示。
- 引擎仍是单线程同步：一个 `ask` 在途，收到匹配 `id` 的 `reply` 才继续。
- 插件不修改协议；`docs/protocol_v1.md` 的字段与可见性约定原样使用。

## 3. 信息可见性

这是整套实现的正确性核心（BRD §11）：

1. **宿主侧投影**：`GameSession.snapshot()` 对面板和 `werewolf` 工具返回同一份
   经过裁剪的状态。
   - `public` 事件：所有人可见。
   - `private(seat)` 事件：只在 `seat === 自己` 时进入 `events` / 身份牌笔记。
   - `moderator` 事件：游戏进行中一律不进入投影；终局复盘才进入。
   - 玩家身份：只暴露自己；狼人额外知道「谁是狼队友」的座位，但**不暴露队友的具体角色**
     （引擎的 `deal` 也只发队友座位）。
   - 发牌种子：进行中不返回 `config.seed`，避免 `randomDeal` 可复现导致身份泄露；
     终局才可用于复盘。
   - `thinkingSeat`：只对公开发言/白天流程的 ask 暴露；夜间私有 ask（狼刀、查验、守护、
     女巫、猎人、学习、狼队私聊等）不返回具体座位，避免与「预言家请睁眼」等公开旁白
     拼出身份。观战上帝视角除外。
2. **AI 侧视图**：每个 `BotBrain` 只收到 `public` 事件 + 发给自己的 `private` 事件；
   `moderator` 事件不进入任何 brain。brain 的决策只依赖这份座位本地视图。
3. **狼队协作**：狼刀/放逐的共识目标由宿主按「明面狼（open wolves）」的本地视图计算，
   不把机械狼等隐藏队友的角色塞给狼队；两只狼且其中一只为真人时，机器人让真人单票决定，
   避免 1:1 平票把夜晚卡成空刀。悍跳反制的触发只看回应狼自己知道的队友：
   若隐藏机械狼跳预言家，明面狼不会因为宿主的真身份表而「恰好」不反制。
4. **测试**：`tests/runtime.test.js` 明确断言进行中只暴露自己（+ 狼队友座位）、
   投影中没有 `moderator` 事件。

## 4. ask → UI 映射

| `qtype` / `kind` | 面板表现 |
| --- | --- |
| `choose` + 夜刀/查验/守护/毒/枪/学习/大刀/投票/警长票/归票/警徽 | 候选人格子 + 牌桌点选 + 确认；`allowSkip` 时显示「跳过 / 弃票」 |
| `confirm` + 解药/上警/退水/归单人/自爆/发言方向 | 两枚语义化按钮（用解药/留着、上警/不上警、归单人/归多人……） |
| `speak` + `Statement` / `LastWords` / `WolfChat` | 受控文本框 + 发送 / 过；Cmd/Ctrl+Enter 发送；显示字数 |
| 任意 | 「让 AI 代打本步」；「结束本局」（二次确认） |

回答经 `POST /werewolf/action` → `GameSession.answer()` → 引擎 `reply`。
面板与工具都会带上所回答的 `sessionId` + `askId`；host 同时校验对局与当前挂起的 ask，
避免面板与 agent 共享牌桌、或快速重开时，过期操作悄悄落到下一个问题/新一局上。
`werewolf` 工具的 `state` 会返回经过可见性过滤的完整事件流与最近事件文本；`reply`/`auto`
必须传入该次 `state` 观察到的 `sessionId` 与 `pendingAsk.id`。
非法候选/缺失必选项在 host 侧拒绝，不会把脏 reply 写进引擎。

## 5. AI 玩家

`lib/brain.js` 是一个**座位本地视角**的启发式 brain，覆盖全部板子与角色：

- 夜间：狼刀（含共识）、查验、守护、解药/毒药、猎人/狼枪开枪、机械狼学习/大刀。
- 白天：上警/退水、警长票、归单人/归多人、放逐票、遗言、狼队私聊。
- 发言：按角色与人设生成短中文发言；预言家/通灵师报验人；好人跟随信息位与警长归票；
  狼人会藏身份、必要时悍跳。
- 信息：从公开叙述解析死讯、警徽、投票详情、警长归票；从私密结果解析验人/通灵/学习/验枪。
- 合法性：`GameSession.autoReply()` 再次校验候选集与 `allowSkip`，异常时回退到引擎允许的默认值，
  保证任何情况下都不会把非法 reply 发给引擎。

bot 节奏可配（悠闲 1300ms / 正常 650ms / 极速 120ms），只影响观感，不影响规则。

## 6. 生命周期与鲁棒性

- **启动**：`ensureEngine()` 先找 `WEREWOLF_ENGINE` / `build/werewolf` / `bin/werewolf`；
  找不到就用系统 C++ 编译器直接编译（不依赖 CMake），带 180s 超时。
- **发牌**：引擎 `--seed` 由宿主生成；宿主不把种子在局中暴露。
- **人类 ask**：引擎 `--ask-timeout 0`，人类思考不设超时；断线/重开由 UI 的「结束本局」或新局处理。
- **中止**：`abort()` 先 resolve 挂起的人类 ask，再 `SIGTERM`，1.5s 后 `SIGKILL`；
  引擎提前退出会被标记为错误，UI 不会永远停在「running」。
- **重开**：`runtime.startGame()` 先 abort 旧 session，再创建新 session；旧进程不会串台。
- **持久化**：只持久化偏好（板子/座位/节奏/观战），不持久化进行中的对局；
  插件重启后回到大厅。对局日志只保留在当前进程内存中。

## 7. DSH 打包

- `package.json` 声明 `dsh.bundle.patch` + `dsh.client`；`cordis.patch.yml` 插入 `werewolf` 行。
- 宿主注入 `webServer` / `connection` / `tools`；客户端注入 `slots` / `locale`。
- 面板注册在 `main` slot（key `werewolf`），侧边栏注册在 `sidebar.panellist`（order 14）。
- 没有构建步骤：`client.js` 是浏览器模块，`lib/*.js` 是纯 ESM。
- 安装为 profile 的 `link:` bundle；C++ 引擎随仓库本地编译。

## 8. 测试

```bash
npm test
```

覆盖：

- `tests/brain.test.js`：验人/信任/投票意图、狼人不刀队友、女巫解药语义、全部 choose 类型的合法性、发言生成。
- `tests/runtime.test.js`：三块板子的全 AI 端到端、真人座位逐 ask 应答、信息隔离、上帝观战、重开。
- `tests/http.test.js`：DSH 路由/工具注册、开局/中止、非法 action。

C++ 侧规则测试仍走原有 `ctest`（需要 CMake + GoogleTest）。

## 9. 已知边界与后续

- 进行中的对局不跨 DSH 重启恢复：引擎子进程随宿主退出；后续可把 `GameState` 快照/动作日志落盘做重放。
- 单真人座位；多真人同局需要 `NetworkChannel` + 每座位鉴权。
- AI 是启发式而非 LLM；`BotBrain` 的输入/输出边界已经与 LLM brain 一致，可在 `autoReply()` 处替换。
- 发言记录/复盘目前是当前进程内的完整事件流；后续可导出 `god_script.md` / `trace.jsonl`（与 Python orchestrator 的 recorder 对齐）。
