#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "flow/game.h"
#include "core/messages.h"
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
    EXPECT_TRUE(hasEvent(dp, txt::becomesSheriff("P2")));
}

TEST(Sheriff, NobodyRunsNoSheriff) {
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, false, false};  // nobody stands
    dp.nightKills = {std::nullopt, 3, 4};

    Game game(mkBoard("none", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_FALSE(game.state().sheriffId.has_value());
    EXPECT_TRUE(hasEvent(dp, txt::noSheriffNobodyRan()));
}

TEST(Sheriff, EveryoneRunsBadgeLost) {
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {true, true, true, true};  // 全员上警 (§7.3)
    dp.nightKills = {std::nullopt, 3, 4};

    Game game(mkBoard("all", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_FALSE(game.state().sheriffId.has_value());
    EXPECT_TRUE(hasEvent(dp, txt::badgeLostEveryoneRan()));
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

    // The voting phase is narrated: header, candidate list, and a tally (§7.2).
    EXPECT_TRUE(hasEvent(dp, txt::sheriffVoteHeader()));
    EXPECT_TRUE(hasEvent(dp, txt::sheriffCandidates("P2, P3")));
    EXPECT_TRUE(hasEvent(dp, txt::sheriffVotes("P2=2, P3=1")));
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
    // 2 wolf + 4 civ so exiling the civilian sheriff (seat 3) does NOT end the game
    // (2 wolves vs 3 town -> ongoing), and the badge transfer is meaningful. A later
    // night kill (seat 5) closes it out via parity.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, true, false, false, false};  // seat 3 auto-elected
    dp.votes = {3, 3, 3, 3, 3};         // non-sheriff voters (1,2,4,5,6) all exile seat 3
    dp.badgeTransfers = {4};            // hand the badge to seat 4
    dp.nightKills = {std::nullopt, 5};  // n1 peaceful ; n2 -> parity WolfWins

    Game game(mkBoard("transfer", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 4}}), dp);
    game.run();
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 4);
    EXPECT_TRUE(game.state().find(4)->isSheriff);
    EXPECT_TRUE(hasEvent(dp, txt::badgeTransferred("P4")));
}

TEST(Sheriff, BadgeDestroyedWhenSheriffDies) {
    // Same ongoing setup; the exiled sheriff tears up the badge instead.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, true, false, false, false};
    dp.votes = {3, 3, 3, 3, 3};
    dp.badgeTransfers = {std::nullopt};  // tear up the badge (撕毁)
    dp.nightKills = {std::nullopt, 5};

    Game game(mkBoard("destroy", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 4}}), dp);
    game.run();
    EXPECT_FALSE(game.state().sheriffId.has_value());
    EXPECT_TRUE(hasEvent(dp, txt::badgeDestroyed()));
}

TEST(Sheriff, NoBadgeTransferWhenDeathEndsGame) {
    // §7.6/§4.2: if the sheriff's death itself decides the game, the (now moot)
    // badge is NOT transferred — the moderator is never even prompted. Here exiling
    // the civilian sheriff (seat 3) leaves 2 wolves vs 2 town -> parity WolfWins.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, true, false, false};
    dp.votes = {3, 3, 3, 3};
    dp.badgeTransfers = {4};  // provided, but must NOT be consumed

    Game game(mkBoard("end-no-transfer", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 3}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_EQ(dp.badgeTransfers.size(), 1u);                  // transfer never asked
    EXPECT_FALSE(hasEvent(dp, txt::badgeTransferred("P4")));  // no transfer happened
    EXPECT_FALSE(hasEvent(dp, txt::badgeDestroyed()));
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
    auto transfer = std::find(dp.events.begin(), dp.events.end(), txt::badgeTransferred("P3"));
    auto shot = std::find(dp.events.begin(), dp.events.end(),
                          txt::out("P1", txt::cause(DeathCause::Shot)));
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
    EXPECT_TRUE(hasEvent(dp, txt::electionDeferred()));
    EXPECT_TRUE(hasEvent(dp, txt::becomesSheriff("P3")));
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 3);
}

TEST(Sheriff, DeferredFromPkCarriesOnlyPkCandidates) {
    // §7.5 情形 B: day 1 reaches the PK (seats 3 & 4 tie), then a THIRD party (wolf
    // seat 1) self-destructs -> defer. Day 2 must carry ONLY the PK candidates {3,4}
    // with NO re-registration, then vote. (runForSheriff has day-1 entries only; if
    // the engine wrongly re-registered, the empty queue would elect nobody.)
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, false, true, true, false};  // day1: seats 3 & 4 run
    dp.selfDestructs = {std::nullopt, 1};      // day1: none initially; seat 1 blows up in the PK
    dp.sheriffVotes = {3, 4, std::nullopt,     // day1 round1 voters [1,2,5]: tie 3 vs 4
                       3};                      // day2: lone voter [2] -> seat 3
    dp.withdraws = {false, false};             // day1 withdrawal window for [3,4]
    dp.nightKills = {std::nullopt, 5};         // n1 peaceful ; n2 kill civ 5
    dp.votes = {4, 2};                          // day2 exile: seat2->4, seat4->2
    dp.sheriffExileBallots = {SheriffBallot{true, 2}};  // sheriff(3) 归单人 -> exile wolf 2

    Game game(mkBoard("defer-pk", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 3}}), dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::BlownUp));
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 3);  // PK candidate won via the carried list
}

// ---------- Night-dead take part in the election (BRD §7.2/§7.4/§11) ----------

TEST(Sheriff, NightDeadRunsWinsThenBadgeFlowsOnReveal) {
    // The day-1 election runs BEFORE 公布死讯, so a player killed last night still
    // "appears alive" and may run/win (§7.2/§7.4); skipping them would also leak who
    // died before the official reveal (§11). Here the night-1 victim (seat 5) is the
    // ONLY one who stands -> auto-elected; THEN the dawn reveal announces the death
    // and the badge transfers to a living player (§7.6). Under the old bug seat 5 was
    // excluded from the roster entirely, so nobody could have been elected.
    ScriptedDecisionProvider dp;
    dp.nightKills = {5, 3, 4};                              // n1 kills the would-be sheriff
    dp.runForSheriff = {false, false, false, false, true};  // only night-dead seat 5 stands
    dp.badgeTransfers = {4};                                // dead P5 hands the badge to P4

    Game game(mkBoard("night-dead", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 4}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);

    // Night-dead seat 5 was actually elected — impossible if the election skipped it.
    EXPECT_TRUE(hasEvent(dp, txt::becomesSheriff("P5")));
    // …and the election finished BEFORE the death was announced (§7.2 / §11 no-leak).
    auto elected = std::find(dp.events.begin(), dp.events.end(), txt::becomesSheriff("P5"));
    auto reveal = std::find(dp.events.begin(), dp.events.end(), txt::outNoCause("P5"));
    ASSERT_NE(elected, dp.events.end());
    ASSERT_NE(reveal, dp.events.end());
    EXPECT_LT(elected - dp.events.begin(), reveal - dp.events.begin());
    // On the reveal the dead holder's badge transfers to a living player (§7.6).
    EXPECT_TRUE(hasEvent(dp, txt::badgeTransferred("P4")));
    EXPECT_FALSE(game.state().find(5)->isAlive());
}
