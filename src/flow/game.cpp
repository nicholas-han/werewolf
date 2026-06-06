#include "flow/game.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <utility>

#include "core/abilities/ability.h"
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
        causes += std::string(to_string(c));
    }
    provider_.notify(p.name() + " is out (" + causes + ")");
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
        provider_.notify("Badge transferred to " + newHolder->name());
    } else {
        state_.sheriffId.reset();  // torn up (撕毁) -> no sheriff for the rest (§7.6)
        provider_.notify("Badge destroyed");
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
            provider_.notify("  -> " + dead->name() + " may give last words (遗言)");
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
        provider_.notify("【公布昨夜死讯】平安夜，无人死亡");  // peaceful night cue
        return GameResult::Ongoing;
    }
    provider_.notify("【公布昨夜死讯】");
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
    std::string s = "[Status] day " + std::to_string(state_.day) + " | alive:";
    for (const Player& p : state_.players) {
        if (!p.isAlive()) continue;
        s += " " + p.name() + "(" + p.role().name();
        if (state_.sheriffId && *state_.sheriffId == p.id()) s += ",SHERIFF";
        s += ")";
    }
    s += " | witch antidote=";
    s += state_.witchAntidoteAvailable ? "Y" : "N";
    s += " poison=";
    s += state_.witchPoisonAvailable ? "Y" : "N";
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

    std::string s = "[Day] speaking order:";
    for (int seatId : order) {
        const Player* p = state_.find(seatId);
        s += " " + (p ? p->name() : ("#" + std::to_string(seatId)));
    }
    provider_.notify(s);
}

GameResult Game::runNight() {
    provider_.notify("=== Night " + std::to_string(state_.day) + " ===  天黑请闭眼");

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

    NightContext ctx;
    for (const Act& a : acts) a.actor->actAtNight(ctx, state_, *a.owner, provider_);

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
        provider_.notify(p->name() + " becomes sheriff");
    }
}

Game::ElectionOutcome Game::runSheriffElection() {
    const bool deferred = electionDeferred_;
    provider_.notify(deferred ? "Sheriff election (deferred, vote only)"
                              : "Sheriff election begins");

    const std::vector<int> alive = aliveIds();

    // 1. Stand for sheriff (§7.2).
    std::vector<int> candidates;
    for (int id : alive) {
        if (provider_.chooseRunForSheriff(state_, id)) candidates.push_back(id);
    }

    if (candidates.empty()) {  // §7.3: nobody ran
        electionResolved_ = true;
        electionDeferred_ = false;
        provider_.notify("No sheriff (nobody ran)");
        return {GameResult::Ongoing, false};
    }
    if (candidates.size() == alive.size()) {  // §7.3: everyone ran -> badge lost
        electionResolved_ = true;
        electionDeferred_ = false;
        provider_.notify("Badge lost (everyone ran)");
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
        provider_.notify("No sheriff (all withdrew)");
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

    provider_.notify("Badge lost (election tie)");
    return {GameResult::Ongoing, false};
}

std::optional<int> Game::resolveExile() {
    const std::vector<int> alive = aliveIds();

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
    std::vector<int> leaders = topCandidates(counts);
    if (leaders.empty()) return std::nullopt;
    if (leaders.size() == 1) return leaders.front();

    // Runoff (§6): everyone except the tied candidates re-votes; the badge counts
    // 1 here (§7.1), i.e. the sheriff is just another voter via chooseVote.
    std::vector<int> runoffVoters;
    for (int id : alive) {
        if (!contains(leaders, id)) runoffVoters.push_back(id);
    }
    std::map<int, double> counts2;
    for (int v : runoffVoters) {
        std::optional<int> pick = provider_.chooseVote(state_, v, leaders);
        if (pick && contains(leaders, *pick)) counts2[*pick] += 1.0;
    }
    std::vector<int> leaders2 = topCandidates(counts2);
    if (leaders2.size() == 1) return leaders2.front();
    return std::nullopt;  // still tied -> nobody exiled
}

GameResult Game::runDay() {
    provider_.notify("=== Day " + std::to_string(state_.day) + " ===  天亮了");
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
    provider_.pause("准备公布昨夜情况");  // ⑤ pacing
    if (GameResult r = announceNightDeaths(); r != GameResult::Ongoing) return r;

    // 发言顺序 cue (③ §7.1.2): single death -> 死左/死右; multi / peaceful -> from sheriff.
    cueSpeechOrder(nightDeathCount, singleDeadSeat);

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
    if (std::optional<int> exiled = resolveExile()) {
        return settleImmediate({{*exiled, DeathCause::Exiled}});
    }
    provider_.notify("No exile this round");
    return evaluateWin(state_, board_.config);
}

GameResult Game::run() {
    constexpr int kMaxCycles = 1000;
    for (int cycle = 0; cycle < kMaxCycles; ++cycle) {
        state_.phase = Phase::Night;
        if (GameResult r = runNight(); r != GameResult::Ongoing) {
            provider_.notify(std::string("Result: ") + std::string(to_string(r)));
            return r;
        }

        state_.phase = Phase::Day;
        if (GameResult r = runDay(); r != GameResult::Ongoing) {
            provider_.notify(std::string("Result: ") + std::string(to_string(r)));
            return r;
        }

        state_.day += 1;
    }
    return GameResult::Ongoing;
}

}  // namespace ww
