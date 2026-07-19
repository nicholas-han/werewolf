#include <gtest/gtest.h>

#include <optional>
#include <tuple>
#include <vector>

#include "core/abilities/ability.h"
#include "core/abilities/role_abilities.h"
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

TEST(MechanicWolf, LearnNotifiesMechanicOfRole) {
    GameState s = psychicBoard();  // mechanic = seat 4; witch = seat 6
    MechanicLearn learn;
    NightContext ctx;
    ScriptedDecisionProvider dp;

    dp.mechanicLearns = {6};  // learn the witch
    learn.actAtNight(ctx, s, *s.find(4), dp);
    ASSERT_EQ(dp.mechanicLearnResults.size(), 1u);
    const auto& [mechId, targetId, role, stock] = dp.mechanicLearnResults.front();
    EXPECT_EQ(mechId, 4);
    EXPECT_EQ(targetId, 6);
    EXPECT_EQ(role, RoleKind::Witch);
    // Learning the witch also reports the copied potion stock (§2/§11): both
    // potions untouched here.
    ASSERT_TRUE(stock.has_value());
    EXPECT_TRUE(stock->antidote);
    EXPECT_TRUE(stock->poison);

    // Declining to learn delivers no result.
    GameState s2 = psychicBoard();
    ScriptedDecisionProvider dp2;  // empty mechanicLearns -> nullopt -> no learn
    NightContext ctx2;
    learn.actAtNight(ctx2, s2, *s2.find(4), dp2);
    EXPECT_TRUE(dp2.mechanicLearnResults.empty());
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
    HunterGunCheck gc(PoisonReflect::ReflectToPoisoner, /*poisonBlocksShot=*/true);
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

TEST(HunterGunCheck, MechanicWitchPoisonAlsoBlocksShot) {
    // §2/§3: the gun-check must also see the mechanic's learned-witch poison (it now
    // runs after the mechanic acts). hunter = seat 7 on the psychic board.
    GameState s = psychicBoard();
    HunterGunCheck gc(PoisonReflect::ReflectToPoisoner, /*poisonBlocksShot=*/true);
    ScriptedDecisionProvider dp;

    NightContext mechPoison;
    mechPoison.mechPoisonTarget = 7;  // mechanic's witch copy poisons the hunter
    gc.actAtNight(mechPoison, s, *s.find(7), dp);
    ASSERT_EQ(dp.hunterGunChecks.size(), 1u);
    EXPECT_FALSE(dp.hunterGunChecks.back().second);  // will die poisoned -> cannot shoot

    // Edge: a mechanic learned-guard on the hunter reflects the poison away, so he
    // survives and can still shoot (mirrors the dawn reflect rule, §2 机械狼).
    NightContext reflected;
    reflected.poisonTarget = 7;
    reflected.poisonSourceId = 6;
    reflected.mechanicGuardTarget = 7;  // mech-guard protects the hunter -> reflected
    gc.actAtNight(reflected, s, *s.find(7), dp);
    EXPECT_TRUE(dp.hunterGunChecks.back().second);
}

TEST(HunterGunCheck, NoneModeGuardDoesNotSaveFromPoison) {
    // §3: in PoisonReflect::None the mechanic learned-guard blocks nothing, so a
    // guarded-but-poisoned hunter still dies and cannot shoot — the gun-check must
    // mirror the None dawn rule (the opposite of the reflect case above).
    GameState s = psychicBoard();
    HunterGunCheck gc(PoisonReflect::None, /*poisonBlocksShot=*/true);
    ScriptedDecisionProvider dp;

    NightContext guardedButNone;
    guardedButNone.poisonTarget = 7;
    guardedButNone.poisonSourceId = 6;
    guardedButNone.mechanicGuardTarget = 7;  // guarded, but None -> no protection
    gc.actAtNight(guardedButNone, s, *s.find(7), dp);
    ASSERT_EQ(dp.hunterGunChecks.size(), 1u);
    EXPECT_FALSE(dp.hunterGunChecks.back().second);  // dies poisoned -> cannot shoot
}

// ---------- M9 Phase 2: learned active abilities (BRD §2) ----------

TEST(MechanicLearned, HunterShotDeathTimeGatedThenPoisonBlocked) {
    // §2 生效时机: the learned shot is live from the DAY after the learning night,
    // judged by the death stamp — deferred announcement does not matter.
    {   // Died during the learning night itself (e.g. knifed) -> no shot, even
        // though the announcement (and this trigger) runs the next day.
        GameState s = psychicBoard();
        MechanicLearnedShoot shoot;
        ScriptedDecisionProvider dp;
        s.mechanicLearned = RoleKind::Hunter;
        s.mechanicLearnDay = 1;
        Player& mech = *s.find(4);
        mech.recordDeath(DeathCause::Killed, 1, Phase::Night);
        s.day = 1;
        dp.hunterShots = {1};
        std::vector<PendingDeath> out;
        shoot.onDeath(s, mech, dp, out);
        EXPECT_TRUE(out.empty());
    }
    {   // Exiled the very next day (same day number, Day phase) -> the shot fires.
        GameState s = psychicBoard();
        MechanicLearnedShoot shoot;
        ScriptedDecisionProvider dp;
        s.mechanicLearned = RoleKind::Hunter;
        s.mechanicLearnDay = 1;
        Player& mech = *s.find(4);
        mech.recordDeath(DeathCause::Exiled, 1, Phase::Day);
        s.day = 1;
        dp.hunterShots = {1};
        std::vector<PendingDeath> out;
        shoot.onDeath(s, mech, dp, out);
        ASSERT_EQ(out.size(), 1u);
        EXPECT_EQ(out[0].playerId, 1);
        EXPECT_EQ(out[0].cause, DeathCause::Shot);
    }
    {   // Poisoned on a LATER night -> past the gate, but blocked by cause.
        GameState s = psychicBoard();
        MechanicLearnedShoot shoot;
        ScriptedDecisionProvider dp;
        s.mechanicLearned = RoleKind::Hunter;
        s.mechanicLearnDay = 1;
        Player& mech = *s.find(4);
        mech.recordDeath(DeathCause::Poisoned, 2, Phase::Night);
        s.day = 2;
        dp.hunterShots = {1};
        std::vector<PendingDeath> out;
        shoot.onDeath(s, mech, dp, out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(MechanicLearned, GunCheckLearnNightNoThenMirrorsPoison) {
    // §2 每晚验枪 (mechanic flavor): the learning night is always 不可开枪 (the
    // shot goes live the next day); later nights mirror the hunter's poison rule.
    GameState s = psychicBoard();
    MechanicLearnedNightStatus gc(/*poisonBlocksShot=*/true);
    ScriptedDecisionProvider dp;
    s.mechanicLearned = RoleKind::Hunter;
    s.mechanicLearnDay = 1;

    s.day = 1;  // learning night -> always no
    NightContext ctx1;
    gc.actAtNight(ctx1, s, *s.find(4), dp);
    ASSERT_EQ(dp.mechanicGunChecks.size(), 1u);
    EXPECT_EQ(dp.mechanicGunChecks.back().first, 4);
    EXPECT_FALSE(dp.mechanicGunChecks.back().second);

    s.day = 2;  // later night, no poison -> can shoot
    NightContext ctx2;
    gc.actAtNight(ctx2, s, *s.find(4), dp);
    ASSERT_EQ(dp.mechanicGunChecks.size(), 2u);
    EXPECT_TRUE(dp.mechanicGunChecks.back().second);

    NightContext ctx3;  // later night, the real witch poisons the mechanic -> no
    ctx3.poisonTarget = 4;
    gc.actAtNight(ctx3, s, *s.find(4), dp);
    ASSERT_EQ(dp.mechanicGunChecks.size(), 3u);
    EXPECT_FALSE(dp.mechanicGunChecks.back().second);

    // Board where poison does NOT block the learned shot -> gesture stays yes.
    MechanicLearnedNightStatus gcNoBlock(/*poisonBlocksShot=*/false);
    NightContext ctx4;
    ctx4.poisonTarget = 4;
    gcNoBlock.actAtNight(ctx4, s, *s.find(4), dp);
    ASSERT_EQ(dp.mechanicGunChecks.size(), 4u);
    EXPECT_TRUE(dp.mechanicGunChecks.back().second);
}

TEST(MechanicLearned, NightStatusSilentForRolesWithTheirOwnAsks) {
    GameState s = psychicBoard();
    MechanicLearnedNightStatus gc(/*poisonBlocksShot=*/true);
    ScriptedDecisionProvider dp;
    NightContext ctx;
    gc.actAtNight(ctx, s, *s.find(4), dp);  // nothing learned -> silent
    s.mechanicLearned = RoleKind::Witch;    // learned witch: its own component touches
    s.mechanicLearnDay = 1;
    s.day = 2;
    gc.actAtNight(ctx, s, *s.find(4), dp);
    EXPECT_TRUE(dp.mechanicGunChecks.empty());
    EXPECT_TRUE(dp.events.empty());
}

TEST(MechanicLearned, NightStatusNeutralTouchForCivilianAndWolf) {
    // §11: a mechanic that learned 平民/狼人 has no nightly ask — it still gets one
    // private touch per night, so the visible flow cannot partition learned roles.
    GameState s = psychicBoard();
    MechanicLearnedNightStatus gc(/*poisonBlocksShot=*/true);
    ScriptedDecisionProvider dp;
    s.mechanicLearned = RoleKind::Civilian;
    s.mechanicLearnDay = 1;
    s.day = 1;  // fires from the learning night onward
    NightContext ctx;
    gc.actAtNight(ctx, s, *s.find(4), dp);
    ASSERT_EQ(dp.events.size(), 1u);
    EXPECT_EQ(dp.events.back(), txt::mechanicNoNightAction());
    EXPECT_TRUE(dp.mechanicGunChecks.empty());  // not a gun gesture

    s.mechanicLearned = RoleKind::Werewolf;
    s.day = 2;
    gc.actAtNight(ctx, s, *s.find(4), dp);
    EXPECT_EQ(dp.events.size(), 2u);

    // Lone mechanic: its own knife (MechanicLoneKill) already asks tonight, so
    // the neutral notice is skipped — no double touch, no false "无需行动".
    for (int w : {1, 2, 3}) s.find(w)->recordDeath(DeathCause::Exiled, 2, Phase::Day);
    gc.actAtNight(ctx, s, *s.find(4), dp);
    EXPECT_EQ(dp.events.size(), 2u);  // unchanged
}

TEST(MechanicLearned, ShotFailsClosedWithoutLearnDay) {
    // The learn day is always recorded with the learned role, but if that invariant
    // ever slips the gate must block the shot, not skip itself.
    GameState s = psychicBoard();
    MechanicLearnedShoot shoot;
    ScriptedDecisionProvider dp;
    s.mechanicLearned = RoleKind::Hunter;  // learnDay deliberately unset
    Player& mech = *s.find(4);
    mech.recordDeath(DeathCause::Exiled, 1, Phase::Day);
    s.day = 1;
    dp.hunterShots = {1};
    std::vector<PendingDeath> out;
    shoot.onDeath(s, mech, dp, out);
    EXPECT_TRUE(out.empty());
}

TEST(MechanicLearned, WitchUniformFlowNotices) {
    // §2/§11 uniform flow, learned-witch flavor: antidote in hand + 空刀 -> explicit
    // 无人被刀; both copied potions gone -> a neutral nightly touch, never silence.
    GameState s = psychicBoard();
    MechanicLearnedWitch mw(/*bothPotionsSameNight=*/false);
    ScriptedDecisionProvider dp;
    s.mechanicLearned = RoleKind::Witch;
    s.mechanicLearnDay = 1;
    s.day = 2;

    s.mechanicAntidoteAvailable = true;
    s.mechanicPoisonAvailable = true;
    NightContext noKnife;  // 空刀 night
    mw.actAtNight(noKnife, s, *s.find(4), dp);
    ASSERT_EQ(dp.events.size(), 1u);
    EXPECT_EQ(dp.events.back(), txt::witchNoKnife());

    s.mechanicAntidoteAvailable = false;
    s.mechanicPoisonAvailable = false;
    NightContext spent;
    mw.actAtNight(spent, s, *s.find(4), dp);
    ASSERT_EQ(dp.events.size(), 2u);
    EXPECT_EQ(dp.events.back(), txt::witchNoPotionsLeft());
}

TEST(Game, LearnedHunterShootsWhenExiledTheNextDay) {
    // Learn the hunter on night 1 -> exiled on the following day -> the shot IS
    // available (§2 生效时机: live from the day after the learning night).
    Board board;
    board.name = "mech-hunter-exile";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1},
                    {RoleKind::Hunter, 1},   {RoleKind::Civilian, 2}};
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt};      // n1 空刀
    dp.mechanicLearns = {3};             // n1 learn the hunter
    dp.votes = {2, 2, 2, 2, 2};          // day1 exile the mechanic
    dp.hunterShots = {1};                // its learned shot takes the werewolf

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);  // both wolves out
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Shot));
    // The learning night's gun gesture said 不可开枪 (shot not live that night).
    ASSERT_EQ(dp.mechanicGunChecks.size(), 1u);
    EXPECT_EQ(dp.mechanicGunChecks[0], (std::pair<int, bool>{2, false}));
}

TEST(Game, LearnedHunterCannotShootWhenPoisonedOnLearningNight) {
    // The witch poisons the mechanic the very night it learns the hunter -> at the
    // (deferred) morning announcement the shot never fires (§2 生效时机 + 带毒禁枪).
    Board board;
    board.name = "mech-hunter-poisoned";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1},
                    {RoleKind::Witch, 1},    {RoleKind::Hunter, 1},
                    {RoleKind::Civilian, 2}};
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt};      // n1 空刀
    dp.witchPoisons = {2};               // n1 the witch poisons the mechanic
    dp.mechanicLearns = {4};             // n1 (after the witch) it learns the hunter
    dp.hunterShots = {1};                // must never be consumed
    dp.votes = {1, 1, 1, 1, 1};          // day1 exile the werewolf -> town wins

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    const Player* mech = game.state().find(2);
    EXPECT_TRUE(mech->hasDeathCause(DeathCause::Poisoned));
    EXPECT_EQ(dp.hunterShots.size(), 1u);  // the learned shot was never offered
    for (const Player& p : game.state().players) {
        EXPECT_FALSE(p.hasDeathCause(DeathCause::Shot));
    }
    // Learning night gesture: 不可开枪.
    ASSERT_EQ(dp.mechanicGunChecks.size(), 1u);
    EXPECT_FALSE(dp.mechanicGunChecks[0].second);
    // The learn result itself still arrived (role only — no potion stock).
    ASSERT_EQ(dp.mechanicLearnResults.size(), 1u);
    EXPECT_EQ(std::get<2>(dp.mechanicLearnResults[0]), RoleKind::Hunter);
    EXPECT_FALSE(std::get<3>(dp.mechanicLearnResults[0]).has_value());
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
    // The learn result reports exactly the copied stock (§2/§11).
    ASSERT_EQ(dp.mechanicLearnResults.size(), 1u);
    const auto& stock = std::get<3>(dp.mechanicLearnResults.front());
    ASSERT_TRUE(stock.has_value());
    EXPECT_FALSE(stock->antidote);
    EXPECT_TRUE(stock->poison);

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

TEST(Game, BigKnifeUnsaveableAndHiddenFromWitch) {
    // §2: 破盾大刀 is a guaranteed kill — it ignores the guard AND the witch's
    // antidote, and the witch is never told its target. seats: 1 wolf, 2 mechanic,
    // 3 witch, 4-5 civ. After learning wolf and exiling the wolf, the lone mechanic
    // 空刀 on its normal knife but fires the big knife at civ 5; the witch has her
    // antidote and would save anyone she's offered — but she is never offered the
    // big-knife target, so civ 5 dies and her antidote stays unused.
    Board board;
    board.name = "bigknife-unsaveable";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Witch, 1},
                    {RoleKind::Civilian, 2}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {1};                       // n1 learn the werewolf -> gains 大刀
    dp.nightKills = {std::nullopt, std::nullopt};   // n1 空刀 ; n2 normal lone knife 空刀
    dp.mechanicBigKnives = {5};                    // n2 大刀 -> civ 5 (no normal target)
    dp.witchSaves = {true};                        // she'd save if ever asked — but isn't
    dp.votes = {2, 1, 1, 1, 1, /*day2*/ 3, 2, 2};  // d1 exile wolf(1); d2 exile mechanic(2)

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_FALSE(game.state().find(5)->isAlive());                          // big knife killed it
    EXPECT_TRUE(game.state().find(5)->hasDeathCause(DeathCause::Killed));
    EXPECT_TRUE(game.state().witchAntidoteAvailable);  // witch was never offered the big-knife target
}

TEST(Game, MechanicBigKnifeRespectsGuardWhenConfigured) {
    // §2 config: bigKnifePiercesGuard=false -> a guarded target survives the 破盾大刀.
    // wolf, mechanic, guardian, civ (KillAll, sheriff off). After learning the wolf and
    // exiling it, the lone mechanic big-knifes the guarded civ4 (survives); the town
    // then exiles the mechanic for the win, leaving civ4 alive to prove it survived.
    Board board;
    board.name = "bigknife-respects-guard";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1},
                    {RoleKind::Guardian, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;
    board.config.mechanic.bigKnifePiercesGuard = false;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {1};                       // n1 learn the werewolf -> gains 大刀
    dp.guards = {std::nullopt, 4};                 // n1 空守 ; n2 guardian guards civ4
    dp.nightKills = {std::nullopt, std::nullopt};   // n1 团刀空 ; n2 lone 普通刀空
    dp.mechanicBigKnives = {4};                    // n2 大刀 -> the guarded civ4
    dp.votes = {1, 1, 1, 1, /*day2*/ 2, 2, 2};      // d1 exile wolf(1) ; d2 exile mechanic(2)

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_TRUE(game.state().find(4)->isAlive());                          // guard stopped the big knife
    EXPECT_FALSE(game.state().find(4)->hasDeathCause(DeathCause::Killed));
}

TEST(Game, MechanicBigKnifeSaveableWhenConfigured) {
    // §2 config: bigKnifePiercesAntidote=false -> the witch IS offered the 破盾大刀
    // target and a save blocks it. wolf, mechanic, witch, civ (KillAll, sheriff off).
    Board board;
    board.name = "bigknife-saveable";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1},
                    {RoleKind::Witch, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;
    board.config.mechanic.bigKnifePiercesAntidote = false;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {1};                       // n1 learn the werewolf -> gains 大刀
    dp.nightKills = {std::nullopt, std::nullopt};   // n1 团刀空 ; n2 lone 普通刀空
    dp.mechanicBigKnives = {4};                    // n2 大刀 -> civ4
    dp.witchSaves = {true};                        // n2 witch saves the offered big-knife target
    dp.votes = {1, 1, 1, 1, /*day2*/ 2, 2, 2};      // d1 exile wolf(1) ; d2 exile mechanic(2)

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_TRUE(game.state().find(4)->isAlive());                          // antidote stopped the big knife
    EXPECT_FALSE(game.state().find(4)->hasDeathCause(DeathCause::Killed));
    EXPECT_FALSE(game.state().witchAntidoteAvailable);                     // antidote was spent on the save
}

// The mechanic learned-guard vs. poison rule has three board-configurable modes
// (§3, BoardConfig::mechanic.poisonReflect). The three tests below share a board —
// werewolf, mechanic, witch, guardian, civilian (KillAll, sheriff off) — where on
// n2 the mechanic (having learned the guardian on n1) protects civ5 and the witch
// poisons civ5; only the mode changes who lives and dies.

TEST(Game, MechanicGuardReflectsPoison) {
    Board board;
    board.name = "reflect";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Witch, 1},
                    {RoleKind::Guardian, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;
    board.config.mechanic.poisonReflect = PoisonReflect::ReflectToPoisoner;

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

TEST(Game, MechanicGuardProtectOnlyPoison) {
    // ProtectOnly: the protected civ5 lives, but the poison does NOT reflect — the
    // witch is unharmed. With nobody dying on n2, the wolves knife civ5 then the
    // guardian on n3/n4 to reach the vote-binding win (§4.3).
    Board board;
    board.name = "protect-only";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Witch, 1},
                    {RoleKind::Guardian, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;
    board.config.mechanic.poisonReflect = PoisonReflect::ProtectOnly;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {4};  // n1 learn the guardian
    // guards: n1 real ; n2 real, mech(5) ; n3 real, mech ; n4 real, mech.
    dp.guards = {std::nullopt, std::nullopt, 5, std::nullopt, std::nullopt,
                 std::nullopt, std::nullopt};
    dp.witchPoisons = {std::nullopt, 5};                       // n2 poison the protected civ5
    dp.witchSaves = {false, false};                           // n3/n4: witch declines to save
    dp.nightKills = {std::nullopt, std::nullopt, 5, 4};        // n3 knife civ5 ; n4 knife guardian

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(3)->isAlive());                            // witch NOT reflected
    EXPECT_FALSE(game.state().find(3)->hasDeathCause(DeathCause::Poisoned));
    EXPECT_FALSE(game.state().find(5)->hasDeathCause(DeathCause::Poisoned)); // poison was nullified
    EXPECT_TRUE(game.state().find(5)->hasDeathCause(DeathCause::Killed));    // civ5 died later by knife
}

TEST(Game, MechanicGuardNonePoison) {
    // None: the mechanic learned-guard blocks nothing, so the poisoned civ5 dies and
    // the witch is unharmed (no reflect). The guardian is knifed on n3 for the win.
    Board board;
    board.name = "none";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::MechanicWolf, 1}, {RoleKind::Witch, 1},
                    {RoleKind::Guardian, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;
    board.config.mechanic.poisonReflect = PoisonReflect::None;

    ScriptedDecisionProvider dp;
    dp.mechanicLearns = {4};  // n1 learn the guardian
    dp.guards = {std::nullopt, std::nullopt, 5, std::nullopt, std::nullopt};
    dp.witchPoisons = {std::nullopt, 5};                       // n2 poison civ5 (guard blocks nothing)
    dp.witchSaves = {false};                                  // n3: witch declines to save guardian
    dp.nightKills = {std::nullopt, std::nullopt, 4};          // n3 knife guardian -> wolf win

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_FALSE(game.state().find(5)->isAlive());                          // poisoned civ5 died
    EXPECT_TRUE(game.state().find(5)->hasDeathCause(DeathCause::Poisoned));
    EXPECT_TRUE(game.state().find(3)->isAlive());                           // witch NOT reflected
    EXPECT_FALSE(game.state().find(3)->hasDeathCause(DeathCause::Poisoned));
}
