#pragma once

#include <string>
#include <vector>

#include "core/enums.h"

// Board = a data-driven configuration (BRD §3): role composition + rule
// switches + win rule. Adding a new board should mostly be adding data.
namespace ww {

// Rule switches (BRD §3 config table). Defaults match the 9-player board.
struct BoardConfig {
    bool sheriffEnabled = true;
    WinRule winRule = WinRule::KillSide;
    WitchSelfRescue witchSelfRescue = WitchSelfRescue::Never;
    bool witchBothPotionsSameNight = false;
    bool blownUpEnabled = true;
    bool abstainAllowed = true;
    ExileTieRule exileTieRule = ExileTieRule::RunoffThenNoExile;
};

// One entry of the roster: `count` players of `kind`.
struct RoleSlot {
    RoleKind kind;
    int count;
};

struct Board {
    std::string name;
    std::vector<RoleSlot> roster;
    BoardConfig config;

    // Total number of players = sum of roster counts.
    int totalPlayers() const;
};

// First board: 9-player Seer/Witch/Hunter (BRD §3).
// 3 Werewolf + 3 Civilian + Seer + Witch + Hunter.
Board makeBoard9_SeerWitchHunter();

}  // namespace ww
