import test from "node:test";
import assert from "node:assert/strict";

import { BotBrain } from "../lib/brain.js";

function brainFor(overrides = {}) {
  return new BotBrain({
    seat: 1,
    role: "Seer",
    faction: "Town",
    teammates: [],
    seed: 7,
    ...overrides,
  });
}

test("seer trusts its own check and votes the checked wolf", () => {
  const brain = brainFor();
  brain.observe({
    vis: "private",
    seat: 1,
    etype: "result_private",
    text: "【查验结果】#3 是 狼人（查杀）",
    day: 1,
    phase: "Night",
    data: { kind: "Inspect", target: 3, isWolf: true },
  });

  const choice = brain.decideChoose({
    kind: "Vote",
    qtype: "choose",
    candidates: [
      { seat: 2, name: "P2" },
      { seat: 3, name: "P3" },
      { seat: 4, name: "P4" },
    ],
    allowSkip: true,
  });
  assert.equal(choice, 3);
});

test("a public seer claim carries a vote intent that other town seats follow", () => {
  const brain = brainFor({ seat: 4, role: "Civilian" });
  brain.observe({
    vis: "public",
    etype: "speech",
    text: "P2：我是预言家。第1天验 P3 是金水，今天我的票在 P6。",
    day: 1,
    phase: "Day",
    data: { speaker: 2, kind: "Statement" },
  });
  assert.equal(brain.seerVoteIntent(), 6);

  const choice = brain.decideChoose({
    kind: "Vote",
    qtype: "choose",
    candidates: [3, 5, 6, 7].map((seat) => ({ seat, name: `P${seat}` })),
    allowSkip: true,
  });
  assert.equal(choice, 6);
});

test("a wolf never chooses a teammate for the night kill", () => {
  const brain = brainFor({ seat: 1, role: "Werewolf", faction: "Wolf", teammates: [2, 3] });
  for (let index = 0; index < 40; index += 1) {
    const choice = brain.decideChoose({
      kind: "NightKill",
      qtype: "choose",
      candidates: [1, 2, 3, 4, 5, 6].map((seat) => ({ seat, name: `P${seat}` })),
      allowSkip: true,
    });
    assert.notEqual(choice, 2);
    assert.notEqual(choice, 3);
    assert.ok([4, 5, 6].includes(choice));
  }
});

test("witch save reads the knifed seat from the prompt", () => {
  const brain = brainFor({ seat: 4, role: "Witch" });
  brain.claims.set(6, "Seer");
  const save = brain.decideConfirm({
    kind: "WitchSave",
    qtype: "confirm",
    prompt: "今晚 P6 被刀，是否使用解药？",
  });
  assert.equal(save, true);

  const self = brain.decideConfirm({
    kind: "WitchSave",
    qtype: "confirm",
    prompt: "今晚 P4 被刀，是否使用解药？",
  });
  assert.equal(self, false);
});

test("every choose decision returns a legal candidate or an allowed skip", () => {
  const kinds = [
    "NightKill",
    "Vote",
    "RunoffVote",
    "Inspect",
    "Guard",
    "WitchPoison",
    "HunterShot",
    "MechanicLearn",
    "MechanicBigKnife",
    "SheriffVote",
    "BallotTarget",
    "BadgeTransfer",
  ];
  const candidates = [1, 2, 3, 4, 5].map((seat) => ({ seat, name: `P${seat}` }));

  for (const role of ["Seer", "Werewolf", "Witch", "Guardian", "Civilian"]) {
    for (const kind of kinds) {
      const brain = brainFor({
        seat: 3,
        role,
        faction: role === "Werewolf" ? "Wolf" : "Town",
        teammates: role === "Werewolf" ? [2] : [],
      });
      const choice = brain.decideChoose({
        kind,
        qtype: "choose",
        candidates,
        allowSkip: true,
      });
      if (choice !== null) {
        assert.ok(
          candidates.some((candidate) => candidate.seat === choice),
          `${role}/${kind} returned illegal seat ${choice}`,
        );
        if (kind !== "Guard") {
          assert.notEqual(choice, 3, `${role}/${kind} chose itself`);
        }
      }
    }
  }
});

test("guard can protect itself", () => {
  const guard = brainFor({ seat: 4, role: "Guardian", faction: "Town" });
  const choice = guard.decideChoose({
    kind: "Guard",
    qtype: "choose",
    candidates: [{ seat: 4, name: "P4" }],
    allowSkip: true,
  });
  assert.equal(choice, 4);
});

test("the mechanic wolf's sheriff decision ignores hidden pack seats", () => {
  const withPack = brainFor({
    seat: 4,
    role: "MechanicWolf",
    faction: "Wolf",
    teammates: [],
  });
  const withoutPack = brainFor({
    seat: 4,
    role: "MechanicWolf",
    faction: "Wolf",
    teammates: [],
  });
  const ask = { kind: "RunForSheriff", qtype: "confirm", prompt: "是否上警？" };
  assert.equal(
    withPack.decideConfirm(ask, { lowestOpenWolfSeat: 2 }),
    withoutPack.decideConfirm(ask, { lowestOpenWolfSeat: null }),
  );
});

test("a living open wolf still coordinates sheriff registration with its pack", () => {
  const wolf = brainFor({ seat: 3, role: "Werewolf", faction: "Wolf", teammates: [2] });
  const ask = { kind: "RunForSheriff", qtype: "confirm", prompt: "是否上警？" };
  assert.equal(wolf.decideConfirm(ask, { lowestOpenWolfSeat: 2 }), false);
  assert.equal(wolf.decideConfirm(ask, { lowestOpenWolfSeat: 3 }), true);
});

test("the seer's last words only name seats that exist on this board", () => {
  const seer = brainFor({ seat: 1, role: "Seer" });
  seer.knownChecks.set(2, { isWolf: false, day: 1 });
  seer.deaths.push(1);
  const text = seer.decideSpeak(
    { kind: "LastWords", qtype: "speak", prompt: "请发表遗言" },
    { candidates: [2, 3, 4, 5, 6, 7, 8, 9].map((seat) => ({ seat })) },
  );
  assert.doesNotMatch(text, /P1[0-2]/);
  assert.match(text, /好人继续出 P[2-9]/);
});

test("the seer announces a living checked wolf, not a dead historical one", () => {
  const seer = brainFor({ seat: 1, role: "Seer" });
  seer.knownChecks.set(3, { isWolf: true, day: 1 });
  seer.knownChecks.set(5, { isWolf: true, day: 2 });
  seer.observe({
    vis: "public",
    etype: "narration",
    text: "P3 出局",
    day: 2,
    phase: "Day",
  });
  const text = seer.decideSpeak(
    { kind: "Statement", qtype: "speak", prompt: "请发言" },
    { candidates: [{ seat: 2 }, { seat: 5 }, { seat: 6 }] },
  );
  assert.match(text, /今天我的票在 P5/);
  assert.doesNotMatch(text, /今天我的票在 P3/);
});

test("a later role declaration replaces the earlier claim", () => {
  const brain = brainFor({ seat: 4, role: "Civilian" });
  brain.observe({
    vis: "public",
    etype: "speech",
    text: "P2：我是平民，今天先听信息位。",
    day: 1,
    phase: "Day",
    data: { speaker: 2, kind: "Statement" },
  });
  assert.equal(brain.claims.get(2), "Civilian");

  brain.observe({
    vis: "public",
    etype: "speech",
    text: "P2：我是预言家，P3 查杀。",
    day: 2,
    phase: "Day",
    data: { speaker: 2, kind: "Statement" },
  });
  assert.equal(brain.claims.get(2), "Seer");
  assert.deepEqual(brain.seerClaimants(), [2]);
  assert.ok(
    brain.seerReports.some(
      (report) => report.seer === 2 && report.target === 3 && report.wolf,
    ),
  );
});

test("speech generation mentions the role and stays non-empty", () => {
  const seer = brainFor();
  seer.observe({
    vis: "private",
    seat: 1,
    etype: "result_private",
    data: { kind: "Inspect", target: 3, isWolf: true },
    text: "【查验结果】#3 是 狼人（查杀）",
    day: 1,
  });
  const statement = seer.decideSpeak({
    kind: "Statement",
    qtype: "speak",
    prompt: "请发言",
  });
  assert.match(statement, /预言家/);
  assert.match(statement, /P3/);

  const lastWords = seer.decideSpeak({
    kind: "LastWords",
    qtype: "speak",
    prompt: "请发表遗言",
  });
  assert.match(lastWords, /遗言/);
});
