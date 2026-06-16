#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/messages.h"
#include "flow/game.h"
#include "flow/transcript.h"
#include "flow/win_condition.h"
#include "io/bot_channel.h"
#include "io/console_decision_provider.h"
#include "io/decision_provider.h"
#include "io/json_decision_provider.h"
#include "io/pass_and_play_decision_provider.h"
#include "io/player_channel.h"
#include "io/routing_decision_provider.h"

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

// JSON-protocol mode (M15, docs/protocol_v1.md): the engine speaks the per-seat
// protocol on stdin/stdout for an external orchestrator. No interactive prompts;
// stdout carries ONLY protocol lines (debug goes to stderr).
int runJson(int boardSel, unsigned seed, bool haveSeed) {
    Board board = (boardSel == 3) ? makeBoard12_PsychicMechanic()
                  : (boardSel == 2) ? makeBoard12_GuardWolfGun()
                                    : makeBoard9_SeerWitchHunter();
    if (!haveSeed) {
        std::random_device rd;
        seed = rd();
    }
    std::vector<RoleKind> seatRoles = randomDeal(board, seed);
    JsonDecisionProvider provider(std::cin, std::cout, board.name, seed);
    Game game(board, provider, seatRoles);
    provider.emitGameStart(game.state());
    provider.emitDeals(game.state());
    GameResult result = game.run();
    provider.emitGameOver(result);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ww;

    bool jsonMode = false;
    int boardSel = 1;
    unsigned seed = 0;
    bool haveSeed = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") {
            jsonMode = true;
        } else if (a == "--board" && i + 1 < argc) {
            boardSel = std::atoi(argv[++i]);
        } else if (a == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            haveSeed = true;
        } else if (a == "--ask-timeout" && i + 1 < argc) {
            ++i;  // accepted for protocol compatibility; not enforced in v1
        }
    }
    if (jsonMode) return runJson(boardSel, seed, haveSeed);

    std::cout << "=== 狼人杀（法官控制台）===\n";
    std::cout << "选择板子：1) 9 人预女猎  2) 12 人预女猎守 + 狼枪  3) 12 人通灵机械狼\n> ";
    std::string choice;
    std::getline(std::cin, choice);
    const char b = choice.empty() ? '1' : choice[0];
    Board board = (b == '3') ? makeBoard12_PsychicMechanic()
                  : (b == '2') ? makeBoard12_GuardWolfGun()
                               : makeBoard9_SeerWitchHunter();
    std::cout << "板子：" << board.name << "（" << board.totalPlayers() << " 人）\n";

    std::optional<std::vector<RoleKind>> seatRoles = promptSetup(board, std::cin, std::cout);

    std::cout << "玩法：1) 单屏法官（一人主持）  2) 传递游玩（一台设备多人）  "
                 "3) AI 自动对战（多座位 bot 演示）\n> ";
    std::string modeLine;
    std::getline(std::cin, modeLine);
    const char mode = modeLine.empty() ? '1' : modeLine[0];
    const bool passAndPlay = (mode == '2');
    const bool botDemo = (mode == '3');

    bool recordSpeech = false;
    if (!botDemo) {
        std::cout << "记录发言（结束后可复盘）：1) 否（默认）  2) 是\n> ";
        std::string recLine;
        std::getline(std::cin, recLine);
        recordSpeech = (!recLine.empty() && recLine[0] == '2');
    }

    std::unique_ptr<DecisionProvider> provider;
    PassAndPlayDecisionProvider* pnp = nullptr;
    ConsoleDecisionProvider* console = nullptr;  // base view for shared toggles
    // Bot-demo seats: one BotChannel per player, routed by RoutingDecisionProvider
    // (engine talks to per-seat channels; std::cout is the spectator view).
    std::vector<std::unique_ptr<BotChannel>> bots;
    std::map<int, PlayerChannel*> channels;
    if (botDemo) {
        for (int seat = 1; seat <= board.totalPlayers(); ++seat) {
            bots.push_back(std::make_unique<BotChannel>(seat));
            channels[seat] = bots.back().get();
        }
        provider = std::make_unique<RoutingDecisionProvider>(channels, &std::cout);
    } else if (passAndPlay) {
        auto p = std::make_unique<PassAndPlayDecisionProvider>(std::cin, std::cout);
        pnp = p.get();
        console = p.get();
        provider = std::move(p);
    } else {
        auto p = std::make_unique<ConsoleDecisionProvider>(std::cin, std::cout);
        console = p.get();
        provider = std::move(p);
    }
    if (console) console->setRecordSpeech(recordSpeech);

    Game game(board, *provider, seatRoles);

    if (botDemo) {
        // Spectator view: show the deal, then watch the bots play it out.
        std::cout << "【观战视角】座位 -> 身份（bot 自动对战）:\n";
        for (const Player& p : game.state().players) {
            std::cout << "  座位 " << p.seat() << ": " << txt::role(p.role().kind()) << "\n";
        }
        std::cout << "--------------------------------------------\n";
    } else if (passAndPlay) {
        // Each player privately learns their own role (wolves see their team; the
        // mechanic does not meet the pack). No public moderator view.
        std::vector<int> openWolves;
        for (const Player& p : game.state().players) {
            if (p.faction() == Faction::Wolf && p.role().kind() != RoleKind::MechanicWolf) {
                openWolves.push_back(p.seat());
            }
        }
        for (const Player& p : game.state().players) {
            std::string msg = "你的身份是：" + txt::role(p.role().kind());
            if (p.role().kind() == RoleKind::MechanicWolf) {
                msg += "\n（你是机械狼，不与狼队见面；其他狼出局后可独立行动）";
            } else if (p.faction() == Faction::Wolf) {
                msg += "\n你的狼队友：";
                for (int seat : openWolves) {
                    if (seat != p.seat()) msg += "P" + std::to_string(seat) + " ";
                }
            }
            pnp->privateAnnounce(p.id(), msg);
        }
    } else {
        std::cout << "【法官视角】座位 -> 身份（你作为法官可见全部信息，BRD §11）:\n";
        for (const Player& p : game.state().players) {
            std::cout << "  座位 " << p.seat() << " (" << p.name()
                      << "): " << txt::role(p.role().kind()) << "\n";
        }
        std::cout << "--------------------------------------------\n";
    }

    GameResult result = game.run();

    std::cout << "============================================\n";
    std::cout << (result == GameResult::TownWins ? txt::resultTown() : txt::resultWolf()) << "\n";

    if (recordSpeech) {  // §4 发言记录: print the replay transcript
        std::cout << "\n" << formatTranscript(game.state());
    }
    return 0;
}
