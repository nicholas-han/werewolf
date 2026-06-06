#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/console_decision_provider.h"

// Command-line entry point (BRD §12 app/, M4 + M5): play one game of the
// 9-player Seer/Witch/Hunter board on the terminal, moderator-operated.
namespace {

using namespace ww;

const std::vector<RoleKind> kRoleMenu = {RoleKind::Werewolf, RoleKind::Seer, RoleKind::Witch,
                                         RoleKind::Hunter, RoleKind::Civilian};

// Prompts the moderator for the actually-dealt role of each seat (BRD M5 §setup).
// Returns std::nullopt to fall back to the default roster-order layout.
std::optional<std::vector<RoleKind>> promptSetup(const Board& board, std::istream& in,
                                                 std::ostream& out) {
    out << "Enter dealt roles per seat manually? (y = enter, n = use default layout)\n> ";
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;
    if (line.empty() || (line[0] != 'y' && line[0] != 'Y')) return std::nullopt;

    // Remaining role pool from the roster.
    std::map<RoleKind, int> pool;
    for (const RoleSlot& slot : board.roster) pool[slot.kind] += slot.count;

    const int total = board.totalPlayers();
    std::vector<RoleKind> seatRoles;
    out << "Role codes: 1)Werewolf 2)Seer 3)Witch 4)Hunter 5)Civilian\n";

    for (int seat = 1; seat <= total; ++seat) {
        for (;;) {
            out << "  seat " << seat << " role? remaining:";
            for (RoleKind r : kRoleMenu) {
                if (pool[r] > 0) out << " " << to_string(r) << "x" << pool[r];
            }
            out << "\n  > ";
            if (!std::getline(in, line)) return std::nullopt;  // EOF -> abandon manual setup
            int code = 0;
            try {
                code = std::stoi(line);
            } catch (...) {
                out << "  ! enter 1-5\n";
                continue;
            }
            if (code < 1 || code > 5) {
                out << "  ! enter 1-5\n";
                continue;
            }
            RoleKind picked = kRoleMenu[code - 1];
            if (pool[picked] <= 0) {
                out << "  ! none of that role left\n";
                continue;
            }
            pool[picked] -= 1;
            seatRoles.push_back(picked);
            break;
        }
    }
    return seatRoles;
}

}  // namespace

int main() {
    using namespace ww;

    Board board = makeBoard9_SeerWitchHunter();
    std::cout << "=== Werewolf: 9-player Seer/Witch/Hunter (moderator console) ===\n";

    std::optional<std::vector<RoleKind>> seatRoles = promptSetup(board, std::cin, std::cout);

    ConsoleDecisionProvider provider(std::cin, std::cout);
    Game game(board, provider, seatRoles);

    std::cout << "[Moderator view] seat -> role (you, the judge, see everything; BRD §11):\n";
    for (const Player& p : game.state().players) {
        std::cout << "  seat " << p.seat() << " (" << p.name() << "): " << p.role().name() << "\n";
    }
    std::cout << "--------------------------------------------\n";

    GameResult result = game.run();

    std::cout << "============================================\n";
    std::cout << "Game over: " << to_string(result) << "\n";
    return 0;
}
