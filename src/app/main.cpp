#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/messages.h"
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
    out << "是否手动录入每个座位的真实身份？（y=录入，n=用默认布局）\n> ";
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;
    if (line.empty() || (line[0] != 'y' && line[0] != 'Y')) return std::nullopt;

    // Remaining role pool from the roster.
    std::map<RoleKind, int> pool;
    for (const RoleSlot& slot : board.roster) pool[slot.kind] += slot.count;

    const int total = board.totalPlayers();
    std::vector<RoleKind> seatRoles;
    out << "身份编号：1)狼人 2)预言家 3)女巫 4)猎人 5)平民\n";

    for (int seat = 1; seat <= total; ++seat) {
        for (;;) {
            out << "  座位 " << seat << " 的身份？剩余:";
            for (RoleKind r : kRoleMenu) {
                if (pool[r] > 0) out << " " << txt::role(r) << "x" << pool[r];
            }
            out << "\n  > ";
            if (!std::getline(in, line)) return std::nullopt;  // EOF -> abandon manual setup
            int code = 0;
            try {
                code = std::stoi(line);
            } catch (...) {
                out << "  ！请输入 1-5\n";
                continue;
            }
            if (code < 1 || code > 5) {
                out << "  ！请输入 1-5\n";
                continue;
            }
            RoleKind picked = kRoleMenu[code - 1];
            if (pool[picked] <= 0) {
                out << "  ！该身份已分配完\n";
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
    std::cout << "=== 狼人杀：9 人预女猎（法官控制台）===\n";

    std::optional<std::vector<RoleKind>> seatRoles = promptSetup(board, std::cin, std::cout);

    ConsoleDecisionProvider provider(std::cin, std::cout);
    Game game(board, provider, seatRoles);

    std::cout << "【法官视角】座位 -> 身份（你作为法官可见全部信息，BRD §11）:\n";
    for (const Player& p : game.state().players) {
        std::cout << "  座位 " << p.seat() << " (" << p.name() << "): " << txt::role(p.role().kind())
                  << "\n";
    }
    std::cout << "--------------------------------------------\n";

    GameResult result = game.run();

    std::cout << "============================================\n";
    std::cout << (result == GameResult::TownWins ? txt::resultTown() : txt::resultWolf()) << "\n";
    return 0;
}
