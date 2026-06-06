#include <iostream>

#include "core/board.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/console_decision_provider.h"

// Command-line entry point (BRD §12 app/, M4): play one game of the 9-player
// Seer/Witch/Hunter board on the terminal, moderator-operated.
int main() {
    using namespace ww;

    Board board = makeBoard9_SeerWitchHunter();
    ConsoleDecisionProvider provider(std::cin, std::cout);
    Game game(board, provider);

    std::cout << "=== Werewolf: 9-player Seer/Witch/Hunter ===\n";
    std::cout << "[Moderator view] seat -> role (single-terminal limitation, BRD §11):\n";
    for (const Player& p : game.state().players) {
        std::cout << "  seat " << p.seat() << " (" << p.name() << "): " << p.role().name() << "\n";
    }
    std::cout << "--------------------------------------------\n";

    GameResult result = game.run();

    std::cout << "============================================\n";
    std::cout << "Game over: " << to_string(result) << "\n";
    return 0;
}
