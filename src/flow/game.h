#pragma once

#include <optional>
#include <vector>

#include "core/abilities/ability.h"
#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/win_condition.h"
#include "io/decision_provider.h"

// Game is the flow orchestrator (BRD §5/§9). M2 wires real role abilities into
// the night/day sequence and resolves deaths with the death-trigger chain.
namespace ww {

class Game {
public:
    Game(Board board, DecisionProvider& provider);

    // Runs to completion and returns the result. Internal safety cap on cycles.
    GameResult run();

    const GameState& state() const { return state_; }

private:
    Board board_;
    DecisionProvider& provider_;
    GameState state_;

    GameResult runNight();  // §5.1: wolves -> seer -> witch -> dawn settle
    GameResult runDay();    // §5.3: self-destruct? -> exile vote -> settle

    // Settles a death batch. The batch is recorded simultaneously (§5.2, so
    // 同刀同毒 records both causes), the win is checked once, then death triggers
    // (e.g. hunter shot) fire and any chained deaths are settled one-at-a-time
    // with a win check after each (§4.2 — a decided game stops the chain).
    GameResult settle(std::vector<PendingDeath> batch);

    // Exile vote (BRD §6): plurality, then one runoff among tied candidates voted
    // by the remaining alive players; still tied -> no exile.
    std::optional<int> resolveExile();

    void announceDeath(const Player& p);
    std::vector<int> aliveIds() const;
};

}  // namespace ww
