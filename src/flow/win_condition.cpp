#include "flow/win_condition.h"

namespace ww {

GameResult evaluateWin(const GameState& state, const BoardConfig& config) {
    const int wolves = state.countAlive(Faction::Wolf);
    const int town = state.countAlive(Faction::Town);

    // Town wins when every wolf is out (holds for both KillSide and KillAll, §4.1).
    if (wolves == 0) {
        return GameResult::TownWins;
    }

    // Wolf vote-binding victory (§4.3): "可睁眼狼人" >= 存活好人 -> locked wolf win.
    // M1: every wolf opens eyes, so countAlive(Wolf) is the eye-opening count.
    if (wolves >= town) {
        return GameResult::WolfWins;
    }

    if (config.winRule == WinRule::KillSide) {
        // 屠边: wolves win if all gods OR all civilians are out (§4.1).
        const int gods = state.countAlive(SubKind::PowerRole);
        const int civilians = state.countAlive(SubKind::Civilian);
        if (gods == 0 || civilians == 0) {
            return GameResult::WolfWins;
        }
    } else {  // WinRule::KillAll
        // 屠城: wolves win only when all townsfolk are out (§4.1).
        if (town == 0) {
            return GameResult::WolfWins;
        }
    }

    return GameResult::Ongoing;
}

}  // namespace ww
