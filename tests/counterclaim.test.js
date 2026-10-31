import test from "node:test";
import assert from "node:assert/strict";

import { GameSession } from "../lib/game.js";
import { packageRoot } from "../lib/engine.js";

function sessionWithPack() {
  const session = new GameSession({
    root: packageRoot,
    options: {
      board: 3,
      humanSeat: 1,
      speed: "fast",
      godView: false,
      wolfChatRounds: 2,
      interrupts: false,
    },
  });
  session.day = 2;
  for (const seat of [1, 2, 3, 4, 5]) {
    session.players.set(seat, { seat, name: `P${seat}`, alive: true, sheriff: false });
  }
  session.truthRoles.set(2, "Werewolf");
  session.truthRoles.set(3, "Werewolf");
  session.truthRoles.set(4, "MechanicWolf");
  session.truthRoles.set(5, "Civilian");
  // Open wolves 2 and 3 know each other; the mechanic wolf 4 is hidden from them.
  session.teammates.set(2, new Set([3]));
  session.teammates.set(3, new Set([2]));
  session.brains.set(2, { fakeSeer: false, fakeSeerTarget: null });
  return session;
}

function claim(session, speaker) {
  session.maybeArmFakeSeer({
    vis: "public",
    etype: "speech",
    text: `P${speaker}：我是预言家，P3 是查杀。`,
    day: 2,
    phase: "Day",
    data: { speaker },
  });
}

test("a known open teammate's seer claim does not arm another counterclaim", () => {
  const session = sessionWithPack();
  claim(session, 3);
  assert.equal(session.fakeSeerSeat, null);
  assert.equal(session.brains.get(2).fakeSeer, false);
});

test("the hidden mechanic wolf's seer claim is treated like any outsider claim", () => {
  const session = sessionWithPack();
  claim(session, 4);
  assert.equal(session.fakeSeerSeat, 2);
  assert.equal(session.brains.get(2).fakeSeer, true);
  assert.equal(session.brains.get(2).fakeSeerTarget, 4);
});

test("a civilian seer claim still arms a wolf counterclaim", () => {
  const session = sessionWithPack();
  claim(session, 5);
  assert.equal(session.fakeSeerSeat, 2);
  assert.equal(session.brains.get(2).fakeSeerTarget, 5);
});
