#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "core/messages.h"
#include "core/player.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

Board mk(std::string name, std::vector<RoleSlot> roster, bool sheriff) {
    Board b;
    b.name = std::move(name);
    b.roster = std::move(roster);
    b.config.winRule = WinRule::KillAll;  // godless probe boards aren't a vacuous 屠边
    b.config.sheriffEnabled = sheriff;
    return b;
}

bool hasEvent(const ScriptedDecisionProvider& dp, const std::string& e) {
    return std::find(dp.events.begin(), dp.events.end(), e) != dp.events.end();
}
bool anyContains(const ScriptedDecisionProvider& dp, const std::string& needle) {
    for (const std::string& e : dp.events) {
        if (e.find(needle) != std::string::npos) return true;
    }
    return false;
}
int indexOf(const ScriptedDecisionProvider& dp, const std::string& e) {
    for (size_t i = 0; i < dp.events.size(); ++i) {
        if (dp.events[i] == e) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace

// ---------- Point 1: witch may poison herself (§2 自毒) ----------

TEST(M13, WitchCanSelfPoison) {
    // seats: 1 wolf, 2 witch, 3-4 civ. Night 1: 空刀; the witch poisons herself.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt};  // 空刀 -> no knife, so no save step
    dp.witchPoisons = {2};           // the witch (seat 2) targets herself
    dp.votes = {3, 1, 1};            // day 1: exile the wolf (seat 1) -> TownWins

    Game game(mk("witch-self-poison", {{RoleKind::Werewolf, 1}, {RoleKind::Witch, 1},
                                       {RoleKind::Civilian, 2}}, false),
              dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_FALSE(game.state().find(2)->isAlive());
    EXPECT_TRUE(game.state().find(2)->hasDeathCause(DeathCause::Poisoned));
}

// ---------- Point 8: the lone mechanic knifes BEFORE the witch, so she can save ----------

TEST(M13, WitchCanSaveLoneMechanicKnife) {
    // seats: 1 wolf, 2 mechanic, 3 witch, 4-5 civ. Exile the wolf day 1 so the
    // mechanic is lone; night 2 it knifes seat 4 — the witch must be able to save.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 4};        // n1 空刀 ; n2 lone-mechanic knifes seat 4
    dp.witchSaves = {true};                   // n2 the witch saves the knifed seat 4
    dp.votes = {2, 1, 1, 1, 1, /*day2*/ 3, 2, 2, 2};  // d1 exile wolf(1); d2 exile mechanic(2)

    Game game(mk("mech-before-witch",
                 {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Witch, 1},
                  {RoleKind::Civilian, 2}},
                 false),
              dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_TRUE(game.state().find(4)->isAlive());  // saved -> the knife resolved before the witch
}

// ---------- Point 3: 自爆吞毒 (poisoned wolf self-destructs on the警台) ----------

TEST(M13, SelfDestructSwallowsPoison) {
    // seats: 1-2 wolf, 3 witch, 4 civ; sheriff on. Night 1: 空刀; witch poisons wolf
    // seat 2. Day 1 election: seat 1 runs (so the self-destruct window is reached),
    // then the *poisoned* seat 2 self-destructs before its death is announced.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 3};   // n1 空刀 ; n2 wolf knifes the witch -> parity WolfWins
    dp.witchPoisons = {2};               // n1 poison wolf seat 2
    dp.runForSheriff = {true, false, false};  // alive d1 = [1,3,4]; only seat 1 runs
    dp.selfDestructs = {2};              // the pending-dead wolf seat 2 自爆吞毒

    Game game(mk("swallow-poison",
                 {{RoleKind::Werewolf, 2}, {RoleKind::Witch, 1}, {RoleKind::Civilian, 1}}, true),
              dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);

    // Both causes recorded; publicly only 自爆 shown; the poison is hidden.
    EXPECT_TRUE(game.state().find(2)->hasDeathCause(DeathCause::Poisoned));
    EXPECT_TRUE(game.state().find(2)->hasDeathCause(DeathCause::BlownUp));
    EXPECT_TRUE(hasEvent(dp, txt::out("P2", txt::cause(DeathCause::BlownUp))));  // P2 出局（自爆）
    EXPECT_FALSE(anyContains(dp, txt::cause(DeathCause::Poisoned)));  // 毒杀 never public (吞毒)
    EXPECT_FALSE(hasEvent(dp, txt::lastWordsCue("P2")));              // 自爆无遗言
}

// ---------- Point 4: PK-台 self-destruct ----------

TEST(M13, PkCandidateSelfDestructHandsBadgeToOther) {
    // seats: 1-2 wolf, 3-4 civ; sheriff on. Seats 1 & 3 run, tie -> runoff; then the
    // PK candidate seat 1 (a wolf) self-destructs -> seat 3 auto-wins.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 3};        // n2 kills seat 3 -> parity WolfWins
    dp.runForSheriff = {true, false, true, false};  // seats 1 & 3 run
    dp.sheriffVotes = {1, 3};                 // voters [2,4] tie 1 vs 3
    dp.selfDestructs = {std::nullopt, 1};     // initial window: none; runoff: seat 1 自爆

    Game game(mk("pk-blowup", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 2}}, true), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::BlownUp));
    EXPECT_TRUE(hasEvent(dp, txt::becomesSheriff("P3")));  // the other PK candidate auto-won
}

TEST(M13, ThirdPartySelfDestructInRunoffInterrupts) {
    // seats: 1-2 wolf, 3-4 civ; sheriff on. Civs 3 & 4 tie -> runoff; then a third
    // party (wolf seat 1, not on the PK) self-destructs -> election interrupted,
    // no sheriff.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 3};        // n2 -> parity WolfWins
    dp.runForSheriff = {false, false, true, true};  // seats 3 & 4 run
    dp.sheriffVotes = {3, 4};                 // voters [1,2] tie 3 vs 4
    dp.selfDestructs = {std::nullopt, 1};     // runoff: wolf seat 1 (non-PK) 自爆

    Game game(mk("pk-third-blowup", {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 2}}, true), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::BlownUp));
    EXPECT_FALSE(anyContains(dp, "当选警长"));  // nobody became sheriff (interrupted)
}

// ---------- Point 5: withdrawing to one in the runoff auto-elects ----------

TEST(M13, RunoffWithdrawToOneAutoElects) {
    // seats: 1 wolf, 2-4 civ; sheriff on. Civs 2 & 3 tie -> runoff; seat 2 退水 ->
    // seat 3 auto-wins; then seat 3 (sheriff) leads exiling the wolf -> TownWins.
    ScriptedDecisionProvider dp;
    dp.runForSheriff = {false, true, true, false};       // seats 2 & 3 run
    dp.withdraws = {false, false, /*runoff*/ true, false};  // runoff: seat 2 withdraws
    dp.sheriffVotes = {2, 3};                            // voters [1,4] tie 2 vs 3
    dp.votes = {2, 1, 1};                                // exile: non-sheriff [1,2,4]
    dp.sheriffExileBallots = {SheriffBallot{true, 1}};   // sheriff(3) 归单人 -> seat 1

    Game game(mk("runoff-withdraw", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}, true), dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    ASSERT_TRUE(game.state().sheriffId.has_value());
    EXPECT_EQ(*game.state().sheriffId, 3);                // seat 3 auto-won the runoff
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Exiled));
}

// ---------- Points 6/7: death-announce disclaimer + seat order + first-night last words ----------

TEST(M13, NightDeathsAnnouncedWithDisclaimerInSeatOrder) {
    // seats: 1 wolf, 2 witch, 3-4 civ. Night 1: wolf knifes seat 4, witch poisons
    // seat 3 -> two deaths -> parity WolfWins. Both die night 1 -> both get last words.
    ScriptedDecisionProvider dp;
    dp.nightKills = {4};       // knife seat 4
    dp.witchSaves = {false};   // don't save the knifed seat 4
    dp.witchPoisons = {3};     // poison seat 3

    Game game(mk("two-deaths", {{RoleKind::Werewolf, 1}, {RoleKind::Witch, 1},
                                {RoleKind::Civilian, 2}}, false),
              dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);

    EXPECT_TRUE(hasEvent(dp, txt::deathOrderDisclaimer()));  // §5.2 "死亡顺序不分先后"
    // Announced by seat number ascending (P3 before P4), regardless of who/how.
    const int p3 = indexOf(dp, txt::out("P3", txt::cause(DeathCause::Poisoned)));
    const int p4 = indexOf(dp, txt::out("P4", txt::cause(DeathCause::Killed)));
    ASSERT_GE(p3, 0);
    ASSERT_GE(p4, 0);
    EXPECT_LT(p3, p4);
    // First-night deaths both get last words (order is randomized, §5.2/§5.3).
    EXPECT_TRUE(hasEvent(dp, txt::lastWordsCue("P3")));
    EXPECT_TRUE(hasEvent(dp, txt::lastWordsCue("P4")));
}
