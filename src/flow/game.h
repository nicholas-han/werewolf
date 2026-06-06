#pragma once

#include <optional>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/win_condition.h"
#include "io/decision_provider.h"

// Game is the M1 flow skeleton (BRD §5/§9): it loops Night -> Day(vote) and
// settles deaths sequentially, checking the win condition after each death
// (§4.2). Role abilities (kill/inspect/save/shoot) are stubbed: night uses a
// single provider-chosen kill as a stand-in until M2.
namespace ww {

class Game {
public:
    Game(Board board, DecisionProvider& provider);

    // Runs to completion and returns the result. Has an internal safety cap on
    // cycles to avoid an infinite loop if no resolution is reachable.
    GameResult run();

    const GameState& state() const { return state_; }

private:
    Board board_;
    DecisionProvider& provider_;
    GameState state_;

    GameResult runNight();
    GameResult runDay();

    // Records one death and immediately re-checks the win condition (§4.2).
    GameResult applyDeath(int playerId, DeathCause cause);

    // Resolves the exile vote (BRD §6): plurality, then one runoff among tied
    // candidates voted by the remaining alive players; still tied -> no exile.
    // Returns the exiled player id, or std::nullopt if nobody is exiled.
    std::optional<int> resolveExile();

    std::vector<int> aliveIds() const;
};

}  // namespace ww
