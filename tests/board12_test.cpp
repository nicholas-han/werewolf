#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "core/abilities/ability.h"
#include "core/abilities/role_abilities.h"
#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "core/player.h"
#include "flow/game.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

// Small KillAll board with the sheriff disabled, to isolate night mechanics.
Board killAll(std::string name, std::vector<RoleSlot> roster) {
    Board b;
    b.name = std::move(name);
    b.roster = std::move(roster);
    b.config.winRule = WinRule::KillAll;
    b.config.sheriffEnabled = false;
    return b;
}

bool anyHasCause(const GameState& s, DeathCause c) {
    for (const Player& p : s.players) {
        if (p.hasDeathCause(c)) return true;
    }
    return false;
}

}  // namespace

// ---------- The 12-player board itself (BRD §3) ----------

TEST(Board12, Composition) {
    Board b = makeBoard12_GuardWolfGun();
    EXPECT_EQ(b.totalPlayers(), 12);
    GameState s = buildInitialState(b);
    EXPECT_EQ(s.countAlive(Faction::Wolf), 4);          // 3 werewolf + 1 wolfgun
    EXPECT_EQ(s.countAlive(SubKind::PowerRole), 4);      // seer/witch/hunter/guard
    EXPECT_EQ(s.countAlive(SubKind::Civilian), 4);
    EXPECT_EQ(s.countAliveRole(RoleKind::Guardian), 1);
    EXPECT_EQ(s.countAliveRole(RoleKind::WolfGun), 1);
}

// ---------- Guardian (BRD §2/§5.2) ----------
// Board: seat 1 wolf, 2 guardian, 3 witch, 4 civilian.

TEST(Guardian, SameGuardAndSaveStillDies) {
    // 同守同救 = 死: guard civ4 + witch antidotes civ4 (both act) -> civ4 dies.
    ScriptedDecisionProvider dp;
    dp.guards = {4};          // night1 guard civ4
    dp.nightKills = {4, 2};   // night1 knife civ4 ; night2 knife guard2 -> wolf win
    dp.witchSaves = {true};   // night1 witch saves civ4

    Game game(killAll("gg", {{RoleKind::Werewolf, 1}, {RoleKind::Guardian, 1},
                             {RoleKind::Witch, 1}, {RoleKind::Civilian, 1}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_FALSE(game.state().find(4)->isAlive());                    // died despite guard+save
    EXPECT_TRUE(game.state().find(4)->hasDeathCause(DeathCause::Killed));
    EXPECT_FALSE(game.state().witchAntidoteAvailable);               // antidote was spent
}

TEST(Guardian, GuardAloneSavesTheKnifedTarget) {
    // Guard civ4, wolves knife civ4, witch does NOT save -> civ4 survives.
    ScriptedDecisionProvider dp;
    dp.guards = {4};
    dp.nightKills = {4, 3, 2};  // n1 knife guarded civ4 (saved); n2 witch3; n3 guard2 -> win
    // witchSaves empty -> witch declines

    Game game(killAll("gs", {{RoleKind::Werewolf, 1}, {RoleKind::Guardian, 1},
                             {RoleKind::Witch, 1}, {RoleKind::Civilian, 1}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(4)->isAlive());  // guard saved it on night 1
}

TEST(Guardian, ProtectionDoesNotBlockPoison) {
    // Guard civ4, wolves 空刀, witch poisons civ4 -> civ4 still dies (poison).
    ScriptedDecisionProvider dp;
    dp.guards = {4};
    dp.nightKills = {std::nullopt, 2};  // n1 空刀 ; n2 knife guard2 -> win
    dp.witchPoisons = {4};              // n1 poison the guarded civ4

    Game game(killAll("gp", {{RoleKind::Werewolf, 1}, {RoleKind::Guardian, 1},
                             {RoleKind::Witch, 1}, {RoleKind::Civilian, 1}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_FALSE(game.state().find(4)->isAlive());
    EXPECT_TRUE(game.state().find(4)->hasDeathCause(DeathCause::Poisoned));
}

TEST(Guardian, CannotProtectSameTargetTwoNightsRunning) {
    // Ability-level: with last night's target set, it is excluded from candidates
    // unless the board allows consecutive same-target guarding.
    struct CaptureGuard : ScriptedDecisionProvider {
        std::vector<int> candidates;
        std::optional<int> chooseGuard(const GameState&, int,
                                       const std::vector<int>& c) override {
            candidates = c;
            return std::nullopt;
        }
    };

    GameState s = buildInitialState(makeBoard12_GuardWolfGun());
    s.lastGuardedId = 5;  // guarded seat 5 last night
    NightContext ctx;

    CaptureGuard cap;
    Protect noConsecutive(/*allowConsecutiveSameTarget=*/false);
    noConsecutive.actAtNight(ctx, s, *s.find(8), cap);  // seat 8 is the guardian
    EXPECT_EQ(std::count(cap.candidates.begin(), cap.candidates.end(), 5), 0);  // excluded
    EXPECT_NE(std::count(cap.candidates.begin(), cap.candidates.end(), 6), 0);  // others ok

    CaptureGuard cap2;
    Protect allowConsecutive(/*allowConsecutiveSameTarget=*/true);
    allowConsecutive.actAtNight(ctx, s, *s.find(8), cap2);
    EXPECT_NE(std::count(cap2.candidates.begin(), cap2.candidates.end(), 5), 0);  // allowed
}

// ---------- WolfGun (BRD §2) ----------

TEST(WolfGun, SelfKnifeLetsItShoot) {
    // Wolves self-knife the wolfgun (allowed); it dies by knife -> may shoot.
    // Board: 1 wolfgun, 1 werewolf, 2 civilians.
    ScriptedDecisionProvider dp;
    dp.nightKills = {1};   // night1: knife the wolfgun (seat 1)
    dp.hunterShots = {3};  // wolfgun shoots civ3

    Game game(killAll("wg", {{RoleKind::WolfGun, 1}, {RoleKind::Werewolf, 1},
                             {RoleKind::Civilian, 2}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Killed));
    EXPECT_TRUE(game.state().find(3)->hasDeathCause(DeathCause::Shot));  // the shot fired
}

TEST(WolfGun, SelfDestructBlocksTheShot) {
    // Wolfgun self-destructs -> cannot shoot, even if a shot is scripted.
    // Board: 1 wolfgun, 1 werewolf, 3 civilians.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 3, 4};  // 空刀, then whittle town to a wolf win
    dp.selfDestructs = {1};                // day1: wolfgun self-destructs
    dp.hunterShots = {2};                  // must be ignored

    Game game(killAll("wg-sd", {{RoleKind::WolfGun, 1}, {RoleKind::Werewolf, 1},
                                {RoleKind::Civilian, 3}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::BlownUp));
    EXPECT_TRUE(game.state().find(2)->isAlive());          // the other wolf was NOT shot
    EXPECT_FALSE(anyHasCause(game.state(), DeathCause::Shot));
}

TEST(WolfGun, PoisonBlocksTheShot) {
    // Wolfgun knifed (self-knife) AND poisoned -> 同刀同毒, poison blocks the shot.
    // Board: 1 wolfgun, 1 werewolf, 1 witch, 2 civilians.
    ScriptedDecisionProvider dp;
    dp.nightKills = {1, 4, 5};  // n1 self-knife wolfgun ; then whittle to a win
    dp.witchPoisons = {1};      // n1 poison the wolfgun too
    dp.hunterShots = {2};       // must be ignored

    Game game(killAll("wg-p", {{RoleKind::WolfGun, 1}, {RoleKind::Werewolf, 1},
                               {RoleKind::Witch, 1}, {RoleKind::Civilian, 2}}), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Killed));
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Poisoned));
    EXPECT_TRUE(game.state().find(2)->isAlive());  // no shot fired
    EXPECT_FALSE(anyHasCause(game.state(), DeathCause::Shot));
}
