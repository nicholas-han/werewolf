#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/player.h"

// GameState is the single source of truth (BRD §8.1, §11 "真相层"). The flow
// layer mutates it; player-facing views are derived/filtered elsewhere.
namespace ww {

class GameState {
public:
    std::vector<Player> players;
    Phase phase = Phase::Night;
    int day = 1;  // 1-based; the game opens on Night of day 1.

    // The badge holder, if any (BRD §7.1: a mutable single-value reference,
    // empty = no sheriff). Transferable on death (§7.6).
    std::optional<int> sheriffId;

    // Event / history log.
    std::vector<std::string> log;

    Player* find(int id);
    const Player* find(int id) const;

    // Living players (pointers into `players`).
    std::vector<Player*> alive();
    std::vector<const Player*> alive() const;

    // Living counts by faction / sub-kind (drives win checks in M1, BRD §4).
    int countAlive(Faction faction) const;
    int countAlive(SubKind subKind) const;
    int countAliveRole(RoleKind kind) const;
};

// Builds the initial state from a board: one Player per roster slot, seats
// 1..N in roster order (deterministic; seat assignment / shuffling is a flow
// concern added later). Names default to "P{seat}".
GameState buildInitialState(const Board& board);

}  // namespace ww
