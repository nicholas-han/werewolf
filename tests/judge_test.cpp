#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/game.h"
#include "flow/last_words.h"
#include "flow/speech_order.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

Board killAll(std::string name, std::vector<RoleSlot> roster) {
    Board b;
    b.name = std::move(name);
    b.roster = std::move(roster);
    b.config.winRule = WinRule::KillAll;
    b.config.sheriffEnabled = false;  // isolate last-words cues from the election
    return b;
}

bool anyEventContains(const ScriptedDecisionProvider& dp, const std::string& sub) {
    for (const std::string& e : dp.events) {
        if (e.find(sub) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

// ---------- ② Last-words predicate (BRD §5.3) ----------

TEST(LastWords, DerivedFromPhaseAndCause) {
    EXPECT_TRUE(hasLastWords(Phase::Day, 3, {DeathCause::Exiled}));   // daytime exile
    EXPECT_TRUE(hasLastWords(Phase::Day, 3, {DeathCause::Shot}));     // daytime hunter shot
    EXPECT_FALSE(hasLastWords(Phase::Day, 3, {DeathCause::BlownUp})); // self-destruct: none
    EXPECT_TRUE(hasLastWords(Phase::Night, 1, {DeathCause::Killed})); // first night
    EXPECT_FALSE(hasLastWords(Phase::Night, 2, {DeathCause::Killed})); // later nights: none
}

// ---------- ① Setup: explicit seat->role assignment ----------

TEST(Setup, BuildsFromExplicitSeatRoles) {
    Board board = makeBoard9_SeerWitchHunter();
    std::vector<RoleKind> seats = {
        RoleKind::Civilian, RoleKind::Werewolf, RoleKind::Seer,    RoleKind::Civilian,
        RoleKind::Witch,    RoleKind::Werewolf, RoleKind::Hunter,  RoleKind::Werewolf,
        RoleKind::Civilian};
    EXPECT_TRUE(seatRolesMatchRoster(board, seats));

    GameState s = buildInitialState(board, seats);
    ASSERT_EQ(s.players.size(), 9u);
    EXPECT_EQ(s.players[0].role().kind(), RoleKind::Civilian);  // seat 1
    EXPECT_EQ(s.players[1].role().kind(), RoleKind::Werewolf);  // seat 2
    EXPECT_EQ(s.countAlive(Faction::Wolf), 3);
    EXPECT_EQ(s.countAlive(SubKind::PowerRole), 3);
}

TEST(Setup, RejectsMismatchedRoster) {
    Board board = makeBoard9_SeerWitchHunter();
    std::vector<RoleKind> wrong(9, RoleKind::Civilian);  // 9 civilians != roster
    EXPECT_FALSE(seatRolesMatchRoster(board, wrong));
}

TEST(Setup, RandomDealIsAValidPermutation) {
    Board board = makeBoard9_SeerWitchHunter();
    std::vector<RoleKind> deal = randomDeal(board, 12345u);
    EXPECT_EQ(deal.size(), 9u);
    EXPECT_TRUE(seatRolesMatchRoster(board, deal));        // valid permutation of roster
    EXPECT_EQ(deal, randomDeal(board, 12345u));            // reproducible per seed
}

// ---------- ② Random speech order (no sheriff) time helpers ----------

TEST(SpeechOrder, TimeHelpers) {
    EXPECT_EQ(timeDirection(2), SpeechDirection::Left);   // even -> Left
    EXPECT_EQ(timeDirection(3), SpeechDirection::Right);  // odd  -> Right
    EXPECT_EQ(timeFirstSpeaker(10, 3), 1);
    EXPECT_EQ(timeFirstSpeaker(9, 3), 0);
    EXPECT_EQ(timeFirstSpeaker(11, 4), 3);
    EXPECT_EQ(timeFirstSpeaker(5, 0), 0);  // guard against div-by-zero
}

TEST(Game, NoSheriffSpeechOrderIsAnnounced) {
    // Sheriff disabled -> the engine still announces a (randomised) speaking order.
    ScriptedDecisionProvider dp;
    dp.nightKills = {2, 3};  // a single night death drives the cue
    Game game(killAll("speak", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_TRUE(anyEventContains(dp, "发言顺序"));
}

// ---------- ② Last-words cue inside a real game ----------

TEST(Game, FirstNightDeathIsCuedButLaterNightsAreNot) {
    // 1 wolf + 4 civ. Knife civ2 (night 1) then civ3 (night 2); abstain by day.
    ScriptedDecisionProvider dp;
    dp.nightKills = {2, 3, 4};

    Game game(killAll("ln", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 4}}), dp);
    game.run();

    EXPECT_TRUE(anyEventContains(dp, "P2 可发表遗言"));   // first night -> cued
    EXPECT_FALSE(anyEventContains(dp, "P3 可发表遗言"));  // night 2 -> not cued
}

TEST(Game, NightRolesAreCuedOpenAndClose) {
    // ⑤ Each night role group gets "<role>请睁眼 / 请闭眼" narration.
    ScriptedDecisionProvider dp;
    dp.nightKills = {7, 8, 9};  // wipe the civilians -> wolf win, a few nights cued

    Game game(makeBoard9_SeerWitchHunter(), dp);
    game.run();
    EXPECT_TRUE(anyEventContains(dp, "狼人请睁眼"));
    EXPECT_TRUE(anyEventContains(dp, "狼人请闭眼"));
    EXPECT_TRUE(anyEventContains(dp, "预言家请睁眼"));
    EXPECT_TRUE(anyEventContains(dp, "女巫请睁眼"));
}

TEST(Game, PeacefulNightIsAnnounced) {
    // Night 1 is 空刀 -> the morning must explicitly announce the peaceful night.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 2, 3};  // peaceful night 1, then push to a wolf win

    Game game(killAll("peace", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();
    EXPECT_TRUE(anyEventContains(dp, "平安夜"));
}

TEST(Game, DaytimeExileIsCuedForLastWords) {
    // 1 wolf + 3 civ. Day-1 exile of civ2 (daytime, non-self-destruct) -> last words.
    ScriptedDecisionProvider dp;
    dp.nightKills = {std::nullopt, 3};  // 空刀 night 1, then push to a wolf win
    dp.votes = {2, 2, 2, 2};            // exile civ2 on day 1

    Game game(killAll("ex", {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 3}}), dp);
    game.run();

    EXPECT_TRUE(anyEventContains(dp, "P2 可发表遗言"));
}
