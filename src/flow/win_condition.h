#pragma once

#include <string_view>

#include "core/board.h"
#include "core/game_state.h"

// Win evaluation (BRD §4). Called after each individual death and after each
// exile (BRD §4 判定时机). Because the flow checks after *every* single death
// (§4.2 顺序结算), there is never a "both sides win at once" snapshot.
namespace ww {

enum class GameResult { Ongoing, TownWins, WolfWins };

constexpr std::string_view to_string(GameResult r) {
    switch (r) {
        case GameResult::Ongoing: return "Ongoing";
        case GameResult::TownWins: return "TownWins";
        case GameResult::WolfWins: return "WolfWins";
    }
    return "?";
}

// Evaluates the current snapshot under the board's win rule, including the
// wolf vote-binding (绑票) victory (§4.3). Returns Ongoing if undecided.
GameResult evaluateWin(const GameState& state, const BoardConfig& config);

}  // namespace ww
