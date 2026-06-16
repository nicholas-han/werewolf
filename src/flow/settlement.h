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

    // Applies a same-night batch one player at a time in the night's resolution
    // order (knife before poison — the batch's first-appearance order), checking
    // the win after each (BRD §4.2 strict sequential: the FIRST death to complete
    // a win condition decides it; later simultaneous deaths still apply but don't
    // change the winner). Returns that result; `newlyOut` collects the newly-dead.
    GameResult recordBatchSequential(const std::vector<PendingDeath>& batch,
                                     std::vector<Player*>& newlyOut);

    // Announces a death batch together in seat order, then takes last words
    // (`lastWordsOrder`, else seat order) — §5.2 公布顺序 / §5.3 遗言. No triggers,
    // no win check (used when the batch already decided the game, §4.2).
    void announceBatch(const std::vector<Player*>& deaths,
                       const std::vector<int>& lastWordsOrder = {});

    // Announces + transfers badge + fires triggers for already-recorded deaths,
    // chaining further deaths with a per-death win check (§4.2). Stops on a win.
    //
    // The simultaneous batch is announced together in seat order first, then last
    // words are taken (§5.2 公布顺序). `lastWordsOrder` (player ids) overrides the
    // batch's last-words order — used for the first-night multi-death random order
    // (§5.2/§5.3); empty = seat order. Chained (triggered) deaths announce + take
    // last words as they occur.
    GameResult resolveRecorded(std::deque<Player*> worklist,
                               const std::vector<int>& lastWordsOrder = {});

    // record() + resolveRecorded(): the all-in-one for same-moment deaths
    // (exile, self-destruct, sandbox 拍刀 steps).
    GameResult apply(std::vector<PendingDeath> batch);

private:
    GameState& state_;
    const BoardConfig& config_;
    DecisionProvider& provider_;

    void announceDeath(const Player& p);
    void collectLastWords(Player& dead);  // cue + capture text + pause, if eligible
    void maybeTransferBadge(Player& dead);
};

}  // namespace ww
