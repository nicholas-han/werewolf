#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "flow/game.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

// Small KillAll board with the sheriff election enabled (avoids the vacuous
// 屠边 win that a godless board would otherwise trigger).
Board mkBoard(std::string name, std::vector<RoleSlot> roster) {
    Board b;
    b.name = std::move(name);
    b.roster = std::move(roster);
    b.config.winRule = WinRule::KillAll;
    b.config.sheriffEnabled = true;
    return b;
}

bool hasEvent(const ScriptedDecisionProvider& dp, const std::string& e) {
    return std::find(dp.events.begin(), dp.events.end(), e) != dp.events.end();
}

}  // namespace

// ---------- Election outcomes (BRD §7.2/§7.3) ----------

TEST(Sheriff, SingleCandidateAutoElected) {
    // 1 wolf + 3 civ; only seat 2 runs -> auto-elected (§7.2-6).
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, true, false, false};
    dp.nightKills = {std::nullopt, 3, 4};  // 空刀, then whittle town to parity

    Game game(mkBoard("auto", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 2);
    EXPECT_TRUE(hasEvent(dp, "P2 becomes sheriff"));
}

TEST(Sheriff, NobodyRunsNoSheriff) {
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, false, false};  // nobody stands
    dp.nightKills = {std::nullopt, 3, 4};

    Game game(mkBoard("none", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_FALSE(game.state().sheriffId.has_value());
    EXPECT_TRUE(hasEvent(dp, "No sheriff (nobody ran)"));
}

TEST(Sheriff, EveryoneRunsBadgeLost) {
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {true, true, true, true};  // 全员上警 (§7.3)
    dp.nightKills = {std::nullopt, 3, 4};

    Game game(mkBoard("all", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_FALSE(game.state().sheriffId.has_value());
    EXPECT_TRUE(hasEvent(dp, "Badge lost (everyone ran)"));
}

TEST(Sheriff, NonCandidatesElectTheWinner) {
    // 1 wolf + 4 civ; seats 2 & 3 run, non-candidates (1,4,5) vote 2,2,3 -> seat 2.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, true, true, false, false};
    dp.sheriffVotes = {2, 2, 3};                 // voters are seats 1, 4, 5
    dp.nightKills = {std::nullopt, 4, 5, 3};     // drive to a wolf win, sparing seat 2

    Game game(mkBoard("vote", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 4}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 2);
}

// ---------- 归票 vote weight (BRD §7.1) ----------

TEST(Sheriff, ConsolidateSingleBadgeBreaksTie) {
    // 1 wolf + 4 civ. Seat 2 is sheriff. Non-sheriff votes tie seat1 vs seat3 (2-2);
    // the sheriff 归单人 (1.5) on seat 1 breaks it -> wolf exiled -> TownWins.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, true, false, false, false};  // seat 2 auto-elected
    // exile round-1 chooseVote order = non-sheriff alive (seats 1,3,4,5):
    dp.votes = {3, 1, 1, 3};  // seat1<-{3,4}=2 ; seat3<-{1,5}=2  (tie before badge)
    dp.sheriffExileBallots = {SheriffBallot{true, 1}};  // 归单人 -> seat1 +1.5

    Game game(mkBoard("consolidate", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 4}}), dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Exiled));  // wolf exiled
    EXPECT_TRUE(game.state().find(3)->isAlive());  // tie resolved toward seat 1
}

// ---------- Badge transfer on death (BRD §7.6) ----------

TEST(Sheriff, BadgeTransferredWhenSheriffExiled) {
    // 2 wolf + 3 civ. Civilian seat 3 is sheriff, gets exiled, hands badge to seat 4.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, true, false, false};  // seat 3 auto-elected
    dp.votes = {3, 3, 3, 3};            // non-sheriff voters (1,2,4,5) all exile seat 3
    dp.badgeTransfers = {4};            // hand the badge to seat 4

    Game game(mkBoard("transfer", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 4);
    EXPECT_TRUE(game.state().find(4)->isSheriff);
    EXPECT_TRUE(hasEvent(dp, "Badge transferred to P4"));
}

TEST(Sheriff, BadgeDestroyedWhenSheriffDies) {
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, true, false, false};
    dp.votes = {3, 3, 3, 3};
    dp.badgeTransfers = {std::nullopt};  // tear up the badge (撕毁)

    Game game(mkBoard("destroy", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_FALSE(game.state().sheriffId.has_value());
    EXPECT_TRUE(hasEvent(dp, "Badge destroyed"));
}

TEST(Sheriff, TransferHappensBeforeHunterShot) {
    // §7.6: a hunter-sheriff exiled -> transfer the badge FIRST, then shoot.
    // 1 wolf + 1 hunter + 3 civ. Hunter (seat 2) is sheriff, exiled.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, true, false, false, false};  // hunter seat 2 auto-elected
    dp.votes = {2, 2, 2, 2};        // non-sheriff voters (1,3,4,5) exile the hunter
    dp.badgeTransfers = {3};        // hand badge to seat 3
    dp.hunterShots = {1};           // then shoot the wolf -> TownWins

    Game game(mkBoard("order", {{RoleKind::Werewolf, 1}, {RoleKind::Hunter, 1},
                                {RoleKind::Civilian, 3}}), dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);

    // Badge transfer must be logged before the wolf is shot.
    auto transfer = std::find(dp.events.begin(), dp.events.end(), "Badge transferred to P3");
    auto shot = std::find(dp.events.begin(), dp.events.end(), "P1 is out (Shot)");
    ASSERT_NE(transfer, dp.events.end());
    ASSERT_NE(shot, dp.events.end());
    EXPECT_LT(transfer - dp.events.begin(), shot - dp.events.begin());
}

// ---------- Self-destruct interrupts election (BRD §7.4/§7.5) ----------

TEST(Sheriff, SelfDestructInterruptsElectionThenDefersToDay2) {
    // Day 1: seat 3 runs; a wolf self-destructs mid-election -> interrupt + defer.
    // Day 2: vote-only election re-runs; seat 3 auto-elected.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {/*day1 seats1-5*/ false, false, true, false, false,
                        /*day2 seats2-5*/ false, true, false, false};
    dp.selfDestructs = {1};                          // day-1 election: wolf seat 1 blows up
    dp.nightKills = {std::nullopt, std::nullopt, 4, 5};  // later whittle to parity

    Game game(mkBoard("defer", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 3}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);

    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::BlownUp));
    EXPECT_TRUE(hasEvent(dp, "Sheriff election (deferred, vote only)"));
    EXPECT_TRUE(hasEvent(dp, "P3 becomes sheriff"));
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 3);
}
