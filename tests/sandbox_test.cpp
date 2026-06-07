#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/paidao.h"

using namespace ww;

namespace {

Board killAllBoard(std::vector<RoleSlot> roster) {
    Board b;
    b.name = "paidao-probe";
    b.roster = std::move(roster);
    b.config.winRule = WinRule::KillAll;
    return b;
}

}  // namespace

// ---------- Phase A: rollbackable GameState (BRD §4.4 foundation) ----------

TEST(Snapshot, RestoreUndoesAllMutations) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());
    s.sheriffId = 1;
    s.find(1)->isSheriff = true;

    GameState::Snapshot snap = s.snapshot();

    // Mutate a bunch of state after the snapshot.
    s.find(2)->recordDeath(DeathCause::Killed, 1, Phase::Night);
    s.find(3)->recordDeath(DeathCause::Poisoned, 2, Phase::Night);
    s.witchAntidoteAvailable = false;
    s.witchPoisonAvailable = false;
    s.day = 5;
    s.phase = Phase::Day;
    s.sheriffId = 4;
    s.find(4)->isSheriff = true;
    s.log.push_back("sandbox noise");
    EXPECT_EQ(s.alive().size(), 7u);

    s.restore(snap);

    // Everything is back to the snapshot.
    EXPECT_EQ(s.alive().size(), 9u);
    EXPECT_TRUE(s.find(2)->isAlive());
    EXPECT_TRUE(s.find(2)->deathCauses().empty());
    EXPECT_TRUE(s.find(3)->isAlive());
    EXPECT_TRUE(s.witchAntidoteAvailable);
    EXPECT_TRUE(s.witchPoisonAvailable);
    EXPECT_EQ(s.day, 1);
    EXPECT_EQ(s.phase, Phase::Night);
    EXPECT_EQ(s.sheriffId, std::optional<int>(1));
    EXPECT_TRUE(s.find(1)->isSheriff);
    EXPECT_FALSE(s.find(4)->isSheriff);
    EXPECT_TRUE(s.log.empty());
}

TEST(Snapshot, IndependentSnapshotsRestoreToTheRightPoint) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());

    s.find(7)->recordDeath(DeathCause::Killed, 1, Phase::Night);
    GameState::Snapshot afterOneDeath = s.snapshot();  // 8 alive

    s.find(8)->recordDeath(DeathCause::Exiled, 2, Phase::Day);
    EXPECT_EQ(s.alive().size(), 7u);

    s.restore(afterOneDeath);
    EXPECT_EQ(s.alive().size(), 8u);
    EXPECT_FALSE(s.find(7)->isAlive());  // the pre-snapshot death stays
    EXPECT_TRUE(s.find(8)->isAlive());   // the post-snapshot death is undone
}

// ---------- Phase B: specific-line 拍刀 simulation (BRD §4.4) ----------

TEST(Paidao, ForcedWinSucceeds) {
    // 2 wolves + 2 civilians (KillAll). One wolf self-destructs taking a civilian
    // -> 1 wolf vs 1 civilian -> parity -> wolves win -> 拍刀 succeeds.
    Board board = killAllBoard({{RoleKind::Werewolf, 2}, {RoleKind::Civilian, 2}});
    GameState s = buildInitialState(board);  // seats 1,2 wolf; 3,4 civ

    PaidaoLine line;
    line.steps = {PaidaoStep{/*selfDestructWolf=*/1, /*knifeTarget=*/3}};

    EXPECT_EQ(simulatePaidaoLine(board, s, line), GameResult::WolfWins);
    EXPECT_TRUE(paidaoSucceeds(board, s, line));
    // The live state is untouched by the sandbox.
    EXPECT_TRUE(s.find(1)->isAlive());
    EXPECT_TRUE(s.find(3)->isAlive());
    EXPECT_EQ(s.alive().size(), 4u);
}

TEST(Paidao, HunterShotDefeatsTheLine) {
    // 2 wolves + hunter + 2 civilians (KillAll). Wolf takes the hunter, but the
    // hunter (good's optimal) shoots the remaining wolf -> good wins -> 拍刀 fails.
    Board board =
        killAllBoard({{RoleKind::Werewolf, 2}, {RoleKind::Hunter, 1}, {RoleKind::Civilian, 2}});
    GameState s = buildInitialState(board);  // 1,2 wolf; 3 hunter; 4,5 civ

    PaidaoLine line;
    line.steps = {PaidaoStep{/*wolf=*/1, /*knife=*/3}};  // take the hunter
    line.hunterShots = {2};                               // hunter shoots the other wolf

    EXPECT_EQ(simulatePaidaoLine(board, s, line), GameResult::TownWins);
    EXPECT_FALSE(paidaoSucceeds(board, s, line));
}

TEST(Paidao, WitchAntidoteDefeatsTheLine) {
    // 2 wolves + witch + 1 civilian (KillAll). Wolf takes the civilian; without a
    // save it'd be parity (wolf win), but the witch antidotes the target -> the
    // line does not reach a wolf win -> 拍刀 fails.
    Board board =
        killAllBoard({{RoleKind::Werewolf, 2}, {RoleKind::Witch, 1}, {RoleKind::Civilian, 1}});
    GameState s = buildInitialState(board);  // 1,2 wolf; 3 witch; 4 civ

    PaidaoLine noSave;
    noSave.steps = {PaidaoStep{1, 4}};
    EXPECT_TRUE(paidaoSucceeds(board, s, noSave));  // unsaved -> wolves win

    PaidaoLine saved;
    saved.steps = {PaidaoStep{1, 4, /*witchSaveKnife=*/true, std::nullopt}};
    EXPECT_FALSE(paidaoSucceeds(board, s, saved));  // antidote breaks the line
}
