#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "io/pass_and_play_decision_provider.h"

using namespace ww;

namespace {
GameState board9() { return buildInitialState(makeBoard9_SeerWitchHunter()); }
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
}  // namespace

TEST(PassAndPlay, PrivateDecisionHandsOffToActor) {
    GameState s = board9();
    std::istringstream in("\n5\n");  // hand-off enter, then vote for seat 5
    std::ostringstream out;
    PassAndPlayDecisionProvider dp(in, out);

    EXPECT_EQ(dp.chooseVote(s, 3, {1, 2, 3, 4, 5}), std::optional<int>(5));
    EXPECT_TRUE(contains(out.str(), "请将设备交给【玩家 P3】"));
}

TEST(PassAndPlay, PrivateNoticeShownInActorsHandoff) {
    std::istringstream in("\n");  // hand-off enter
    std::ostringstream out;
    PassAndPlayDecisionProvider dp(in, out);

    dp.onInspectResult(4, 1, /*isWolf=*/true);  // standalone notice -> own hand-off
    EXPECT_TRUE(contains(out.str(), "请将设备交给【玩家 P4】"));
    EXPECT_TRUE(contains(out.str(), "狼人（查杀）"));
}

TEST(PassAndPlay, PublicNotifyEndsTheHandoff) {
    GameState s = board9();
    // hand-off enter, vote, then end-turn enter (triggered by the public notify).
    std::istringstream in("\n5\n\n");
    std::ostringstream out;
    PassAndPlayDecisionProvider dp(in, out);

    dp.chooseVote(s, 3, {1, 2, 3, 4, 5});
    dp.notify("【公布昨夜死讯】平安夜，无人死亡");
    const std::string o = out.str();
    EXPECT_TRUE(contains(o, "交回设备"));                       // the turn was closed
    EXPECT_TRUE(contains(o, "【公布昨夜死讯】平安夜，无人死亡"));  // then shown publicly
}

TEST(PassAndPlay, PrivateAnnounceRevealsRole) {
    std::istringstream in("\n");
    std::ostringstream out;
    PassAndPlayDecisionProvider dp(in, out);

    dp.privateAnnounce(2, "你的身份是：狼人");
    EXPECT_TRUE(contains(out.str(), "请将设备交给【玩家 P2】"));
    EXPECT_TRUE(contains(out.str(), "你的身份是：狼人"));
}
