#include "flow/game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <map>
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

GameResult Game::announceNightDeaths() {
    if (pendingNightDeaths_.empty()) {
        provider_.notify(txt::peacefulNight());  // peaceful night cue
        return GameResult::Ongoing;
    }
    provider_.notify(txt::announceHeader());
    std::deque<Player*> worklist;
    for (int id : pendingNightDeaths_) {
        if (Player* p = state_.find(id)) worklist.push_back(p);
    }
    pendingNightDeaths_.clear();
    return settlement_.resolveRecorded(std::move(worklist));
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

GameResult Game::runNight() {
    provider_.notify(txt::nightBanner(state_.day));

    for (Player& p : state_.players) {
        p.guardedTonight = false;
        p.poisonedTonight = false;
    }

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
    // 破盾大刀: ignores the guard; only an antidote could save it (§2).
    if (ctx.bigKnifeTarget && !saved(*ctx.bigKnifeTarget)) {
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

    std::vector<Player*> newly = settlement_.record(batch);

    // Win check on the direct night deaths (§4.2). If decided, the game ends now:
    // announce them (resolveRecorded returns at the win check before any triggers,
    // so e.g. a knifed last-townsfolk ends it and the hunter never shoots).
    if (evaluateWin(state_, board_.config) != GameResult::Ongoing) {
        provider_.notify(txt::announceHeader());
        return settlement_.resolveRecorded(std::deque<Player*>(newly.begin(), newly.end()));
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

Game::ElectionOutcome Game::runSheriffElection() {
    const bool deferred = electionDeferred_;
    provider_.notify(deferred ? txt::electionDeferred() : txt::electionBegin());

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

    // (Candidate speeches are cosmetic and skipped.)

    // Withdraw / self-destruct window (§7.2-4). A wolf self-destruct interrupts
    // the election (§7.4); on day 2 a second interruption kills the badge (§7.5).
    {
        const std::vector<int> wolves = aliveWolfIds();
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                if (deferred) {
                    badgeAbandoned_ = true;
                    electionResolved_ = true;
                } else {
                    electionDeferred_ = true;
                }
                // §7.4: announce last night's deaths first, then resolve the blast.
                if (GameResult r = announceNightDeaths(); r != GameResult::Ongoing) {
                    return {r, false};
                }
                GameResult r = settlement_.apply({{*sd, DeathCause::BlownUp}});
                return {r, r == GameResult::Ongoing};
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

    // Runoff (§7.3): everyone except the tied candidates re-votes among them.
    provider_.notify(txt::sheriffRunoffTie(joinNames(state_, leaders)));
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
