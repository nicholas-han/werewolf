#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>

#include "core/board.h"
#include "core/game_state.h"
#include "io/console_decision_provider.h"

using namespace ww;

namespace {
GameState board9() { return buildInitialState(makeBoard9_SeerWitchHunter()); }
}  // namespace

TEST(Console, ParsesTargetChoice) {
    GameState s = board9();
    std::istringstream in("3\n");
    std::ostringstream out;
    ConsoleDecisionProvider dp(in, out);
    EXPECT_EQ(dp.chooseNightKill(s, {1, 2, 3}), std::optional<int>(3));
}

TEST(Console, BlankMeansNoChoice) {
    GameState s = board9();
    std::istringstream in("\n");
    std::ostringstream out;
    ConsoleDecisionProvider dp(in, out);
    EXPECT_FALSE(dp.chooseNightKill(s, {1, 2, 3}).has_value());  // 空刀
}

TEST(Console, RepromptsOnInvalidThenAccepts) {
    GameState s = board9();
    std::istringstream in("9\nfoo\n2\n");  // out-of-range, non-numeric, then valid
    std::ostringstream out;
    ConsoleDecisionProvider dp(in, out);
    EXPECT_EQ(dp.chooseVote(s, 1, {1, 2, 3}), std::optional<int>(2));
}

TEST(Console, YesNoParsing) {
    GameState s = board9();
    {
        std::istringstream in("y\n");
        std::ostringstream out;
        ConsoleDecisionProvider dp(in, out);
        EXPECT_TRUE(dp.chooseWitchSave(s, 5, 7));
    }
    {
        std::istringstream in("n\n");
        std::ostringstream out;
        ConsoleDecisionProvider dp(in, out);
        EXPECT_FALSE(dp.chooseWitchSave(s, 5, 7));
    }
}

TEST(Console, SheriffBallotConsolidateSingle) {
    GameState s = board9();
    std::istringstream in("y\n1\n");  // 归单人 -> target 1
    std::ostringstream out;
    ConsoleDecisionProvider dp(in, out);
    SheriffBallot b = dp.chooseSheriffExileBallot(s, 2, {1, 3, 4});
    EXPECT_TRUE(b.consolidateSingle);
    EXPECT_EQ(b.target, std::optional<int>(1));
}

TEST(Console, SheriffBallotPkAbstain) {
    GameState s = board9();
    std::istringstream in("n\n\n");  // 归多人PK, then blank = abstain
    std::ostringstream out;
    ConsoleDecisionProvider dp(in, out);
    SheriffBallot b = dp.chooseSheriffExileBallot(s, 2, {1, 3, 4});
    EXPECT_FALSE(b.consolidateSingle);
    EXPECT_FALSE(b.target.has_value());
}

TEST(Console, BadgeTransferBlankTearsUp) {
    GameState s = board9();
    std::istringstream in("\n");  // blank = destroy
    std::ostringstream out;
    ConsoleDecisionProvider dp(in, out);
    EXPECT_FALSE(dp.chooseBadgeTransfer(s, 2, {1, 3}).has_value());
}
