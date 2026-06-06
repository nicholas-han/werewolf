#include "flow/game.h"

#include <algorithm>
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

}  // namespace

Game::Game(Board board, DecisionProvider& provider,
           std::optional<std::vector<RoleKind>> seatRoles)
    : board_(std::move(board)),
      provider_(provider),
      state_(seatRoles ? buildInitialState(board_, *seatRoles) : buildInitialState(board_)) {}

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

void Game::announceDeath(const Player& p) {
    std::string causes;
    for (DeathCause c : p.deathCauses()) {
        if (!causes.empty()) causes += "+";
        causes += txt::cause(c);
    }
    provider_.notify(txt::out(p.name(), causes));
}

std::vector<Player*> Game::recordDeaths(const std::vector<PendingDeath>& batch) {
    std::vector<Player*> newly;
    for (const PendingDeath& pd : batch) {
        Player* p = state_.find(pd.playerId);
        if (p == nullptr) continue;
        const bool wasAlive = p->isAlive();
        p->recordDeath(pd.cause, state_.day, state_.phase);  // accumulates causes (同刀同毒, §5.2)
        if (wasAlive) newly.push_back(p);
    }
    return newly;
}

void Game::maybeTransferBadge(Player& dead) {
    if (!state_.sheriffId || *state_.sheriffId != dead.id()) return;

    // §7.6: transfer happens at the death; the dead holder loses the badge first.
    dead.isSheriff = false;
    std::vector<int> candidates = aliveIds();  // dead is already Out -> excluded
    std::optional<int> heir = provider_.chooseBadgeTransfer(state_, dead.id(), candidates);

    Player* newHolder = heir ? state_.find(*heir) : nullptr;
    if (newHolder != nullptr && newHolder->isAlive()) {
        state_.sheriffId = newHolder->id();
        newHolder->isSheriff = true;
        provider_.notify(txt::badgeTransferred(newHolder->name()));
    } else {
        state_.sheriffId.reset();  // torn up (撕毁) -> no sheriff for the rest (§7.6)
        provider_.notify(txt::badgeDestroyed());
    }
}

GameResult Game::resolveDeaths(std::deque<Player*> worklist) {
    std::sort(worklist.begin(), worklist.end(),
              [](const Player* a, const Player* b) { return a->seat() < b->seat(); });

    while (!worklist.empty()) {
        Player* dead = worklist.front();
        worklist.pop_front();

        announceDeath(*dead);
        if (hasLastWords(*dead)) {  // ② §5.3 last-words cue
            provider_.notify(txt::lastWordsCue(dead->name()));
            provider_.pause(txt::lastWordsPause(dead->name()));  // ⑤ pacing for last words
        }
        maybeTransferBadge(*dead);  // §7.6: transfer before death-triggered skills

        if (GameResult r = evaluateWin(state_, board_.config); r != GameResult::Ongoing) {
            return r;  // §4.2: decided -> stop, no further triggers
        }

        for (const auto& ability : dead->role().abilities()) {
            auto* trigger = dynamic_cast<DeathTrigger*>(ability.get());
            if (trigger == nullptr) continue;
            std::vector<PendingDeath> triggered;
            trigger->onDeath(state_, *dead, provider_, triggered);
            for (const PendingDeath& td : triggered) {
                Player* t = state_.find(td.playerId);
                if (t == nullptr) continue;
                const bool wasAlive = t->isAlive();
                t->recordDeath(td.cause, state_.day, state_.phase);
                if (wasAlive) worklist.push_back(t);
            }
        }
    }
    return GameResult::Ongoing;
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
    return resolveDeaths(std::move(worklist));
}

GameResult Game::settleImmediate(std::vector<PendingDeath> batch) {
    std::vector<Player*> newly = recordDeaths(batch);
    std::deque<Player*> worklist(newly.begin(), newly.end());
    return resolveDeaths(std::move(worklist));
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

void Game::cueSpeechOrder(int nightDeathCount, int singleDeadSeat) {
    if (!state_.sheriffId) return;  // no sheriff -> moderator improvises (§7.1.2)
    const int sheriff = *state_.sheriffId;
    const Player* sp = state_.find(sheriff);
    const bool single = (nightDeathCount == 1);
    const int anchorSeat = single ? singleDeadSeat : (sp ? sp->seat() : 1);

    SpeechDirection dir = provider_.chooseSpeechDirection(state_, sheriff, anchorSeat, single);

    const int total = static_cast<int>(state_.players.size());
    const int step = (dir == SpeechDirection::Left) ? 1 : -1;
    std::vector<int> order;
    int seat = anchorSeat;
    for (int i = 0; i < total; ++i) {
        seat = ((seat - 1 + step + total) % total) + 1;  // next seat in dir, wrap 1..total
        const Player* p = state_.find(seat);
        if (p && p->isAlive()) order.push_back(seat);
    }

    std::string names;
    for (int seatId : order) {
        const Player* p = state_.find(seatId);
        if (!names.empty()) names += " → ";
        names += p ? p->name() : ("#" + std::to_string(seatId));
    }
    provider_.notify(txt::speakingOrder(names));
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
    std::vector<Act> acts;
    for (Player& p : state_.players) {
        if (!p.isAlive()) continue;
        for (const auto& ability : p.role().abilities()) {
            if (auto* na = dynamic_cast<NightActor*>(ability.get())) acts.push_back({&p, na});
        }
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act& a, const Act& b) {
        return a.actor->nightOrder() < b.actor->nightOrder();
    });

    // Run each role group with "<role>请睁眼 / 请闭眼" narration (BRD M5 ⑤). Actors
    // are sorted by night order, so same-cue actors (e.g. all wolves) are contiguous.
    NightContext ctx;
    std::string openCue;
    for (const Act& a : acts) {
        const std::string cue = a.actor->nightCue();
        if (cue != openCue) {
            if (!openCue.empty()) provider_.notify(txt::closeEyes(openCue));
            provider_.notify(txt::openEyes(cue));
            openCue = cue;
        }
        a.actor->actAtNight(ctx, state_, *a.owner, provider_);
    }
    if (!openCue.empty()) provider_.notify(txt::closeEyes(openCue));

    std::vector<PendingDeath> batch;
    if (ctx.wolfTarget && ctx.savedTarget != ctx.wolfTarget) {
        batch.push_back({*ctx.wolfTarget, DeathCause::Killed});
    }
    if (ctx.poisonTarget) {
        batch.push_back({*ctx.poisonTarget, DeathCause::Poisoned});
    }

    std::vector<Player*> newly = recordDeaths(batch);

    // Win check on the direct night deaths (§4.2). If decided, the game ends now
    // with no triggers (e.g. the last townsfolk is knifed -> hunter never shoots).
    if (GameResult r = evaluateWin(state_, board_.config); r != GameResult::Ongoing) {
        std::sort(newly.begin(), newly.end(),
                  [](const Player* a, const Player* b) { return a->seat() < b->seat(); });
        for (const Player* p : newly) announceDeath(*p);
        return r;
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
                GameResult r = settleImmediate({{*sd, DeathCause::BlownUp}});
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

    std::vector<int> firstVoters;
    for (int id : alive) {
        if (!contains(candidates, id)) firstVoters.push_back(id);
    }
    std::vector<int> leaders = topCandidates(tallySheriff(firstVoters, remaining));
    if (leaders.size() == 1) {
        electSheriff(leaders.front());
        return {GameResult::Ongoing, false};
    }

    // Runoff (§7.3): everyone except the tied candidates re-votes among them.
    std::vector<int> runoffVoters;
    for (int id : alive) {
        if (!contains(leaders, id)) runoffVoters.push_back(id);
    }
    std::vector<int> leaders2 = topCandidates(tallySheriff(runoffVoters, leaders));
    if (leaders2.size() == 1) {
        electSheriff(leaders2.front());
        return {GameResult::Ongoing, false};
    }

    provider_.notify(txt::badgeLostTie());
    return {GameResult::Ongoing, false};
}

std::optional<int> Game::resolveExile() {
    const std::vector<int> alive = aliveIds();

    auto wt = [](double w) -> std::string {
        const double r = std::round(w * 2) / 2;  // weights are multiples of 0.5
        const long whole = static_cast<long>(std::floor(r));
        return (r == std::floor(r)) ? std::to_string(whole) : std::to_string(whole) + ".5";
    };
    auto fmtVotes = [&](const std::map<int, double>& counts) -> std::string {
        if (counts.empty()) return "(无人投票)";
        std::string s;
        for (const auto& [id, w] : counts) {
            if (!s.empty()) s += ", ";
            const Player* p = state_.find(id);
            s += (p ? p->name() : ("#" + std::to_string(id))) + "=" + wt(w);
        }
        return s;
    };
    auto names = [&](const std::vector<int>& ids) -> std::string {
        std::string s;
        for (int id : ids) {
            if (!s.empty()) s += ", ";
            const Player* p = state_.find(id);
            s += p ? p->name() : ("#" + std::to_string(id));
        }
        return s;
    };

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
    provider_.notify(txt::firstRoundVotes(fmtVotes(counts)));
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
    provider_.notify(txt::runoffVotes(fmtVotes(counts2)));
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
    provider_.notify(moderatorStatus());  // ④ status board

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
    cueSpeechOrder(nightDeathCount, singleDeadSeat);
    provider_.notify(txt::speechPhase());

    // Daytime self-destruct during 发言 (§2): day ends immediately, no vote.
    if (board_.config.blownUpEnabled) {
        const std::vector<int> wolves = aliveWolfIds();
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                return settleImmediate({{*sd, DeathCause::BlownUp}});
            }
        }
    }

    // Exile vote (§6 + §7.1 归票).
    provider_.notify(txt::voteTransition());
    if (std::optional<int> exiled = resolveExile()) {
        return settleImmediate({{*exiled, DeathCause::Exiled}});
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
