import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

import { boardInfo, DEFAULT_OPTIONS, normalizeOptions, SPEEDS } from "./constants.js";
import { GameSession } from "./game.js";

export function defaultDataDirectory() {
  const home = process.env.DSH_HOME || join(homedir(), ".dsh");
  return join(home, "werewolf");
}

class SettingsStore {
  constructor(directory) {
    this.directory = directory;
    this.file = join(directory, "settings.json");
  }

  read() {
    try {
      if (!existsSync(this.file)) return null;
      return JSON.parse(readFileSync(this.file, "utf8"));
    } catch {
      return null;
    }
  }

  write(value) {
    try {
      mkdirSync(this.directory, { recursive: true });
      const temporary = `${this.file}.tmp`;
      writeFileSync(temporary, JSON.stringify(value, null, 2), "utf8");
      renameSync(temporary, this.file);
    } catch (error) {
      console.warn(`[werewolf] cannot save settings: ${error.message}`);
    }
  }
}

function idleSnapshot(options, revision = 0) {
  const board = boardInfo(options.board);
  return {
    revision,
    status: "idle",
    buildMessage: null,
    error: null,
    board,
    config: {
      board: board.id,
      humanSeat: options.humanSeat,
      speed: options.speed,
      speedLabel: SPEEDS[options.speed]?.label || SPEEDS.normal.label,
      godView: Boolean(options.godView),
      seed: null,
    },
    self: null,
    players: Array.from({ length: board.players }, (_, index) => ({
      seat: index + 1,
      name: `P${index + 1}`,
      alive: true,
      sheriff: false,
      isSelf: index + 1 === options.humanSeat,
      isTeammate: false,
      role: null,
      faction: null,
      roleName: null,
    })),
    day: 1,
    phase: "Night",
    sheriffSeat: null,
    pendingAsk: null,
    thinkingSeat: null,
    result: null,
    resultText: null,
    events: [],
    eventCount: 0,
    startedAt: null,
    finishedAt: null,
    canAnswer: false,
  };
}

export class WerewolfRuntime {
  constructor({ root, dataDir } = {}) {
    this.root = root;
    this.store = new SettingsStore(dataDir || defaultDataDirectory());
    this.options = normalizeOptions({ ...DEFAULT_OPTIONS, ...(this.store.read() || {}) });
    this.session = null;
    this.revision = 0;
  }

  snapshot() {
    if (this.session) return this.session.snapshot();
    return idleSnapshot(this.options, this.revision);
  }

  optionsForNew(input = {}) {
    const merged = normalizeOptions({ ...this.options, ...input });
    if (input?.seed !== undefined && input.seed !== null) {
      const seed = Number(input.seed);
      if (Number.isInteger(seed) && seed > 0) merged.seed = seed;
    }
    return merged;
  }

  startGame(input = {}) {
    if (this.session) this.session.abort("已开始新的一局。");

    this.options = this.optionsForNew(input);
    this.store.write({
      board: this.options.board,
      humanSeat: this.options.humanSeat,
      speed: this.options.speed,
      godView: this.options.godView,
      wolfChatRounds: this.options.wolfChatRounds,
      interrupts: this.options.interrupts,
    });

    this.session = new GameSession({
      root: this.root,
      options: this.options,
      onUpdate: () => {
        this.revision += 1;
      },
    });
    const state = this.session.start();
    this.revision += 1;
    return state;
  }

  answer(input) {
    if (!this.session) throw new Error("还没有开始对局。");
    const state = this.session.answer(input);
    this.revision += 1;
    return state;
  }

  autoAnswer(input = {}) {
    if (!this.session) throw new Error("还没有开始对局。");
    const state = this.session.autoAnswer(input);
    this.revision += 1;
    return state;
  }

  setSpeed(speed) {
    if (this.session) this.session.setSpeed(speed);
    const next = SPEEDS[speed];
    if (next) {
      this.options.speed = next.id;
      this.store.write({
        board: this.options.board,
        humanSeat: this.options.humanSeat,
        speed: this.options.speed,
        godView: this.options.godView,
        wolfChatRounds: this.options.wolfChatRounds,
        interrupts: this.options.interrupts,
      });
    }
    this.revision += 1;
    return this.snapshot();
  }

  abort() {
    if (this.session) this.session.abort("已结束本局。");
    this.revision += 1;
    return this.snapshot();
  }

  dispose() {
    if (this.session) this.session.dispose();
    this.session = null;
  }
}
