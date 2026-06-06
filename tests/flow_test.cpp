#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/game.h"
#include "flow/win_condition.h"
#include "io/scripted_decision_provider.h"

using namespace ww;

namespace {

// Board9 roster order (seats): 1-3 Werewolf, 4 Seer, 5 Witch, 6 Hunter, 7-9 Civilian.
GameState freshState() { return buildInitialState(makeBoard9_SeerWitchHunter()); }

void killSeat(GameState& s, int seat) {
    Player* p = s.find(seat);
    ASSERT_NE(p, nullptr);
    p->recordDeath(DeathCause::Exiled, 1);
}

const BoardConfig kKillSide{};  // defaults: KillSide

}  // namespace

// ---------- WinCondition snapshots (BRD §4) ----------

TEST(WinCondition, FreshGameIsOngoing) {
    EXPECT_EQ(evaluateWin(freshState(), kKillSide), GameResult::Ongoing);
}

TEST(WinCondition, AllWolvesOutTownWins) {
    GameState s = freshState();
    killSeat(s, 1);
    killSeat(s, 2);
    killSeat(s, 3);
    EXPECT_EQ(evaluateWin(s, kKillSide), GameResult::TownWins);
}

TEST(WinCondition, AllGodsOutWolfWins_KillSide) {
    // 屠边: all PowerRoles (seats 4,5,6) out -> wolves win.
    GameState s = freshState();
    killSeat(s, 4);
    killSeat(s, 5);
    killSeat(s, 6);
    EXPECT_EQ(evaluateWin(s, kKillSide), GameResult::WolfWins);
}

TEST(WinCondition, AllCiviliansOutWolfWins_KillSide) {
    // 屠边: all civilians (seats 7,8,9) out -> wolves win.
    GameState s = freshState();
    killSeat(s, 7);
    killSeat(s, 8);
    killSeat(s, 9);
    EXPECT_EQ(evaluateWin(s, kKillSide), GameResult::WolfWins);
}

TEST(WinCondition, VoteBindingParityWolfWins) {
    // §4.3: wolves(3) >= town(3) with neither gods nor civilians fully wiped.
    // Kill 1 god (4) + 2 civilians (7,8): gods=2, civ=1, town=3, wolves=3.
    GameState s = freshState();
    killSeat(s, 4);
    killSeat(s, 7);
    killSeat(s, 8);
    EXPECT_EQ(s.countAlive(Faction::Wolf), 3);
    EXPECT_EQ(s.countAlive(Faction::Town), 3);
    EXPECT_NE(s.countAlive(SubKind::PowerRole), 0);   // not a 屠边 win
    EXPECT_NE(s.countAlive(SubKind::Civilian), 0);
    EXPECT_EQ(evaluateWin(s, kKillSide), GameResult::WolfWins);
}

TEST(WinCondition, KillAllVariantNeedsAllTownOut) {
    BoardConfig killAll;
    killAll.winRule = WinRule::KillAll;
    GameState s = freshState();
    // All gods out but civilians alive: under KillAll this is NOT yet a win...
    killSeat(s, 4);
    killSeat(s, 5);
    killSeat(s, 6);
    // ...but parity already decides it: wolves(3) >= town(3). Confirm WolfWins.
    EXPECT_EQ(evaluateWin(s, killAll), GameResult::WolfWins);
}

// ---------- End-to-end skeleton games (BRD §5/§9) ----------

TEST(Game, TownWinsByExilingAllWolves) {
    ScriptedDecisionProvider provider;
    // Nights: 空刀 (leave nightKills empty -> default no-kill).
    // Day 1: 9 alive all vote seat 1; Day 2: 8 alive vote seat 2; Day 3: 7 vote seat 3.
    for (int i = 0; i < 9; ++i) provider.votes.push_back(1);
    for (int i = 0; i < 8; ++i) provider.votes.push_back(2);
    for (int i = 0; i < 7; ++i) provider.votes.push_back(3);

    Game game(makeBoard9_SeerWitchHunter(), provider);
    EXPECT_EQ(game.run(), GameResult::TownWins);

    EXPECT_EQ(game.state().countAlive(Faction::Wolf), 0);
    EXPECT_FALSE(game.state().find(1)->isAlive());
    EXPECT_FALSE(game.state().find(2)->isAlive());
    EXPECT_FALSE(game.state().find(3)->isAlive());
}

TEST(Game, WolvesWinByNightKillingTheCivilians) {
    ScriptedDecisionProvider provider;
    // Nights kill the three civilians (seats 7,8,9); days all abstain.
    provider.nightKills = {7, 8, 9};
    // votes empty -> everyone abstains -> no exile.

    Game game(makeBoard9_SeerWitchHunter(), provider);
    EXPECT_EQ(game.run(), GameResult::WolfWins);

    EXPECT_EQ(game.state().countAlive(SubKind::Civilian), 0);
    EXPECT_EQ(game.state().countAlive(Faction::Wolf), 3);  // wolves untouched
}

TEST(Game, TieGoesToRunoffThenNoExile) {
    // Tiny custom board (1 wolf + 2 civilians) to force a tie the runoff can't break.
    // seats: 1 = wolf, 2 = civ, 3 = civ.
    Board board;
    board.name = "tie-probe";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Civilian, 2}};
    // KillAll so the godless probe board isn't a vacuous 屠边 win; the ending is
    // driven by parity (§4.3) after the night-2 kill.
    board.config.winRule = WinRule::KillAll;

    ScriptedDecisionProvider provider;
    // Cycle 0 -- night 1: 空刀 (std::nullopt). Day 1 vote:
    //   round 1 voters [1,2,3]: seat1->2, seat2->3, seat3 abstain => {2:1, 3:1} tie.
    //   runoff voters = alive minus tied{2,3} = [1]: abstains => still tied -> no exile.
    // Cycle 1 -- night 2: kill seat 2 -> 1 wolf vs 1 civilian -> parity -> WolfWins.
    provider.nightKills = {std::nullopt, 2};
    provider.votes = {2, 3, std::nullopt, std::nullopt};

    Game game(board, provider);
    EXPECT_EQ(game.run(), GameResult::WolfWins);

    // The decisive assertion: day 1's tie exiled nobody.
    bool noExileLogged = false;
    for (const std::string& e : provider.events) {
        if (e == "No exile this round") noExileLogged = true;
    }
    EXPECT_TRUE(noExileLogged);

    // Day 1 banished no one; seat 2 only died at night 2; seat 3 still alive.
    EXPECT_FALSE(game.state().find(2)->isAlive());
    EXPECT_TRUE(game.state().find(3)->isAlive());
    EXPECT_TRUE(game.state().find(1)->isAlive());
}
