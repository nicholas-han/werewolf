#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "core/abilities/ability.h"
#include "core/board.h"
#include "core/enums.h"
#include "core/game_state.h"
#include "flow/settlement.h"
#include "flow/win_condition.h"
#include "io/decision_provider.h"

// Game is the flow orchestrator (BRD §5/§7/§9).
//
// M3 makes the night/day split faithful (BRD §7.2/§2):
//   - Night resolves actions and records the direct night deaths *silently*,
//     checking the win only on those deaths.
//   - Day announces the night deaths and fires their death triggers (e.g. a
//     knifed hunter shoots at dawn-of-day, §2). On Day 1 the sheriff election
//     runs *before* that announcement (§7.2).
namespace ww {

class Game {
public:
    // `seatRoles` (BRD M5 §setup): explicit seat->role assignment entered by the
    // moderator. If empty, the deterministic roster-order layout is used.
    Game(Board board, DecisionProvider& provider,
         std::optional<std::vector<RoleKind>> seatRoles = std::nullopt);

    GameResult run();
    const GameState& state() const { return state_; }

private:
    Board board_;
    DecisionProvider& provider_;
    GameState state_;
    Settlement settlement_;

    // Night deaths recorded but not yet announced (awaiting the day's 公布死讯).
    std::vector<int> pendingNightDeaths_;

    // Sheriff-election bookkeeping (BRD §7.4/§7.5).
    bool electionResolved_ = false;   // election finished (with or without a sheriff)
    bool electionDeferred_ = false;   // interrupted on day 1 -> day 2 vote-only
    bool badgeAbandoned_ = false;     // interrupted twice -> no badge for the whole game

    GameResult runNight();
    GameResult runDay();

    // Result of the sheriff election attempt.
    struct ElectionOutcome {
        GameResult result = GameResult::Ongoing;
        bool interrupted = false;  // a wolf self-destructed mid-election (§7.4)
    };
    ElectionOutcome runSheriffElection();

    // Announces the pending night deaths and resolves their triggers (§5.3/§2).
    GameResult announceNightDeaths();

    // Exile vote with the sheriff's 归票 weighting (BRD §6/§7.1).
    std::optional<int> resolveExile();

    void electSheriff(int playerId);

    // M5 moderator cues:
    std::string moderatorStatus() const;                 // ④ status board
    void cueSpeechOrder(int nightDeathCount, int singleDeadSeat);  // ③ 死左/死右

    std::vector<int> aliveIds() const;
    std::vector<int> aliveWolfIds() const;
};

}  // namespace ww
