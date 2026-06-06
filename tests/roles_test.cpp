#include <gtest/gtest.h>

#include <algorithm>
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
#include "core/messages.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

// board9 seats: 1-3 Werewolf, 4 Seer, 5 Witch, 6 Hunter, 7-9 Civilian.
GameState board9State() { return buildInitialState(makeBoard9_SeerWitchHunter()); }

}  // namespace

// ---------- Ability-level unit tests (BRD §2) ----------

TEST(Inspect, RevealsWolfOrTownPrivatelyToSeer) {
    GameState s = board9State();
    Inspect inspect;
    NightContext ctx;
    ScriptedDecisionProvider dp;

    dp.inspects = {1};  // seat 1 is a wolf
    inspect.actAtNight(ctx, s, *s.find(4), dp);
    ASSERT_EQ(dp.inspectResults.size(), 1u);
    EXPECT_EQ(std::get<0>(dp.inspectResults[0]), 4);   // seer id
    EXPECT_EQ(std::get<1>(dp.inspectResults[0]), 1);   // target
    EXPECT_TRUE(std::get<2>(dp.inspectResults[0]));    // isWolf

    dp.inspects = {7};  // seat 7 is a civilian
    inspect.actAtNight(ctx, s, *s.find(4), dp);
    ASSERT_EQ(dp.inspectResults.size(), 2u);
    EXPECT_FALSE(std::get<2>(dp.inspectResults[1]));   // not a wolf
}

TEST(Witch, AntidoteRescuesKnifedPlayer) {
    GameState s = board9State();
    WitchPotions witch(WitchSelfRescue::Never, /*bothPotionsSameNight=*/false);
    NightContext ctx;
    ctx.wolvesActed = true;
    ctx.wolfTarget = 7;  // wolves knifed a civilian
    ScriptedDecisionProvider dp;
    dp.witchSaves = {true};

    witch.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(ctx.savedTarget, std::optional<int>(7));
    EXPECT_FALSE(s.witchAntidoteAvailable);
}

TEST(Witch, PoisonMarksTarget) {
    GameState s = board9State();
    WitchPotions witch(WitchSelfRescue::Never, false);
    NightContext ctx;  // nobody knifed
    ScriptedDecisionProvider dp;
    dp.witchPoisons = {8};

    witch.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(ctx.poisonTarget, std::optional<int>(8));
    EXPECT_FALSE(s.witchPoisonAvailable);
}

TEST(Witch, OnePotionPerNight_SaveBlocksPoison) {
    GameState s = board9State();
    WitchPotions witch(WitchSelfRescue::Never, /*bothPotionsSameNight=*/false);
    NightContext ctx;
    ctx.wolvesActed = true;
    ctx.wolfTarget = 7;
    ScriptedDecisionProvider dp;
    dp.witchSaves = {true};
    dp.witchPoisons = {8};  // should NOT be consumed

    witch.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_EQ(ctx.savedTarget, std::optional<int>(7));
    EXPECT_FALSE(ctx.poisonTarget.has_value());      // poison blocked this night
    EXPECT_EQ(dp.witchPoisons.size(), 1u);           // not asked
    EXPECT_TRUE(s.witchPoisonAvailable);             // still in stock
}

TEST(Witch, NeverSelfRescue) {
    GameState s = board9State();
    WitchPotions witch(WitchSelfRescue::Never, false);
    NightContext ctx;
    ctx.wolvesActed = true;
    ctx.wolfTarget = 5;  // the witch herself is knifed
    ScriptedDecisionProvider dp;
    dp.witchSaves = {true};  // even if scripted, must not apply

    witch.actAtNight(ctx, s, *s.find(5), dp);
    EXPECT_FALSE(ctx.savedTarget.has_value());
    EXPECT_TRUE(s.witchAntidoteAvailable);  // antidote untouched
    EXPECT_EQ(dp.witchSaves.size(), 1u);    // not even asked
}

// ---------- Game-level integration (BRD §2/§4.2/§5.2) ----------

TEST(Game, HunterShootsWhenExiled) {
    // KillAll board: 1 wolf, 1 hunter, 2 civilians. seats: 1 wolf, 2 hunter, 3/4 civ.
    Board board;
    board.name = "hunter-exile";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Hunter, 1}, {RoleKind::Civilian, 2}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;  // isolate ability behaviour from the election

    ScriptedDecisionProvider dp;
    // Night 1: 空刀. Day 1: everyone votes out the hunter (seat 2); hunter shoots
    // the wolf (seat 1) -> wolves = 0 -> TownWins.
    dp.votes = {2, 2, 2, 2};
    dp.hunterShots = {1};

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::TownWins);
    EXPECT_FALSE(game.state().find(1)->isAlive());  // wolf shot
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::Shot));
    EXPECT_TRUE(game.state().find(2)->hasDeathCause(DeathCause::Exiled));
}

TEST(Game, HunterCannotShootWhenPoisoned) {
    // KillAll: 1 wolf, 1 hunter, 1 witch, 2 civilians.
    // seats: 1 wolf, 2 hunter, 3 witch, 4/5 civ.
    Board board;
    board.name = "hunter-poisoned";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Hunter, 1},
                    {RoleKind::Witch, 1}, {RoleKind::Civilian, 2}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;  // isolate ability behaviour from the election

    ScriptedDecisionProvider dp;
    // Night 1: wolves knife civ 4; witch poisons hunter 2. Hunter dies poisoned ->
    // cannot shoot (BRD §2) even though a shot is scripted.
    // Night 2: wolves knife civ 5 -> only witch left vs 1 wolf -> parity WolfWins.
    dp.nightKills = {4, 5};
    dp.witchPoisons = {2};
    dp.hunterShots = {1};  // must be ignored

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->isAlive());  // wolf NOT shot
    EXPECT_TRUE(game.state().find(2)->hasDeathCause(DeathCause::Poisoned));
}

TEST(Game, KnifeAndPoisonRecordBothCausesAndBlockShot) {
    // 同刀同毒 on the hunter: both causes recorded, poison blocks the shot (§5.2/§2).
    // KillAll: 1 wolf, 1 witch, 1 hunter, 1 civilian. seats: 1 wolf, 2 witch, 3 hunter, 4 civ.
    Board board;
    board.name = "knife-and-poison";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Witch, 1},
                    {RoleKind::Hunter, 1}, {RoleKind::Civilian, 1}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;  // isolate ability behaviour from the election

    ScriptedDecisionProvider dp;
    // Night 1: wolves knife hunter 3; witch poisons hunter 3 too.
    // Night 2: wolves knife civ 4 -> only witch left vs wolf -> parity WolfWins.
    dp.nightKills = {3, 4};
    dp.witchPoisons = {3};
    dp.hunterShots = {1};  // must be ignored (poison present)

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(3)->hasDeathCause(DeathCause::Killed));
    EXPECT_TRUE(game.state().find(3)->hasDeathCause(DeathCause::Poisoned));
    EXPECT_TRUE(game.state().find(1)->isAlive());  // wolf NOT shot
}

TEST(Game, GameDecidedOnLethalBlowSkipsHunterTrigger) {
    // §4.2: when the hunter's death itself decides the game, the chain stops and
    // the hunter never shoots. Board9: kill seer, witch, then hunter (last god).
    ScriptedDecisionProvider dp;
    dp.nightKills = {4, 5, 6};  // seer, witch, hunter over three nights
    dp.hunterShots = {1};       // scripted, but must never fire

    Game game(makeBoard9_SeerWitchHunter(), dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_FALSE(game.state().find(6)->isAlive());  // hunter out
    EXPECT_TRUE(game.state().find(1)->isAlive());   // wolf NOT shot
}

TEST(Game, SelfDestructEndsDayWithNoVote) {
    // KillAll: 2 wolves + 3 civilians (avoids an immediate parity win at night 1).
    // seats: 1/2 wolf, 3/4/5 civ.
    Board board;
    board.name = "self-destruct";
    board.roster = {{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 3}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;  // isolate ability behaviour from the election

    ScriptedDecisionProvider dp;
    // Night 1: 空刀. Day 1: wolf seat 1 self-destructs -> day ends immediately, no vote.
    // Nights 2-3: wolves knife civ 3 then civ 4 -> 1 wolf vs 1 civ -> parity WolfWins.
    dp.nightKills = {std::nullopt, 3, 4};
    dp.selfDestructs = {1};

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_TRUE(game.state().find(1)->hasDeathCause(DeathCause::BlownUp));

    // Day 1 went straight to the self-destruct, before any exile vote (§2/§5.3):
    // the BlownUp death must precede the first "【放逐投票】" header (day 2's).
    auto blown = std::find(dp.events.begin(), dp.events.end(), txt::out("P1", txt::cause(DeathCause::BlownUp)));
    ASSERT_NE(blown, dp.events.end());
    auto firstVote = std::find(dp.events.begin(), dp.events.end(), "【放逐投票】");
    EXPECT_TRUE(firstVote == dp.events.end() ||
                (blown - dp.events.begin()) < (firstVote - dp.events.begin()));
}
