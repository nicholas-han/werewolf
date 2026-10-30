// Shared domain vocabulary for the host half of the plugin.
// The browser half carries its own display dictionary; this file is the single
// source of truth for board shape, Chinese role labels, and bot pacing.

export const BOARDS = {
  1: {
    id: 1,
    name: "9 人预女猎",
    short: "9 人",
    players: 9,
    tag: "经典",
    pitch: "3 狼 · 预言家 · 女巫 · 猎人 · 3 平民",
    description: "节奏最快的一板。三狼屠边，神职齐全，适合第一次上手。",
  },
  2: {
    id: 2,
    name: "12 人预女猎守 + 狼枪",
    short: "12 人",
    players: 12,
    tag: "进阶",
    pitch: "3 狼 · 狼枪 · 预言家 · 女巫 · 猎人 · 守卫 · 4 平民",
    description: "多一个守卫和一把狼枪，夜间博弈更厚，白天容错更低。",
  },
  3: {
    id: 3,
    name: "12 人通灵机械狼",
    short: "12 人",
    players: 12,
    tag: "变体",
    pitch: "3 狼 · 机械狼 · 通灵师 · 女巫 · 猎人 · 守卫 · 4 平民",
    description: "通灵师看具体身份，机械狼不与狼队见面、能偷学技能。",
  },
};

export const ROLE_LABELS = {
  Werewolf: "狼人",
  WolfGun: "狼枪",
  MechanicWolf: "机械狼",
  Seer: "预言家",
  Psychic: "通灵师",
  Witch: "女巫",
  Hunter: "猎人",
  Guardian: "守卫",
  Civilian: "平民",
};

export const FACTION_LABELS = {
  Wolf: "狼人阵营",
  Town: "好人阵营",
};

export const ROLE_DESCRIPTIONS = {
  Werewolf: "每晚与狼队共同选择击杀目标，白天隐藏身份、带偏好人。",
  WolfGun: "狼人阵营。出局后可以开枪带走一名玩家，是狼队的最后一颗子弹。",
  MechanicWolf:
    "狼人阵营，但不与狼队见面。全局可学习一名玩家的身份，学到的技能从下一晚起生效。",
  Seer: "每晚查验一名玩家是好人还是狼人，是好人阵营最核心的信息位。",
  Psychic: "每晚得知一名玩家的具体身份。机械狼在通灵师眼里仍显示为机械狼。",
  Witch: "拥有一瓶解药和一瓶毒药，各只能用一次。本板不可自救。",
  Hunter: "出局时可以开枪带走一名玩家；被毒杀时无法开枪。",
  Guardian: "每晚守护一名玩家，不能连续两晚守护同一人。",
  Civilian: "没有夜间技能。你的武器是发言、逻辑和投票。",
};

export const SPEEDS = {
  calm: { id: "calm", label: "悠闲", botDelay: 1300 },
  normal: { id: "normal", label: "正常", botDelay: 650 },
  fast: { id: "fast", label: "极速", botDelay: 120 },
};

export const DEFAULT_OPTIONS = {
  board: 1,
  humanSeat: 1,
  speed: "normal",
  godView: false,
  wolfChatRounds: 2,
  interrupts: false,
};

export function boardInfo(id) {
  return BOARDS[id] || BOARDS[1];
}

export function roleLabel(role) {
  return ROLE_LABELS[role] || role || "未知";
}

export function factionLabel(faction) {
  return FACTION_LABELS[faction] || faction || "";
}

export function normalizeOptions(input) {
  const board = Number(input?.board);
  const info = boardInfo(board);
  const humanSeatRaw = Number(input?.humanSeat);
  const humanSeat =
    Number.isInteger(humanSeatRaw) && humanSeatRaw >= 0
      ? Math.min(humanSeatRaw, info.players)
      : DEFAULT_OPTIONS.humanSeat;
  const speed = SPEEDS[input?.speed] ? input.speed : DEFAULT_OPTIONS.speed;

  return {
    board: info.id,
    humanSeat,
    speed,
    godView: Boolean(input?.godView),
    wolfChatRounds:
      Number.isInteger(input?.wolfChatRounds) && input.wolfChatRounds >= 0
        ? Math.min(input.wolfChatRounds, 5)
        : DEFAULT_OPTIONS.wolfChatRounds,
    interrupts: Boolean(input?.interrupts),
  };
}
