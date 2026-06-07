#include "core/board.h"

namespace ww {

int Board::totalPlayers() const {
    int total = 0;
    for (const RoleSlot& slot : roster) {
        total += slot.count;
    }
    return total;
}

Board makeBoard9_SeerWitchHunter() {
    Board board;
    board.name = "Board9_SeerWitchHunter";
    board.roster = {
        {RoleKind::Werewolf, 3},
        {RoleKind::Seer, 1},
        {RoleKind::Witch, 1},
        {RoleKind::Hunter, 1},
        {RoleKind::Civilian, 3},
    };
    // config left at BoardConfig defaults (matches BRD §3 for this board).
    return board;
}

Board makeBoard12_GuardWolfGun() {
    Board board;
    board.name = "Board12_GuardWolfGun";
    board.roster = {
        {RoleKind::Werewolf, 3},
        {RoleKind::WolfGun, 1},
        {RoleKind::Seer, 1},
        {RoleKind::Witch, 1},
        {RoleKind::Hunter, 1},
        {RoleKind::Guardian, 1},
        {RoleKind::Civilian, 4},
    };
    // Defaults match BRD §3: KillSide, witch never self-rescue, guard no-consecutive.
    return board;
}

Board makeBoard12_PsychicMechanic() {
    Board board;
    board.name = "Board12_PsychicMechanic";
    board.roster = {
        {RoleKind::Werewolf, 3},
        {RoleKind::MechanicWolf, 1},
        {RoleKind::Psychic, 1},
        {RoleKind::Witch, 1},
        {RoleKind::Hunter, 1},
        {RoleKind::Guardian, 1},
        {RoleKind::Civilian, 4},
    };
    return board;
}

}  // namespace ww
