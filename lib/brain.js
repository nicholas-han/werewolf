// Heuristic in-process brains for the AI seats.
//
// A brain is deliberately *not* given the engine's GameState. It only sees what
// its seat is allowed to see: public events plus private events addressed to it.
// The runtime feeds those events in through observe(), and every decision below
// is derived from that seat-local view. That keeps the BRD §11 information
// boundary intact even though the brains run in the orchestrator process.

const SEAT_RE = /P(\d+)/g;
const ROLE_CLAIM_RE =
  /(?:我|本人|这里)\s*(?:是|跳|跳个|跳一张|跳的是)?\s*(预言家|通灵师|女巫|猎人|守卫|平民|狼人)/;
const CHECK_RE =
  /(?:查验|验人|验了?|通灵)\s*(?:了)?\s*P(\d+)[^。；;，,\n]{0,16}?(金水|查杀|好人|狼人)/g;
const SHORT_CHECK_RE = /P(\d+)\s*(?:是|为)?\s*(金水|查杀)/g;
const EXACT_ROLE_RE =
  /P(\d+)\s*的?身份(?:是|为)\s*(狼人|狼枪|机械狼|预言家|通灵师|女巫|猎人|守卫|平民)/g;
const VOTE_INTENT_RE = /(?:投|票|出|归)[^P。；;\n]{0,6}P(\d+)/g;
const ACCUSE_RE = /(?:不太信|不信|怀疑|觉得|认为|踩|打|针对)\s*P(\d+)/g;

const CLAIM_TO_ROLE = {
  预言家: "Seer",
  通灵师: "Psychic",
  女巫: "Witch",
  猎人: "Hunter",
  守卫: "Guardian",
  平民: "Civilian",
  狼人: "Werewolf",
};

const POWER_CLAIMS = new Set(["Seer", "Psychic", "Witch", "Hunter", "Guardian"]);

function seededNoise(...parts) {
  let h = 2166136261;
  for (const part of parts) {
    const text = String(part);
    for (let i = 0; i < text.length; i += 1) {
      h ^= text.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
  }
  return ((h >>> 0) % 1000) / 1000;
}

function firstSeat(text) {
  const match = /P(\d+)/.exec(text || "");
  return match ? Number(match[1]) : null;
}

function stripSpeechPrefix(text) {
  return String(text || "").replace(/^P\d+[^：:]*[：:]\s*/, "");
}

export class BotBrain {
  constructor({ seat, role, faction, subKind, teammates = [], seed = 1 }) {
    this.seat = Number(seat);
    this.role = role;
    this.faction = faction;
    this.subKind = subKind;
    this.teammates = new Set((teammates || []).map(Number).filter((s) => s !== this.seat));
    this.seed = Number(seed) || 1;

    this.alive = true;
    this.sheriff = false;
    this.day = 1;
    this.phase = "Night";

    this.claims = new Map(); // seat -> current role claim
    this.claimOrder = []; // first-claim history (any role)
    this.infoClaimOrder = []; // first Seer/Psychic claim, used for trust order
    this.statedVotes = new Map(); // speaker -> the seat they said they would vote
    this.seerReports = []; // { seer, target, wolf, day }
    this.knownChecks = new Map(); // target -> { isWolf?, role?, day }
    this.votes = new Map(); // voter -> target | null
    this.exiled = [];
    this.deaths = [];
    this.sheriffSeat = null;
    this.sheriffBallot = null; // { sheriff, target } from the open 归票 line
    this.accusedBy = new Set();
    this.speeches = [];
    this.wolfChat = [];
    this.wolfKnife = null;

    this.learnedRole = null;
    this.learnedAtDay = null;
    this.canShoot = null;
    this.lastGuardTarget = null;
    this.antidoteUsed = false;
    this.poisonUsed = false;
    this.fakeSeer = false;
    this.fakeSeerTarget = null;
  }

  get isWolf() {
    return this.faction === "Wolf";
  }

  get isOpenWolf() {
    return this.faction === "Wolf" && this.role !== "MechanicWolf";
  }

  observe(event) {
    if (!event || typeof event !== "object") return;
    if (event.day !== undefined) this.day = event.day;
    if (event.phase) this.phase = event.phase;

    if (event.etype === "deal") {
      this.role = event.data?.role || this.role;
      this.faction = event.data?.faction || this.faction;
      this.subKind = event.data?.subKind || this.subKind;
      if (Array.isArray(event.data?.teammates)) {
        this.teammates = new Set(
          event.data.teammates.map(Number).filter((s) => s !== this.seat),
        );
      }
      return;
    }

    if (event.vis === "public") {
      if (event.etype === "narration" || event.etype === "status") {
        this.ingestNarration(event.text);
      } else if (event.etype === "speech") {
        this.ingestSpeech(event);
      }
      return;
    }

    if (event.vis === "private" && Number(event.seat) === this.seat) {
      if (event.etype === "result_private") this.ingestPrivate(event);
      else if (event.etype === "narration") {
        const knife = /【狼队】今晚刀[:：]\s*(P\d+|空刀)/.exec(event.text || "");
        if (knife) this.wolfKnife = knife[1] === "空刀" ? null : Number(knife[1].slice(1));
      } else if (event.etype === "speech") {
        this.wolfChat.push(event.text || "");
      }
    }
  }

  ingestNarration(text) {
    if (!text) return;

    for (const match of text.matchAll(/(P\d+)\s*出局/g)) {
      const seat = Number(match[1].slice(1));
      if (!this.deaths.includes(seat)) this.deaths.push(seat);
      if (seat === this.seat) this.alive = false;
    }

    const elected = /(P\d+)\s*当选警长/.exec(text) || /仅\s*(P\d+)\s*一人竞选/.exec(text);
    if (elected) {
      this.sheriffSeat = Number(elected[1].slice(1));
      if (this.sheriffSeat === this.seat) this.sheriff = true;
    }
    if (/警徽被撕毁|警徽流失|本局无警长|全员退水|无人上警/.test(text)) {
      this.sheriffSeat = null;
      this.sheriff = false;
    }
    const transfer = /警徽移交给\s*(P\d+)/.exec(text);
    if (transfer) {
      this.sheriffSeat = Number(transfer[1].slice(1));
      this.sheriff = this.sheriffSeat === this.seat;
    }

    const exile = /放逐结果[：:]\s*(P\d+)/.exec(text);
    if (exile) {
      const seat = Number(exile[1].slice(1));
      if (!this.exiled.includes(seat)) this.exiled.push(seat);
    }

    const sheriffBallot = /警长\s*(P\d+)\s*归票[：:][^\n]*?投\s*(P\d+)/.exec(text);
    if (sheriffBallot) {
      this.sheriffBallot = {
        sheriff: Number(sheriffBallot[1].slice(1)),
        target: Number(sheriffBallot[2].slice(1)),
      };
    } else if (/警长\s*P\d+\s*归票[：:][^\n]*?弃票/.test(text)) {
      this.sheriffBallot = null;
    }

    const ballots = /投票详情[：:]\s*(.*)$/.exec(text);
    if (ballots) {
      for (const part of ballots[1].split("、")) {
        const arrow = /(P\d+)(?:\(警长\))?[→>-]+(P\d+)/.exec(part);
        if (arrow) {
          this.votes.set(Number(arrow[1].slice(1)), Number(arrow[2].slice(1)));
          continue;
        }
        const abstain = /(P\d+)(?:\(警长\))?\s*弃票/.exec(part);
        if (abstain) this.votes.set(Number(abstain[1].slice(1)), null);
      }
    }
  }

  ingestSpeech(event) {
    const speaker =
      Number(event.data?.speaker) ||
      Number((/^P(\d+)/.exec(event.text || "") || [])[1]) ||
      Number(event.seat) ||
      0;
    if (!speaker) return;

    const text = stripSpeechPrefix(event.text);
    this.speeches.push({ speaker, text, day: event.day || this.day });

    const claim = ROLE_CLAIM_RE.exec(text);
    if (claim) {
      const role = CLAIM_TO_ROLE[claim[1]];
      if (role) {
        // A player can revise a claim (civilian on day 1, seer/hunter later,
        // or a wolf switching from civilian to a fake seer claim). Keep the
        // first-claim history, but trust the current declaration.
        this.claims.set(speaker, role);
        if (!this.claimOrder.includes(speaker)) this.claimOrder.push(speaker);
        if (
          (role === "Seer" || role === "Psychic") &&
          !this.infoClaimOrder.includes(speaker)
        ) {
          this.infoClaimOrder.push(speaker);
        }
      }
    }

    const isSeerish = this.claims.get(speaker) === "Seer" || this.claims.get(speaker) === "Psychic";
    if (isSeerish) {
      for (const match of text.matchAll(CHECK_RE)) {
        this.addReport(speaker, Number(match[1]), match[2], event.day || this.day);
      }
      for (const match of text.matchAll(SHORT_CHECK_RE)) {
        this.addReport(speaker, Number(match[1]), match[2], event.day || this.day);
      }
      for (const match of text.matchAll(EXACT_ROLE_RE)) {
        const role = match[2];
        this.addReport(
          speaker,
          Number(match[1]),
          role === "狼人" || role === "狼枪" || role === "机械狼" ? "查杀" : "金水",
          event.day || this.day,
        );
      }
    }

    let voteIntent = null;
    for (const match of text.matchAll(VOTE_INTENT_RE)) {
      voteIntent = Number(match[1]);
    }
    if (voteIntent !== null) {
      this.statedVotes.set(speaker, voteIntent);
      if (speaker !== this.seat && voteIntent === this.seat) this.accusedBy.add(speaker);
    }
    for (const match of text.matchAll(ACCUSE_RE)) {
      if (Number(match[1]) === this.seat && speaker !== this.seat) this.accusedBy.add(speaker);
    }
    if (new RegExp(`P${this.seat}[^。；\\n]{0,8}(像狼|是狼|有问题|出掉|踩)`).test(text)) {
      this.accusedBy.add(speaker);
    }
  }

  addReport(seer, target, verdict, day) {
    if (!target || target === seer) return;
    const wolf = verdict === "查杀" || verdict === "狼人";
    const existing = this.seerReports.find(
      (report) => report.seer === seer && report.target === target,
    );
    if (existing) {
      existing.wolf = wolf;
      existing.day = day;
      return;
    }
    this.seerReports.push({ seer, target, wolf, day });
    if (seer === this.seat && this.alive) {
      // A report we made ourselves (echoed back as public speech) is a real check.
      if (this.role === "Seer") this.knownChecks.set(target, { isWolf: wolf, day });
    }
  }

  ingestPrivate(event) {
    const data = event.data || {};
    switch (data.kind) {
      case "Inspect":
        this.knownChecks.set(Number(data.target), {
          isWolf: Boolean(data.isWolf),
          day: event.day || this.day,
        });
        break;
      case "Psychic":
        this.knownChecks.set(Number(data.target), {
          role: data.shownRole,
          day: event.day || this.day,
        });
        break;
      case "GunCheck":
      case "MechanicGunCheck":
        this.canShoot = Boolean(data.canShoot);
        break;
      case "Learn":
        this.learnedRole = data.learnedRole || null;
        this.learnedAtDay = event.day || this.day;
        if (data.learnedRole === "Witch") {
          this.learnedStock = {
            antidote: Boolean(data.antidote),
            poison: Boolean(data.poison),
          };
        }
        break;
      default:
        break;
    }
  }

  // --- view helpers -------------------------------------------------------

  aliveSeats(candidates) {
    return (candidates || [])
      .map((value) => (typeof value === "number" ? value : value?.seat))
      .filter((seat) => Number.isInteger(seat) && seat !== this.seat);
  }

  seerClaimants() {
    // Keep a dead seer's claim: their last words remain the best town clue.
    // The psychic board has no Seer, so a psychic claim is the info anchor too.
    // Order by the first Seer/Psychic claim, not by an earlier civilian claim.
    const seers = this.infoClaimOrder.filter((seat) => this.claims.get(seat) === "Seer");
    if (seers.length > 0) return seers;
    return this.infoClaimOrder.filter((seat) => this.claims.get(seat) === "Psychic");
  }

  seerVoteIntent() {
    const trusted = this.trustedSeer();
    if (trusted === null) return null;
    return this.statedVotes.get(trusted) ?? null;
  }

  trustedSeer() {
    if (this.role === "Seer" || this.role === "Psychic") return this.seat;
    const claimants = this.seerClaimants();
    if (claimants.length === 0) return null;
    // A claimant who reports *us* as a wolf is less credible to us.
    const selfAccused = claimants.find((seat) =>
      this.seerReports.some(
        (report) => report.seer === seat && report.target === this.seat && report.wolf,
      ),
    );
    const credible = claimants.filter((seat) => seat !== selfAccused);
    return credible[0] ?? claimants[0];
  }

  reportsAbout(seat) {
    return this.seerReports.filter((report) => report.target === seat);
  }

  suspicion(seat) {
    if (seat === this.seat) return -100;
    if (this.deaths.includes(seat)) return -Infinity;

    const claim = this.claims.get(seat);
    const noise = seededNoise(this.seed, this.seat, seat, this.day, this.claims.size);
    let score;

    if (this.isWolf) {
      if (this.teammates.has(seat)) return -100;
      score = 0.3;
      if (claim === "Seer") score += 2.4;
      else if (claim === "Witch") score += 1.5;
      else if (claim === "Hunter") score += 1.2;
      else if (claim === "Guardian") score += 1.0;
      else if (claim === "Psychic") score += 2.0;
      if (this.seerReports.some((report) => report.seer === seat && report.wolf)) {
        score += 0.4;
      }
      const voted = this.votes.get(seat);
      if (voted !== undefined && voted !== null && this.teammates.has(voted)) score += 0.9;
      if (this.sheriffSeat === seat) score += 0.4;
      return score + noise * 0.7;
    }

    score = 0.25;
    if (claim === "Seer") score -= 0.5;
    else if (claim === "Psychic") score -= 0.35;
    else if (POWER_CLAIMS.has(claim)) score -= 0.2;

    const claimants = this.seerClaimants();
    if (claimants.length > 1 && seat !== claimants[0]) score += 1.5;

    const trusted = this.trustedSeer();
    for (const report of this.reportsAbout(seat)) {
      if (report.wolf) {
        if (report.seer === this.seat) score += 20;
        else if (report.seer === trusted) score += 8;
        else score += 1.1;
      } else if (report.seer === trusted) {
        score -= 5;
      }
    }

    const own = this.knownChecks.get(seat);
    if (own) {
      if (own.isWolf === true) score += 20;
      else if (own.isWolf === false) score -= 20;
      else if (own.role) {
        score += own.role === "Werewolf" || own.role === "WolfGun" || own.role === "MechanicWolf" ? 20 : -20;
      }
    }

    if (this.accusedBy.has(seat)) score += 1.2;

    const seerPush = this.seerVoteIntent();
    if (seerPush !== null && seerPush === seat && seat !== this.seat) score += 3.5;

    if (
      this.sheriffBallot &&
      this.sheriffBallot.target === seat &&
      this.sheriffBallot.sheriff !== this.seat &&
      seat !== this.seat
    ) {
      score += 2.2;
    }

    const voted = this.votes.get(seat);
    if (voted === this.seat) score += 1.6;
    if (trusted !== null && trusted !== this.seat && voted === trusted) score += 0.7;
    if (this.sheriffSeat === seat) score += 0.15;

    return score + noise * 0.5;
  }

  bestTarget(candidates, { skip = false } = {}) {
    const seats = this.aliveSeats(candidates);
    if (seats.length === 0) return skip ? null : null;

    let best = null;
    let bestScore = -Infinity;
    for (const seat of seats) {
      const score = this.suspicion(seat);
      if (score > bestScore) {
        bestScore = score;
        best = seat;
      }
    }
    if (best === null) return null;
    if (skip && bestScore < 0.2) return null;
    return best;
  }

  bestTownTarget(candidates, { excludeSelf = true } = {}) {
    const seats = this.aliveSeats(candidates).filter(
      (seat) => !(excludeSelf && seat === this.seat) && !this.teammates.has(seat),
    );
    if (seats.length === 0) return null;

    let best = null;
    let bestScore = -Infinity;
    for (const seat of seats) {
      let score = this.suspicion(seat);
      if (this.claims.get(seat) === "Seer") score += 5;
      else if (this.claims.get(seat) === "Psychic") score += 4;
      else if (this.claims.get(seat) === "Witch") score += 2;
      else if (this.claims.get(seat) === "Hunter") score += 1.5;
      if (this.sheriffSeat === seat) score += 0.6;
      if (score > bestScore) {
        bestScore = score;
        best = seat;
      }
    }
    return best;
  }

  checkedSeats() {
    return new Set(this.knownChecks.keys());
  }

  // --- decisions ----------------------------------------------------------

  decideChoose(ask, context = {}) {
    const candidates = ask.candidates || [];
    const kind = ask.kind;

    switch (kind) {
      case "NightKill": {
        if (context.wolfKillTarget !== undefined) return context.wolfKillTarget;
        return this.bestTownTarget(candidates);
      }

      case "Inspect": {
        const seats = this.aliveSeats(candidates);
        if (seats.length === 0) return null;
        const unchecked = seats.filter((seat) => !this.knownChecks.has(seat));
        const pool = unchecked.length > 0 ? unchecked : seats;
        let best = pool[0];
        let bestScore = -Infinity;
        for (const seat of pool) {
          const score = this.suspicion(seat) + seededNoise(this.seed, seat, this.day) * 0.4;
          if (score > bestScore) {
            bestScore = score;
            best = seat;
          }
        }
        return best;
      }

      case "Guard": {
        const seats = this.aliveSeats(candidates);
        if (seats.length === 0) return null;
        const trusted = this.trustedSeer();
        const order = [];
        if (trusted && seats.includes(trusted)) order.push(trusted);
        if (seats.includes(this.seat)) order.push(this.seat);
        order.push(...seats);
        for (const seat of order) {
          if (seat !== this.lastGuardTarget) {
            this.lastGuardTarget = seat;
            return seat;
          }
        }
        return null;
      }

      case "WitchPoison": {
        const seats = this.aliveSeats(candidates);
        if (seats.length === 0) return null;
        const trusted = this.trustedSeer();
        const confirmedWolf = seats.find((seat) =>
          this.seerReports.some(
            (report) => report.seer === trusted && report.target === seat && report.wolf,
          ),
        );
        if (confirmedWolf !== undefined) return confirmedWolf;
        const best = this.bestTarget(seats);
        if (best !== null && this.suspicion(best) >= 2.8 && this.day >= 3) return best;
        return null;
      }

      case "HunterShot": {
        const best = this.bestTarget(candidates);
        return best;
      }

      case "MechanicLearn": {
        const seats = this.aliveSeats(candidates);
        if (seats.length === 0) return null;
        const unchecked = seats.filter((seat) => !this.knownChecks.has(seat));
        const pool = unchecked.length > 0 ? unchecked : seats;
        let best = pool[0];
        let bestScore = -Infinity;
        for (const seat of pool) {
          let score = this.suspicion(seat);
          if (this.claims.get(seat) === "Seer") score += 4;
          else if (this.claims.get(seat) === "Psychic") score += 3;
          score += seededNoise(this.seed, seat, this.day) * 0.3;
          if (score > bestScore) {
            bestScore = score;
            best = seat;
          }
        }
        return best;
      }

      case "MechanicBigKnife": {
        const best = this.bestTownTarget(candidates);
        if (best === null) return null;
        if (this.learnedRole !== "Werewolf") return null;
        return best;
      }

      case "Vote":
      case "RunoffVote": {
        if (this.isWolf && this.isOpenWolf && context.wolfVoteTarget !== undefined) {
          if (context.wolfVoteTarget === null) return null;
          if (this.aliveSeats(candidates).includes(context.wolfVoteTarget)) {
            return context.wolfVoteTarget;
          }
        }
        const best = this.bestTarget(candidates);
        return best;
      }

      case "SheriffVote": {
        const seats = this.aliveSeats(candidates);
        if (seats.length === 0) return null;
        if (this.isWolf) {
          const teammate = seats.find((seat) => this.teammates.has(seat));
          if (teammate !== undefined) return teammate;
          return this.bestTownTarget(candidates);
        }
        const trusted = this.trustedSeer();
        if (trusted !== null && seats.includes(trusted)) return trusted;
        const claimed = seats.find((seat) => this.claims.get(seat) === "Seer");
        if (claimed !== undefined) return claimed;
        return seats[0];
      }

      case "BallotTarget": {
        if (this.isWolf && this.isOpenWolf && context.wolfVoteTarget !== undefined) {
          if (
            context.wolfVoteTarget !== null &&
            this.aliveSeats(candidates).includes(context.wolfVoteTarget)
          ) {
            return context.wolfVoteTarget;
          }
        }
        return this.bestTarget(candidates);
      }

      case "BadgeTransfer": {
        const seats = this.aliveSeats(candidates);
        if (seats.length === 0) return null;
        if (this.isWolf) {
          const teammate = seats.find((seat) => this.teammates.has(seat));
          if (teammate !== undefined) return teammate;
          return this.bestTownTarget(candidates);
        }
        const trusted = this.trustedSeer();
        if (trusted !== null && seats.includes(trusted)) return trusted;
        let best = seats[0];
        let bestScore = Infinity;
        for (const seat of seats) {
          const score = this.suspicion(seat);
          if (score < bestScore) {
            bestScore = score;
            best = seat;
          }
        }
        return best;
      }

      default:
        return this.bestTarget(candidates);
    }
  }

  decideConfirm(ask, context = {}) {
    const kind = ask.kind;
    const prompt = ask.prompt || "";

    switch (kind) {
      case "SelfDestruct":
        return false;

      case "WitchSave": {
        const target = firstSeat(prompt);
        if (target === null) return false;
        if (target === this.seat) return false; // this board never allows self-rescue
        if (this.knownChecks.get(target)?.isWolf === true) return false;
        if (this.claims.get(target) === "Seer" || this.claims.get(target) === "Psychic") {
          return true;
        }
        if (this.day === 1) {
          return seededNoise(this.seed, "save", target, this.day) > 0.25;
        }
        return this.suspicion(target) < 0.4;
      }

      case "RunForSheriff": {
        if (this.role === "Seer" || this.role === "Psychic") return true;
        if (this.isWolf) {
          const lowest = context.lowestOpenWolfSeat;
          return lowest === null || lowest === undefined || lowest === this.seat;
        }
        if (this.role === "Hunter") {
          return seededNoise(this.seed, "sheriff", this.seat) > 0.45;
        }
        if (this.role === "Guardian") {
          return seededNoise(this.seed, "sheriff", this.seat) > 0.6;
        }
        return seededNoise(this.seed, "sheriff", this.seat) > 0.78;
      }

      case "Withdraw": {
        if (this.role === "Seer" || this.role === "Psychic") return false;
        const candidates = context.sheriffCandidates || [];
        if (candidates.length >= 4) return true;
        if (this.isWolf) return false;
        if (candidates.length === 3) {
          return seededNoise(this.seed, "withdraw", this.seat) > 0.5;
        }
        return false;
      }

      case "ConsolidateSingle": {
        return true;
      }

      case "SpeechDirection": {
        return (this.seat + this.day) % 2 === 1;
      }

      default:
        return false;
    }
  }

  decideSpeak(ask, context = {}) {
    if (ask.kind === "WolfChat") return this.wolfChatLine(context);
    if (ask.kind === "LastWords") return this.lastWords(context);
    return this.statement(context);
  }

  phrase(phrases) {
    const index = Math.floor(
      seededNoise(this.seed, this.seat, this.day, this.accusedBy.size, this.deaths.length) *
        phrases.length,
    );
    return phrases[Math.min(index, phrases.length - 1)];
  }

  statement(context = {}) {
    const day = this.day;
    const trusted = this.trustedSeer();
    const target = this.isWolf
      ? this.bestTownTarget(context.candidates || [])
      : this.bestTarget(context.candidates || []);

    if (this.role === "Seer") {
      const checks = [...this.knownChecks.entries()]
        .filter(([, value]) => value.isWolf !== undefined)
        .sort((a, b) => (a[1].day || 0) - (b[1].day || 0));
      if (checks.length === 0) {
        return `我是预言家。昨晚的查验结果还没整理好，先听完前置位发言，我会把验人报清楚。`;
      }
      const lines = checks.map(
        ([seat, value]) =>
          `第${value.day || day}天验 P${seat} 是${value.isWolf ? "查杀" : "金水"}`,
      );
      const wolfCheck = checks.find(([, value]) => value.isWolf);
      const accusers = [...this.accusedBy].filter(
        (seat) => !this.deaths.includes(seat) && this.knownChecks.get(seat)?.isWolf !== false,
      );
      const vote = wolfCheck
        ? wolfCheck[0]
        : accusers.length > 0
          ? accusers[0]
          : target;
      const callout =
        accusers.length > 0
          ? ` ${accusers
              .slice(0, 3)
              .map((seat) => `P${seat}`)
              .join("、")} 都在往我身上带票，这个视角很像狼。`
          : "";
      const push = vote ? `今天我的票在 P${vote}。` : "今天我会跟着逻辑走。";
      return `我是预言家。${lines.join("，")}。${callout}${push}`;
    }

    if (this.role === "Psychic") {
      const checks = [...this.knownChecks.entries()].sort(
        (a, b) => (a[1].day || 0) - (b[1].day || 0),
      );
      const last = checks[checks.length - 1];
      const claim = last
        ? `我是通灵师，最近一次看到 P${last[0]} 的身份是${roleLabel(last[1].role)}。`
        : "我是通灵师，昨晚的结果我先记着。";
      const push = target !== null ? `今天先出 P${target}。` : "今天我会跟票。";
      return `${claim}${push}`;
    }

    if (this.role === "Witch") {
      if (this.antidoteUsed || this.poisonUsed) {
        return `我是女巫，药已经用过了。${target !== null ? `我目前最怀疑 P${target}，今天投他。` : "今天先听信息位的。"}`;
      }
      return `我是平民，先听信息位报验人。${target !== null ? `P${target} 的发言我不太信，先记一票。` : "目前场上信息还不够。"}`;
    }

    if (this.role === "Hunter") {
      if (day >= 2 || (trusted === this.seat && this.accusedBy.size > 0)) {
        return `我是猎人，枪还在。${target !== null ? `今天我的票给 P${target}，别把我票出去。` : "好人别把我票出去。"}`;
      }
      return `我是平民。${target !== null ? `P${target} 的视角有点怪，我先盯住他。` : "先听信息位。"}`;
    }

    if (this.role === "Guardian") {
      return `我是守卫，昨晚守的人先不说。${target !== null ? `我目前怀疑 P${target}。` : "今天跟信息位走。"}`;
    }

    if (this.isWolf) {
      if (this.fakeSeer && this.fakeSeerTarget !== null) {
        return `我是预言家。昨晚我查验了 P${this.fakeSeerTarget}，是查杀。${trusted !== null && trusted !== this.seat ? `P${trusted} 跳身份是在悍跳，今天出 P${trusted}。` : `今天出 P${this.fakeSeerTarget}。`}`;
      }
      const push =
        trusted !== null && trusted !== this.seat && !this.teammates.has(trusted)
          ? trusted
          : target;
      if (push === null) return "我是平民。今天先听警长归票。";
      return this.phrase([
        `我是平民。P${push} 的发言太飘，我今天是 P${push} 的票。`,
        `我是平民。我不认 P${push} 的身份，他的视角不像。今天出 P${push}。`,
        `我是平民。先听警长归票，但我个人会出 P${push}。`,
        `我是平民。P${push} 一直在打好人，我的票给他。`,
      ]);
    }

    if (target === null) return "我是平民。今天先听信息位归票。";
    const push = this.seerVoteIntent();
    if (push !== null && push !== this.seat) {
      return this.phrase([
        `我是平民。我跟信息位的票，今天出 P${push}。`,
        `我是平民。警长和信息位的方向我认，票 P${push}。`,
        `我是平民。P${push} 的发言没站住，我跟票。`,
      ]);
    }
    return `我是平民。${`我目前最怀疑 P${target}，今天投他。`}`;
  }

  lastWords() {
    if (this.role === "Seer") {
      const checks = [...this.knownChecks.entries()]
        .filter(([, value]) => value.isWolf !== undefined)
        .map(([seat, value]) => `P${seat}=${value.isWolf ? "查杀" : "金水"}`);
      const target = this.bestTarget(
        [...Array(12)].map((_, index) => index + 1).filter((seat) => !this.deaths.includes(seat)),
      );
      return `我是预言家，遗言报验人：${checks.length ? checks.join("，") : "没有有效验人"}。${target !== null ? `好人继续出 P${target}。` : "好人加油。"}`;
    }
    if (this.isWolf) {
      return `我是平民，你们出错人了。${this.teammates.size > 0 ? "后面看票型吧。" : ""}`;
    }
    return `我是${roleLabel(this.role)}，好人继续找狼，别被带节奏。`;
  }

  wolfChatLine(context = {}) {
    const target = context.wolfKillTarget;
    if (context.round > 1) return "过。";
    if (!target) return "今晚先空刀，明天看发言。";
    if (context.speakerIsFirst) {
      const reason =
        this.claims.get(target) === "Seer"
          ? "他跳预言家"
          : this.claims.get(target) === "Psychic"
            ? "他跳通灵师"
            : "他威胁最大";
      return `今晚刀 P${target}，${reason}。`;
    }
    return `同意，刀 P${target}。`;
  }
}

export function roleLabel(role) {
  return (
    {
      Werewolf: "狼人",
      WolfGun: "狼枪",
      MechanicWolf: "机械狼",
      Seer: "预言家",
      Psychic: "通灵师",
      Witch: "女巫",
      Hunter: "猎人",
      Guardian: "守卫",
      Civilian: "平民",
    }[role] || role || "未知"
  );
}
