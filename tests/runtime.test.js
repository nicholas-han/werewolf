import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { packageRoot } from "../lib/engine.js";
import { isPublicThinkingAsk } from "../lib/game.js";
import { WerewolfRuntime } from "../lib/runtime.js";

function tempDir() {
  return mkdtempSync(join(tmpdir(), "werewolf-plugin-"));
}

async function waitFor(runtime, predicate, timeoutMs = 90000) {
  const started = Date.now();
  while (Date.now() - started < timeoutMs) {
    const state = runtime.snapshot();
    if (predicate(state)) return state;
    await new Promise((resolve) => setTimeout(resolve, 60));
  }
  throw new Error(`timed out; last status=${runtime.snapshot().status}`);
}

for (const board of [1, 2, 3]) {
  test(`all-AI game finishes on board ${board}`, { timeout: 150000 }, async () => {
    const dataDir = tempDir();
    const runtime = new WerewolfRuntime({ root: packageRoot, dataDir });
    try {
      runtime.startGame({ board, humanSeat: 0, speed: "fast", seed: 4242 + board });
      const state = await waitFor(runtime, (value) =>
        ["finished", "error", "aborted"].includes(value.status),
      );
      assert.equal(state.status, "finished", state.error || "");
      assert.ok(["TownWins", "WolfWins"].includes(state.result));
      assert.equal(state.players.length, board === 1 ? 9 : 12);
      assert.ok(state.players.every((player) => player.role), "all roles revealed at the end");
      assert.ok(state.events.length > 30);
      assert.ok(state.events.some((event) => event.type === "speech"));
    } finally {
      runtime.dispose();
      rmSync(dataDir, { recursive: true, force: true });
    }
  });
}

test("a human seat can answer every ask and reach a result", { timeout: 150000 }, async () => {
  const dataDir = tempDir();
  const runtime = new WerewolfRuntime({ root: packageRoot, dataDir });
  try {
    runtime.startGame({ board: 1, humanSeat: 1, speed: "fast", seed: 1234 });
    let answered = 0;

    for (;;) {
      const state = await waitFor(
        runtime,
        (value) =>
          Boolean(value.pendingAsk) ||
          ["finished", "error", "aborted"].includes(value.status),
        60000,
      );
      if (state.status !== "running") {
        assert.equal(state.status, "finished", state.error || "");
        break;
      }
      assert.ok(state.pendingAsk);
      runtime.autoAnswer({
        sessionId: state.sessionId,
        askId: state.pendingAsk.id,
      });
      answered += 1;
      assert.ok(answered < 100, "the game should not need 100 human asks");
    }

    assert.ok(answered > 0, "the human seat should be asked at least once");
    assert.ok(["TownWins", "WolfWins"].includes(runtime.snapshot().result));
  } finally {
    runtime.dispose();
    rmSync(dataDir, { recursive: true, force: true });
  }
});

test("only publicly observable asks expose a thinking seat", () => {
  for (const kind of [
    "NightKill",
    "Inspect",
    "Guard",
    "WitchSave",
    "WitchPoison",
    "HunterShot",
    "MechanicLearn",
    "MechanicBigKnife",
    "SelfDestruct",
    "WolfChat",
    "BadgeTransfer",
  ]) {
    assert.equal(isPublicThinkingAsk({ kind }), false, kind);
  }
  for (const kind of [
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
  ]) {
    assert.equal(isPublicThinkingAsk({ kind }), true, kind);
  }
});

test("rejects an answer that belongs to a stale ask", { timeout: 60000 }, async () => {
  const dataDir = tempDir();
  const runtime = new WerewolfRuntime({ root: packageRoot, dataDir });
  try {
    runtime.startGame({ board: 1, humanSeat: 1, speed: "fast", seed: 1234 });
    const state = await waitFor(
      runtime,
      (value) =>
        Boolean(value.pendingAsk) || ["finished", "error"].includes(value.status),
      60000,
    );
    assert.ok(state.pendingAsk, state.error || "");
    const askId = state.pendingAsk.id;
    const sessionId = state.sessionId;

    assert.throws(() => runtime.autoAnswer(), /缺少对局标识/);
    assert.throws(
      () => runtime.autoAnswer({ sessionId }),
      /缺少请求标识/,
    );
    assert.throws(
      () => runtime.autoAnswer({ sessionId: "another-session", askId }),
      /对局已经更换/,
    );
    assert.throws(
      () => runtime.autoAnswer({ sessionId, askId: askId + 1000 }),
      /过期/,
    );
    assert.equal(runtime.snapshot().pendingAsk?.id, askId);

    runtime.autoAnswer({ sessionId, askId });
    assert.equal(runtime.snapshot().pendingAsk, null);
  } finally {
    runtime.dispose();
    rmSync(dataDir, { recursive: true, force: true });
  }
});

test("hidden roles and moderator events stay hidden while playing", { timeout: 60000 }, async () => {
  const dataDir = tempDir();
  const runtime = new WerewolfRuntime({ root: packageRoot, dataDir });
  try {
    runtime.startGame({ board: 1, humanSeat: 2, speed: "fast", seed: 42 });
    const state = await waitFor(runtime, (value) => Boolean(value.self?.role), 20000);
    const visibleRoles = state.players.filter((player) => player.role);
    assert.ok(visibleRoles.length <= 1 + (state.self?.teammates?.length || 0));
    assert.ok(state.players.every((player) => player.isSelf || !player.role || player.isTeammate));
    assert.equal(state.events.filter((event) => event.vis === "moderator").length, 0);
    assert.equal(
      state.events.filter((event) => event.vis === "private").every((event) => event.seat === 2),
      true,
    );
    // A private night ask must not identify the acting seat.
    if (state.phase === "Night") assert.equal(state.thinkingSeat, null);
    // Filtered event indexes must be dense, and the count must describe the
    // visible stream rather than leaking private/moderator event volume.
    assert.deepEqual(
      state.events.map((event) => event.seq),
      state.events.map((_, index) => index + 1),
    );
    assert.equal(state.eventCount, state.events.length);
  } finally {
    runtime.dispose();
    rmSync(dataDir, { recursive: true, force: true });
  }
});

test("a spectator with god view sees every role", { timeout: 60000 }, async () => {
  const dataDir = tempDir();
  const runtime = new WerewolfRuntime({ root: packageRoot, dataDir });
  try {
    runtime.startGame({ board: 1, humanSeat: 0, speed: "calm", godView: true, seed: 9 });
    const state = await waitFor(
      runtime,
      (value) =>
        value.players.filter((player) => player.role).length >= 9 &&
        value.events.some((event) => event.vis === "moderator"),
      20000,
    );
    assert.equal(state.players.filter((player) => player.role).length, 9);
    assert.ok(state.events.some((event) => event.vis === "moderator"));
  } finally {
    runtime.dispose();
    rmSync(dataDir, { recursive: true, force: true });
  }
});

test("legacy starting a second game aborts the first cleanly", { timeout: 60000 }, async () => {
  const dataDir = tempDir();
  const runtime = new WerewolfRuntime({ root: packageRoot, dataDir });
  try {
    runtime.startGame({ board: 1, humanSeat: 1, speed: "fast", seed: 5 });
    await waitFor(runtime, (value) => value.status === "running", 30000);
    runtime.startGame({ board: 2, humanSeat: 3, speed: "fast", seed: 6 });
    const state = await waitFor(runtime, (value) => value.board?.id === 2, 30000);
    assert.equal(state.board.id, 2);
    assert.equal(state.config.humanSeat, 3);
  } finally {
    runtime.dispose();
    rmSync(dataDir, { recursive: true, force: true });
  }
});
