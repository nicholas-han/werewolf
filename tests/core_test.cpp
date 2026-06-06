#include <gtest/gtest.h>

#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "core/player.h"
#include "core/roles/role.h"

using namespace ww;

// --- Role metadata (BRD §2/§3) ---

TEST(Role, MetadataPerKind) {
    auto seer = makeRole(RoleKind::Seer);
    EXPECT_EQ(seer->kind(), RoleKind::Seer);
    EXPECT_EQ(seer->faction(), Faction::Town);
    EXPECT_EQ(seer->subKind(), SubKind::PowerRole);

    auto wolf = makeRole(RoleKind::Werewolf);
    EXPECT_EQ(wolf->faction(), Faction::Wolf);
    EXPECT_EQ(wolf->subKind(), SubKind::None);

    auto civ = makeRole(RoleKind::Civilian);
    EXPECT_EQ(civ->faction(), Faction::Town);
    EXPECT_EQ(civ->subKind(), SubKind::Civilian);

    // Hunter and Witch are Town power roles.
    EXPECT_EQ(makeRole(RoleKind::Hunter)->subKind(), SubKind::PowerRole);
    EXPECT_EQ(makeRole(RoleKind::Witch)->subKind(), SubKind::PowerRole);
}

TEST(Role, AbilitiesComposedPerKind) {
    // M2: roles are composed from ability components (BRD §8).
    EXPECT_TRUE(makeRole(RoleKind::Civilian)->abilities().empty());  // vanilla
    EXPECT_EQ(makeRole(RoleKind::Werewolf)->abilities().size(), 1u);
    EXPECT_EQ(makeRole(RoleKind::Seer)->abilities().size(), 1u);
    EXPECT_EQ(makeRole(RoleKind::Witch)->abilities().size(), 1u);
    EXPECT_EQ(makeRole(RoleKind::Hunter)->abilities().front()->name(), "HunterShot");
}

// --- Player status & death causes (BRD §1/§5.2) ---

TEST(Player, StartsAlive) {
    Player p(1, "P1", 1, makeRole(RoleKind::Civilian));
    EXPECT_TRUE(p.isAlive());
    EXPECT_EQ(p.status(), Status::Alive);
    EXPECT_TRUE(p.deathCauses().empty());
    EXPECT_FALSE(p.deathDay().has_value());
    EXPECT_EQ(p.faction(), Faction::Town);
}

TEST(Player, RecordDeathFlipsToOutAndStampsDay) {
    Player p(1, "P1", 1, makeRole(RoleKind::Hunter));
    p.recordDeath(DeathCause::Exiled, 2);
    EXPECT_FALSE(p.isAlive());
    EXPECT_EQ(p.status(), Status::Out);
    ASSERT_TRUE(p.deathDay().has_value());
    EXPECT_EQ(*p.deathDay(), 2);
    EXPECT_TRUE(p.hasDeathCause(DeathCause::Exiled));
}

TEST(Player, AccumulatesMultipleDeathCauses) {
    // 同刀同毒: knifed + poisoned the same night -> both causes recorded (BRD §5.2).
    Player p(1, "P1", 1, makeRole(RoleKind::Hunter));
    p.recordDeath(DeathCause::Killed, 1);
    p.recordDeath(DeathCause::Poisoned, 1);
    EXPECT_EQ(p.deathCauses().size(), 2u);
    EXPECT_TRUE(p.hasDeathCause(DeathCause::Killed));
    EXPECT_TRUE(p.hasDeathCause(DeathCause::Poisoned));
    // Death day is stamped by the first lethal cause and not overwritten.
    EXPECT_EQ(*p.deathDay(), 1);
}

// --- Board (BRD §3) ---

TEST(Board, Nine_SeerWitchHunter_Composition) {
    Board board = makeBoard9_SeerWitchHunter();
    EXPECT_EQ(board.name, "Board9_SeerWitchHunter");
    EXPECT_EQ(board.totalPlayers(), 9);

    // Default config matches BRD §3 for this board.
    EXPECT_TRUE(board.config.sheriffEnabled);
    EXPECT_EQ(board.config.winRule, WinRule::KillSide);
    EXPECT_EQ(board.config.witchSelfRescue, WitchSelfRescue::Never);
    EXPECT_FALSE(board.config.witchBothPotionsSameNight);
    EXPECT_TRUE(board.config.blownUpEnabled);
    EXPECT_TRUE(board.config.abstainAllowed);
    EXPECT_EQ(board.config.exileTieRule, ExileTieRule::RunoffThenNoExile);
}

// --- GameState build & queries (BRD §8.1, supports §4 win checks) ---

TEST(GameState, BuildFromBoardSeatsAndCounts) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());
    ASSERT_EQ(s.players.size(), 9u);

    // Seats are 1..9, ids match seats, all alive at start.
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(s.players[i].seat(), i + 1);
        EXPECT_EQ(s.players[i].id(), i + 1);
        EXPECT_TRUE(s.players[i].isAlive());
    }

    // Faction / sub-kind tallies: 3 wolves, 6 town (3 gods + 3 civilians).
    EXPECT_EQ(s.countAlive(Faction::Wolf), 3);
    EXPECT_EQ(s.countAlive(Faction::Town), 6);
    EXPECT_EQ(s.countAlive(SubKind::PowerRole), 3);
    EXPECT_EQ(s.countAlive(SubKind::Civilian), 3);

    // Role tallies.
    EXPECT_EQ(s.countAliveRole(RoleKind::Werewolf), 3);
    EXPECT_EQ(s.countAliveRole(RoleKind::Seer), 1);
    EXPECT_EQ(s.countAliveRole(RoleKind::Witch), 1);
    EXPECT_EQ(s.countAliveRole(RoleKind::Hunter), 1);
    EXPECT_EQ(s.countAliveRole(RoleKind::Civilian), 3);

    // No sheriff at start (BRD §7).
    EXPECT_FALSE(s.sheriffId.has_value());
}

TEST(GameState, FindAndAliveReflectDeaths) {
    GameState s = buildInitialState(makeBoard9_SeerWitchHunter());

    Player* p1 = s.find(1);
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->id(), 1);
    EXPECT_EQ(s.find(999), nullptr);

    EXPECT_EQ(s.alive().size(), 9u);

    // Kill a wolf -> alive count and faction tally drop.
    p1->recordDeath(DeathCause::Exiled, 1);  // P1 is a wolf in roster order
    EXPECT_EQ(s.alive().size(), 8u);
    EXPECT_EQ(s.countAlive(Faction::Wolf), 2);
}
