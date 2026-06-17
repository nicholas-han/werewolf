#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "core/messages.h"
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

// §2/§7.2 per-speech interrupts (default off): a wolf may自爆 between day speeches,
// ending the day early — instead of only at the single post-speech window.
TEST(SpeechInterrupts, WolfSelfDestructsMidDaySpeeches) {
    ScriptedDecisionProvider provider;
    provider.nightKills = {std::nullopt};  // peaceful night 1
    provider.speeches = {"A", "B", "C", "D", "E", "F", "G", "H", "I"};
    // Nobody runs for sheriff (default false) -> no election self-destruct checks.
    // chooseSelfDestruct is polled after each day speech: pass, then P1 (a wolf) blows up.
    provider.selfDestructs = {std::nullopt, 1};

    Game game(makeBoard9_SeerWitchHunter(), provider);
    game.setSpeechInterrupts(true);
    game.run();

    const GameState& st = game.state();
    EXPECT_TRUE(st.find(1)->hasDeathCause(DeathCause::BlownUp));  // wolf self-destructed
    int day1Statements = 0;
    for (const auto& e : st.speeches) {
        if (e.day == 1 && e.kind == SpeechKind::Statement) ++day1Statements;
    }
    EXPECT_EQ(day1Statements, 2);  // day ended after the 2nd speech, not all 9
}

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
        if (e.find("本轮无人出局") != std::string::npos) noExileLogged = true;
    }
    EXPECT_TRUE(noExileLogged);

    // Day 1 banished no one; seat 2 only died at night 2; seat 3 still alive.
    EXPECT_FALSE(game.state().find(2)->isAlive());
    EXPECT_TRUE(game.state().find(3)->isAlive());
    EXPECT_TRUE(game.state().find(1)->isAlive());
}

TEST(Game, DeadRolesStillCuedAtNight) {
    // §11: a power role's night phase must be narrated every night even after it
    // dies — a missing 睁眼 cue would reveal the role (and which seat held it) is
    // gone. seats: 1 = wolf, 2 = seer, 3-4 = civilians.
    Board board;
    board.name = "dead-cue";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Seer, 1}, {RoleKind::Civilian, 2}};
    board.config.winRule = WinRule::KillAll;
    board.config.sheriffEnabled = false;

    ScriptedDecisionProvider dp;
    dp.nightKills = {2, 3};  // n1: kill the seer ; n2: kill a civ -> parity -> WolfWins

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
    EXPECT_FALSE(game.state().find(2)->isAlive());  // the seer died on night 1

    int nights = 0, seerCues = 0;
    for (const std::string& e : dp.events) {
        if (e.find("天黑请闭眼") != std::string::npos) ++nights;
        if (e == txt::openEyes("预言家")) ++seerCues;
    }
    EXPECT_EQ(nights, 2);
    EXPECT_EQ(seerCues, nights);  // seer phase narrated every night, alive or dead
}

TEST(Game, SimultaneousNightDeathsDecidedSequentially) {
    // §4.2: the last civilian is knifed AND the last wolf is poisoned the same
    // night. Both "all wolves out" (TownWins) and 屠边 "all civilians out"
    // (WolfWins) are satisfied at once. Strict sequential settlement in the night's
    // resolution order (knife before poison) settles the CIVILIAN's death first ->
    // WolfWins. (The old batch-then-check used evaluateWin's good-priority and would
    // wrongly return TownWins.)
    Board board;  // KillSide so the 屠边 (all-civilians-out) branch applies
    board.name = "simul-death";
    board.roster = {{RoleKind::Werewolf, 1}, {RoleKind::Witch, 1}, {RoleKind::Hunter, 1},
                    {RoleKind::Civilian, 1}};
    board.config.sheriffEnabled = false;  // keep the night -> win path clean

    ScriptedDecisionProvider dp;
    dp.nightKills = {4};       // wolf knifes the lone civilian (seat 4)
    dp.witchSaves = {false};   // witch does not save the civilian
    dp.witchPoisons = {1};     // witch poisons the lone wolf (seat 1)

    Game game(board, dp);
    EXPECT_EQ(game.run(), GameResult::WolfWins);
}
