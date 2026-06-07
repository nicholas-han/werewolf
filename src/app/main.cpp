#include <iostream>
#include <map>
#include <optional>
#include <random>
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

// Prompts the moderator to choose seat->role assignment (BRD M5 §setup / 随机发牌).
// Returns std::nullopt to fall back to the default roster-order layout.
std::optional<std::vector<RoleKind>> promptSetup(const Board& board, std::istream& in,
                                                 std::ostream& out) {
    out << "发牌方式：1) 随机发牌（推荐）  2) 手动录入  3) 默认顺序\n> ";
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;
    const char c = line.empty() ? '1' : line[0];  // default = random

    if (c == '3') return std::nullopt;  // fixed roster-order layout
    if (c != '2') {                     // random deal (default)
        std::random_device rd;
        return randomDeal(board, rd());
    }

    // Manual entry. Remaining role pool from the roster.
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

    std::cout << "=== 狼人杀（法官控制台）===\n";
    std::cout << "选择板子：1) 9 人预女猎  2) 12 人预女猎守 + 狼枪\n> ";
    std::string choice;
    std::getline(std::cin, choice);
    Board board = (!choice.empty() && choice[0] == '2') ? makeBoard12_GuardWolfGun()
                                                        : makeBoard9_SeerWitchHunter();
    std::cout << "板子：" << board.name << "（" << board.totalPlayers() << " 人）\n";

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
