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

}  // namespace ww
