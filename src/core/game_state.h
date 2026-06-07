#pragma once

#include <cstddef>
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

    // Witch potion stock (BRD §2/§8.1). Single witch per board for now.
    bool witchAntidoteAvailable = true;
    bool witchPoisonAvailable = true;

    // Event / history log.
    std::vector<std::string> log;

    // Rollbackable snapshot of all mutable state (BRD §4.4 sandbox foundation):
    // per-player status + the game-level potion/sheriff/phase/day, plus the log
    // length so sandbox narration can be trimmed away on restore.
    struct Snapshot {
        std::vector<Player::Snapshot> players;
        Phase phase;
        int day;
        std::optional<int> sheriffId;
        bool witchAntidoteAvailable;
        bool witchPoisonAvailable;
        std::size_t logSize;
    };
    Snapshot snapshot() const;
    void restore(const Snapshot& snap);

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

// Builds the initial state with an explicit seat->role assignment (BRD M5 §setup:
// the moderator enters the actually-dealt roles). `seatRoles[i]` is seat i+1's
// role; its size must equal board.totalPlayers(). Names default to "P{seat}".
GameState buildInitialState(const Board& board, const std::vector<RoleKind>& seatRoles);

// True if `seatRoles` matches the board roster's role multiset (BRD M5 §setup).
bool seatRolesMatchRoster(const Board& board, const std::vector<RoleKind>& seatRoles);

// Flat list of roster roles in roster order (e.g. {Wolf,Wolf,Wolf,Seer,...}).
std::vector<RoleKind> rosterRoleList(const Board& board);

// Random seat->role deal (BRD roadmap §随机发牌): a shuffled roster, seeded for
// reproducibility. Always a valid permutation of the roster.
std::vector<RoleKind> randomDeal(const Board& board, unsigned seed);

}  // namespace ww
