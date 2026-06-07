#include <gtest/gtest.h>

#include <optional>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"

using namespace ww;

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
