#include "flow/game.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <utility>

#include "core/abilities/ability.h"
#include "core/player.h"
#include "core/roles/role.h"

namespace ww {

namespace {

// Tallies votes for `voters` over `candidates` (each asked once, in order).
// Weight is 1.0 per vote for now; the sheriff's 1.5 (BRD §7.1) arrives in M3.
// Abstentions / out-of-range picks are ignored.
std::map<int, double> tally(DecisionProvider& provider, const GameState& state,
                            const std::vector<int>& voters,
                            const std::vector<int>& candidates) {
    std::map<int, double> counts;
    for (int voter : voters) {
        std::optional<int> pick = provider.chooseVote(state, voter, candidates);
        if (!pick) continue;
        if (std::find(candidates.begin(), candidates.end(), *pick) == candidates.end()) {
            continue;
        }
        counts[*pick] += 1.0;
    }
    return counts;
}

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

}  // namespace

Game::Game(Board board, DecisionProvider& provider)
    : board_(std::move(board)), provider_(provider), state_(buildInitialState(board_)) {}

std::vector<int> Game::aliveIds() const {
    std::vector<int> ids;
    for (const Player& p : state_.players) {
        if (p.isAlive()) ids.push_back(p.id());
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

GameResult Game::settle(std::vector<PendingDeath> batch) {
    // Step 1: record the batch simultaneously (§5.2). A player hit twice (同刀同毒)
    // accumulates both causes; only the first transition is "newly dead".
    std::vector<Player*> newlyDead;
    for (const PendingDeath& pd : batch) {
        Player* p = state_.find(pd.playerId);
        if (p == nullptr) continue;
        const bool wasAlive = p->isAlive();
        p->recordDeath(pd.cause, state_.day);
        if (wasAlive) newlyDead.push_back(p);
    }
    std::sort(newlyDead.begin(), newlyDead.end(),
              [](const Player* a, const Player* b) { return a->seat() < b->seat(); });
    for (const Player* p : newlyDead) announceDeath(*p);

    // Step 2: win check after the simultaneous batch (§4.2).
    if (GameResult r = evaluateWin(state_, board_.config); r != GameResult::Ongoing) {
        return r;
    }

    // Step 3: death triggers, with chained deaths settled one at a time.
    std::deque<Player*> worklist(newlyDead.begin(), newlyDead.end());
    while (!worklist.empty()) {
        Player* dead = worklist.front();
        worklist.pop_front();
        for (const auto& ability : dead->role().abilities()) {
            auto* trigger = dynamic_cast<DeathTrigger*>(ability.get());
            if (trigger == nullptr) continue;

            std::vector<PendingDeath> triggered;
            trigger->onDeath(state_, *dead, provider_, triggered);
            for (const PendingDeath& td : triggered) {
                Player* t = state_.find(td.playerId);
                if (t == nullptr) continue;
                const bool wasAlive = t->isAlive();
                t->recordDeath(td.cause, state_.day);
                if (!wasAlive) continue;
                announceDeath(*t);
                if (GameResult r = evaluateWin(state_, board_.config);
                    r != GameResult::Ongoing) {
                    return r;  // §4.2: a decided game stops the chain
                }
                worklist.push_back(t);
            }
        }
    }
    return GameResult::Ongoing;
}

GameResult Game::runNight() {
    provider_.notify("Night " + std::to_string(state_.day) + " begins");

    // Reset per-night transient markers (used by guard/poison logic).
    for (Player& p : state_.players) {
        p.guardedTonight = false;
        p.poisonedTonight = false;
    }

    // Gather night-acting abilities across the living, ordered by §5.1.
    struct Act {
        Player* owner;
        NightActor* actor;
    };
    std::vector<Act> acts;
    for (Player& p : state_.players) {
        if (!p.isAlive()) continue;
        for (const auto& ability : p.role().abilities()) {
            if (auto* na = dynamic_cast<NightActor*>(ability.get())) {
                acts.push_back({&p, na});
            }
        }
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act& a, const Act& b) {
        return a.actor->nightOrder() < b.actor->nightOrder();
    });

    NightContext ctx;
    for (const Act& a : acts) {
        a.actor->actAtNight(ctx, state_, *a.owner, provider_);
    }

    // Dawn: build the death batch (BRD §5.2).
    std::vector<PendingDeath> batch;
    if (ctx.wolfTarget && ctx.savedTarget != ctx.wolfTarget) {
        batch.push_back({*ctx.wolfTarget, DeathCause::Killed});
    }
    if (ctx.poisonTarget) {
        batch.push_back({*ctx.poisonTarget, DeathCause::Poisoned});
    }
    return settle(std::move(batch));
}

GameResult Game::runDay() {
    provider_.notify("Day " + std::to_string(state_.day) + " begins");

    // Self-destruct opportunity (BRD §2). M2: offered once at day start; the
    // sheriff-election interaction (§7.4) arrives in M3.
    if (board_.config.blownUpEnabled) {
        std::vector<int> wolves;
        for (const Player& p : state_.players) {
            if (p.isAlive() && p.faction() == Faction::Wolf) wolves.push_back(p.id());
        }
        if (!wolves.empty()) {
            if (std::optional<int> sd = provider_.chooseSelfDestruct(state_, wolves)) {
                // 自爆即结束白天进黑夜，无放逐、无遗言 (§2/§5.3).
                return settle({{*sd, DeathCause::BlownUp}});
            }
        }
    }

    if (std::optional<int> exiled = resolveExile()) {
        return settle({{*exiled, DeathCause::Exiled}});
    }
    provider_.notify("No exile this round");
    return evaluateWin(state_, board_.config);
}

std::optional<int> Game::resolveExile() {
    const std::vector<int> alive = aliveIds();

    std::map<int, double> counts = tally(provider_, state_, alive, alive);
    std::vector<int> leaders = topCandidates(counts);

    if (leaders.empty()) return std::nullopt;
    if (leaders.size() == 1) return leaders.front();

    // Runoff (BRD §6): everyone except the tied candidates re-votes among them.
    std::vector<int> runoffVoters;
    for (int id : alive) {
        if (std::find(leaders.begin(), leaders.end(), id) == leaders.end()) {
            runoffVoters.push_back(id);
        }
    }
    std::map<int, double> counts2 = tally(provider_, state_, runoffVoters, leaders);
    std::vector<int> leaders2 = topCandidates(counts2);

    if (leaders2.size() == 1) return leaders2.front();
    return std::nullopt;  // still tied -> nobody exiled
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
