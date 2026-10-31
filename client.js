window.__ModuleLoader__.load({
  id: "@local/dsh-plugin-werewolf",
  factory(require) {
    "use strict";

    const React = require("react");
    const h = React.createElement;
    const PANEL_ID = "werewolf";
    const NS = "werewolf";
    const POLL_MS = 800;

    const DICTIONARIES = {
      en: {
        panel: "Werewolf",
        title: "Werewolf",
        subtitle: "One table. One moon. Find the wolves before they find you.",
        eyebrow: "MOONLIT VILLAGE",
        start: "Start game",
        newGame: "New game",
        backToGame: "Back to game",
        backToLobby: "Lobby",
        lobby: "New game",
        boards: "Board",
        seat: "Your seat",
        randomSeat: "Random",
        spectate: "Watch AI",
        godView: "Reveal roles (spectator)",
        speed: "Bot pace",
        calm: "Calm",
        normal: "Normal",
        fast: "Fast",
        board1: "9-player classic",
        board2: "12-player guard",
        board3: "12-player psychic",
        players: "{n} players",
        noteFirstBuild: "The rules engine is compiled once on first launch.",
        noteLobby: "AI seats speak, use night skills, and vote. When it is your turn, the action card appears on the right.",
        noteDynamic: "No scripted game: wolves lie, power roles claim, and every vote changes the night.",
        noteRestart: "Starting a new game ends the current one.",
        waiting: "Waiting for other players…",
        thinking: "{name} is thinking…",
        yourTurn: "Your move",
        inProgress: "Table in progress",
        night: "Night",
        day: "Day",
        dayN: "Day {n}",
        phaseNight: "Night falls",
        phaseDay: "Day breaks",
        alive: "Alive",
        out: "Out",
        you: "You",
        sheriff: "Sheriff",
        role: "Your role",
        teammates: "Wolf pack",
        notes: "Your notes",
        noNotes: "No private information yet.",
        spectator: "Spectator",
        spectatorHint: "All roles are hidden until the game ends.",
        spectatorGodHint: "God view is on: every role is visible.",
        deadHint: "You are out. The table plays on without you.",
        reveal: "Role reveal",
        resultTown: "Town wins",
        resultWolf: "Wolves win",
        resultDraw: "No winner",
        gameOver: "Game over",
        playAgain: "Play again",
        feed: "Table log",
        filterAll: "All",
        filterSpeech: "Speeches",
        filterJudge: "Judge",
        filterPrivate: "Private",
        filterGod: "God view",
        autoScroll: "Auto-scroll",
        emptyFeed: "The village is quiet…",
        pickTarget: "Choose a target",
        skip: "Skip / abstain",
        pass: "Pass",
        send: "Send",
        confirm: "Confirm",
        yes: "Yes",
        no: "No",
        autoPlay: "Let AI take this step",
        writeSpeech: "Write your speech…",
        writeLastWords: "Say your last words…",
        writeWolfChat: "Talk to your pack…",
        chooseSeat: "Pick one seat, then confirm.",
        serviceError: "The Werewolf service did not respond. Try again in a moment.",
        actionError: "That action was rejected.",
        built: "Ready",
        building: "Preparing the rules engine…",
        seed: "Seed {seed}",
        dayDivider: "Day {n} · {phase}",
        privateTag: "Private",
        godTag: "God",
        wolfChatTag: "Wolf chat",
        lastWordsTag: "Last words",
        speechTag: "Speech",
        abort: "End game",
        abortConfirm: "Click again to end the game",
        errorTitle: "The table hit a problem",
        retry: "Retry",
        revealAll: "All roles",
        close: "Close",
        speechPlaceholder: "Type what you say at the table…",
        charCount: "{n}/600",
        "kind.NightKill": "Wolf kill",
        "kind.SelfDestruct": "Self-destruct",
        "kind.Vote": "Exile vote",
        "kind.RunoffVote": "Runoff vote",
        "kind.Inspect": "Seer check",
        "kind.Guard": "Guard",
        "kind.WitchSave": "Antidote",
        "kind.WitchPoison": "Poison",
        "kind.HunterShot": "Hunter shot",
        "kind.MechanicLearn": "Learn a role",
        "kind.MechanicBigKnife": "Big knife",
        "kind.RunForSheriff": "Run for sheriff",
        "kind.Withdraw": "Withdraw",
        "kind.SheriffVote": "Sheriff vote",
        "kind.ConsolidateSingle": "Sheriff ballot",
        "kind.BallotTarget": "Ballot target",
        "kind.BadgeTransfer": "Badge transfer",
        "kind.SpeechDirection": "Speech direction",
        "kind.Statement": "Speech",
        "kind.LastWords": "Last words",
        "kind.WolfChat": "Wolf chat",
        "confirm.WitchSave.true": "Use the antidote",
        "confirm.WitchSave.false": "Keep it",
        "confirm.RunForSheriff.true": "Run for sheriff",
        "confirm.RunForSheriff.false": "Stay out",
        "confirm.Withdraw.true": "Withdraw",
        "confirm.Withdraw.false": "Stay in",
        "confirm.ConsolidateSingle.true": "Single target (1.5)",
        "confirm.ConsolidateSingle.false": "Multi-target PK",
        "confirm.SpeechDirection.true": "To the left",
        "confirm.SpeechDirection.false": "To the right",
        "confirm.SelfDestruct.true": "Blow up",
        "confirm.SelfDestruct.false": "Not now",
        "confirm.default.true": "Yes",
        "confirm.default.false": "No",
      },
      zh: {
        panel: "狼人杀",
        title: "狼人杀",
        subtitle: "一桌人，一轮月。在天亮之前，把狼找出来。",
        eyebrow: "月下村庄",
        start: "开始游戏",
        newGame: "新对局",
        backToGame: "返回对局",
        backToLobby: "大厅",
        lobby: "新对局",
        boards: "板子",
        seat: "你的座位",
        randomSeat: "随机",
        spectate: "AI 观战",
        godView: "上帝视角（观战）",
        speed: "AI 节奏",
        calm: "悠闲",
        normal: "正常",
        fast: "极速",
        board1: "9 人预女猎",
        board2: "12 人预女猎守",
        board3: "12 人通灵机械狼",
        players: "{n} 人局",
        noteFirstBuild: "首次启动会自动编译一次规则引擎。",
        noteLobby: "AI 会自动发言、使用夜间技能和投票；轮到你时，右侧会出现操作卡。",
        noteDynamic: "没有固定剧本：狼队会悍跳、神职会报验人，每一票都会改变夜晚的长度。",
        noteRestart: "开始新游戏会结束当前对局。",
        waiting: "等待其他玩家…",
        thinking: "{name} 正在思考…",
        yourTurn: "轮到你行动",
        inProgress: "牌桌进行中",
        night: "夜晚",
        day: "白天",
        dayN: "第 {n} 天",
        phaseNight: "天黑请闭眼",
        phaseDay: "天亮了",
        alive: "存活",
        out: "出局",
        you: "你",
        sheriff: "警长",
        role: "你的身份",
        teammates: "狼队友",
        notes: "你的情报",
        noNotes: "暂时没有私密信息。",
        spectator: "观战模式",
        spectatorHint: "所有身份将在终局揭晓。",
        spectatorGodHint: "上帝视角已开启，所有身份可见。",
        deadHint: "你已经出局，牌桌会继续推进。",
        reveal: "身份揭晓",
        resultTown: "好人胜利",
        resultWolf: "狼人胜利",
        resultDraw: "未分胜负",
        gameOver: "游戏结束",
        playAgain: "再来一局",
        feed: "对局记录",
        filterAll: "全部",
        filterSpeech: "发言",
        filterJudge: "法官",
        filterPrivate: "私密",
        filterGod: "上帝",
        autoScroll: "自动滚动",
        emptyFeed: "村庄暂时安静…",
        pickTarget: "选择目标",
        skip: "跳过 / 弃票",
        pass: "过",
        send: "发送",
        confirm: "确认",
        yes: "是",
        no: "否",
        autoPlay: "让 AI 代打本步",
        writeSpeech: "写下你的发言…",
        writeLastWords: "留下你的遗言…",
        writeWolfChat: "和狼队说点什么…",
        chooseSeat: "点选一名玩家，然后确认。",
        serviceError: "狼人杀服务没有响应，请稍后重试。",
        actionError: "这个操作没有被接受。",
        built: "已就绪",
        building: "正在准备规则引擎…",
        seed: "种子 {seed}",
        dayDivider: "第 {n} 天 · {phase}",
        privateTag: "私密",
        godTag: "上帝",
        wolfChatTag: "狼队私聊",
        lastWordsTag: "遗言",
        speechTag: "发言",
        abort: "结束本局",
        abortConfirm: "再点一次结束本局",
        errorTitle: "牌桌遇到了问题",
        retry: "重试",
        revealAll: "所有身份已揭晓",
        close: "关闭",
        speechPlaceholder: "写下你在牌桌上的发言…",
        charCount: "{n}/600",
        "kind.NightKill": "狼队刀口",
        "kind.SelfDestruct": "是否自爆",
        "kind.Vote": "放逐投票",
        "kind.RunoffVote": "决胜投票",
        "kind.Inspect": "预言家查验",
        "kind.Guard": "守卫守护",
        "kind.WitchSave": "女巫解药",
        "kind.WitchPoison": "女巫毒药",
        "kind.HunterShot": "猎人开枪",
        "kind.MechanicLearn": "机械狼学习",
        "kind.MechanicBigKnife": "机械狼大刀",
        "kind.RunForSheriff": "是否上警",
        "kind.Withdraw": "是否退水",
        "kind.SheriffVote": "警长投票",
        "kind.ConsolidateSingle": "警长归票",
        "kind.BallotTarget": "归票目标",
        "kind.BadgeTransfer": "警徽移交",
        "kind.SpeechDirection": "发言方向",
        "kind.Statement": "发言",
        "kind.LastWords": "遗言",
        "kind.WolfChat": "狼队私聊",
        "confirm.WitchSave.true": "用解药",
        "confirm.WitchSave.false": "留着",
        "confirm.RunForSheriff.true": "上警",
        "confirm.RunForSheriff.false": "不上警",
        "confirm.Withdraw.true": "退水",
        "confirm.Withdraw.false": "继续竞选",
        "confirm.ConsolidateSingle.true": "归单人（1.5 票）",
        "confirm.ConsolidateSingle.false": "归多人 PK（1 票）",
        "confirm.SpeechDirection.true": "向左",
        "confirm.SpeechDirection.false": "向右",
        "confirm.SelfDestruct.true": "自爆",
        "confirm.SelfDestruct.false": "暂不",
        "confirm.default.true": "是",
        "confirm.default.false": "否",
      },
    };

    const ROLE_INFO = {
      Werewolf: {
        zh: "狼人",
        en: "Werewolf",
        glyph: "狼",
        hue: 2,
        descZh: "每晚与狼队共同选择刀口，白天隐藏身份、带偏好人。",
        descEn: "Choose a kill with the pack each night. Blend in by day.",
      },
      WolfGun: {
        zh: "狼枪",
        en: "Wolf Gun",
        glyph: "枪",
        hue: 8,
        descZh: "狼人阵营。出局后可以开枪带走一名玩家。",
        descEn: "Wolf faction. When you die, fire one last shot.",
      },
      MechanicWolf: {
        zh: "机械狼",
        en: "Mechanic Wolf",
        glyph: "机",
        hue: 22,
        descZh: "狼人阵营，但不与狼队见面。可学习一名玩家的身份。",
        descEn: "A lone wolf. Learn one player's identity, away from the pack.",
      },
      Seer: {
        zh: "预言家",
        en: "Seer",
        glyph: "预",
        hue: 208,
        descZh: "每晚查验一名玩家是好人还是狼人。",
        descEn: "Check one player's allegiance each night.",
      },
      Psychic: {
        zh: "通灵师",
        en: "Psychic",
        glyph: "通",
        hue: 258,
        descZh: "每晚得知一名玩家的具体身份。",
        descEn: "Learn one player's exact role each night.",
      },
      Witch: {
        zh: "女巫",
        en: "Witch",
        glyph: "女",
        hue: 288,
        descZh: "一瓶解药、一瓶毒药，各只能用一次。本板不可自救。",
        descEn: "One antidote and one poison, each once. No self-rescue.",
      },
      Hunter: {
        zh: "猎人",
        en: "Hunter",
        glyph: "猎",
        hue: 32,
        descZh: "出局时可以开枪带走一名玩家；被毒杀时无法开枪。",
        descEn: "Fire on death, unless you were poisoned.",
      },
      Guardian: {
        zh: "守卫",
        en: "Guardian",
        glyph: "守",
        hue: 168,
        descZh: "每晚守护一人，不能连续两晚守护同一人。",
        descEn: "Protect one player each night, never the same twice in a row.",
      },
      Civilian: {
        zh: "平民",
        en: "Villager",
        glyph: "民",
        hue: 44,
        descZh: "没有夜间技能。你的武器是发言、逻辑和投票。",
        descEn: "No night skill. Speak, reason, and vote.",
      },
    };

    const FACTION_INFO = {
      Wolf: { zh: "狼人阵营", en: "Wolf faction", color: "#e26a5a" },
      Town: { zh: "好人阵营", en: "Town", color: "#6ea8e8" },
    };

    const BOARD_META = {
      1: { zh: "9 人预女猎", en: "9-player classic", tag: "经典", pitch: "3 狼 · 预言家 · 女巫 · 猎人 · 3 平民" },
      2: { zh: "12 人预女猎守 + 狼枪", en: "12-player guard + wolf gun", tag: "进阶", pitch: "3 狼 · 狼枪 · 预言家 · 女巫 · 猎人 · 守卫 · 4 平民" },
      3: { zh: "12 人通灵机械狼", en: "12-player psychic + mechanic", tag: "变体", pitch: "3 狼 · 机械狼 · 通灵师 · 女巫 · 猎人 · 守卫 · 4 平民" },
    };

    const CSS = `
.ww-app {
  --ww-bg: var(--dsw-alias-bg-base, #f6f3ec);
  --ww-surface: var(--dsw-alias-bg-layer-1, #ffffff);
  --ww-surface-2: var(--dsw-alias-bg-layer-2, #f0ece3);
  --ww-border: var(--dsw-alias-border-l1, #d8d2c5);
  --ww-border-strong: var(--dsw-alias-border-l2, #b9b2a3);
  --ww-text: var(--dsw-alias-label-primary, #221f1a);
  --ww-muted: var(--dsw-alias-label-secondary, #6f6a60);
  --ww-accent: var(--dsw-alias-brand-primary, #b8892f);
  --ww-gold: #d4a63c;
  --ww-gold-soft: rgba(212, 166, 60, 0.16);
  --ww-red: #d65b4d;
  --ww-blue: #5d8fd6;
  --ww-green: #4f9d76;
  --ww-night: #0b111d;
  --ww-night-2: #151f33;
  --ww-night-3: #22304c;
  position: relative;
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
  overflow: auto;
  padding: 16px 22px 22px max(22px, var(--dsh-frame-leading-clearance, 0px));
  background: var(--ww-bg);
  color: var(--ww-text);
  font: inherit;
  container-type: inline-size;
}
.ww-app * { box-sizing: border-box; }
.ww-app h1, .ww-app h2, .ww-app h3, .ww-app p { margin: 0; }
.ww-app button, .ww-app input, .ww-app textarea { font: inherit; color: inherit; }
.ww-app button { cursor: pointer; }
.ww-app button:disabled { cursor: default; }

.ww-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 14px;
  flex-wrap: wrap;
}
.ww-brand { display: flex; align-items: center; gap: 12px; min-width: 0; }
.ww-brand-mark {
  width: 42px;
  height: 42px;
  display: grid;
  place-items: center;
  border-radius: 50%;
  color: var(--ww-gold);
  background: radial-gradient(circle at 35% 30%, rgba(212,166,60,.22), rgba(212,166,60,.04));
  border: 1px solid rgba(212,166,60,.34);
  box-shadow: 0 0 28px rgba(212,166,60,.12);
  flex: none;
}
.ww-titles { min-width: 0; }
.ww-titles h1 { font-size: 22px; line-height: 1.15; font-weight: 600; letter-spacing: .01em; }
.ww-titles p {
  margin-top: 3px;
  color: var(--ww-muted);
  font-size: 12.5px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ww-header-actions { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.ww-phase-pill {
  display: inline-flex;
  align-items: center;
  gap: 7px;
  padding: 7px 12px;
  border-radius: 999px;
  border: 1px solid var(--ww-border);
  background: var(--ww-surface);
  font-size: 13px;
  white-space: nowrap;
}
.ww-phase-pill.night { color: #b9c8e6; background: #101827; border-color: #2a3854; }
.ww-phase-pill.day { color: #9a6a13; background: #fff8e8; border-color: #ecd9ac; }
.ww-btn {
  border: 1px solid var(--ww-border);
  background: var(--ww-surface);
  border-radius: 9px;
  padding: 7px 12px;
  font-size: 13px;
  line-height: 1.2;
  transition: background .15s ease, border-color .15s ease, transform .1s ease;
}
.ww-btn:hover:not(:disabled) { background: var(--ww-surface-2); }
.ww-btn:active:not(:disabled) { transform: translateY(1px); }
.ww-btn:disabled { opacity: .45; }
.ww-btn.primary {
  border-color: transparent;
  background: linear-gradient(180deg, #e0b552, #b8892f);
  color: #211a0a;
  font-weight: 600;
  box-shadow: 0 6px 18px rgba(184,137,47,.22);
}
.ww-btn.primary:hover:not(:disabled) { background: linear-gradient(180deg, #e8c264, #c4973a); }
.ww-btn.ghost { background: transparent; }
.ww-btn.danger { color: var(--ww-red); border-color: rgba(214,91,77,.4); }
.ww-btn.small { padding: 5px 9px; font-size: 12px; }
.ww-speed { display: inline-flex; border: 1px solid var(--ww-border); border-radius: 9px; overflow: hidden; }
.ww-speed button { border: 0; background: var(--ww-surface); padding: 6px 9px; font-size: 12px; }
.ww-speed button[aria-pressed="true"] { background: var(--ww-surface-2); color: var(--ww-accent); font-weight: 600; }

.ww-main {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 348px;
  gap: 18px;
  align-items: start;
  margin-top: 16px;
}
.ww-stage { min-width: 0; display: flex; flex-direction: column; gap: 12px; }
.ww-table {
  position: relative;
  width: 100%;
  max-width: 680px;
  margin: 0 auto;
  aspect-ratio: 1 / 1;
  border-radius: 50%;
  background:
    radial-gradient(circle at 50% 38%, rgba(212,166,60,.11), transparent 38%),
    radial-gradient(circle at 50% 120%, rgba(63,93,146,.32), transparent 55%),
    linear-gradient(160deg, var(--ww-night-2), var(--ww-night) 68%);
  border: 1px solid rgba(214,190,139,.18);
  box-shadow:
    inset 0 0 0 10px rgba(255,255,255,.018),
    inset 0 0 70px rgba(0,0,0,.4),
    0 24px 70px rgba(10,14,24,.22);
  color: #e9e4d8;
  overflow: hidden;
}
.ww-table::before {
  content: "";
  position: absolute;
  inset: 17%;
  border-radius: 50%;
  border: 1px dashed rgba(212,166,60,.24);
  pointer-events: none;
}
.ww-table::after {
  content: "";
  position: absolute;
  inset: 0;
  background-image: radial-gradient(rgba(255,255,255,.18) 1px, transparent 1px);
  background-size: 37px 41px;
  opacity: .16;
  pointer-events: none;
}
.ww-seat {
  position: absolute;
  width: 86px;
  transform: translate(-50%, -50%);
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 5px;
  border: 0;
  background: transparent;
  color: inherit;
  padding: 0;
  z-index: 2;
}
.ww-seat:disabled { opacity: 1; }
.ww-avatar {
  position: relative;
  width: 54px;
  height: 54px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  font-weight: 650;
  font-size: 15px;
  letter-spacing: .02em;
  color: #f8f5ed;
  background:
    radial-gradient(circle at 34% 28%, rgba(255,255,255,.22), transparent 42%),
    hsl(var(--seat-hue) 38% 38%);
  border: 1px solid rgba(255,255,255,.16);
  box-shadow: 0 8px 20px rgba(0,0,0,.3);
  transition: transform .15s ease, box-shadow .15s ease, filter .2s ease;
}
.ww-seat:hover .ww-avatar { transform: translateY(-1px); }
.ww-seat.self .ww-avatar {
  box-shadow: 0 0 0 2px var(--ww-gold), 0 0 26px rgba(212,166,60,.28);
}
.ww-seat.sheriff .ww-avatar { border-color: rgba(212,166,60,.6); }
.ww-seat.dead .ww-avatar { filter: grayscale(1) brightness(.62); opacity: .52; }
.ww-seat.dead .ww-seat-name { opacity: .45; text-decoration: line-through; }
.ww-seat.dead::after {
  content: "×";
  position: absolute;
  top: 12px;
  right: 8px;
  width: 18px;
  height: 18px;
  display: grid;
  place-items: center;
  border-radius: 50%;
  background: rgba(12,16,26,.94);
  color: #d9d4c8;
  font-size: 12px;
  border: 1px solid rgba(255,255,255,.14);
}
.ww-seat.thinking .ww-avatar {
  animation: ww-pulse 1.2s ease-in-out infinite;
  box-shadow: 0 0 0 2px rgba(110,168,232,.7), 0 0 24px rgba(110,168,232,.35);
}
.ww-seat.not-target .ww-avatar { opacity: .46; filter: saturate(.45); }
.ww-seat.selectable .ww-avatar { cursor: pointer; }
.ww-seat.selectable:hover .ww-avatar {
  box-shadow: 0 0 0 2px rgba(212,166,60,.9), 0 0 26px rgba(212,166,60,.35);
}
.ww-seat.selected .ww-avatar {
  box-shadow: 0 0 0 3px var(--ww-gold), 0 0 30px rgba(212,166,60,.48);
  transform: translateY(-2px);
}
.ww-seat-name {
  max-width: 84px;
  font-size: 11.5px;
  color: #aab4c6;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  text-shadow: 0 1px 4px rgba(0,0,0,.6);
}
.ww-seat.self .ww-seat-name { color: #f0d896; }
.ww-seat-tags {
  display: flex;
  gap: 4px;
  align-items: center;
  min-height: 15px;
}
.ww-mini-tag {
  font-size: 9.5px;
  line-height: 1;
  padding: 2px 5px;
  border-radius: 999px;
  border: 1px solid rgba(255,255,255,.18);
  color: #d9d4c8;
  background: rgba(255,255,255,.06);
  white-space: nowrap;
}
.ww-mini-tag.role { border-color: rgba(212,166,60,.35); color: #efd58f; }
.ww-mini-tag.wolf { border-color: rgba(226,106,90,.45); color: #f2a89d; }
.ww-mini-tag.you { border-color: rgba(212,166,60,.5); color: #f0d896; }
.ww-sheriff-badge {
  position: absolute;
  top: -5px;
  right: -4px;
  width: 20px;
  height: 20px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  font-size: 10px;
  font-weight: 700;
  color: #2b2108;
  background: linear-gradient(180deg, #f0d27a, #c9992e);
  border: 1px solid rgba(255,255,255,.4);
  box-shadow: 0 2px 8px rgba(0,0,0,.35);
}
.ww-center {
  position: absolute;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  width: 42%;
  max-width: 280px;
  aspect-ratio: 1 / 1;
  border-radius: 50%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 16px;
  background:
    radial-gradient(circle at 50% 34%, rgba(212,166,60,.13), transparent 52%),
    rgba(8,12,20,.62);
  border: 1px solid rgba(212,166,60,.22);
  box-shadow: inset 0 0 36px rgba(0,0,0,.38);
  z-index: 1;
}
.ww-center-icon { color: #e7cd88; opacity: .92; margin-bottom: 6px; }
.ww-center-day { font-size: 17px; font-weight: 600; letter-spacing: .04em; }
.ww-center-phase { margin-top: 2px; font-size: 11.5px; color: #9fabc0; letter-spacing: .08em; text-transform: uppercase; }
.ww-center-status {
  margin-top: 10px;
  font-size: 12px;
  line-height: 1.45;
  color: #d8d1c1;
  max-width: 180px;
}
.ww-center-status.alert { color: #f0d896; }
.ww-center-result { font-size: 15px; font-weight: 650; margin-top: 6px; }
.ww-center-result.town { color: #8bc9a7; }
.ww-center-result.wolf { color: #f0a196; }
.ww-center-result.draw { color: #c7c1b4; }

.ww-side { display: flex; flex-direction: column; gap: 12px; min-width: 0; }
.ww-card {
  border: 1px solid var(--ww-border);
  background: var(--ww-surface);
  border-radius: 13px;
  padding: 14px;
  box-shadow: 0 3px 14px rgba(30,28,22,.04);
}
.ww-card.highlight { border-color: rgba(212,166,60,.55); box-shadow: 0 0 0 3px var(--ww-gold-soft); }
.ww-card-eyebrow {
  font-size: 10.5px;
  letter-spacing: .12em;
  text-transform: uppercase;
  color: var(--ww-muted);
  margin-bottom: 8px;
}
.ww-role-head { display: flex; align-items: center; gap: 11px; }
.ww-role-glyph {
  width: 42px;
  height: 42px;
  border-radius: 12px;
  display: grid;
  place-items: center;
  font-size: 19px;
  font-weight: 650;
  color: #f7f2e6;
  background: hsl(var(--role-hue) 40% 34%);
  border: 1px solid rgba(255,255,255,.16);
  flex: none;
}
.ww-role-name { font-size: 17px; font-weight: 620; line-height: 1.2; }
.ww-role-faction { margin-top: 2px; font-size: 11.5px; color: var(--ww-muted); }
.ww-role-faction.wolf { color: var(--ww-red); }
.ww-role-desc { margin-top: 10px; font-size: 12.5px; line-height: 1.55; color: var(--ww-muted); }
.ww-role-notes { margin-top: 11px; display: flex; flex-direction: column; gap: 5px; }
.ww-role-notes-label { font-size: 10.5px; letter-spacing: .1em; text-transform: uppercase; color: var(--ww-muted); }
.ww-note {
  font-size: 12px;
  line-height: 1.45;
  padding: 6px 8px;
  border-radius: 7px;
  background: var(--ww-surface-2);
  border-left: 2px solid rgba(212,166,60,.55);
}
.ww-note.empty { color: var(--ww-muted); border-left-color: var(--ww-border); }
.ww-teammates { margin-top: 10px; display: flex; flex-wrap: wrap; gap: 5px; }
.ww-chip {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 3px 7px;
  border-radius: 999px;
  border: 1px solid var(--ww-border);
  background: var(--ww-surface-2);
  font-size: 11px;
}
.ww-chip.wolf { border-color: rgba(214,91,77,.4); color: var(--ww-red); }

.ww-action-title { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
.ww-action-title h3 { font-size: 15px; font-weight: 620; }
.ww-action-prompt { margin-top: 8px; font-size: 13px; line-height: 1.55; color: var(--ww-text); }
.ww-action-hint { margin-top: 6px; font-size: 11.5px; line-height: 1.45; color: var(--ww-muted); }
.ww-target-grid {
  margin-top: 11px;
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(78px, 1fr));
  gap: 6px;
}
.ww-target {
  border: 1px solid var(--ww-border);
  background: var(--ww-surface-2);
  border-radius: 9px;
  padding: 7px 6px;
  font-size: 12px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  line-height: 1.2;
}
.ww-target .seat-no { font-weight: 650; }
.ww-target .seat-sub { font-size: 10px; color: var(--ww-muted); }
.ww-target[aria-pressed="true"] {
  border-color: rgba(212,166,60,.8);
  background: var(--ww-gold-soft);
  box-shadow: 0 0 0 2px rgba(212,166,60,.15);
}
.ww-action-row { margin-top: 11px; display: flex; gap: 7px; flex-wrap: wrap; }
.ww-action-row .grow { flex: 1 1 auto; }
.ww-confirm-grid { margin-top: 11px; display: grid; grid-template-columns: 1fr 1fr; gap: 7px; }
.ww-confirm-btn {
  border: 1px solid var(--ww-border);
  background: var(--ww-surface-2);
  border-radius: 10px;
  padding: 12px 8px;
  font-size: 13px;
  line-height: 1.25;
  font-weight: 550;
}
.ww-confirm-btn.good { border-color: rgba(79,157,118,.45); color: #2f7655; background: rgba(79,157,118,.08); }
.ww-confirm-btn.danger { border-color: rgba(214,91,77,.45); color: #a83e33; background: rgba(214,91,77,.08); }
.ww-speech { margin-top: 10px; width: 100%; min-height: 96px; resize: vertical; padding: 9px 10px; border-radius: 9px; border: 1px solid var(--ww-border); background: var(--ww-surface-2); line-height: 1.5; font-size: 13px; }
.ww-speech:focus { outline: 2px solid rgba(212,166,60,.4); outline-offset: 1px; }
.ww-counter { margin-top: 4px; text-align: right; font-size: 10.5px; color: var(--ww-muted); }
.ww-thinking { display: flex; align-items: center; gap: 10px; padding: 8px 0 2px; font-size: 13px; color: var(--ww-muted); }
.ww-dots { display: inline-flex; gap: 3px; }
.ww-dots i { width: 5px; height: 5px; border-radius: 50%; background: currentColor; animation: ww-bounce 1.1s infinite; }
.ww-dots i:nth-child(2) { animation-delay: .14s; }
.ww-dots i:nth-child(3) { animation-delay: .28s; }
.ww-waiting { padding: 12px 0 4px; font-size: 13px; color: var(--ww-muted); }
.ww-result-card { text-align: center; padding: 8px 0 2px; }
.ww-result-card .result-kicker { font-size: 10.5px; letter-spacing: .14em; text-transform: uppercase; color: var(--ww-muted); }
.ww-result-card .result-name { margin-top: 5px; font-size: 25px; font-weight: 650; letter-spacing: .02em; }
.ww-result-card.town .result-name { color: #2f7655; }
.ww-result-card.wolf .result-name { color: #b4473b; }
.ww-result-roles { margin-top: 12px; display: flex; flex-wrap: wrap; gap: 5px; justify-content: center; }
.ww-result-role { font-size: 11px; padding: 3px 7px; border-radius: 999px; border: 1px solid var(--ww-border); background: var(--ww-surface-2); }
.ww-result-role.wolf { border-color: rgba(214,91,77,.35); color: #a83e33; }
.ww-error-banner {
  margin-top: 12px;
  padding: 11px 13px;
  border-radius: 10px;
  border: 1px solid rgba(214,91,77,.35);
  background: rgba(214,91,77,.08);
  color: #a83e33;
  font-size: 12.5px;
  line-height: 1.5;
  white-space: pre-wrap;
}
.ww-building {
  margin-top: 12px;
  padding: 11px 13px;
  border-radius: 10px;
  border: 1px solid rgba(212,166,60,.4);
  background: var(--ww-gold-soft);
  font-size: 12.5px;
  line-height: 1.5;
}
.ww-toast {
  position: absolute;
  top: 14px;
  right: 18px;
  z-index: 20;
  max-width: 360px;
  padding: 9px 12px;
  border-radius: 9px;
  font-size: 12.5px;
  line-height: 1.45;
  border: 1px solid rgba(214,91,77,.35);
  background: #fff5f3;
  color: #9e3b31;
  box-shadow: 0 12px 30px rgba(30,20,18,.16);
}

.ww-feed { margin-top: 16px; border-top: 1px solid var(--ww-border); padding-top: 13px; }
.ww-feed-head { display: flex; align-items: center; justify-content: space-between; gap: 10px; flex-wrap: wrap; }
.ww-feed-head h2 { font-size: 13.5px; font-weight: 600; letter-spacing: .02em; }
.ww-feed-tools { display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }
.ww-filter { display: inline-flex; border: 1px solid var(--ww-border); border-radius: 8px; overflow: hidden; }
.ww-filter button { border: 0; background: var(--ww-surface); padding: 5px 8px; font-size: 11.5px; }
.ww-filter button[aria-pressed="true"] { background: var(--ww-surface-2); color: var(--ww-accent); font-weight: 600; }
.ww-toggle { display: inline-flex; align-items: center; gap: 5px; font-size: 11.5px; color: var(--ww-muted); user-select: none; }
.ww-toggle input { accent-color: var(--ww-accent); }
.ww-feed-list {
  margin-top: 9px;
  max-height: 260px;
  overflow: auto;
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding-right: 4px;
}
.ww-empty { padding: 18px; text-align: center; color: var(--ww-muted); font-size: 12.5px; }
.ww-divider { display: flex; align-items: center; gap: 9px; margin: 9px 0 5px; color: var(--ww-muted); font-size: 10.5px; letter-spacing: .1em; text-transform: uppercase; }
.ww-divider::before, .ww-divider::after { content: ""; flex: 1; height: 1px; background: var(--ww-border); }
.ww-event { display: flex; gap: 8px; align-items: flex-start; padding: 6px 8px; border-radius: 8px; font-size: 12.5px; line-height: 1.5; }
.ww-event.narration { justify-content: center; text-align: center; color: var(--ww-muted); font-style: italic; padding: 3px 8px; }
.ww-event.speech { background: var(--ww-surface); border: 1px solid var(--ww-border); }
.ww-event.private { background: rgba(120, 90, 170, .07); border: 1px solid rgba(120, 90, 170, .22); }
.ww-event.god { background: rgba(212,166,60,.07); border: 1px dashed rgba(212,166,60,.38); color: var(--ww-muted); }
.ww-event-icon { flex: none; width: 20px; height: 20px; display: grid; place-items: center; border-radius: 50%; background: var(--ww-surface-2); font-size: 10px; color: var(--ww-muted); }
.ww-event-body { min-width: 0; flex: 1; }
.ww-event-meta { display: flex; align-items: center; gap: 5px; flex-wrap: wrap; margin-bottom: 1px; }
.ww-speaker { font-size: 11.5px; font-weight: 650; }
.ww-event-text { word-break: break-word; white-space: pre-wrap; }
.ww-tag { font-size: 9.5px; line-height: 1; padding: 2px 5px; border-radius: 999px; border: 1px solid var(--ww-border); color: var(--ww-muted); }
.ww-tag.private { border-color: rgba(120,90,170,.35); color: #7553a4; }
.ww-tag.god { border-color: rgba(184,137,47,.45); color: #8a6417; }
.ww-tag.speech { border-color: rgba(93,143,214,.35); color: #3f6ca8; }
.ww-tag.wolfchat { border-color: rgba(214,91,77,.35); color: #a83e33; }

.ww-lobby {
  position: absolute;
  inset: 0;
  z-index: 12;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 28px 22px 40px;
  overflow: auto;
  background:
    radial-gradient(circle at 18% 8%, rgba(212,166,60,.13), transparent 34%),
    linear-gradient(155deg, #0d1422 0%, #121a2b 52%, #0a0f1a 100%);
  color: #ece6d8;
  animation: ww-fade .18s ease;
}
.ww-lobby-inner { width: 100%; max-width: 980px; display: grid; grid-template-columns: 1.04fr .96fr; gap: 32px; align-items: center; }
.ww-hero .eyebrow { font-size: 10.5px; letter-spacing: .22em; color: #c7a95e; text-transform: uppercase; }
.ww-hero h1 { margin-top: 9px; font-size: clamp(34px, 5cqw, 56px); line-height: 1.02; letter-spacing: .02em; font-weight: 650; }
.ww-hero p { margin-top: 13px; max-width: 430px; color: #a9b2c2; font-size: 14px; line-height: 1.65; }
.ww-hero-moon { margin-bottom: 16px; color: #e7cd88; filter: drop-shadow(0 0 24px rgba(212,166,60,.22)); }
.ww-hero-points { margin-top: 22px; display: flex; flex-direction: column; gap: 9px; }
.ww-hero-point { display: flex; gap: 9px; align-items: flex-start; font-size: 12.5px; color: #c3b9a6; line-height: 1.45; }
.ww-hero-point b { color: #e9dfc9; font-weight: 600; }
.ww-hero-point .dot { width: 5px; height: 5px; border-radius: 50%; background: #c7a95e; margin-top: 7px; flex: none; }
.ww-lobby-card {
  background: rgba(20, 28, 44, .8);
  border: 1px solid rgba(214,190,139,.2);
  border-radius: 16px;
  padding: 20px;
  box-shadow: 0 30px 80px rgba(0,0,0,.35);
  backdrop-filter: blur(10px);
}
.ww-lobby-section + .ww-lobby-section { margin-top: 17px; }
.ww-lobby-label { font-size: 10.5px; letter-spacing: .14em; text-transform: uppercase; color: #9aa4b6; margin-bottom: 8px; }
.ww-board-list { display: flex; flex-direction: column; gap: 7px; }
.ww-board-option {
  text-align: left;
  border: 1px solid rgba(214,190,139,.16);
  background: rgba(255,255,255,.025);
  border-radius: 11px;
  padding: 10px 11px;
  color: #e6dfd0;
  display: flex;
  align-items: center;
  gap: 10px;
}
.ww-board-option:hover { background: rgba(255,255,255,.05); }
.ww-board-option[aria-pressed="true"] {
  border-color: rgba(212,166,60,.65);
  background: rgba(212,166,60,.1);
  box-shadow: 0 0 0 2px rgba(212,166,60,.12);
}
.ww-board-num {
  width: 30px;
  height: 30px;
  border-radius: 9px;
  display: grid;
  place-items: center;
  font-size: 13px;
  font-weight: 650;
  color: #201809;
  background: linear-gradient(180deg, #e3c06a, #b98b30);
  flex: none;
}
.ww-board-copy { min-width: 0; flex: 1; }
.ww-board-copy strong { display: block; font-size: 13px; font-weight: 600; }
.ww-board-copy span { display: block; margin-top: 2px; font-size: 10.5px; color: #98a2b4; line-height: 1.35; }
.ww-board-tag { font-size: 9.5px; padding: 2px 6px; border-radius: 999px; border: 1px solid rgba(212,166,60,.3); color: #d8bd7d; white-space: nowrap; }
.ww-seat-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(38px, 1fr)); gap: 5px; }
.ww-seat-option {
  border: 1px solid rgba(214,190,139,.18);
  background: rgba(255,255,255,.025);
  border-radius: 8px;
  padding: 7px 2px;
  font-size: 12px;
  color: #d8d1c1;
}
.ww-seat-option[aria-pressed="true"] { border-color: rgba(212,166,60,.7); background: rgba(212,166,60,.16); color: #f4dfa4; font-weight: 650; }
.ww-spectate-row { margin-top: 6px; display: flex; gap: 7px; align-items: center; flex-wrap: wrap; }
.ww-spectate-row .ww-btn { border-color: rgba(214,190,139,.22); background: rgba(255,255,255,.03); color: #d8d1c1; }
.ww-spectate-row .ww-btn[aria-pressed="true"] { border-color: rgba(212,166,60,.6); background: rgba(212,166,60,.12); color: #f4dfa4; }
.ww-check { display: inline-flex; align-items: center; gap: 6px; font-size: 11.5px; color: #aab3c1; }
.ww-check input { accent-color: #c7a95e; }
.ww-lobby-speed { display: inline-flex; border: 1px solid rgba(214,190,139,.2); border-radius: 9px; overflow: hidden; }
.ww-lobby-speed button { border: 0; background: rgba(255,255,255,.03); color: #cfc7b6; padding: 7px 13px; font-size: 12px; }
.ww-lobby-speed button[aria-pressed="true"] { background: rgba(212,166,60,.18); color: #f4dfa4; font-weight: 600; }
.ww-lobby-actions { margin-top: 20px; display: flex; align-items: center; gap: 9px; flex-wrap: wrap; }
.ww-lobby-actions .primary { flex: 1 1 auto; padding: 11px 16px; font-size: 14px; }
.ww-lobby-note { margin-top: 10px; font-size: 10.5px; color: #8e99ab; line-height: 1.45; }
.ww-lobby-error { margin-top: 12px; padding: 9px 11px; border-radius: 8px; border: 1px solid rgba(214,91,77,.4); background: rgba(214,91,77,.12); color: #f0b0a7; font-size: 11.5px; line-height: 1.5; white-space: pre-wrap; }
.ww-spectator-banner {
  display: flex; align-items: center; gap: 8px; padding: 9px 11px; border-radius: 9px;
  border: 1px dashed var(--ww-border); color: var(--ww-muted); font-size: 12px; line-height: 1.4;
}
.ww-spectator-banner b { color: var(--ww-text); font-weight: 600; }

@keyframes ww-pulse { 0%, 100% { box-shadow: 0 0 0 2px rgba(110,168,232,.55), 0 0 20px rgba(110,168,232,.25); } 50% { box-shadow: 0 0 0 4px rgba(110,168,232,.18), 0 0 30px rgba(110,168,232,.4); } }
@keyframes ww-bounce { 0%, 80%, 100% { transform: translateY(0); opacity: .5; } 40% { transform: translateY(-3px); opacity: 1; } }
@keyframes ww-fade { from { opacity: 0; } to { opacity: 1; } }

@container (max-width: 980px) {
  .ww-main { grid-template-columns: minmax(0, 1fr); }
  .ww-action-card { order: -1; }
  .ww-lobby { align-items: flex-start; }
  .ww-lobby-inner { grid-template-columns: 1fr; gap: 20px; }
  .ww-hero h1 { font-size: 38px; }
  .ww-hero p { max-width: none; }
}
@container (max-width: 620px) {
  .ww-app { padding: 12px 12px 18px max(12px, var(--dsh-frame-leading-clearance, 0px)); }
  .ww-table { max-width: 100%; }
  .ww-seat { width: 60px; }
  .ww-avatar { width: 38px; height: 38px; font-size: 11px; }
  .ww-seat-name { font-size: 9.5px; max-width: 58px; }
  .ww-mini-tag { font-size: 8px; padding: 1px 3px; }
  .ww-sheriff-badge { width: 16px; height: 16px; font-size: 8.5px; top: -3px; right: -2px; }
  .ww-seat.dead::after { width: 14px; height: 14px; font-size: 10px; top: 9px; right: 5px; }
  .ww-center { width: 46%; padding: 10px; }
  .ww-center-day { font-size: 14px; }
  .ww-center-status { font-size: 10.5px; max-width: 140px; margin-top: 6px; }
  .ww-center-icon { margin-bottom: 2px; }
  .ww-card { padding: 12px; }
  .ww-lobby { padding: 18px 12px 30px; }
  .ww-hero h1 { font-size: 32px; }
  .ww-hero p { font-size: 13px; }
  .ww-lobby-card { padding: 15px; }
}
`;

    const SEAT_HUES = [12, 38, 62, 96, 150, 174, 198, 222, 252, 282, 310, 338];

    function translate(t, key, data) {
      let text = t(key);
      if (typeof text !== "string" || text === key) {
        text = DICTIONARIES.zh[key] ?? DICTIONARIES.en[key] ?? key;
      }
      if (!data) return text;
      return text.replace(/\{(\w+)\}/g, (match, name) =>
        data[name] === undefined ? match : String(data[name]),
      );
    }

    function fallbackT(key) {
      return DICTIONARIES.zh[key] ?? DICTIONARIES.en[key] ?? key;
    }

    function roleInfo(role) {
      return (
        ROLE_INFO[role] || {
          zh: role || "未知",
          en: role || "Unknown",
          glyph: "?",
          hue: 220,
          descZh: "",
          descEn: "",
        }
      );
    }

    function roleName(role, lang) {
      const info = roleInfo(role);
      return lang === "en" ? info.en : info.zh;
    }

    function roleDesc(role, lang) {
      const info = roleInfo(role);
      return lang === "en" ? info.descEn : info.descZh;
    }

    function resultName(result, t) {
      if (result === "TownWins") return t("resultTown");
      if (result === "WolfWins") return t("resultWolf");
      return t("resultDraw");
    }

    function factionName(faction, lang) {
      const info = FACTION_INFO[faction];
      if (!info) return "";
      return lang === "en" ? info.en : info.zh;
    }

    function boardMeta(board, lang) {
      const info = BOARD_META[board?.id] || BOARD_META[1];
      return {
        name: lang === "en" ? info.en : info.zh,
        tag: info.tag,
        pitch: info.pitch,
      };
    }

    function seatHue(seat) {
      return SEAT_HUES[(seat - 1) % SEAT_HUES.length];
    }

    function seatPosition(index, total) {
      const angle = ((-90 + (360 / total) * index) * Math.PI) / 180;
      // Keep the chips (and their role tags) inside the circular table at
      // narrow widths too; the avatar itself is 54px on desktop and 38px on
      // phones, so 36–37% leaves a safe gutter.
      const radius = total >= 12 ? 37 : 36;
      return {
        left: `${50 + radius * Math.cos(angle)}%`,
        top: `${50 + radius * Math.sin(angle)}%`,
      };
    }

    function api(path, body, signal) {
      const url = new URL(`werewolf/${path}`, document.baseURI).href;
      const options = {
        method: body === undefined ? "GET" : "POST",
        headers:
          body === undefined
            ? { accept: "application/json" }
            : { accept: "application/json", "content-type": "application/json" },
        body: body === undefined ? undefined : JSON.stringify(body),
        cache: "no-store",
        signal,
      };

      return fetch(url, options).then(async (response) => {
        let data = null;
        try {
          data = await response.json();
        } catch {
          // service not ready / non-JSON error page
        }
        if (!response.ok || data?.ok === false) {
          throw new Error(data?.error || `HTTP ${response.status}`);
        }
        return data;
      });
    }

    function sleep(ms) {
      return new Promise((resolve) => setTimeout(resolve, ms));
    }

    async function loadState() {
      let lastError;
      for (let attempt = 0; attempt < 6; attempt += 1) {
        try {
          const data = await api("state");
          return data.state;
        } catch (error) {
          lastError = error;
          await sleep(250 + attempt * 200);
        }
      }
      throw lastError;
    }

    function WerewolfIcon(props) {
      const size = props?.size || 22;
      return h(
        "svg",
        {
          viewBox: "0 0 24 24",
          width: size,
          height: size,
          fill: "none",
          stroke: "currentColor",
          strokeWidth: 1.6,
          strokeLinecap: "round",
          strokeLinejoin: "round",
          "aria-hidden": true,
          style: { display: "block", opacity: props?.active === false ? 0.72 : 1 },
        },
        h("path", { d: "M20.4 14.6A8.6 8.6 0 1 1 9.4 3.6a6.9 6.9 0 0 0 11 11Z" }),
        h("path", { d: "M15.6 7.2l.7 1.7 1.8.3-1.3 1.2.4 1.8-1.6-.9-1.6.9.4-1.8-1.3-1.2 1.8-.3Z", opacity: 0.9 }),
        h("path", { d: "M6.4 18.4l1.8-2.2M8.7 20l1.1-1.6", opacity: 0.55 }),
      );
    }

    function MoonIcon(props) {
      const size = props?.size || 42;
      return h(
        "svg",
        {
          viewBox: "0 0 48 48",
          width: size,
          height: size,
          fill: "none",
          stroke: "currentColor",
          strokeWidth: 1.5,
          strokeLinecap: "round",
          "aria-hidden": true,
        },
        h("path", { d: "M38.6 29.2A15.4 15.4 0 1 1 19 9.4a12.3 12.3 0 0 0 19.6 19.8Z" }),
        h("circle", { cx: "31.2", cy: "19.4", r: "1.1", fill: "currentColor", stroke: "none", opacity: 0.9 }),
        h("circle", { cx: "27.4", cy: "24.8", r: "0.8", fill: "currentColor", stroke: "none", opacity: 0.55 }),
        h("circle", { cx: "34.6", cy: "25.6", r: "0.7", fill: "currentColor", stroke: "none", opacity: 0.45 }),
      );
    }

    function SunIcon(props) {
      const size = props?.size || 38;
      return h(
        "svg",
        {
          viewBox: "0 0 48 48",
          width: size,
          height: size,
          fill: "none",
          stroke: "currentColor",
          strokeWidth: 1.6,
          strokeLinecap: "round",
          "aria-hidden": true,
        },
        h("circle", { cx: "24", cy: "24", r: "8.5" }),
        h("path", { d: "M24 5.5v5M24 37.5v5M5.5 24h5M37.5 24h5M11 11l3.5 3.5M33.5 33.5 37 37M37 11l-3.5 3.5M14.5 33.5 11 37" }),
      );
    }

    function Dots() {
      return h("span", { className: "ww-dots", "aria-hidden": true }, h("i"), h("i"), h("i"));
    }

    function eventKindTag(event, t, lang) {
      if (event.type === "speech") {
        if (event.kind === "WolfChat") return t("wolfChatTag");
        if (event.kind === "LastWords") return t("lastWordsTag");
        return t("speechTag");
      }
      if (event.vis === "private") return t("privateTag");
      if (event.vis === "moderator") return t("godTag");
      return null;
    }

    function confirmChoices(kind, t) {
      const make = (suffix) => ({
        yes: t(`confirm.${kind}.true`) === `confirm.${kind}.true`
          ? t(`confirm.default.true`)
          : t(`confirm.${kind}.true`),
        no: t(`confirm.${kind}.false`) === `confirm.${kind}.false`
          ? t(`confirm.default.false`)
          : t(`confirm.${kind}.false`),
        suffix,
      });
      const labels = make(kind);
      if (kind === "WitchSave") {
        return { yes: labels.yes, no: labels.no, yesTone: "good", noTone: "" };
      }
      if (kind === "SelfDestruct") {
        return { yes: labels.yes, no: labels.no, yesTone: "danger", noTone: "" };
      }
      return { yes: labels.yes, no: labels.no, yesTone: "", noTone: "" };
    }

    function WerewolfPanel(props) {
      const t = (key, data) => translate(props.t || fallbackT, key, data);
      const lang = typeof props.locale === "string" && props.locale.startsWith("en") ? "en" : "zh";

      const [state, setState] = React.useState(null);
      const [loadError, setLoadError] = React.useState(null);
      const [notice, setNotice] = React.useState(null);
      const [busy, setBusy] = React.useState(false);
      const [selected, setSelected] = React.useState(null);
      const [draft, setDraft] = React.useState("");
      const [filter, setFilter] = React.useState("all");
      const [autoScroll, setAutoScroll] = React.useState(true);
      const [showLobby, setShowLobby] = React.useState(true);
      const [lobby, setLobby] = React.useState({
        board: 1,
        humanSeat: 1,
        speed: "normal",
        godView: false,
      });
      const [resetArmed, setResetArmed] = React.useState(false);

      const aliveRef = React.useRef(true);
      const stateRef = React.useRef(null);
      const revisionRef = React.useRef(-1);
      const sessionRef = React.useRef(null);
      const epochRef = React.useRef(0);
      const busyRef = React.useRef(false);
      const pendingIdRef = React.useRef(null);
      const feedRef = React.useRef(null);
      const appRef = React.useRef(null);
      const speechRef = React.useRef(null);
      const initializedRef = React.useRef(false);

      stateRef.current = state;

      function applyState(next, force) {
        if (!next) return;
        // Revisions are session-local. Two games (or a new game) can stop at the
        // same revision; comparing only the revision would ignore the new table.
        const sameSession = sessionRef.current === (next.sessionId ?? null);
        if (!force && sameSession && revisionRef.current === next.revision) return;
        sessionRef.current = next.sessionId ?? null;
        revisionRef.current = next.revision;
        stateRef.current = next;
        setState(next);
      }

      function syncLobby(next) {
        if (!next) return;
        setLobby((current) => ({
          board: next.board?.id || current.board,
          humanSeat:
            next.config?.humanSeat === undefined ? current.humanSeat : next.config.humanSeat,
          speed: next.config?.speed || current.speed,
          godView:
            next.config?.godView === undefined ? current.godView : next.config.godView,
        }));
      }

      React.useEffect(() => {
        aliveRef.current = true;
        let cancelled = false;
        let timer = null;

        async function poll() {
          if (cancelled) return;
          if (busyRef.current) {
            timer = setTimeout(poll, POLL_MS);
            return;
          }
          const epoch = epochRef.current;
          try {
            const next = await api("state");
            if (!cancelled && epoch === epochRef.current && !busyRef.current) {
              applyState(next.state, false);
              setLoadError(null);
            }
          } catch {
            // a background poll failure should not wipe a game already on screen
          }
          if (!cancelled) timer = setTimeout(poll, POLL_MS);
        }

        (async () => {
          try {
            const next = await loadState();
            if (cancelled) return;
            applyState(next, true);
            syncLobby(next);
            setShowLobby(next.status === "idle" || next.status === "aborted");
            initializedRef.current = true;
          } catch (error) {
            if (!cancelled) setLoadError(t("serviceError"));
          }
          if (!cancelled) timer = setTimeout(poll, POLL_MS);
        })();

        return () => {
          cancelled = true;
          aliveRef.current = false;
          if (timer) clearTimeout(timer);
        };
      }, []);

      const pending = state?.pendingAsk || null;
      const pendingId = pending?.id ?? null;

      React.useEffect(() => {
        if (pendingId === pendingIdRef.current) return;
        pendingIdRef.current = pendingId;
        setSelected(null);
        setDraft("");
        if (pending?.qtype === "speak") {
          setTimeout(() => {
            try {
              speechRef.current?.focus?.();
            } catch {
              // Some embedded/test DOMs reject focus(); it is only a convenience.
            }
          }, 40);
        }
      }, [pendingId, pending?.qtype]);

      React.useEffect(() => {
        if (state?.status === "finished" || state?.status === "error") setShowLobby(false);
        if (state?.status === "idle" || state?.status === "aborted") setShowLobby(true);
      }, [state?.status]);

      React.useEffect(() => {
        if (!autoScroll) return undefined;
        const element = feedRef.current;
        if (!element) return undefined;
        const frame = requestAnimationFrame(() => {
          element.scrollTop = element.scrollHeight;
        });
        return () => cancelAnimationFrame(frame);
      }, [state?.events?.length, autoScroll, filter]);

      React.useEffect(() => {
        if (!resetArmed) return undefined;
        const timer = setTimeout(() => setResetArmed(false), 4000);
        return () => clearTimeout(timer);
      }, [resetArmed]);

      React.useEffect(() => {
        if (notice && notice.autoHide) {
          const timer = setTimeout(() => setNotice(null), 3600);
          return () => clearTimeout(timer);
        }
        return undefined;
      }, [notice]);

      async function runAction(payload, options = {}) {
        busyRef.current = true;
        setBusy(true);
        const epoch = epochRef.current + 1;
        epochRef.current = epoch;
        try {
          const data = await api("action", payload);
          if (epoch === epochRef.current) {
            applyState(data.state, true);
            if (options.closeLobby) setShowLobby(false);
            if (options.notice) setNotice({ text: options.notice, autoHide: true });
            else setNotice(null);
          }
          return data.state;
        } catch (error) {
          setNotice({
            error: true,
            text: `${t("actionError")} ${error.message || ""}`.trim(),
          });
          return null;
        } finally {
          busyRef.current = false;
          setBusy(false);
        }
      }

      function openLobby() {
        syncLobby(stateRef.current);
        setShowLobby(true);
        setNotice(null);
        if (appRef.current) appRef.current.scrollTop = 0;
      }

      function startGame() {
        if (busyRef.current) return;
        const seat = Math.min(Math.max(Number(lobby.humanSeat) || 0, 0), 12);
        runAction(
          {
            action: "new",
            board: lobby.board,
            humanSeat: seat,
            speed: lobby.speed,
            godView: Boolean(lobby.godView && seat === 0),
          },
          { closeLobby: true },
        );
      }

      function answerChoice(choice) {
        if (!pending || busy || busyRef.current) return;
        runAction({
          action: "reply",
          sessionId: pending.sessionId ?? state?.sessionId,
          askId: pending.id,
          choice,
        });
      }

      function confirmDecision(decision) {
        if (!pending || busy || busyRef.current) return;
        runAction({
          action: "reply",
          sessionId: pending.sessionId ?? state?.sessionId,
          askId: pending.id,
          decision,
        });
      }

      async function sendSpeech(text) {
        if (!pending || busy || busyRef.current) return;
        const result = await runAction({
          action: "reply",
          sessionId: pending.sessionId ?? state?.sessionId,
          askId: pending.id,
          text,
        });
        if (result) setDraft("");
      }

      function autoPlay() {
        if (!pending || busy || busyRef.current) return;
        runAction({
          action: "auto",
          sessionId: pending.sessionId ?? state?.sessionId,
          askId: pending.id,
        });
      }

      function changeSpeed(speed) {
        if (busyRef.current) return;
        if (state?.status === "idle") {
          setLobby((current) => ({ ...current, speed }));
          return;
        }
        runAction({ action: "speed", speed });
      }

      function abortGame() {
        if (busyRef.current) return;
        if (!resetArmed) {
          setResetArmed(true);
          return;
        }
        setResetArmed(false);
        runAction({ action: "abort" }, { closeLobby: false }).then(() => {
          setShowLobby(true);
        });
      }

      function selectSeat(seat) {
        if (!pending || pending.qtype !== "choose" || busy) return;
        if (!pending.candidates.some((entry) => entry.seat === seat)) return;
        setSelected((current) => (current === seat ? null : seat));
      }

      function selectFilter(next) {
        setFilter(next);
        setAutoScroll(true);
      }

      const status = state?.status || "loading";
      const board = state?.board;
      const meta = boardMeta(board, lang);
      const self = state?.self || null;
      const players = state?.players || [];
      const finished = status === "finished";
      const godView = Boolean(state?.config?.godView);
      const spectator = state && state.config?.humanSeat === 0;
      const night = state?.phase === "Night";
      const phaseLabel = night ? t("phaseNight") : t("phaseDay");
      const isLobby = showLobby && status !== "loading";

      const centerStatus = (() => {
        if (status === "starting" || status === "building") return t("building");
        if (status === "error") return t("errorTitle");
        if (pending) return t("yourTurn");
        if (state?.thinkingSeat) return t("thinking", { name: `P${state.thinkingSeat}` });
        if (finished) return t("gameOver");
        if (state) return t("waiting");
        return "";
      })();

      const visibleEvents = React.useMemo(() => {
        const events = state?.events || [];
        const humanSeat = state?.config?.humanSeat ?? 0;
        const god =
          state?.status === "finished" ||
          (state?.config?.godView && humanSeat === 0);

        return events.filter((event) => {
          if (event.type === "deal") return false;
          if (filter === "all") {
            if (!god || humanSeat === 0) return true;
            return (
              event.vis === "public" ||
              event.vis === "moderator" ||
              (event.vis === "private" && event.seat === humanSeat)
            );
          }
          if (filter === "speech") return event.type === "speech";
          if (filter === "judge") return event.type === "narration" || event.type === "status";
          if (filter === "private") return event.vis === "private";
          if (filter === "god") return event.vis === "moderator";
          return true;
        });
      }, [state?.events, state?.status, state?.config?.humanSeat, state?.config?.godView, filter]);

      function renderEvent(event, index, previous) {
        const kindTag = eventKindTag(event, t, lang);
        const speakerSeat = event.speaker ?? event.seat ?? null;
        const speakerPlayer = speakerSeat
          ? players.find((player) => player.seat === speakerSeat)
          : null;
        const isSpeech = event.type === "speech";
        const isPrivate = event.vis === "private";
        const isGod = event.vis === "moderator";
        const isNarration = event.type === "narration" || event.type === "status";
        let text = event.text || "";
        if (isSpeech && speakerSeat) {
          text = text.replace(new RegExp(`^P${speakerSeat}[^：:]*[：:]\\s*`), "");
        }
        const className = [
          "ww-event",
          isNarration ? "narration" : "",
          isSpeech ? "speech" : "",
          isPrivate && !isSpeech ? "private" : "",
          isPrivate && isSpeech ? "private" : "",
          isGod ? "god" : "",
        ]
          .filter(Boolean)
          .join(" ");

        const showDivider =
          filter === "all" &&
          event.day &&
          (!previous || previous.day !== event.day || previous.phase !== event.phase) &&
          event.type !== "deal";

        return h(
          React.Fragment,
          { key: `${event.seq}-${index}` },
          showDivider
            ? h(
                "div",
                { className: "ww-divider" },
                t("dayDivider", {
                  n: event.day,
                  phase: event.phase === "Night" ? t("night") : t("day"),
                }),
              )
            : null,
          h(
            "div",
            { className },
            isNarration
              ? null
              : h(
                  "span",
                  { className: "ww-event-icon", title: kindTag || "" },
                  isSpeech
                    ? String(speakerSeat || "?")
                    : isPrivate
                      ? "私"
                      : isGod
                        ? "上"
                        : "·",
                ),
            h(
              "div",
              { className: "ww-event-body" },
              !isNarration && kindTag
                ? h(
                    "div",
                    { className: "ww-event-meta" },
                    speakerPlayer
                      ? h(
                          "span",
                          { className: "ww-speaker" },
                          speakerPlayer.name && speakerPlayer.name !== `P${speakerPlayer.seat}`
                            ? `P${speakerPlayer.seat} ${speakerPlayer.name}`
                            : `P${speakerPlayer.seat}`,
                        )
                      : null,
                    h(
                      "span",
                      {
                        className:
                          "ww-tag" +
                          (isGod
                            ? " god"
                            : isPrivate
                              ? " private"
                              : isSpeech
                                ? " speech"
                                : ""),
                      },
                      kindTag,
                    ),
                  )
                : null,
              h("div", { className: "ww-event-text" }, text),
            ),
          ),
        );
      }

      const feed = [];
      let previousEvent = null;
      for (let index = 0; index < visibleEvents.length; index += 1) {
        const event = visibleEvents[index];
        feed.push(renderEvent(event, index, previousEvent));
        previousEvent = event;
      }

      const centerIcon = finished
        ? h(WerewolfIcon, { size: 38 })
        : night
          ? h(MoonIcon, { size: 42 })
          : h(SunIcon, { size: 38 });

      const resultTone =
        state?.result === "TownWins" ? "town" : state?.result === "WolfWins" ? "wolf" : "draw";

      const roleCard = (() => {
        if (spectator) {
          return h(
            "section",
            { className: "ww-card" },
            h("div", { className: "ww-card-eyebrow" }, t("role")),
            h(
              "div",
              { className: "ww-spectator-banner" },
              h(WerewolfIcon, { size: 17 }),
              h(
                "span",
                null,
                h("b", null, t("spectator")),
                " · ",
                finished
                  ? t("revealAll")
                  : state?.config?.godView
                    ? t("spectatorGodHint")
                    : t("spectatorHint"),
              ),
            ),
          );
        }
        if (!self) {
          return h(
            "section",
            { className: "ww-card" },
            h("div", { className: "ww-card-eyebrow" }, t("role")),
            h("div", { className: "ww-waiting" }, t("building")),
          );
        }
        const info = roleInfo(self.role);
        const faction = self.faction;
        const alive = self.alive;
        return h(
          "section",
          { className: "ww-card highlight" },
          h("div", { className: "ww-card-eyebrow" }, t("role")),
          h(
            "div",
            { className: "ww-role-head" },
            h(
              "span",
              {
                className: "ww-role-glyph",
                style: { "--role-hue": info.hue },
                "aria-hidden": true,
              },
              info.glyph,
            ),
            h(
              "div",
              null,
              h("div", { className: "ww-role-name" }, roleName(self.role, lang)),
              h(
                "div",
                { className: `ww-role-faction${faction === "Wolf" ? " wolf" : ""}` },
                `${factionName(faction, lang)} · P${self.seat} · ${alive ? t("alive") : t("out")}${self.sheriff ? ` · ${t("sheriff")}` : ""}`,
              ),
            ),
          ),
          h("p", { className: "ww-role-desc" }, roleDesc(self.role, lang)),
          self.isWolf && self.teammates.length > 0
            ? h(
                "div",
                null,
                h("div", { className: "ww-role-notes-label", style: { marginTop: "11px" } }, t("teammates")),
                h(
                  "div",
                  { className: "ww-teammates" },
                  self.teammates.map((seat) =>
                    h("span", { className: "ww-chip wolf", key: seat }, `P${seat}`),
                  ),
                ),
              )
            : null,
          !alive ? h("p", { className: "ww-action-hint" }, t("deadHint")) : null,
          h(
            "div",
            { className: "ww-role-notes" },
            h("div", { className: "ww-role-notes-label" }, t("notes")),
            self.notes.length === 0
              ? h("div", { className: "ww-note empty" }, t("noNotes"))
              : self.notes.map((note, index) =>
                  h("div", { className: "ww-note", key: `${index}-${note}` }, note),
                ),
          ),
        );
      })();

      const actionCard = (() => {
        if (status === "error") {
          return h(
            "section",
            { className: "ww-card highlight ww-action-card" },
            h("div", { className: "ww-card-eyebrow" }, t("errorTitle")),
            h("div", { className: "ww-error-banner" }, state?.error || t("serviceError")),
            h(
              "div",
              { className: "ww-action-row" },
              h(
                "button",
                { type: "button", className: "ww-btn", onClick: openLobby },
                t("lobby"),
              ),
            ),
          );
        }

        if (finished) {
          return h(
            "section",
            { className: "ww-card highlight ww-action-card" },
            h(
              "div",
              { className: `ww-result-card ${resultTone}` },
              h("div", { className: "result-kicker" }, t("gameOver")),
              h(
                "div",
                { className: "result-name" },
                resultName(state?.result, t),
              ),
            ),
            h(
              "div",
              { className: "ww-result-roles" },
              players.map((player) =>
                h(
                  "span",
                  {
                    className:
                      "ww-result-role" + (player.faction === "Wolf" ? " wolf" : ""),
                    key: player.seat,
                  },
                  `P${player.seat} ${player.roleName || roleName(player.role, lang) || "?"}`,
                ),
              ),
            ),
            h(
              "div",
              { className: "ww-action-row" },
              h(
                "button",
                { type: "button", className: "ww-btn primary grow", onClick: openLobby },
                t("playAgain"),
              ),
            ),
          );
        }

        if (pending) {
          const choices = pending.qtype === "confirm" ? confirmChoices(pending.kind, t) : null;
          const candidates = pending.candidates || [];
          const kindKey = `kind.${pending.kind}`;
          const translatedKind = t(kindKey);
          const actionTitle =
            translatedKind && translatedKind !== kindKey
              ? translatedKind
              : pending.title || kindKey;
          return h(
            "section",
            { className: "ww-card highlight ww-action-card" },
            h(
              "div",
              { className: "ww-action-title" },
              h("h3", null, actionTitle),
              h(
                "span",
                { className: "ww-tag" },
                pending.phase === "Night" ? t("night") : t("day"),
              ),
            ),
            h("p", { className: "ww-action-prompt" }, pending.prompt),
            pending.qtype === "choose"
              ? h(
                  "div",
                  null,
                  h(
                    "div",
                    { className: "ww-target-grid", role: "group", "aria-label": t("pickTarget") },
                    candidates.map((candidate) =>
                      h(
                        "button",
                        {
                          type: "button",
                          key: candidate.seat,
                          className: "ww-target",
                          "aria-pressed": selected === candidate.seat,
                          disabled: busy,
                          onClick: () => setSelected((current) => (current === candidate.seat ? null : candidate.seat)),
                        },
                        h("span", { className: "seat-no" }, `P${candidate.seat}`),
                        candidate.name !== `P${candidate.seat}`
                          ? h("span", { className: "seat-sub" }, candidate.name)
                          : null,
                      ),
                    ),
                  ),
                  h(
                    "div",
                    { className: "ww-action-hint" },
                    selected ? `${t("pickTarget")} · P${selected}` : t("chooseSeat"),
                  ),
                  h(
                    "div",
                    { className: "ww-action-row" },
                    h(
                      "button",
                      {
                        type: "button",
                        className: "ww-btn primary grow",
                        disabled: busy || selected === null,
                        onClick: () => answerChoice(selected),
                      },
                      t("confirm"),
                    ),
                    pending.allowSkip
                      ? h(
                          "button",
                          {
                            type: "button",
                            className: "ww-btn",
                            disabled: busy,
                            onClick: () => answerChoice(null),
                          },
                          t("skip"),
                        )
                      : null,
                  ),
                )
              : null,
            pending.qtype === "confirm"
              ? h(
                  "div",
                  { className: "ww-confirm-grid" },
                  h(
                    "button",
                    {
                      type: "button",
                      className: `ww-confirm-btn ${choices.yesTone}`,
                      disabled: busy,
                      onClick: () => confirmDecision(true),
                    },
                    choices.yes,
                  ),
                  h(
                    "button",
                    {
                      type: "button",
                      className: `ww-confirm-btn ${choices.noTone}`,
                      disabled: busy,
                      onClick: () => confirmDecision(false),
                    },
                    choices.no,
                  ),
                )
              : null,
            pending.qtype === "speak"
              ? h(
                  "div",
                  null,
                  h("textarea", {
                    ref: speechRef,
                    className: "ww-speech",
                    value: draft,
                    maxLength: 600,
                    placeholder:
                      pending.kind === "LastWords"
                        ? t("writeLastWords")
                        : pending.kind === "WolfChat"
                          ? t("writeWolfChat")
                          : t("writeSpeech"),
                    disabled: busy,
                    onChange: (event) => setDraft(event.target.value),
                    onKeyDown: (event) => {
                      if ((event.metaKey || event.ctrlKey) && event.key === "Enter") {
                        event.preventDefault();
                        sendSpeech(draft);
                      }
                    },
                  }),
                  h("div", { className: "ww-counter" }, t("charCount", { n: draft.length })),
                  h(
                    "div",
                    { className: "ww-action-row" },
                    h(
                      "button",
                      {
                        type: "button",
                        className: "ww-btn primary grow",
                        disabled: busy,
                        onClick: () => sendSpeech(draft),
                      },
                      t("send"),
                    ),
                    h(
                      "button",
                      {
                        type: "button",
                        className: "ww-btn",
                        disabled: busy,
                        onClick: () => sendSpeech(""),
                      },
                      t("pass"),
                    ),
                  ),
                )
              : null,
            h(
              "div",
              { className: "ww-action-row" },
              h(
                "button",
                {
                  type: "button",
                  className: "ww-btn ghost small grow",
                  disabled: busy,
                  onClick: autoPlay,
                },
                t("autoPlay"),
              ),
              h(
                "button",
                {
                  type: "button",
                  className: `ww-btn small ${resetArmed ? "danger" : "ghost"}`,
                  disabled: busy,
                  onClick: abortGame,
                },
                resetArmed ? t("abortConfirm") : t("abort"),
              ),
            ),
          );
        }

        if (state?.thinkingSeat) {
          return h(
            "section",
            { className: "ww-card ww-action-card" },
            h(
              "div",
              { className: "ww-card-eyebrow" },
              spectator ? t("spectator") : t("inProgress"),
            ),
            h(
              "div",
              { className: "ww-thinking" },
              h(Dots),
              h("span", null, t("thinking", { name: `P${state.thinkingSeat}` })),
            ),
            h(
              "div",
              { className: "ww-action-row" },
              h(
                "button",
                {
                  type: "button",
                  className: "ww-btn ghost small",
                  onClick: openLobby,
                },
                t("newGame"),
              ),
              h(
                "button",
                {
                  type: "button",
                  className: `ww-btn small ${resetArmed ? "danger" : "ghost"}`,
                  disabled: busy,
                  onClick: abortGame,
                },
                resetArmed ? t("abortConfirm") : t("abort"),
              ),
            ),
          );
        }

        return h(
          "section",
          { className: "ww-card ww-action-card" },
          h(
            "div",
            { className: "ww-card-eyebrow" },
            spectator ? t("spectator") : t("inProgress"),
          ),
          h("div", { className: "ww-waiting" }, state ? t("waiting") : t("building")),
          h(
            "div",
            { className: "ww-action-row" },
            h("button", { type: "button", className: "ww-btn small", onClick: openLobby }, t("newGame")),
            h(
              "button",
              {
                type: "button",
                className: `ww-btn small ${resetArmed ? "danger" : "ghost"}`,
                onClick: abortGame,
              },
              resetArmed ? t("abortConfirm") : t("abort"),
            ),
          ),
        );
      })();

      const header = h(
        "header",
        { className: "ww-header" },
        h(
          "div",
          { className: "ww-brand" },
          h("span", { className: "ww-brand-mark" }, h(WerewolfIcon, { size: 23 })),
          h(
            "div",
            { className: "ww-titles" },
            h("h1", null, t("title")),
            h(
              "p",
              null,
              state
                ? `${meta.name} · ${t("players", { n: board?.players || 0 })} · ${board?.pitch || ""}`
                : t("subtitle"),
            ),
          ),
        ),
        h(
          "div",
          { className: "ww-header-actions" },
          state && status !== "idle"
            ? h(
                "span",
                { className: `ww-phase-pill ${night ? "night" : "day"}` },
                night ? h(MoonIcon, { size: 15 }) : h(SunIcon, { size: 14 }),
                `${t("dayN", { n: state.day })} · ${night ? t("night") : t("day")}`,
              )
            : null,
          h(
            "div",
            { className: "ww-speed", role: "group", "aria-label": t("speed") },
            ["calm", "normal", "fast"].map((speed) =>
              h(
                "button",
                {
                  type: "button",
                  key: speed,
                  "aria-pressed": (state?.config?.speed || lobby.speed) === speed,
                  disabled: busy,
                  onClick: () => changeSpeed(speed),
                },
                t(speed),
              ),
            ),
          ),
          h(
            "button",
            { type: "button", className: "ww-btn", disabled: busy, onClick: openLobby },
            t("newGame"),
          ),
        ),
      );

      const table = h(
        "div",
        { className: "ww-stage" },
        h(
          "div",
          { className: "ww-table", "aria-label": t("title") },
          players.map((player, index) => {
            const selectable =
              Boolean(pending) &&
              pending.qtype === "choose" &&
              pending.candidates.some((entry) => entry.seat === player.seat) &&
              !busy;
            const tags = [];
            if (player.isSelf) tags.push(h("span", { className: "ww-mini-tag you", key: "you" }, t("you")));
            if (player.isTeammate) tags.push(h("span", { className: "ww-mini-tag wolf", key: "mate" }, t("teammates")));
            if (player.role) {
              tags.push(
                h(
                  "span",
                  { className: "ww-mini-tag role", key: "role" },
                  roleName(player.role, lang),
                ),
              );
            }
            return h(
              "button",
              {
                type: "button",
                key: player.seat,
                className: [
                  "ww-seat",
                  player.isSelf ? "self" : "",
                  player.sheriff ? "sheriff" : "",
                  !player.alive ? "dead" : "",
                  state?.thinkingSeat === player.seat ? "thinking" : "",
                  selectable ? "selectable" : "",
                  pending &&
                  pending.qtype === "choose" &&
                  !pending.candidates.some((entry) => entry.seat === player.seat)
                    ? "not-target"
                    : "",
                  selected === player.seat ? "selected" : "",
                ]
                  .filter(Boolean)
                  .join(" "),
                style: {
                  ...seatPosition(index, Math.max(players.length, 1)),
                  "--seat-hue": seatHue(player.seat),
                },
                disabled: !selectable,
                "aria-pressed": selected === player.seat,
                onClick: () => selectSeat(player.seat),
                title: `${player.name}${player.alive ? "" : ` · ${t("out")}`}`,
              },
              h(
                "span",
                { className: "ww-avatar" },
                `P${player.seat}`,
                player.sheriff ? h("span", { className: "ww-sheriff-badge" }, "警") : null,
              ),
              player.name && player.name !== `P${player.seat}`
                ? h("span", { className: "ww-seat-name" }, player.name)
                : null,
              h("span", { className: "ww-seat-tags" }, tags),
            );
          }),
          h(
            "div",
            { className: "ww-center" },
            h("span", { className: "ww-center-icon" }, centerIcon),
            h("div", { className: "ww-center-day" }, t("dayN", { n: state?.day || 1 })),
            h(
              "div",
              { className: "ww-center-phase" },
              finished
                ? t("gameOver")
                : night
                  ? t("phaseNight")
                  : t("phaseDay"),
            ),
            finished
              ? h(
                  "div",
                  { className: `ww-center-result ${resultTone}` },
                  resultName(state?.result, t),
                )
              : h(
                  "div",
                  {
                    className:
                      "ww-center-status" + (pending || state?.thinkingSeat ? " alert" : ""),
                  },
                  centerStatus,
                ),
          ),
        ),
        status === "building" || status === "starting"
          ? h("div", { className: "ww-building" }, state?.buildMessage || t("building"))
          : null,
        state?.error && status === "error" ? h("div", { className: "ww-error-banner" }, state.error) : null,
      );

      const side = h(
        "aside",
        { className: "ww-side" },
        roleCard,
        actionCard,
      );

      const feedElement = h(
        "section",
        { className: "ww-feed" },
        h(
          "div",
          { className: "ww-feed-head" },
          h("h2", null, t("feed")),
          h(
            "div",
            { className: "ww-feed-tools" },
            h(
              "div",
              { className: "ww-filter", role: "group", "aria-label": t("feed") },
              [
                ["all", t("filterAll")],
                ["speech", t("filterSpeech")],
                ["judge", t("filterJudge")],
                ["private", t("filterPrivate")],
                ...(finished || godView ? [["god", t("filterGod")]] : []),
              ].map(([value, label]) =>
                h(
                  "button",
                  {
                    type: "button",
                    key: value,
                    "aria-pressed": filter === value,
                    onClick: () => selectFilter(value),
                  },
                  label,
                ),
              ),
            ),
            h(
              "label",
              { className: "ww-toggle" },
              h("input", {
                type: "checkbox",
                checked: autoScroll,
                onChange: (event) => setAutoScroll(event.target.checked),
              }),
              t("autoScroll"),
            ),
          ),
        ),
        h(
          "div",
          {
            className: "ww-feed-list",
            ref: feedRef,
            onScroll: (event) => {
              const element = event.currentTarget;
              const atBottom =
                element.scrollHeight - element.scrollTop - element.clientHeight < 28;
              if (atBottom && !autoScroll) setAutoScroll(true);
            },
            onWheel: (event) => {
              if (event.deltaY < 0 && autoScroll) setAutoScroll(false);
            },
            onTouchMove: () => {
              if (autoScroll) setAutoScroll(false);
            },
          },
          feed.length === 0 ? h("div", { className: "ww-empty" }, t("emptyFeed")) : feed,
        ),
      );

      const lobbyElement = isLobby
        ? h(
            "div",
            { className: "ww-lobby", role: "dialog", "aria-modal": "true", "aria-label": t("lobby") },
            h(
              "div",
              { className: "ww-lobby-inner" },
              h(
                "div",
                { className: "ww-hero" },
                h("div", { className: "ww-hero-moon" }, h(MoonIcon, { size: 58 })),
                h("div", { className: "eyebrow" }, t("eyebrow")),
                h("h1", null, t("title")),
                h("p", null, t("subtitle")),
                h(
                  "div",
                  { className: "ww-hero-points" },
                  h(
                    "div",
                    { className: "ww-hero-point" },
                    h("span", { className: "dot" }),
                    h("span", null, t("noteLobby")),
                  ),
                  h(
                    "div",
                    { className: "ww-hero-point" },
                    h("span", { className: "dot" }),
                    h("span", null, t("noteFirstBuild")),
                  ),
                  h(
                    "div",
                    { className: "ww-hero-point" },
                    h("span", { className: "dot" }),
                    h("span", null, t("noteDynamic")),
                  ),
                ),
                loadError ? h("div", { className: "ww-lobby-error" }, loadError) : null,
              ),
              h(
                "div",
                { className: "ww-lobby-card" },
                h(
                  "div",
                  { className: "ww-lobby-section" },
                  h("div", { className: "ww-lobby-label" }, t("boards")),
                  h(
                    "div",
                    { className: "ww-board-list" },
                    [1, 2, 3].map((id) => {
                      const info = BOARD_META[id];
                      const name = lang === "en" ? info.en : info.zh;
                      return h(
                        "button",
                        {
                          type: "button",
                          key: id,
                          className: "ww-board-option",
                          "aria-pressed": lobby.board === id,
                          onClick: () =>
                            setLobby((current) => ({
                              ...current,
                              board: id,
                              humanSeat: Math.min(
                                current.humanSeat,
                                id === 1 ? 9 : 12,
                              ),
                            })),
                        },
                        h("span", { className: "ww-board-num" }, String(id)),
                        h(
                          "span",
                          { className: "ww-board-copy" },
                          h("strong", null, name),
                          h("span", null, info.pitch),
                        ),
                        h("span", { className: "ww-board-tag" }, info.tag),
                      );
                    }),
                  ),
                ),
                h(
                  "div",
                  { className: "ww-lobby-section" },
                  h("div", { className: "ww-lobby-label" }, t("seat")),
                  (() => {
                    const total = lobby.board === 1 ? 9 : 12;
                    return h(
                      "div",
                      { className: "ww-seat-grid" },
                      Array.from({ length: total }, (_, index) => index + 1).map((seat) =>
                        h(
                          "button",
                          {
                            type: "button",
                            key: seat,
                            className: "ww-seat-option",
                            "aria-pressed": lobby.humanSeat === seat,
                            onClick: () =>
                              setLobby((current) => ({ ...current, humanSeat: seat })),
                          },
                          `P${seat}`,
                        ),
                      ),
                    );
                  })(),
                  h(
                    "div",
                    { className: "ww-spectate-row" },
                    h(
                      "button",
                      {
                        type: "button",
                        className: "ww-btn",
                        onClick: () => {
                          const total = lobby.board === 1 ? 9 : 12;
                          setLobby((current) => ({
                            ...current,
                            humanSeat: 1 + Math.floor(Math.random() * total),
                          }));
                        },
                      },
                      t("randomSeat"),
                    ),
                    h(
                      "button",
                      {
                        type: "button",
                        className: "ww-btn",
                        "aria-pressed": lobby.humanSeat === 0,
                        onClick: () => setLobby((current) => ({ ...current, humanSeat: 0 })),
                      },
                      t("spectate"),
                    ),
                    lobby.humanSeat === 0
                      ? h(
                          "label",
                          { className: "ww-check" },
                          h("input", {
                            type: "checkbox",
                            checked: Boolean(lobby.godView),
                            onChange: (event) =>
                              setLobby((current) => ({
                                ...current,
                                godView: event.target.checked,
                              })),
                          }),
                          t("godView"),
                        )
                      : null,
                  ),
                ),
                h(
                  "div",
                  { className: "ww-lobby-section" },
                  h("div", { className: "ww-lobby-label" }, t("speed")),
                  h(
                    "div",
                    { className: "ww-lobby-speed", role: "group", "aria-label": t("speed") },
                    ["calm", "normal", "fast"].map((speed) =>
                      h(
                        "button",
                        {
                          type: "button",
                          key: speed,
                          "aria-pressed": lobby.speed === speed,
                          onClick: () => setLobby((current) => ({ ...current, speed })),
                        },
                        t(speed),
                      ),
                    ),
                  ),
                ),
                h(
                  "div",
                  { className: "ww-lobby-actions" },
                  h(
                    "button",
                    {
                      type: "button",
                      className: "ww-btn primary",
                      disabled: busy || status === "starting" || status === "building",
                      onClick: startGame,
                    },
                    status === "building" || status === "starting" ? t("building") : t("start"),
                  ),
                  state && status !== "idle" && status !== "aborted"
                    ? h(
                        "button",
                        {
                          type: "button",
                          className: "ww-btn ghost",
                          onClick: () => setShowLobby(false),
                        },
                        t("backToGame"),
                      )
                    : null,
                ),
                h(
                  "div",
                  { className: "ww-lobby-note" },
                  state?.config?.seed ? `${t("seed", { seed: state.config.seed })} · ` : "",
                  t("noteFirstBuild"),
                  ["running", "starting", "building"].includes(status)
                    ? ` ${t("noteRestart")}`
                    : "",
                ),
              ),
            ),
          )
        : null;

      if (status === "loading" && !state) {
        return h(
          "div",
          { className: "ww-app" },
          h("style", null, CSS),
          h(
            "div",
            { className: "ww-empty", style: { marginTop: "18vh" } },
            loadError ? loadError : t("building"),
          ),
          loadError
            ? h(
                "div",
                { style: { textAlign: "center", marginTop: "12px" } },
                h(
                  "button",
                  {
                    type: "button",
                    className: "ww-btn",
                    onClick: () => window.location.reload(),
                  },
                  t("retry"),
                ),
              )
            : null,
        );
      }

      return h(
        "div",
        { className: "ww-app", ref: appRef },
        h("style", null, CSS),
        header,
        notice
          ? h("div", { className: "ww-toast", role: "status", "aria-live": "polite" }, notice.text)
          : null,
        h("div", { className: "ww-main" }, table, side),
        feedElement,
        lobbyElement,
      );
    }

    function apply(ctx) {
      ctx.effect(() => ctx.locale.register(NS, DICTIONARIES), "werewolf: locale");
      const t = ctx.locale.bind(NS);

      ctx.effect(
        () =>
          ctx.slots.inject("main", () =>
            ctx.slots.register(
              { name: "main", key: PANEL_ID, locale: NS },
              WerewolfPanel,
            ),
          ),
        "werewolf: panel",
      );

      ctx.effect(
        () =>
          ctx.slots.inject("sidebar.panellist", () =>
            ctx.slots.register(
              {
                name: "sidebar.panellist",
                id: PANEL_ID,
                order: 14,
                label: () => t("panel"),
                locale: NS,
              },
              WerewolfIcon,
            ),
          ),
        "werewolf: sidebar entry",
      );
    }

    return { inject: ["slots", "locale"], apply, WerewolfPanel, WerewolfIcon };
  },
});
