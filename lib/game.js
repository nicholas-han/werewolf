import { randomInt, randomUUID } from "node:crypto";
import { createInterface } from "node:readline";

import { BotBrain } from "./brain.js";
import { boardInfo, factionLabel, roleLabel, SPEEDS } from "./constants.js";
import { ensureEngine, spawnEngine } from "./engine.js";

const ROLE_SET = new Set([
  "Werewolf",
  "WolfGun",
  "MechanicWolf",
  "Seer",
  "Psychic",
  "Witch",
  "Hunter",
  "Guardian",
  "Civilian",
]);

const WOLF_ROLES = new Set(["Werewolf", "WolfGun", "MechanicWolf"]);

// `thinkingSeat` is only safe for asks whose actor is already public. Night
// abilities (wolf knife, seer check, witch potions, guard, hunter shot, learn…)
// would otherwise pair a seat with the just-narrated "seer opens eyes" cue and
// reveal a role. Wolf chat is private too. Spectator god view bypasses this.
const PUBLIC_THINKING_KINDS = new Set([
  "Statement",
  "LastWords",
  "Vote",
  "RunoffVote",
  "RunForSheriff",
  "Withdraw",
  "SheriffVote",
  "ConsolidateSingle",
  "BallotTarget",
  "SpeechDirection",
]);

const ASK_TITLES = {
  NightKill: "狼队选择刀口",
  SelfDestruct: "是否自爆",
  Vote: "放逐投票",
  RunoffVote: "放逐决胜投票",
  Inspect: "预言家查验",
  Guard: "守卫守护",
  WitchSave: "女巫解药",
  WitchPoison: "女巫毒药",
  HunterShot: "猎人开枪",
  MechanicLearn: "机械狼学习",
  MechanicBigKnife: "机械狼大刀",
  RunForSheriff: "是否上警",
  Withdraw: "是否退水",
  SheriffVote: "警长投票",
  ConsolidateSingle: "警长归票",
  BallotTarget: "警长投票目标",
  BadgeTransfer: "警徽移交",
  SpeechDirection: "发言方向",
};

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function clampText(text, max = 800) {
  const value = String(text ?? "");
  return value.length > max ? `${value.slice(0, max)}…` : value;
}

function roleOfSet(value) {
  return ROLE_SET.has(value) ? value : null;
}

function wolfRole(role) {
  return WOLF_ROLES.has(role);
}

function askTitle(kind) {
  return ASK_TITLES[kind] || "等待你的决定";
}

export function isPublicThinkingAsk(ask) {
  return Boolean(ask) && PUBLIC_THINKING_KINDS.has(ask.kind);
}

export class GameSession {
  constructor({ root, options, onUpdate } = {}) {
    this.root = root;
    this.onUpdate = onUpdate;
    this.options = options;
    this.board = boardInfo(options.board);
    this.humanSeat = Number(options.humanSeat) || 0;
    this.speed = SPEEDS[options.speed] || SPEEDS.normal;
    this.botDelay = this.speed.botDelay;

    this.status = "idle";
    this.sessionId = randomUUID();
    this.revision = 0;
    this.error = null;
    this.buildMessage = null;
    this.seed = null;
    this.startedAt = null;
    this.finishedAt = null;

    this.roster = [];
    this.players = new Map(); // seat -> { seat, name, alive, sheriff }
    this.truthRoles = new Map(); // seat -> role
    this.truthFactions = new Map(); // seat -> faction
    this.teammates = new Map(); // seat -> Set of teammate seats
    this.brains = new Map();

    this.rawEvents = [];
    this.selfNotes = [];
    this.day = 1;
    this.phase = "Night";
    this.result = null;
    this.sheriffSeat = null;

    this.pendingAsk = null;
    this.pendingResolve = null;
    this.thinkingSeat = null;

    this.aborted = false;
    this.process = null;
    this.reader = null;
    this.stderr = "";
    this.loopDone = null;

    this.plannedKillDay = null;
    this.plannedKillSeat = null;
    this.plannedVoteDay = null;
    this.plannedVoteSeat = null;
    this.sheriffCandidates = [];
    this.wolfChatAsked = 0;
    this.wolfChatDay = null;
    this.fakeSeerSeat = null;
    this.fakeSeerDay = null;
    this.claimedSeerSeats = new Set();
  }

  touch() {
    this.revision += 1;
    this.onUpdate?.(this);
  }

  // --- lifecycle ---------------------------------------------------------

  start() {
    this.status = "starting";
    this.startedAt = Date.now();
    this.touch();
    this.loopDone = this.run().catch((error) => {
      if (this.aborted) return;
      this.status = "error";
      this.error = error instanceof Error ? error.message : String(error);
      this.finishedAt = Date.now();
      this.touch();
      try {
        this.process?.kill("SIGTERM");
      } catch {
        // already gone
      }
    });
    return this.snapshot();
  }

  async run() {
    const engine = await ensureEngine({
      root: this.root,
      onProgress: (progress) => {
        if (this.aborted) return;
        if (progress.stage === "building") {
          this.status = "building";
          this.buildMessage = progress.message;
          this.touch();
        }
      },
    });

    if (this.aborted) return;

    this.seed =
      Number.isInteger(this.options.seed) && this.options.seed > 0
        ? this.options.seed
        : randomInt(1, 2 ** 31 - 1);

    this.process = spawnEngine(engine, {
      board: this.board.id,
      seed: this.seed,
      askTimeout: 0,
      wolfChatRounds: this.options.wolfChatRounds,
      interrupts: this.options.interrupts,
    });

    this.status = "running";
    this.buildMessage = null;
    this.touch();

    this.process.stderr.on("data", (chunk) => {
      this.stderr = `${this.stderr}${chunk.toString("utf8")}`.slice(-8_000);
    });
    this.process.stdin.on("error", () => {
      // The engine can close its pipe while the UI is tearing down a game.
    });

    this.process.on("error", (error) => {
      if (this.aborted || this.status === "finished") return;
      this.status = "error";
      this.error = error.message;
      this.finishedAt = Date.now();
      this.touch();
    });

    this.process.on("close", (code) => {
      if (this.status === "finished" || this.aborted) return;
      if (this.status === "error") return;
      const detail = this.stderr.trim().split("\n").slice(-4).join("\n");
      this.status = "error";
      this.error = `规则引擎意外退出（exit ${code}）${detail ? `\n${detail}` : ""}`;
      this.finishedAt = Date.now();
      this.touch();
    });

    this.reader = createInterface({ input: this.process.stdout, crlfDelay: Infinity });
    for await (const line of this.reader) {
      if (this.aborted) break;
      const trimmed = line.trim();
      if (!trimmed) continue;

      let message;
      try {
        message = JSON.parse(trimmed);
      } catch {
        continue;
      }

      try {
        await this.handleMessage(message);
      } catch (error) {
        if (this.aborted) break;
        this.status = "error";
        this.error = error instanceof Error ? error.message : String(error);
        this.finishedAt = Date.now();
        this.touch();
        try {
          this.process?.kill("SIGTERM");
        } catch {
          // already gone
        }
        break;
      }
    }

    try {
      this.reader.close();
    } catch {
      // already closed
    }

    // A clean engine always ends with game_over. If the stream closed first
    // (crash, kill, truncated pipe), never leave the UI showing "running".
    if (!this.aborted && this.status === "running" && this.result === null) {
      this.status = "error";
      this.error = "规则引擎在给出终局前结束。";
      this.finishedAt = Date.now();
      this.touch();
    }
  }

  async handleMessage(message) {
    switch (message.t) {
      case "game_start":
        this.handleGameStart(message);
        break;
      case "event":
        this.handleEvent(message);
        break;
      case "ask":
        await this.handleAsk(message);
        break;
      case "game_over":
        this.result = message.result || null;
        this.status = "finished";
        this.finishedAt = Date.now();
        this.pendingAsk = null;
        this.thinkingSeat = null;
        this.touch();
        break;
      default:
        break;
    }
  }

  handleGameStart(message) {
    this.seed = Number(message.seed) || this.seed;
    this.roster = (message.seats || []).map((entry) => ({
      seat: Number(entry.seat),
      name: String(entry.name || `P${entry.seat}`),
    }));
    for (const entry of this.roster) {
      this.players.set(entry.seat, {
        seat: entry.seat,
        name: entry.name,
        alive: true,
        sheriff: false,
      });
    }
    this.touch();
  }

  handleEvent(event) {
    const seq = this.rawEvents.length + 1;
    const stored = { ...event, seq };
    this.rawEvents.push(stored);

    if (event.day !== undefined) this.day = event.day;
    if (event.phase) this.phase = event.phase;

    if (event.etype === "deal") this.handleDeal(event);
    this.updatePublicFacts(event);

    // Route the event to seat-local views. Public -> everyone; private -> the
    // addressee only; moderator -> nobody (the host may use it for the reveal).
    if (event.vis === "public") {
      for (const brain of this.brains.values()) brain.observe(event);
    } else if (event.vis === "private" && event.seat !== undefined) {
      const brain = this.ensureBrain(Number(event.seat));
      brain?.observe(event);
      if (Number(event.seat) === this.humanSeat) this.captureSelfNote(event);
    }

    if (event.vis === "public" && event.etype === "speech") {
      this.maybeArmFakeSeer(event);
    }
    if (event.vis === "public") this.updateElectionFacts(event);

    this.touch();
  }

  handleDeal(event) {
    const seat = Number(event.seat);
    if (!seat) return;
    const data = event.data || {};
    const role = roleOfSet(data.role);
    const faction = data.faction === "Wolf" ? "Wolf" : data.faction === "Town" ? "Town" : null;
    if (role) this.truthRoles.set(seat, role);
    if (faction) this.truthFactions.set(seat, faction);
    if (Array.isArray(data.teammates)) {
      this.teammates.set(
        seat,
        new Set(data.teammates.map(Number).filter((value) => value !== seat)),
      );
    }

    const brain = this.ensureBrain(seat);
    if (brain && role) {
      brain.observe(event);
    }
  }

  ensureBrain(seat) {
    if (this.brains.has(seat)) return this.brains.get(seat);
    const role = this.truthRoles.get(seat);
    if (!role) return null;
    const faction = this.truthFactions.get(seat) || (wolfRole(role) ? "Wolf" : "Town");
    const brain = new BotBrain({
      seat,
      role,
      faction,
      teammates: [...(this.teammates.get(seat) || [])],
      seed: this.seed || 1,
    });
    this.brains.set(seat, brain);
    return brain;
  }

  updatePublicFacts(event) {
    if (event.vis !== "public") return;
    const text = event.text || "";

    if (event.etype === "narration") {
      for (const match of text.matchAll(/(P\d+)\s*出局/g)) {
        const seat = Number(match[1].slice(1));
        const player = this.players.get(seat);
        if (player) player.alive = false;
      }

      const elected = /(P\d+)\s*当选警长/.exec(text) || /仅\s*(P\d+)\s*一人竞选/.exec(text);
      if (elected) this.setSheriff(Number(elected[1].slice(1)));
      if (/警徽被撕毁|警徽流失|本局无警长|全员退水|无人上警/.test(text)) this.setSheriff(null);
      const transfer = /警徽移交给\s*(P\d+)/.exec(text);
      if (transfer) this.setSheriff(Number(transfer[1].slice(1)));
    }
  }

  updateElectionFacts(event) {
    if (event.etype !== "narration") return;
    const text = event.text || "";
    const runners = /上警竞选警长[：:]\s*(.*)$/.exec(text);
    if (runners) {
      this.sheriffCandidates = [...runners[1].matchAll(/P(\d+)/g)].map((match) =>
        Number(match[1]),
      );
    }
    if (/无人上警|警徽流失|本局无警长|全员退水/.test(text)) {
      this.sheriffCandidates = [];
    }
  }

  setSheriff(seat) {
    this.sheriffSeat = seat;
    for (const player of this.players.values()) {
      player.sheriff = seat !== null && player.seat === seat;
    }
  }

  captureSelfNote(event) {
    if (event.etype === "result_private") {
      const text = clampText(event.text, 240);
      if (text && !this.selfNotes.includes(text)) this.selfNotes.push(text);
    } else if (event.etype === "narration") {
      const text = clampText(event.text, 240);
      if (
        text &&
        !this.selfNotes.includes(text) &&
        /^【(女巫|狼队|机械狼)】/.test(text)
      ) {
        this.selfNotes.push(text);
      }
    } else if (event.etype === "deal") {
      const teammates = event.data?.teammates;
      if (Array.isArray(teammates) && teammates.length > 0) {
        const text = `你的狼队友：${teammates.map((seat) => `P${seat}`).join("、")}`;
        if (!this.selfNotes.includes(text)) this.selfNotes.push(text);
      }
    }
  }

  maybeArmFakeSeer(event) {
    if (this.fakeSeerSeat !== null) return;
    if (this.day < 2) return;
    const speaker =
      Number(event.data?.speaker) ||
      Number((/^P(\d+)/.exec(event.text || "") || [])[1]) ||
      0;
    if (!speaker) return;
    if (
      !/(?:我|本人|这里)\s*(?:是|跳|跳个|跳一张|跳的是)?\s*预言家/.test(event.text || "")
    ) {
      return;
    }
    const wolfSeat = this.lowestOpenWolfSeat((seat) => seat !== this.humanSeat);
    if (wolfSeat === null) return;
    const brain = this.brains.get(wolfSeat);
    if (!brain) return;

    // Coordinate only from the responding wolf's own knowledge. The pack knows
    // its open teammates, but not the hidden mechanic wolf (board 3); a claim by
    // the mechanic must look exactly as suspicious as a civilian's claim here.
    const knownTeammates = this.teammates.get(wolfSeat) || new Set();
    if (speaker === wolfSeat || knownTeammates.has(speaker)) return;

    this.claimedSeerSeats.add(speaker);
    brain.fakeSeer = true;
    brain.fakeSeerTarget = speaker;
    this.fakeSeerSeat = wolfSeat;
    this.fakeSeerDay = this.day;
  }

  // --- asks --------------------------------------------------------------

  async handleAsk(ask) {
    this.pendingAsk = null;
    this.thinkingSeat = null;
    if (ask.day !== undefined) this.day = ask.day;
    if (ask.phase) this.phase = ask.phase;
    if (ask.kind === "WolfChat") {
      if (this.wolfChatDay !== this.day) {
        this.wolfChatDay = this.day;
        this.wolfChatAsked = 0;
      }
      this.wolfChatAsked += 1;
    }
    this.touch();

    let reply;
    if (this.isHumanAsk(ask)) {
      this.pendingAsk = this.describeAsk(ask);
      this.thinkingSeat = null;
      this.touch();

      reply = await new Promise((resolve) => {
        this.pendingResolve = resolve;
      });

      this.pendingResolve = null;
      this.pendingAsk = null;
      this.thinkingSeat = null;
      this.touch();
    } else {
      // Never identify the actor of a private night ask: it would leak roles.
      const revealActor =
        isPublicThinkingAsk(ask) || (this.options.godView && this.humanSeat === 0);
      this.thinkingSeat = revealActor ? Number(ask.seat) || null : null;
      this.touch();
      await sleep(this.botDelay);
      if (this.aborted) return;
      reply = this.autoReply(ask);
      this.thinkingSeat = null;
      this.touch();
    }

    if (this.aborted || !reply) return;
    this.sendReply(reply);
  }

  isHumanAsk(ask) {
    return this.humanSeat > 0 && Number(ask.seat) === this.humanSeat;
  }

  describeAsk(ask) {
    const candidates = Array.isArray(ask.candidates)
      ? ask.candidates.map((entry) => ({
          seat: Number(entry.seat),
          name: String(entry.name || `P${entry.seat}`),
        }))
      : [];
    return {
      id: Number(ask.id),
      sessionId: this.sessionId,
      seat: Number(ask.seat),
      qtype: ask.qtype,
      kind: ask.kind,
      title: askTitle(ask.kind),
      prompt: ask.prompt || "",
      candidates,
      allowSkip: Boolean(ask.allowSkip),
      day: ask.day ?? this.day,
      phase: ask.phase ?? this.phase,
    };
  }

  contextFor(ask) {
    const openWolfCount = this.openWolfSeats().length;
    let wolfKillTarget = this.contextWolfKill(ask);

    // Wolf chat happens *before* the knife ask. Plan the knife here so the pack
    // can actually talk about a concrete target; the NightKill asks reuse it.
    if (ask.kind === "WolfChat" && openWolfCount > 0) {
      if (this.plannedKillDay !== this.day || this.plannedKillSeat === undefined) {
        const candidates = [...this.players.values()]
          .filter((player) => player.alive)
          .map((player) => ({ seat: player.seat }));
        this.plannedKillDay = this.day;
        this.plannedKillSeat = this.pickWolfTarget(candidates, this.openWolfSeats());
      }
      wolfKillTarget = this.plannedKillSeat;
    }

    const askCandidates = Array.isArray(ask.candidates) ? ask.candidates : [];
    const speechCandidates =
      askCandidates.length > 0
        ? askCandidates
        : [...this.players.values()]
            .filter((player) => player.alive)
            .map((player) => ({ seat: player.seat, name: player.name }));

    return {
      day: this.day,
      openWolfCount,
      lowestOpenWolfSeat: this.lowestOpenWolfSeat(),
      sheriffCandidates: this.sheriffCandidates,
      candidateCount: askCandidates.length,
      candidates: speechCandidates,
      wolfKillTarget,
      wolfVoteTarget: this.contextWolfVote(ask),
      round: Math.ceil(this.wolfChatAsked / Math.max(openWolfCount, 1)),
      speakerIsFirst: this.wolfChatAsked % Math.max(openWolfCount, 1) === 1,
    };
  }

  contextWolfKill(ask) {
    if (ask.kind !== "NightKill") return undefined;
    const openWolves = this.openWolfSeats();
    if (openWolves.length === 0) return undefined; // lone mechanic acts alone

    if (this.plannedKillDay !== this.day || this.plannedKillSeat === undefined) {
      const candidates = ask.candidates || [];
      const target = this.pickWolfTarget(candidates, openWolves);
      this.plannedKillDay = this.day;
      this.plannedKillSeat = target;
    }
    return this.plannedKillSeat;
  }

  contextWolfVote(ask) {
    if (!["Vote", "RunoffVote", "BallotTarget"].includes(ask.kind)) return undefined;
    const openWolves = this.openWolfSeats();
    if (openWolves.length === 0) return undefined;
    if (this.plannedVoteDay !== this.day || this.plannedVoteSeat === undefined) {
      const target = this.pickWolfTarget(ask.candidates || [], openWolves);
      this.plannedVoteDay = this.day;
      this.plannedVoteSeat = target;
    }
    return this.plannedVoteSeat;
  }

  pickWolfTarget(candidates, openWolves) {
    const seats = (candidates || [])
      .map((entry) => Number(entry.seat ?? entry))
      .filter((seat) => Number.isInteger(seat));
    if (seats.length === 0) return null;

    // Only the open pack is known to the pack; the lone mechanic wolf is not.
    const wolfSeats = new Set(openWolves);
    const town = seats.filter((seat) => !wolfSeats.has(seat));
    const pool = town.length > 0 ? town : seats;

    const rep = this.brains.get(openWolves[0]);
    if (rep) {
      const target = rep.bestTownTarget(pool.map((seat) => ({ seat })));
      if (target !== null) return target;
    }
    return pool[0] ?? null;
  }

  autoReply(ask) {
    const seat = Number(ask.seat);
    let brain = this.ensureBrain(seat);
    if (!brain) {
      // Should not happen after the deal, but never leave the engine hanging.
      return this.fallbackReply(ask);
    }

    const context = this.contextFor(ask);
    try {
      if (ask.qtype === "choose") {
        let choice = brain.decideChoose(ask, context);

        // With a two-wolf pack where one seat is the human, a bot ballot that
        // disagrees with the human produces a tie (no knife). Let the human's
        // single ballot decide instead of silently nulling the night.
        if (
          ask.kind === "NightKill" &&
          seat !== this.humanSeat &&
          this.openWolfSeats().length === 2 &&
          this.openWolfSeats().includes(this.humanSeat) &&
          this.players.get(this.humanSeat)?.alive
        ) {
          choice = null;
        }
        const candidates = (ask.candidates || []).map((entry) => Number(entry.seat));
        if (choice === null || choice === undefined) {
          if (!ask.allowSkip) choice = candidates.find((value) => value !== seat) ?? candidates[0] ?? null;
        }
        if (choice !== null && !candidates.includes(Number(choice))) {
          choice = candidates.find((value) => value !== seat) ?? candidates[0] ?? null;
        }
        if (choice === null && !ask.allowSkip) choice = candidates[0] ?? null;
        return { t: "reply", id: ask.id, choice, reasoning: "bot" };
      }

      if (ask.qtype === "confirm") {
        return { t: "reply", id: ask.id, decision: Boolean(brain.decideConfirm(ask, context)) };
      }

      if (ask.qtype === "speak") {
        const text = clampText(brain.decideSpeak(ask, context), 600);
        return { t: "reply", id: ask.id, text, reasoning: "bot" };
      }
    } catch (error) {
      console.warn("[werewolf] bot decision failed:", error.message);
    }

    return this.fallbackReply(ask);
  }

  fallbackReply(ask) {
    if (ask.qtype === "confirm") return { t: "reply", id: ask.id, decision: false };
    if (ask.qtype === "speak") return { t: "reply", id: ask.id, text: "" };
    const candidates = (ask.candidates || []).map((entry) => Number(entry.seat));
    const choice = ask.allowSkip ? null : candidates[0] ?? null;
    return { t: "reply", id: ask.id, choice };
  }

  sendReply(reply) {
    if (!this.process || !this.process.stdin || this.process.stdin.destroyed) return;
    try {
      this.process.stdin.write(`${JSON.stringify(reply)}\n`);
    } catch {
      // The process may have exited on the last game_over.
    }
  }

  // --- human actions -----------------------------------------------------

  assertAskFresh(input, ask) {
    const session = input?.sessionId;
    if (typeof session !== "string" || session.length === 0) {
      throw new Error("缺少对局标识，请刷新后重试。");
    }
    if (session !== this.sessionId) {
      throw new Error("对局已经更换，请刷新后重试。");
    }
    const claimed = input?.askId;
    if (claimed === undefined || claimed === null || claimed === "") {
      throw new Error("缺少请求标识，请刷新后重试。");
    }
    if (!Number.isInteger(Number(claimed)) || Number(claimed) !== ask.id) {
      throw new Error("这个请求已经过期，请刷新后重试。");
    }
  }

  answer(input) {
    if (!this.pendingAsk) throw new Error("现在没有等待你回答的请求。");
    const ask = this.pendingAsk;
    this.assertAskFresh(input, ask);

    if (ask.qtype === "choose") {
      const raw = input?.choice;
      let choice = null;
      if (raw !== null && raw !== undefined && raw !== "") {
        choice = Number(raw);
        if (!Number.isInteger(choice)) throw new Error("请选择一名玩家。");
        if (!ask.candidates.some((entry) => entry.seat === choice)) {
          throw new Error("这名玩家不在候选列表中。");
        }
      }
      if (choice === null && !ask.allowSkip) throw new Error("这一步必须选择一名玩家。");
      this.resolvePending({ t: "reply", id: ask.id, choice, reasoning: "human" });
      return this.snapshot();
    }

    if (ask.qtype === "confirm") {
      const decision = Boolean(input?.decision);
      this.resolvePending({ t: "reply", id: ask.id, decision, reasoning: "human" });
      return this.snapshot();
    }

    if (ask.qtype === "speak") {
      const text = clampText(typeof input?.text === "string" ? input.text : "", 2_000);
      this.resolvePending({ t: "reply", id: ask.id, text, reasoning: "human" });
      return this.snapshot();
    }

    throw new Error("未知的请求类型。");
  }

  autoAnswer(input = {}) {
    if (!this.pendingAsk) throw new Error("现在没有等待你回答的请求。");
    const ask = this.pendingAsk;
    this.assertAskFresh(input, ask);
    const reply = this.autoReply({ ...ask, answersForHuman: true });
    this.resolvePending(reply);
    return this.snapshot();
  }

  resolvePending(reply) {
    const resolve = this.pendingResolve;
    this.pendingResolve = null;
    this.pendingAsk = null;
    if (resolve) resolve(reply);
    this.touch();
  }

  setSpeed(speed) {
    const next = SPEEDS[speed];
    if (next) {
      this.speed = next;
      this.botDelay = next.botDelay;
      this.options.speed = next.id;
      this.touch();
    }
  }

  abort(reason = "已重开一局。") {
    if (this.aborted) return;
    this.aborted = true;
    this.pendingAsk = null;
    this.thinkingSeat = null;
    const resolve = this.pendingResolve;
    this.pendingResolve = null;
    if (resolve) resolve(null);

    if (this.process && this.process.exitCode === null) {
      try {
        this.process.kill("SIGTERM");
      } catch {
        // already gone
      }
      const child = this.process;
      setTimeout(() => {
        // `child.killed` only says a signal was sent; a hung engine may still be
        // alive. Use exitCode to decide whether SIGKILL is needed.
        if (child.exitCode === null) {
          try {
            child.kill("SIGKILL");
          } catch {
            // already gone
          }
        }
      }, 1_500).unref?.();
    }

    if (this.status !== "finished") {
      // Back to the lobby: an aborted game has no result to show. Keeping the
      // session around preserves the board/config while snapshot() reports idle.
      this.status = "idle";
      this.error = null;
      this.finishedAt = Date.now();
    }
    this.touch();
  }

  // --- projections -------------------------------------------------------

  openWolfSeatsAll() {
    const seats = [];
    for (const [seat, role] of this.truthRoles) {
      if (wolfRole(role) && this.players.get(seat)?.alive) seats.push(seat);
    }
    return seats.sort((a, b) => a - b);
  }

  openWolfSeats() {
    return this.openWolfSeatsAll().filter((seat) => this.truthRoles.get(seat) !== "MechanicWolf");
  }

  lowestOpenWolfSeat(predicate = () => true) {
    for (const seat of this.openWolfSeats()) {
      if (predicate(seat)) return seat;
    }
    return null;
  }

  visibleEvents() {
    const god =
      this.status === "finished" || (this.options.godView && this.humanSeat === 0);
    const seenPrivate = new Set();

    return this.rawEvents
      .filter((event) => {
        if (god) return true;
        if (event.vis === "public") return true;
        if (event.vis === "private" && Number(event.seat) === this.humanSeat) return true;
        return false;
      })
      .filter((event) => {
        // The engine sends wolf chat and the wolf knife notice once per pack
        // member. After the reveal that would render the same line N times.
        if (event.vis !== "private") return true;
        const wolfChat =
          event.etype === "speech" && event.data?.kind === "WolfChat";
        const wolfKnife =
          event.etype === "narration" && (event.text || "").startsWith("【狼队】");
        if (!wolfChat && !wolfKnife) return true;
        const key = [
          event.etype,
          event.data?.kind ?? "",
          event.data?.speaker ?? "",
          event.day ?? "",
          event.phase ?? "",
          event.text ?? "",
        ].join("|");
        if (seenPrivate.has(key)) return false;
        seenPrivate.add(key);
        return true;
      })
      .map((event, index) => ({
        // Reindex after visibility filtering: raw engine sequence numbers reveal
        // how many private/moderator events happened between two public cues.
        seq: index + 1,
        type: event.etype,
        vis: event.vis,
        seat: event.seat,
        text: event.text,
        day: event.day,
        phase: event.phase,
        speaker: event.data?.speaker ?? null,
        kind: event.data?.kind ?? null,
        target: event.data?.target ?? null,
      }));
  }

  roleVisible(seat, role) {
    if (this.status === "finished") return true;
    if (this.options.godView && this.humanSeat === 0) return true;
    if (seat === this.humanSeat) return true;
    // A wolf sees *who* the pack is (dealt as teammate seats), not their exact
    // role card. Exact teammate roles wait for the reveal, matching the engine's
    // deal payload rather than leaking information the protocol never sent.
    return false;
  }

  isHumanWolf() {
    const role = this.truthRoles.get(this.humanSeat);
    return wolfRole(role);
  }

  snapshot() {
    const humanSeat = this.humanSeat;
    const humanRole = this.truthRoles.get(humanSeat) || null;
    const humanFaction = this.truthFactions.get(humanSeat) || (wolfRole(humanRole) ? "Wolf" : null);
    const teammates = this.teammates.get(humanSeat) || new Set();

    const players = this.roster.map((entry) => {
      const state = this.players.get(entry.seat);
      const role = this.truthRoles.get(entry.seat) || null;
      const faction = this.truthFactions.get(entry.seat) || (wolfRole(role) ? "Wolf" : null);
      const visibleRole = role && this.roleVisible(entry.seat, role) ? role : null;
      const isTeammate = this.isHumanWolf() && teammates.has(entry.seat);
      return {
        seat: entry.seat,
        name: entry.name,
        alive: state ? state.alive : true,
        sheriff: state ? state.sheriff : false,
        isSelf: entry.seat === humanSeat,
        isTeammate,
        role: visibleRole,
        faction: visibleRole ? faction : isTeammate ? "Wolf" : null,
        roleName: visibleRole ? roleLabel(visibleRole) : null,
      };
    });

    const self = humanSeat
      ? {
          seat: humanSeat,
          role: humanRole,
          roleName: humanRole ? roleLabel(humanRole) : null,
          faction: humanFaction,
          factionName: humanFaction ? factionLabel(humanFaction) : null,
          alive: this.players.get(humanSeat)?.alive ?? true,
          sheriff: this.players.get(humanSeat)?.sheriff ?? false,
          teammates: [...teammates],
          notes: [...this.selfNotes],
          isWolf: wolfRole(humanRole),
        }
      : null;

    const events = this.visibleEvents();

    return {
      revision: this.revision,
      sessionId: this.sessionId,
      status: this.status,
      buildMessage: this.buildMessage,
      error: this.error,
      board: this.board,
      config: {
        board: this.board.id,
        humanSeat: this.humanSeat,
        speed: this.speed.id,
        speedLabel: this.speed.label,
        godView: Boolean(this.options.godView),
        // Do not leak the deal seed while the game is live: randomDeal is
        // reproducible, so knowing it would reveal every seat's role.
        seed: this.status === "finished" ? this.seed : null,
      },
      self,
      players,
      day: this.day,
      phase: this.phase,
      sheriffSeat: this.sheriffSeat,
      pendingAsk: this.pendingAsk,
      thinkingSeat: this.thinkingSeat,
      result: this.result,
      resultText:
        this.result === "TownWins"
          ? "好人胜利"
          : this.result === "WolfWins"
            ? "狼人胜利"
            : this.result
              ? "对局结束"
              : null,
      events,
      eventCount: events.length,
      startedAt: this.startedAt,
      finishedAt: this.finishedAt,
      canAnswer: Boolean(this.pendingAsk),
    };
  }

  dispose() {
    this.abort("插件正在关闭。");
  }
}

export { askTitle };
