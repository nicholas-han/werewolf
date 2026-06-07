#pragma once

#include <deque>
#include <optional>
#include <vector>

#include "core/board.h"
#include "core/game_state.h"
#include "flow/win_condition.h"

// 拍刀 sandbox — Phase B (BRD §4.4): simulate a *specific declared line* of play
// on an independent copy of the live state and report the result. The live state
// is never modified. Phase C (auto "optimal good" search) will sit on top by
// enumerating the good-side reactions and reusing this simulator.
namespace ww {

// One 拍刀 step: a wolf self-destructs and takes (knifes) one target. The good
// side's witch reactions for this step are recorded inline; hunter shots are
// supplied separately (consumed when a hunter dies during settlement).
struct PaidaoStep {
    int selfDestructWolf;            // the wolf blowing up this step
    int knifeTarget;                 // whom the blast takes (拍刀 刀)
    bool witchSaveKnife = false;     // good: witch antidote on knifeTarget
    std::optional<int> witchPoison;  // good: witch poisons someone this step
};

struct PaidaoLine {
    std::vector<PaidaoStep> steps;                // wolves' declared sequence (+witch reactions)
    std::deque<std::optional<int>> hunterShots;   // good's hunter-shot targets, in order
};

// Builds an independent GameState equivalent to `live` (fresh ability objects +
// restored mutable state) for sandbox experimentation.
GameState sandboxClone(const Board& board, const GameState& live);

// Runs the declared line on a sandbox copy; returns the final standing after the
// whole sequence (§4.4: judged immediately by the current board). `live` is not
// modified.
GameResult simulatePaidaoLine(const Board& board, const GameState& live, const PaidaoLine& line);

// 拍刀 succeeds iff the simulated result is still a wolf win (§4.4).
inline bool paidaoSucceeds(const Board& board, const GameState& live, const PaidaoLine& line) {
    return simulatePaidaoLine(board, live, line) == GameResult::WolfWins;
}

}  // namespace ww
