#include "flow/paidao.h"

#include <utility>

#include "core/abilities/ability.h"  // PendingDeath
#include "core/enums.h"
#include "core/player.h"
#include "flow/settlement.h"
#include "io/scripted_decision_provider.h"

namespace ww {

GameState sandboxClone(const Board& board, const GameState& live) {
    std::vector<RoleKind> seatRoles;
    seatRoles.reserve(live.players.size());
    for (const Player& p : live.players) seatRoles.push_back(p.role().kind());

    GameState copy = buildInitialState(board, seatRoles);  // fresh abilities, all alive
    copy.restore(live.snapshot());                         // apply current mutable state
    return copy;
}

GameResult simulatePaidaoLine(const Board& board, const GameState& live, const PaidaoLine& line) {
    GameState sandbox = sandboxClone(board, live);
    sandbox.phase = Phase::Day;  // 拍刀 happens during the day

    ScriptedDecisionProvider good;  // good-side reactions (hunter shots); silent pause
    good.hunterShots = line.hunterShots;
    Settlement settle(sandbox, board.config, good);

    for (const PaidaoStep& step : line.steps) {
        // The wolf self-destructs (N-1 of them across the line).
        if (GameResult r = settle.apply({{step.selfDestructWolf, DeathCause::BlownUp}});
            r != GameResult::Ongoing) {
            return r;
        }

        // The blast takes one target, unless the witch antidotes them; plus an
        // optional witch poison this step (good-side reactions, §4.4).
        std::vector<PendingDeath> batch;
        const bool saved = step.witchSaveKnife && sandbox.witchAntidoteAvailable;
        if (saved) {
            sandbox.witchAntidoteAvailable = false;
        } else {
            batch.push_back({step.knifeTarget, DeathCause::Killed});
        }
        if (step.witchPoison && sandbox.witchPoisonAvailable) {
            sandbox.witchPoisonAvailable = false;
            batch.push_back({*step.witchPoison, DeathCause::Poisoned});
        }
        if (GameResult r = settle.apply(std::move(batch)); r != GameResult::Ongoing) {
            return r;
        }
    }

    // Whole declared sequence done -> judge by the current board (§4.4: immediate).
    return evaluateWin(sandbox, board.config);
}

}  // namespace ww
