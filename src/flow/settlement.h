#pragma once

#include <deque>
#include <vector>

#include "core/abilities/ability.h"  // PendingDeath
#include "core/board.h"
#include "core/game_state.h"
#include "flow/win_condition.h"
#include "io/decision_provider.h"

// Settlement is the reusable death-resolution core (BRD §4.2/§5.2/§7.6), shared
// by the live Game flow and the 拍刀 sandbox (§4.4). It records deaths, announces
// them (+ last-words cue), transfers the badge, fires death triggers, and chains
// further deaths — checking the win condition after each one.
namespace ww {

class Settlement {
public:
    Settlement(GameState& state, const BoardConfig& config, DecisionProvider& provider);

    // Records a batch's causes simultaneously (同刀同毒); returns the newly-out
    // players. No announce / trigger / win-check (used by the night phase, which
    // defers announcement to the day).
    std::vector<Player*> record(const std::vector<PendingDeath>& batch);

    // Announces + transfers badge + fires triggers for already-recorded deaths,
    // chaining further deaths with a per-death win check (§4.2). Stops on a win.
    GameResult resolveRecorded(std::deque<Player*> worklist);

    // record() + resolveRecorded(): the all-in-one for same-moment deaths
    // (exile, self-destruct, sandbox 拍刀 steps).
    GameResult apply(std::vector<PendingDeath> batch);

private:
    GameState& state_;
    const BoardConfig& config_;
    DecisionProvider& provider_;

    void announceDeath(const Player& p);
    void maybeTransferBadge(Player& dead);
};

}  // namespace ww
