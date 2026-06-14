#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <sstream>
#include <string>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/routing_decision_provider.h"
#include "io/scripted_channel.h"

using namespace ww;

namespace {
bool toldContains(const ScriptedChannel& c, const std::string& needle) {
    for (const std::string& m : c.told) {
        if (m.find(needle) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

// ---------- directed vs public routing (BRD §11) ----------

TEST(Routing, PrivateInspectReachesOnlyTheSeer) {
    ScriptedChannel c1, c2;
    std::ostringstream log;
    RoutingDecisionProvider dp({{1, &c1}, {2, &c2}}, &log);

    dp.onInspectResult(1, 2, /*isWolf=*/true);
    EXPECT_TRUE(toldContains(c1, "狼人（查杀）"));
    EXPECT_TRUE(c2.told.empty());        // the target is not told
    EXPECT_TRUE(log.str().empty());      // private notices never hit the spectator log
}

TEST(Routing, PublicNotifyReachesEveryoneAndSpectator) {
    ScriptedChannel c1, c2;
    std::ostringstream log;
    RoutingDecisionProvider dp({{1, &c1}, {2, &c2}}, &log);

    dp.notify("天亮了");
    EXPECT_TRUE(toldContains(c1, "天亮了"));
    EXPECT_TRUE(toldContains(c2, "天亮了"));
    EXPECT_NE(log.str().find("天亮了"), std::string::npos);
}

TEST(Routing, ModeratorStatusNeverReachesPlayers) {
    ScriptedChannel c1, c2;
    std::ostringstream log;
    RoutingDecisionProvider dp({{1, &c1}, {2, &c2}}, &log);

    dp.notifyModerator("【状态】存活: P1(狼人) P2(预言家)");
    EXPECT_TRUE(c1.told.empty());        // god-view stays away from players (§11)
    EXPECT_TRUE(c2.told.empty());
    EXPECT_NE(log.str().find("狼人"), std::string::npos);  // spectator log gets it
}

TEST(Routing, PrivateNotifyReachesOnlyTarget) {
    ScriptedChannel c1, c2;
    RoutingDecisionProvider dp({{1, &c1}, {2, &c2}}, nullptr);

    dp.notifyPlayer(2, "【女巫】你今晚被刀（无法自救）");
    EXPECT_TRUE(toldContains(c2, "你今晚被刀"));
    EXPECT_TRUE(c1.told.empty());
}

// ---------- wolf-team representative (designated decider) ----------

TEST(Routing, WolfRepresentativeDecidesAndTeammatesAreTold) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());  // seats 1-3 wolves, 4 seer
    ScriptedChannel c1, c2, c3, c4;
    c1.choices = {5};  // the representative (lowest-seat wolf = seat 1) knifes seat 5
    RoutingDecisionProvider dp({{1, &c1}, {2, &c2}, {3, &c3}, {4, &c4}}, nullptr);

    std::optional<int> target = dp.chooseNightKill(s, {1, 2, 3, 4, 5, 6, 7, 8, 9});
    EXPECT_EQ(target, std::optional<int>(5));
    EXPECT_TRUE(c1.told.empty());                 // the decider isn't "told" its own choice
    EXPECT_TRUE(toldContains(c2, "今晚刀"));        // teammates learn the target
    EXPECT_TRUE(toldContains(c3, "今晚刀"));
    EXPECT_TRUE(c4.told.empty());                  // the seer (non-wolf) is not told
}

// ---------- a full game driven entirely through per-seat channels ----------

TEST(Routing, FullScriptedMultiSeatGame) {
    // seat1 = wolf, seats 2-4 = civilians; KillAll, no sheriff. One extra civilian
    // keeps the night kill from triggering a vote-binding parity win (§4.3), so the
    // day vote actually runs.
    Board board;
    board.name = "routed";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;

    ScriptedChannel c1, c2, c3, c4;
    c1.choices = {2, 3};  // n1: wolf (rep) knifes civ seat2 ; day1: votes seat3
    c3.choices = {1};     // day1: seat3 votes the wolf (seat1)
    c4.choices = {1};     // day1: seat4 votes the wolf -> seat1 exiled -> TownWins

    RoutingDecisionProvider dp({{1, &c1}, {2, &c2}, {3, &c3}, {4, &c4}}, nullptr);
    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);

    EXPECT_TRUE(game.state().find(2)->hasDeathCause(DeathCause::Killed));   // routed night kill
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Exiled));   // routed exile vote
    EXPECT_TRUE(toldContains(c3, "天亮"));  // public narration reached the seat's channel
}
