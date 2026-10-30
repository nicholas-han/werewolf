import { BOARDS } from "./constants.js";

const ACTIONS = ["state", "new", "reply", "auto", "speed", "abort"];

function listBoardsText() {
  return Object.values(BOARDS)
    .map((board) => `- ${board.id}: ${board.name} (${board.players} seats) — ${board.pitch}`)
    .join("\n");
}

function pendingText(ask) {
  if (!ask) return null;
  const candidates = ask.candidates
    .map((entry) =>
      entry.name && entry.name !== `P${entry.seat}`
        ? `P${entry.seat} ${entry.name}`
        : `P${entry.seat}`,
    )
    .join(", ");
  const parts = [
    `Pending ask #${ask.id}: ${ask.kind} (${ask.qtype}) — ${ask.prompt}`,
  ];
  if (candidates) parts.push(`Candidates: ${candidates}`);
  if (ask.allowSkip) parts.push("You may skip / abstain.");
  if (ask.qtype === "confirm") parts.push("Reply with decision: true or false.");
  if (ask.qtype === "speak") parts.push("Reply with text (empty string = pass).");
  if (ask.qtype === "choose") parts.push("Reply with choice: <seat> or null to skip.");
  return parts.join("\n");
}

function stateText(state) {
  if (state.status === "idle") {
    return `No Werewolf game is running. Boards:\n${listBoardsText()}`;
  }

  const lines = [
    `${state.board.name} · ${state.status} · Day ${state.day} ${state.phase}`,
  ];
  if (state.sessionId) lines.push(`Session id: ${state.sessionId}`);

  if (state.self) {
    const tags = [state.self.roleName || "unknown", state.self.factionName || ""].filter(Boolean);
    lines.push(
      `Your seat: P${state.self.seat} — ${tags.join(" / ")} (${state.self.alive ? "alive" : "out"}${state.self.sheriff ? ", sheriff" : ""})`,
    );
    if (state.self.teammates.length) {
      lines.push(`Wolf teammates: ${state.self.teammates.map((seat) => `P${seat}`).join(", ")}`);
    }
    if (state.self.notes.length) lines.push(`Your notes: ${state.self.notes.join(" | ")}`);
  } else {
    lines.push("You are spectating (no human seat).");
  }

  lines.push(
    `Players: ${state.players
      .map(
        (player) =>
          `P${player.seat}${player.alive ? "" : "(out)"}${player.sheriff ? "(sheriff)" : ""}${
            player.isSelf ? "(you)" : ""
          }${player.role ? `[${player.roleName}]` : ""}`,
      )
      .join(" ")}`,
  );

  const pending = pendingText(state.pendingAsk);
  if (pending) lines.push(pending);
  else if (state.thinkingSeat) lines.push(`P${state.thinkingSeat} is thinking…`);

  if (state.status === "finished") {
    lines.push(`Result: ${state.resultText || state.result}`);
    lines.push(
      `Final roles: ${state.players
        .map((player) => `P${player.seat}=${player.roleName || "?"}`)
        .join(", ")}`,
    );
  }

  if (state.events?.length) {
    lines.push("Recent events:");
    const recent = state.events.slice(-30);
    for (const event of recent) {
      const tag =
        event.vis === "private" ? "private" : event.vis === "moderator" ? "god" : "public";
      lines.push(`- (${tag}) ${event.text}`);
    }
    if (state.events.length > recent.length) {
      lines.push(`… ${state.events.length - recent.length} earlier event(s) omitted.`);
    }
  }

  if (state.error) lines.push(`Error: ${state.error}`);
  return lines.join("\n");
}

function toolResult(state) {
  return {
    ok: true,
    status: state.status,
    sessionId: state.sessionId,
    board: state.board,
    config: state.config,
    self: state.self,
    players: state.players,
    day: state.day,
    phase: state.phase,
    pendingAsk: state.pendingAsk,
    thinkingSeat: state.thinkingSeat,
    result: state.result,
    resultText: state.resultText,
    canAnswer: state.canAnswer,
    events: state.events,
    text: stateText(state),
  };
}

function requireSessionId(args) {
  if (typeof args?.sessionId !== "string" || args.sessionId.length === 0) {
    throw new Error("Pass the sessionId from the state call that showed pendingAsk.");
  }
  return args.sessionId;
}

function requireAskId(args) {
  if (!Number.isInteger(args?.askId)) {
    throw new Error("Pass the pendingAsk.id from the state call you are answering.");
  }
  return args.askId;
}

function executeWerewolf(runtime, args) {
  const action = typeof args?.action === "string" ? args.action : "state";

  switch (action) {
    case "state":
      return toolResult(runtime.snapshot());
    case "new": {
      const board = Number(args.board);
      if (args.board !== undefined && !BOARDS[board]) {
        throw new Error("board must be 1, 2, or 3.");
      }
      runtime.startGame({
        board: args.board ?? runtime.options.board,
        humanSeat: args.humanSeat ?? runtime.options.humanSeat,
        speed: args.speed ?? runtime.options.speed,
        godView: args.godView ?? runtime.options.godView,
      });
      return {
        ...toolResult(runtime.snapshot()),
        text: `Started a new game.\n\n${listBoardsText()}\n\n${stateText(runtime.snapshot())}`,
      };
    }
    case "reply": {
      const state = runtime.snapshot();
      if (!state.pendingAsk) {
        throw new Error("There is no pending ask for your seat right now.");
      }
      const sessionId = requireSessionId(args);
      const askId = requireAskId(args);
      const payload = {};
      if (state.pendingAsk.qtype === "choose") payload.choice = args.choice ?? null;
      else if (state.pendingAsk.qtype === "confirm") payload.decision = Boolean(args.decision);
      else if (state.pendingAsk.qtype === "speak") payload.text = args.text ?? "";
      runtime.answer({ ...payload, sessionId, askId });
      return toolResult(runtime.snapshot());
    }
    case "auto": {
      const state = runtime.snapshot();
      if (!state.pendingAsk) {
        throw new Error("There is no pending ask for your seat right now.");
      }
      runtime.autoAnswer({
        sessionId: requireSessionId(args),
        askId: requireAskId(args),
      });
      return toolResult(runtime.snapshot());
    }
    case "speed":
      runtime.setSpeed(args.speed || "normal");
      return toolResult(runtime.snapshot());
    case "abort":
      runtime.abort();
      return toolResult(runtime.snapshot());
    default:
      throw new Error(`Unknown action: ${action}. Use one of ${ACTIONS.join(", ")}.`);
  }
}

function output() {
  return {
    schema: { type: "object" },
    render: (_args, value) => [{ type: "text", text: value?.text || JSON.stringify(value) }],
  };
}

const PARAMETERS = {
  type: "object",
  properties: {
    action: {
      type: "string",
      enum: ACTIONS,
      description:
        "state = read the live table and any pending ask; new = start a game; reply = answer the pending ask for your seat; auto = let the AI answer the pending ask; speed = change bot pacing; abort = stop the current game.",
    },
    board: {
      type: "integer",
      enum: [1, 2, 3],
      description: "Board for new: 1 = 9-player seer/witch/hunter, 2 = 12-player guard + wolf gun, 3 = 12-player psychic/mechanic wolf.",
    },
    humanSeat: {
      type: "integer",
      minimum: 0,
      maximum: 12,
      description: "Your seat for new (1..N). 0 = spectate with all AI seats.",
    },
    speed: {
      type: "string",
      enum: ["calm", "normal", "fast"],
      description: "Bot pacing for new/speed.",
    },
    sessionId: {
      type: "string",
      description:
        "Required for reply/auto: the sessionId returned by the state call that showed pendingAsk.",
    },
    askId: {
      type: "integer",
      description:
        "Required for reply/auto: the pendingAsk.id returned by the state call you are answering.",
    },
    godView: {
      type: "boolean",
      description: "Spectator-only role reveal. Has no effect when humanSeat is 1..N.",
    },
    choice: {
      type: ["integer", "null"],
      description: "Seat number for a choose ask; null = skip/abstain when the engine allows it.",
    },
    decision: {
      type: "boolean",
      description: "Yes/no answer for a confirm ask.",
    },
    text: {
      type: "string",
      description: "Speech text for a speak ask; empty string = pass.",
    },
  },
  required: ["action"],
  additionalProperties: false,
};

export function registerWerewolfTools(ctx, runtime) {
  ctx.effect(
    () =>
      ctx.tools.register({
        name: "werewolf",
        description:
          "Read or play the live Werewolf game in the Harness Werewolf panel. The panel and the agent share one table. Hidden roles of other players are not exposed before the game ends, unless you explicitly start a spectator game with godView=true. Use action=state first; when the state contains pendingAsk, answer it with action=reply (choice/decision/text) and pass the sessionId plus pendingAsk.id from that state call, or action=auto with the same identifiers to let the AI seat play that step. The result includes the visibility-filtered event log.",
        parameters: PARAMETERS,
        output: output(),
        timeoutMs: 30000,
        isConcurrencySafe: () => false,
        execute: async (args) => executeWerewolf(runtime, args),
      }),
    "werewolf: tool",
  );
}
