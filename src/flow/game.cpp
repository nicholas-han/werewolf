#include "flow/game.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace ww {

namespace {

// Tallies votes for the given voters over the given candidates. Each voter is
// asked once (in the order provided). Weight is 1.0 per vote for now; the
// sheriff's 1.5 (BRD §7.1) arrives in M3. Abstentions / out-of-range picks are
// ignored. Returns candidate -> total weight (only candidates with >0 votes).
std::map<int, double> tally(DecisionProvider& provider, const GameState& state,
                            const std::vector<int>& voters,
                            const std::vector<int>& candidates) {
    std::map<int, double> counts;
    for (int voter : voters) {
        std::optional<int> pick = provider.chooseVote(state, voter, candidates);
        if (!pick) continue;  // abstain
        if (std::find(candidates.begin(), candidates.end(), *pick) == candidates.end()) {
            continue;  // invalid target ignored
        }
        counts[*pick] += 1.0;
    }
    return counts;
}

// Returns the candidates sharing the highest tally (empty if no votes cast).
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

GameResult Game::applyDeath(int playerId, DeathCause cause) {
    Player* p = state_.find(playerId);
    if (p == nullptr || !p->isAlive()) {
        // Nothing to do; report the current standing.
        return evaluateWin(state_, board_.config);
    }
    p->recordDeath(cause, state_.day);
    provider_.notify(p->name() + " is out (" + std::string(to_string(cause)) + ")");
    return evaluateWin(state_, board_.config);  // §4.2 check after each death
}

GameResult Game::runNight() {
    provider_.notify("Night " + std::to_string(state_.day) + " begins");
    const std::vector<int> candidates = aliveIds();
    std::optional<int> target = provider_.chooseNightKill(state_, candidates);
    if (target) {
        return applyDeath(*target, DeathCause::Killed);
    }
    return evaluateWin(state_, board_.config);
}

std::optional<int> Game::resolveExile() {
    const std::vector<int> alive = aliveIds();

    // Round 1: every alive player votes on every alive player.
    std::map<int, double> counts = tally(provider_, state_, alive, alive);
    std::vector<int> leaders = topCandidates(counts);

    if (leaders.empty()) return std::nullopt;       // all abstained
    if (leaders.size() == 1) return leaders.front();  // clear plurality

    // Runoff (BRD §6): tied candidates speak; everyone *except* the tied
    // candidates votes again, choosing among the tied candidates.
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

GameResult Game::runDay() {
    provider_.notify("Day " + std::to_string(state_.day) + " begins");
    std::optional<int> exiled = resolveExile();
    if (exiled) {
        return applyDeath(*exiled, DeathCause::Exiled);
    }
    provider_.notify("No exile this round");
    return evaluateWin(state_, board_.config);
}

GameResult Game::run() {
    constexpr int kMaxCycles = 1000;  // safety net against non-terminating loops
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

        state_.day += 1;  // advance to the next night/day cycle
    }
    return GameResult::Ongoing;
}

}  // namespace ww
