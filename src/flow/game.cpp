#include "flow/game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <map>
#include <random>
#include <string>
#include <utility>

#include "core/abilities/ability.h"
#include "core/messages.h"
#include "core/player.h"
#include "core/roles/role.h"
#include "flow/last_words.h"
#include "flow/speech_order.h"

namespace ww {

namespace {

// Candidates sharing the highest tally (empty if no votes cast).
std::vector<int> topCandidates(const std::map<int, double>& counts) {
    std::vector<int> leaders;
    double best = 0.0;
    for (const auto& [id, weight] : counts) {
        if (weight > best) {
            best = weight;
            leaders.clear();
            leaders.push_back(id);
        } else if (weight == best) {
            leaders.push_back(id);
        }
    }
    return leaders;
}

bool contains(const std::vector<int>& v, int x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

long long nowTicks() {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

// Vote-tally formatting shared by the exile vote and the sheriff election.
std::string weightStr(double w) {  // weights are multiples of 0.5 (警徽 1.5)
    const double r = std::round(w * 2) / 2;
    const long whole = static_cast<long>(std::floor(r));
    return (r == std::floor(r)) ? std::to_string(whole) : std::to_string(whole) + ".5";
}

std::string nameOrId(const GameState& s, int id) {
    const Player* p = s.find(id);
    return p ? p->name() : ("#" + std::to_string(id));
}

std::string joinNames(const GameState& s, const std::vector<int>& ids) {
    std::string out;
    for (int id : ids) {
        if (!out.empty()) out += ", ";
        out += nameOrId(s, id);
    }
    return out;
}

std::string fmtVotes(const GameState& s, const std::map<int, double>& counts) {
    if (counts.empty()) return "(无人投票)";
    std::string out;
    for (const auto& [id, w] : counts) {
        if (!out.empty()) out += ", ";
        out += nameOrId(s, id) + "=" + weightStr(w);
    }
    return out;
}

}  // namespace

Game::Game(Board board, DecisionProvider& provider,
           std::optional<std::vector<RoleKind>> seatRoles)
    : board_(std::move(board)),
      provider_(provider),
      state_(seatRoles ? buildInitialState(board_, *seatRoles) : buildInitialState(board_)),
      settlement_(state_, board_.config, provider_) {}

std::vector<int> Game::aliveIds() const {
    std::vector<int> ids;
    for (const Player& p : state_.players) {
        if (p.isAlive()) ids.push_back(p.id());
    }
    return ids;
}

std::vector<int> Game::aliveWolfIds() const {
    std::vector<int> ids;
    for (const Player& p : state_.players) {
        if (p.isAlive() && p.faction() == Faction::Wolf) ids.push_back(p.id());
    }
    return ids;
}

GameResult Game::announceNightBatch(std::vector<Player*> dead, GameResult decided) {
    provider_.notify(txt::announceHeader());
    if (dead.size() > 1) provider_.notify(txt::deathOrderDisclaimer());  // §5.2 公布顺序

    // First-night multi-death: randomize the last-words order (§5.2/§5.3). Other
    // nights: night deaths have no last words, so order is moot -> seat order.
    std::vector<int> lwOrder;
    if (state_.day == 1 && dead.size() > 1) {
        for (Player* p : dead) lwOrder.push_back(p->id());
        std::mt19937 rng(static_cast<unsigned>(nowTicks()));
        std::shuffle(lwOrder.begin(), lwOrder.end(), rng);
    }

    if (decided != GameResult::Ongoing) {
        // §4.2: the batch already settled the game — announce + last words, but no
        // death triggers fire (e.g. a knifed hunter never gets to shoot).
        settlement_.announceBatch(dead, lwOrder);
        return decided;
    }
    std::deque<Player*> q(dead.begin(), dead.end());
    return settlement_.resolveRecorded(std::move(q), lwOrder);
}

GameResult Game::announceNightDeaths() {
    if (pendingNightDeaths_.empty()) {
        provider_.notify(txt::peacefulNight());  // peaceful night cue
        return GameResult::Ongoing;
    }
    std::vector<Player*> dead;
    for (int id : pendingNightDeaths_) {
        if (Player* p = state_.find(id)) dead.push_back(p);
    }
    pendingNightDeaths_.clear();
    return announceNightBatch(std::move(dead));
}

std::string Game::moderatorStatus() const {
    std::string s = "【状态】第 " + std::to_string(state_.day) + " 天 | 存活:";
    for (const Player& p : state_.players) {
        if (!p.isAlive()) continue;
        s += " " + p.name() + "(" + txt::role(p.role().kind());
        if (state_.sheriffId && *state_.sheriffId == p.id()) s += ",警长";
        s += ")";
    }
    s += " | 女巫解药=";
    s += state_.witchAntidoteAvailable ? "有" : "无";
    s += " 毒药=";
    s += state_.witchPoisonAvailable ? "有" : "无";
    return s;
}

std::vector<int> Game::cueSpeechOrder(int nightDeathCount, int singleDeadSeat) {
    std::vector<int> aliveSeats;
    for (const Player& p : state_.players) {
        if (p.isAlive()) aliveSeats.push_back(p.seat());
    }
    if (aliveSeats.empty()) return {};
    std::sort(aliveSeats.begin(), aliveSeats.end());

    const int total = static_cast<int>(state_.players.size());
    const bool single = (nightDeathCount == 1);
    auto nextSeat = [total](int s, int step) { return ((s - 1 + step + total) % total) + 1; };
    auto firstAliveAfter = [&](int seat, int step) {
        int s = seat;
        for (int i = 0; i < total; ++i) {
            s = nextSeat(s, step);
            const Player* p = state_.find(s);
            if (p && p->isAlive()) return s;
        }
        return seat;
    };

    // Decide direction, then the first speaker.
    SpeechDirection dir;
    if (state_.sheriffId) {  // §7.1.2: the sheriff sets it
        const Player* sp = state_.find(*state_.sheriffId);
        const int anchor = single ? singleDeadSeat : (sp ? sp->seat() : aliveSeats.front());
        dir = provider_.chooseSpeechDirection(state_, *state_.sheriffId, anchor, single);
    } else {  // §随机发言: direction from system time
        dir = timeDirection(nowTicks());
    }
    const int step = (dir == SpeechDirection::Left) ? 1 : -1;

    int startSpeaker;
    if (single) {
        // 死左/死右: start from the lone victim's neighbour in the chosen direction.
        startSpeaker = firstAliveAfter(singleDeadSeat, step);
    } else if (state_.sheriffId) {
        const Player* sp = state_.find(*state_.sheriffId);
        startSpeaker = firstAliveAfter(sp ? sp->seat() : aliveSeats.front(), step);
    } else {
        // Peaceful / multi-death with no sheriff: random first speaker from time.
        startSpeaker = aliveSeats[timeFirstSpeaker(nowTicks(),
                                                   static_cast<int>(aliveSeats.size()))];
    }

    std::vector<int> order;
    int seat = startSpeaker;
    for (int i = 0; i < total; ++i) {
        const Player* p = state_.find(seat);
        if (p && p->isAlive()) order.push_back(seat);
        seat = nextSeat(seat, step);
    }

    std::string names;
    for (int seatId : order) {
        const Player* p = state_.find(seatId);
        if (!names.empty()) names += " → ";
        names += p ? p->name() : ("#" + std::to_string(seatId));
    }
    provider_.notify(txt::speakingOrder(names));
    return order;
}

void Game::collectDaySpeeches(const std::vector<int>& orderSeats) {
    for (int seat : orderSeats) {
        const Player* p = state_.find(seat);
        if (p == nullptr || !p->isAlive()) continue;
        std::string text =
            provider_.collectSpeech(state_, p->id(), SpeechKind::Statement, state_.day);
        state_.recordSpeech(state_.day, SpeechKind::Statement, p->id(), seat, std::move(text));
    }
}

void Game::runWolfChat() {
    // §5.4: collect each open wolf's night chat (mechanic doesn't meet the pack).
    // The provider relays it privately to the other wolves; non-AI providers no-op.
    std::vector<int> open;
    for (const Player& p : state_.players) {
        if (p.isAlive() && p.faction() == Faction::Wolf &&
            p.role().kind() != RoleKind::MechanicWolf) {
            open.push_back(p.id());
        }
    }
    if (open.empty()) return;
    for (int w : open) {
        const Player* wp = state_.find(w);
        std::string text = provider_.collectWolfChat(state_, w, open);
        state_.recordSpeech(state_.day, SpeechKind::WolfChat, w, wp ? wp->seat() : 0,
                            std::move(text));  // empty text is ignored by recordSpeech
    }
}

GameResult Game::runNight() {
    provider_.notify(txt::nightBanner(state_.day));

    for (Player& p : state_.players) {
        p.guardedTonight = false;
        p.poisonedTonight = false;
    }

    runWolfChat();  // §5.4: wolves coordinate before the knife (no mechanical effect)

    struct Act {
        Player* owner;
        NightActor* actor;
    };
    // Collect from ALL players, dead included: every night role-phase in the
    // roster is narrated every night so an outsider can't tell a power role is
    // gone from a missing 睁眼 cue (BRD §11 — skipping a dead role's phase would
    // leak that the role, and thus which seat held it, is out). Only living
    // owners actually act.
    std::vector<Act> acts;
    for (Player& p : state_.players) {
        for (const auto& ability : p.role().abilities()) {
            if (auto* na = dynamic_cast<NightActor*>(ability.get())) acts.push_back({&p, na});
        }
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act& a, const Act& b) {
        return a.actor->nightOrder() < b.actor->nightOrder();
    });

    // Run each role group with "<role>请睁眼 / 请闭眼" narration (BRD M5 ⑤). Actors
    // are sorted by night order, so same-cue actors (e.g. all wolves) are contiguous.
    // The cue is emitted for every group (alive or not); only living owners act.
    NightContext ctx;
    std::string openCue;
    for (const Act& a : acts) {
        const std::string cue = a.actor->nightCue();
        if (cue != openCue) {
            if (!openCue.empty()) provider_.notify(txt::closeEyes(openCue));
            provider_.notify(txt::openEyes(cue));
            openCue = cue;
        }
        if (a.owner->isAlive()) a.actor->actAtNight(ctx, state_, *a.owner, provider_);
    }
    if (!openCue.empty()) provider_.notify(txt::closeEyes(openCue));

    std::vector<PendingDeath> batch;
    auto guarded = [&](int x) {
        return (ctx.guardTarget && *ctx.guardTarget == x) ||
               (ctx.mechanicGuardTarget && *ctx.mechanicGuardTarget == x);
    };
    auto saved = [&](int x) {
        return (ctx.savedTarget && *ctx.savedTarget == x) ||
               (ctx.mechSavedTarget && *ctx.mechSavedTarget == x);
    };

    // 普通刀: dies iff guard and antidote *agree* — both (同守同救) or neither (§5.2).
    if (ctx.wolfTarget && guarded(*ctx.wolfTarget) == saved(*ctx.wolfTarget)) {
        batch.push_back({*ctx.wolfTarget, DeathCause::Killed});
    }
    // 破盾大刀: a guaranteed kill — ignores BOTH the guard and the witch's antidote
    // (the witch is never even told its target, §2/§5.2). Always dies.
    if (ctx.bigKnifeTarget) {
        batch.push_back({*ctx.bigKnifeTarget, DeathCause::Killed});
    }
    // Poison: the guard does not block it — EXCEPT the mechanic's learned guard,
    // which reflects it (the poisoner dies, the protected player lives, §2).
    auto applyPoison = [&](std::optional<int> tgt, std::optional<int> src) {
        if (!tgt) return;
        if (ctx.mechanicGuardTarget && *ctx.mechanicGuardTarget == *tgt && src) {
            batch.push_back({*src, DeathCause::Poisoned});  // reflected to the poisoner
        } else {
            batch.push_back({*tgt, DeathCause::Poisoned});
        }
    };
    applyPoison(ctx.poisonTarget, ctx.poisonSourceId);
    applyPoison(ctx.mechPoisonTarget, ctx.mechPoisonSourceId);

    // Apply the direct night deaths one at a time, in the night's resolution order
    // (knife before poison), checking the win after each (§4.2 strict sequential —
    // the FIRST death to complete a condition decides it, no good-priority).
    std::vector<Player*> newly;
    const GameResult decided = settlement_.recordBatchSequential(batch, newly);

    // If the batch already decided the game, end it now (announce the deaths but
    // fire no triggers — e.g. a knifed last-townsfolk wins it and the hunter never
    // shoots, §4.2).
    if (decided != GameResult::Ongoing) {
        return announceNightBatch(newly, decided);
    }

    // Otherwise defer announcement + triggers to the day (§7.2 / §2).
    pendingNightDeaths_.clear();
    for (const Player* p : newly) pendingNightDeaths_.push_back(p->id());
    return GameResult::Ongoing;
}

void Game::electSheriff(int playerId) {
    state_.sheriffId = playerId;
    if (Player* p = state_.find(playerId)) {
        p->isSheriff = true;
        provider_.notify(txt::becomesSheriff(p->name()));
    }
}

std::vector<int> Game::selfDestructWolfCandidates() const {
    std::vector<int> ids = aliveWolfIds();
    // §2 自爆吞毒: a wolf that died at night but whose death isn't announced yet
    // (only during the day-1 pre-announcement election window) can still自爆.
    for (int id : pendingNightDeaths_) {
        const Player* p = state_.find(id);
        if (p && p->faction() == Faction::Wolf) ids.push_back(id);
    }
    return ids;
}

GameResult Game::announceThenSelfDestruct(int sd) {
    // §7.4-1 + §2 自爆吞毒: if `sd` is a not-yet-announced night death, stamp
    // BlownUp over its night cause and let the night batch announce it (as 自爆,
    // poison hidden). Otherwise announce the night deaths, then blow up the
    // (still-living) wolf separately.
    const bool swallow = contains(pendingNightDeaths_, sd);
    if (swallow) {
        if (Player* p = state_.find(sd)) {
            p->recordDeath(DeathCause::BlownUp, state_.day, Phase::Day);  // appends cause
        }
    }
    if (GameResult r = announceNightDeaths(); r != GameResult::Ongoing) return r;
    if (swallow) return GameResult::Ongoing;  // already announced + settled in the batch
    return settlement_.apply({{sd, DeathCause::BlownUp}});
}

Game::ElectionOutcome Game::resolveElectionSelfDestruct(int sd, const std::vector<int>& pkLeaders,
                                                        bool wasDeferred) {
    std::vector<int> pkLeft;
    for (int id : pkLeaders) {
        if (id != sd) pkLeft.push_back(id);
    }
    // §7.4 警徽落地归另一人: a PK candidate blowing up leaves exactly one -> auto-win.
    const bool pkCollapseElect = contains(pkLeaders, sd) && pkLeft.size() == 1;

    bool electSurvivor = false;
    if (wasDeferred) {  // §7.5: a 2nd interruption (day 2) kills the badge forever
        badgeAbandoned_ = true;        // (even a PK candidate blowing up — §7.4 only
        electionResolved_ = true;      //  hands the badge over on day 1)
        electionDeferred_ = false;
    } else if (pkCollapseElect) {  // §7.4: day-1 PK collapse -> the survivor auto-wins
        electionResolved_ = true;
        electionDeferred_ = false;
        electSurvivor = true;
    } else {  // §7.4: day-1 first interruption -> defer to day 2 (§7.5)
        electionResolved_ = false;
        electionDeferred_ = true;
        // Remember the PK survivors for day 2 (§7.5 情形 B). Empty when interrupted
        // before the PK (情形 A -> day 2 re-registers).
        deferredPkCandidates_ = pkLeft;
    }

    GameResult r = announceThenSelfDestruct(sd);
    if (r != GameResult::Ongoing) return {r, false};

    if (electSurvivor) {
        provider_.notify(txt::autoSheriff(nameOrId(state_, pkLeft.front())));
        electSheriff(pkLeft.front());
    }
    return {GameResult::Ongoing, true};  // self-destruct ends the day -> night
}

Game::ElectionOutcome Game::runDeferredElection() {
    provider_.notify(txt::electionDeferred());  // §7.5: 顺延，仅投票（无发言）
    const std::vector<int> alive = aliveIds();
    const bool fromPk = !deferredPkCandidates_.empty();

    // Candidates (§7.5): interrupted AT the PK (情形 B) -> carry only the still-alive
    // PK survivors, no re-registration; interrupted before it (情形 A) -> everyone
    // re-decides 上警 (no speeches).
    std::vector<int> candidates;
    if (fromPk) {
        for (int id : deferredPkCandidates_) {
            const Player* p = state_.find(id);
            if (p && p->isAlive()) candidates.push_back(id);
        }
    } else {
        for (int id : alive) {
            if (provider_.chooseRunForSheriff(state_, id)) candidates.push_back(id);
        }
    }
    deferredPkCandidates_.clear();

    if (candidates.empty()) {  // nobody ran / no PK survivor -> no sheriff
        electionResolved_ = true;
        electionDeferred_ = false;
        provider_.notify(txt::noSheriffNobodyRan());
        return {GameResult::Ongoing, false};
    }
    if (!fromPk && candidates.size() == alive.size()) {  // §7.3 全员上警 -> badge lost
        electionResolved_ = true;
        electionDeferred_ = false;
        provider_.notify(txt::badgeLostEveryoneRan());
        return {GameResult::Ongoing, false};
    }

    // A second self-destruct on day 2 kills the badge for the whole game (§7.5).
    {
        const std::vector<int> wolves = selfDestructWolfCandidates();
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                return resolveElectionSelfDestruct(*sd, candidates, /*wasDeferred=*/true);
            }
        }
    }

    electionResolved_ = true;
    electionDeferred_ = false;

    if (candidates.size() == 1) {  // only one candidate -> auto-elected
        provider_.notify(txt::autoSheriff(nameOrId(state_, candidates.front())));
        electSheriff(candidates.front());
        return {GameResult::Ongoing, false};
    }

    // Single direct vote — everyone NOT a candidate votes (abstain allowed, §7.5).
    provider_.notify(txt::sheriffVoteHeader());
    provider_.notify(txt::sheriffCandidates(joinNames(state_, candidates)));
    std::map<int, double> counts;
    for (int v : alive) {
        if (contains(candidates, v)) continue;
        std::optional<int> pick = provider_.chooseSheriffVote(state_, v, candidates);
        if (pick && contains(candidates, *pick)) counts[*pick] += 1.0;
    }
    provider_.notify(txt::sheriffVotes(fmtVotes(state_, counts)));
    std::vector<int> leaders = topCandidates(counts);
    if (leaders.size() == 1) {
        electSheriff(leaders.front());
        return {GameResult::Ongoing, false};
    }
    provider_.notify(txt::badgeLostTie());  // day-2 tie: no runoff speeches -> badge lost
    return {GameResult::Ongoing, false};
}

Game::ElectionOutcome Game::runSheriffElection() {
    if (electionDeferred_) return runDeferredElection();  // §7.5 day-2 vote-only

    provider_.notify(txt::electionBegin());
    const std::vector<int> alive = aliveIds();

    // 1. Stand for sheriff (§7.2).
    std::vector<int> candidates;
    for (int id : alive) {
        if (provider_.chooseRunForSheriff(state_, id)) candidates.push_back(id);
    }

    if (candidates.empty()) {  // §7.3: nobody ran
        electionResolved_ = true;
        electionDeferred_ = false;
        provider_.notify(txt::noSheriffNobodyRan());
        return {GameResult::Ongoing, false};
    }
    if (candidates.size() == alive.size()) {  // §7.3: everyone ran -> badge lost
        electionResolved_ = true;
        electionDeferred_ = false;
        provider_.notify(txt::badgeLostEveryoneRan());
        return {GameResult::Ongoing, false};
    }

    // Candidate speeches (BRD §7.2-2): each candidate pitches in registration order.
    // Recorded as day statements; no-op for providers that don't collect speech.
    for (int cid : candidates) {
        const Player* cp = state_.find(cid);
        if (cp == nullptr) continue;
        std::string pitch = provider_.collectSpeech(state_, cid, SpeechKind::Statement, state_.day);
        state_.recordSpeech(state_.day, SpeechKind::Statement, cid, cp->seat(), std::move(pitch));
    }

    // Withdraw / self-destruct window (§7.2-4). A wolf self-destruct interrupts
    // the election (§7.4); on day 2 a second interruption kills the badge (§7.5).
    // 吞毒-eligible: pending (un-announced) night-dead wolves may also自爆 (§2).
    {
        const std::vector<int> wolves = selfDestructWolfCandidates();
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                return resolveElectionSelfDestruct(*sd, /*pkLeaders=*/{}, /*wasDeferred=*/false);
            }
        }
    }

    // Withdrawals (§7.2-3).
    std::vector<int> remaining;
    for (int id : candidates) {
        if (!provider_.chooseWithdraw(state_, id)) remaining.push_back(id);
    }

    electionResolved_ = true;
    electionDeferred_ = false;

    if (remaining.empty()) {  // §7.3: all withdrew
        provider_.notify(txt::noSheriffAllWithdrew());
        return {GameResult::Ongoing, false};
    }
    if (remaining.size() == 1) {  // §7.2-6: auto-elected
        provider_.notify(txt::autoSheriff(nameOrId(state_, remaining.front())));
        electSheriff(remaining.front());
        return {GameResult::Ongoing, false};
    }

    // Vote: only non-candidates vote (§7.2-5). Withdrawn candidates abstain too.
    auto tallySheriff = [&](const std::vector<int>& voters, const std::vector<int>& cands) {
        std::map<int, double> counts;
        for (int v : voters) {
            std::optional<int> pick = provider_.chooseSheriffVote(state_, v, cands);
            if (pick && contains(cands, *pick)) counts[*pick] += 1.0;
        }
        return counts;
    };

    provider_.notify(txt::sheriffVoteHeader());
    provider_.notify(txt::sheriffCandidates(joinNames(state_, remaining)));

    std::vector<int> firstVoters;
    for (int id : alive) {
        if (!contains(candidates, id)) firstVoters.push_back(id);
    }
    std::map<int, double> counts = tallySheriff(firstVoters, remaining);
    provider_.notify(txt::sheriffVotes(fmtVotes(state_, counts)));
    std::vector<int> leaders = topCandidates(counts);
    if (leaders.size() == 1) {
        electSheriff(leaders.front());
        return {GameResult::Ongoing, false};
    }

    // Runoff (§7.3): the tied candidates PK.
    provider_.notify(txt::sheriffRunoffTie(joinNames(state_, leaders)));

    // PK-台 self-destruct (§7.4): a PK candidate blowing up hands the badge to the
    // sole survivor; a third person (incl. 吞毒) interrupts as usual.
    {
        const std::vector<int> wolves = selfDestructWolfCandidates();
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                return resolveElectionSelfDestruct(*sd, leaders, /*wasDeferred=*/false);
            }
        }
    }

    // PK-台 退水 (§7.3): withdraw any time; reducing to one auto-elects it.
    {
        std::vector<int> pk;
        for (int id : leaders) {
            if (!provider_.chooseWithdraw(state_, id)) pk.push_back(id);
        }
        if (pk.size() == 1) {
            provider_.notify(txt::autoSheriff(nameOrId(state_, pk.front())));
            electSheriff(pk.front());
            return {GameResult::Ongoing, false};
        }
        if (pk.empty()) {  // all PK candidates withdrew -> badge lost
            provider_.notify(txt::badgeLostTie());
            return {GameResult::Ongoing, false};
        }
        leaders = pk;
    }

    // Runoff vote: everyone except the (remaining) PK candidates re-votes.
    std::vector<int> runoffVoters;
    for (int id : alive) {
        if (!contains(leaders, id)) runoffVoters.push_back(id);
    }
    std::map<int, double> counts2 = tallySheriff(runoffVoters, leaders);
    provider_.notify(txt::sheriffRunoffVotes(fmtVotes(state_, counts2)));
    std::vector<int> leaders2 = topCandidates(counts2);
    if (leaders2.size() == 1) {
        electSheriff(leaders2.front());
        return {GameResult::Ongoing, false};
    }

    provider_.notify(txt::badgeLostTie());
    return {GameResult::Ongoing, false};
}

std::optional<int> Game::resolveExile() {
    const std::vector<int> alive = aliveIds();
    auto fmt = [&](const std::map<int, double>& counts) { return fmtVotes(state_, counts); };
    auto names = [&](const std::vector<int>& ids) { return joinNames(state_, ids); };

    provider_.notify(txt::voteHeader());

    // Round 1: the sheriff votes via 归票 (1.5 single / 1.0 PK), others weight 1 (§7.1).
    std::map<int, double> counts;
    for (int v : alive) {
        if (state_.sheriffId && *state_.sheriffId == v) {
            SheriffBallot b = provider_.chooseSheriffExileBallot(state_, v, alive);
            if (b.target && contains(alive, *b.target)) {
                counts[*b.target] += b.consolidateSingle ? 1.5 : 1.0;
            }
        } else {
            std::optional<int> pick = provider_.chooseVote(state_, v, alive);
            if (pick && contains(alive, *pick)) counts[*pick] += 1.0;
        }
    }
    provider_.notify(txt::firstRoundVotes(fmt(counts)));
    std::vector<int> leaders = topCandidates(counts);

    if (leaders.empty()) {
        provider_.notify(txt::noVotesNoExile());
        return std::nullopt;
    }
    if (leaders.size() == 1) {
        const Player* p = state_.find(leaders.front());
        provider_.notify(txt::exiled(p ? p->name() : "?"));
        return leaders.front();
    }

    // Runoff (§6): everyone except the tied candidates re-votes; the badge counts
    // 1 here (§7.1), i.e. the sheriff is just another voter via chooseVote.
    provider_.notify(txt::firstRoundTie(names(leaders)));
    std::vector<int> runoffVoters;
    for (int id : alive) {
        if (!contains(leaders, id)) runoffVoters.push_back(id);
    }
    std::map<int, double> counts2;
    for (int v : runoffVoters) {
        std::optional<int> pick = provider_.chooseVote(state_, v, leaders);
        if (pick && contains(leaders, *pick)) counts2[*pick] += 1.0;
    }
    provider_.notify(txt::runoffVotes(fmt(counts2)));
    std::vector<int> leaders2 = topCandidates(counts2);

    if (leaders2.size() == 1) {
        const Player* p = state_.find(leaders2.front());
        provider_.notify(txt::exiledRunoff(p ? p->name() : "?"));
        return leaders2.front();
    }
    provider_.notify(txt::runoffStillTie());
    return std::nullopt;
}

GameResult Game::runDay() {
    provider_.notify(txt::dayBanner(state_.day));
    provider_.notifyModerator(moderatorStatus());  // ④ status board (god-view, §11)

    // Sheriff election on day 1 (or the deferred day-2 vote), BEFORE 公布死讯 (§7.2).
    const bool doElection = board_.config.sheriffEnabled && !electionResolved_ &&
                            !badgeAbandoned_ && (state_.day == 1 || electionDeferred_);
    if (doElection) {
        ElectionOutcome eo = runSheriffElection();
        if (eo.result != GameResult::Ongoing) return eo.result;
        if (eo.interrupted) return GameResult::Ongoing;  // §7.4: day ends -> night
    }

    // Capture last night's toll for the speaking-order cue (③) before announcing.
    const int nightDeathCount = static_cast<int>(pendingNightDeaths_.size());
    int singleDeadSeat = -1;
    if (nightDeathCount == 1) {
        if (const Player* d = state_.find(pendingNightDeaths_.front())) singleDeadSeat = d->seat();
    }

    // 公布死讯 + 夜死触发（猎人翻枪）(§5.3 / §2).
    provider_.pause(txt::announcePause());  // ⑤ pacing
    if (GameResult r = announceNightDeaths(); r != GameResult::Ongoing) return r;

    // 发言顺序 cue (③ §7.1.2): single death -> 死左/死右; multi / peaceful -> from sheriff.
    const std::vector<int> speakOrder = cueSpeechOrder(nightDeathCount, singleDeadSeat);
    provider_.notify(txt::speechPhase());
    collectDaySpeeches(speakOrder);  // §4 发言记录

    // Daytime self-destruct during 发言 (§2): day ends immediately, no vote.
    if (board_.config.blownUpEnabled) {
        const std::vector<int> wolves = aliveWolfIds();
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                return settlement_.apply({{*sd, DeathCause::BlownUp}});
            }
        }
    }

    // Exile vote (§6 + §7.1 归票).
    provider_.notify(txt::voteTransition());
    if (std::optional<int> exiled = resolveExile()) {
        return settlement_.apply({{*exiled, DeathCause::Exiled}});
    }
    return evaluateWin(state_, board_.config);
}

GameResult Game::run() {
    constexpr int kMaxCycles = 1000;
    for (int cycle = 0; cycle < kMaxCycles; ++cycle) {
        state_.phase = Phase::Night;
        if (GameResult r = runNight(); r != GameResult::Ongoing) {
            provider_.notify(r == GameResult::TownWins ? txt::resultTown() : txt::resultWolf());
            return r;
        }

        state_.phase = Phase::Day;
        if (GameResult r = runDay(); r != GameResult::Ongoing) {
            provider_.notify(r == GameResult::TownWins ? txt::resultTown() : txt::resultWolf());
            return r;
        }

        state_.day += 1;
    }
    return GameResult::Ongoing;
}

}  // namespace ww
