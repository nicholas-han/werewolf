#include <gtest/gtest.h>

#include <optional>
#include <tuple>
#include <vector>

#include "core/abilities/ability.h"
#include "core/abilities/role_abilities.h"
#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "core/player.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

// Board12_PsychicMechanic seats: 1-3 Werewolf, 4 MechanicWolf, 5 Psychic, 6 Witch,
// 7 Hunter, 8 Guardian, 9-12 Civilian.
GameState psychicBoard() { return buildInitialState(makeBoard12_PsychicMechanic()); }

RoleKind lastPsychic(const ScriptedDecisionProvider& dp) {
    return std::get<2>(dp.psychicResults.back());
}

}  // namespace

TEST(PsychicMechanic, BoardComposition) {
    Board b = makeBoard12_PsychicMechanic();
    EXPECT_EQ(b.totalPlayers(), 12);
    GameState s = buildInitialState(b);
    EXPECT_EQ(s.countAlive(Faction::Wolf), 4);       // 3 werewolf + mechanic
    EXPECT_EQ(s.countAliveOpenWolves(), 3);          // mechanic excluded
    EXPECT_EQ(s.countAlive(SubKind::PowerRole), 4);  // psychic/witch/hunter/guard
    EXPECT_EQ(s.countAlive(SubKind::Civilian), 4);
    EXPECT_EQ(s.countAliveRole(RoleKind::Psychic), 1);
    EXPECT_EQ(s.countAliveRole(RoleKind::MechanicWolf), 1);
}

// ---------- Psychic (BRD §2) ----------

TEST(Psychic, RevealsExactRole) {
    GameState s = psychicBoard();
    PsychicInspect psy;
    NightContext ctx;
    ScriptedDecisionProvider dp;

    dp.inspects = {6};  // witch
    psy.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(lastPsychic(dp), RoleKind::Witch);

    dp.inspects = {1};  // werewolf
    psy.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(lastPsychic(dp), RoleKind::Werewolf);
}

// ---------- MechanicWolf disguise + learn (BRD §2) ----------

TEST(MechanicWolf, DisguiseShownToPsychic) {
    GameState s = psychicBoard();
    PsychicInspect psy;
    NightContext ctx;
    ScriptedDecisionProvider dp;

    // Not learned -> the psychic sees "MechanicWolf".
    dp.inspects = {4};
    psy.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(lastPsychic(dp), RoleKind::MechanicWolf);

    // After learning a civilian, the psychic sees Civilian.
    s.mechanicLearned = RoleKind::Civilian;
    dp.inspects = {4};
    psy.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(lastPsychic(dp), RoleKind::Civilian);
}

TEST(MechanicWolf, LearnRecordsRoleOnceOnly) {
    GameState s = psychicBoard();
    MechanicLearn learn;
    NightContext ctx;
    ScriptedDecisionProvider dp;

    dp.mechanicLearns = {9};  // learn a civilian
    learn.actAtNight(ctx, s, *s.find(4), dp);
    ASSERT_TRUE(s.mechanicLearned.has_value());
    EXPECT_EQ(*s.mechanicLearned, RoleKind::Civilian);

    dp.mechanicLearns = {6};  // a later attempt must be ignored (global once)
    learn.actAtNight(ctx, s, *s.find(4), dp);
    EXPECT_EQ(*s.mechanicLearned, RoleKind::Civilian);
}

// ---------- MechanicWolf lone knife (BRD §2) ----------

TEST(MechanicWolf, LoneKnifeOnlyAfterOtherWolvesDie) {
    GameState s = psychicBoard();
    MechanicLoneKill lone;
    ScriptedDecisionProvider dp;
    dp.nightKills = {9};

    NightContext c1;
    lone.actAtNight(c1, s, *s.find(4), dp);  // werewolves 1-3 still alive
    EXPECT_FALSE(c1.wolfTarget.has_value());  // mechanic does NOT knife yet

    s.find(1)->recordDeath(DeathCause::Exiled, 1);
    s.find(2)->recordDeath(DeathCause::Exiled, 1);
    s.find(3)->recordDeath(DeathCause::Exiled, 1);

    NightContext c2;
    lone.actAtNight(c2, s, *s.find(4), dp);  // now the lone wolf
    EXPECT_EQ(c2.wolfTarget, std::optional<int>(9));
}

TEST(Game, MechanicLoneKnifesAfterTeamGone) {
    // KillAll, sheriff off: 1 werewolf + 1 mechanic + 3 civilians.
    Board board;
    board.name = "mech-lone";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Civilian, 3}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.nightKills = {3, 4, 5};   // n1 team knife civ3 ; n2/n3 mechanic lone-knifes
    dp.votes = {1, 1, 1, 1};     // day1 exiles the werewolf (seat 1)

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(2)->isAlive());  // the mechanic survives
    EXPECT_TRUE(game.state().find(4)->hasDeathCause(DeathCause::Killed));  // lone-knife kills
    EXPECT_TRUE(game.state().find(5)->hasDeathCause(DeathCause::Killed));
}

// ---------- MechanicWolf excluded from vote-binding parity (BRD §4.3) ----------

TEST(WinCondition, MechanicNotCountedForVoteBinding) {
    BoardConfig killAll;
    killAll.winRule = WinRule::KillAll;  // isolate parity from 屠边

    // Keep only the mechanic (4) + one civilian (9): open wolves = 0 vs town = 1.
    GameState s1 = psychicBoard();
    for (Player& p : s1.players) {
        if (p.id() != 4 && p.id() != 9) p.recordDeath(DeathCause::Exiled, 1);
    }
    EXPECT_EQ(s1.countAliveOpenWolves(), 0);
    EXPECT_EQ(evaluateWin(s1, killAll), GameResult::Ongoing);  // mechanic alone can't bind

    // Contrast: a normal werewolf (1) + one civilian (9): 1 >= 1 -> wolves win.
    GameState s2 = psychicBoard();
    for (Player& p : s2.players) {
        if (p.id() != 1 && p.id() != 9) p.recordDeath(DeathCause::Exiled, 1);
    }
    EXPECT_EQ(evaluateWin(s2, killAll), GameResult::WolfWins);
}

// ---------- Hunter nightly gun-check gesture (BRD §2/§5.1) ----------

TEST(HunterGunCheck, ReflectsTonightsPoison) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());  // hunter is seat 6
    HunterGunCheck gc;
    ScriptedDecisionProvider dp;

    NightContext clean;  // nobody poisoned -> can shoot
    gc.actAtNight(clean, s, *s.find(6), dp);
    ASSERT_EQ(dp.hunterGunChecks.size(), 1u);
    EXPECT_TRUE(dp.hunterGunChecks.back().second);

    NightContext poisoned;
    poisoned.poisonTarget = 6;  // witch poisons the hunter tonight -> cannot shoot
    gc.actAtNight(poisoned, s, *s.find(6), dp);
    EXPECT_FALSE(dp.hunterGunChecks.back().second);
}

// ---------- M9 Phase 2: learned active abilities (BRD §2) ----------

TEST(MechanicLearned, HunterShotGatedThenPoisonBlocked) {
    GameState s = psychicBoard();
    MechanicLearnedShoot shoot;
    ScriptedDecisionProvider dp;
    s.mechanicLearned = RoleKind::Hunter;
    s.mechanicLearnDay = 1;
    Player& mech = *s.find(4);

    s.day = 1;  // learn night -> not active yet
    dp.hunterShots = {1};
    {
        std::vector<PendingDeath> out;
        shoot.onDeath(s, mech, dp, out);
        EXPECT_TRUE(out.empty());
    }

    s.day = 2;  // next night -> active, not poisoned -> shoots
    {
        std::vector<PendingDeath> out;
        shoot.onDeath(s, mech, dp, out);
        ASSERT_EQ(out.size(), 1u);
        EXPECT_EQ(out[0].playerId, 1);
        EXPECT_EQ(out[0].cause, DeathCause::Shot);
    }

    mech.recordDeath(DeathCause::Poisoned, 2);  // poisoned -> blocked
    dp.hunterShots = {1};
    {
        std::vector<PendingDeath> out;
        shoot.onDeath(s, mech, dp, out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(MechanicLearned, PsychicInspectGatedToNextNight) {
    GameState s = psychicBoard();
    MechanicLearnedInspect insp;
    NightContext ctx;
    ScriptedDecisionProvider dp;
    s.mechanicLearned = RoleKind::Psychic;
    s.mechanicLearnDay = 1;
    dp.inspects = {6};  // witch

    s.day = 1;  // learn night -> inactive, no result and no consumption
    insp.actAtNight(ctx, s, *s.find(4), dp);
    EXPECT_TRUE(dp.psychicResults.empty());

    s.day = 2;  // next night -> active
    insp.actAtNight(ctx, s, *s.find(4), dp);
    ASSERT_EQ(dp.psychicResults.size(), 1u);
    EXPECT_EQ(std::get<2>(dp.psychicResults.back()), RoleKind::Witch);
}

TEST(MechanicLearned, WitchPotionsCopiedAtLearnAndIndependent) {
    GameState s = psychicBoard();
    s.witchAntidoteAvailable = false;  // the real witch already used her antidote
    s.witchPoisonAvailable = true;

    MechanicLearn learn;
    NightContext lctx;
    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {6};  // learn the witch
    s.day = 1;
    learn.actAtNight(lctx, s, *s.find(4), dp);
    ASSERT_EQ(s.mechanicLearned, std::optional<RoleKind>(RoleKind::Witch));
    EXPECT_FALSE(s.mechanicAntidoteAvailable);  // copied current stock: no antidote
    EXPECT_TRUE(s.mechanicPoisonAvailable);     // copied current stock: poison

    MechanicLearnedWitch mw(/*bothPotionsSameNight=*/false);
    NightContext nctx;
    dp.witchPoisons = {1};
    s.day = 2;  // next night -> active
    mw.actAtNight(nctx, s, *s.find(4), dp);
    EXPECT_EQ(nctx.mechPoisonTarget, std::optional<int>(1));
    EXPECT_EQ(nctx.mechPoisonSourceId, std::optional<int>(4));
    EXPECT_FALSE(s.mechanicPoisonAvailable);
}

TEST(Game, MechanicBigKnifePiercesGuard) {
    // KillAll, sheriff off: werewolf, mechanic, guardian, civilian.
    Board board;
    board.name = "bigknife";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1},
                    {RoleKind::Guardian, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {1};                // n1 learn the werewolf -> gains 大刀
    dp.nightKills = {std::nullopt, 3};       // n1 空刀 ; n2 mechanic 普通刀 -> guard 3
    dp.mechanicBigKnives = {4};             // n2 大刀 -> the guarded civ 4
    dp.guards = {2, 4};                      // n1 guard seat 2 ; n2 guardian guards civ 4
    dp.votes = {1, 1, 1, 1};                 // day1 exile the werewolf -> mechanic is lone

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    // civ4 was guarded yet the 破盾大刀 still killed it.
    EXPECT_TRUE(game.state().find(4)->hasDeathCause(DeathCause::Killed));
}

TEST(Game, MechanicGuardReflectsPoison) {
    // KillAll, sheriff off: werewolf, mechanic, witch, guardian, civilian.
    Board board;
    board.name = "reflect";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Witch, 1},
                    {RoleKind::Guardian, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {4};  // n1 learn the guardian (gains reflecting protect)
    // guards consumed: n1 real-guard ; n2 real-guard, mechanic-guard ; n3 same.
    dp.guards = {std::nullopt, std::nullopt, 5, std::nullopt, std::nullopt};
    dp.witchPoisons = {std::nullopt, 5};        // n2: witch poisons the protected civ5
    dp.nightKills = {std::nullopt, std::nullopt, 4};  // n3 knife guardian -> wolf win

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(3)->hasDeathCause(DeathCause::Poisoned));  // witch reflected dead
    EXPECT_TRUE(game.state().find(5)->isAlive());                            // protected civ survived
}
