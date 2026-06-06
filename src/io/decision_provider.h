#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/game_state.h"

// DecisionProvider decouples the pure-logic engine from I/O (BRD §10): whenever
// the flow needs a player decision it asks through this interface and never
// touches the console directly. Implementations: Scripted (tests), Console
// (humans), Bot (simulation).
//
// M1 exposes only the two decisions the skeleton loop needs (night kill, exile
// vote). As real role abilities arrive (M2+), this can grow into a more generic
// requestAction(Decision, ...) form; kept concrete for now for simplicity.
namespace ww {

class DecisionProvider {
public:
    virtual ~DecisionProvider() = default;

    // Night: the wolf team picks a kill target among `candidates`
    // (std::nullopt = 空刀 / no kill). M1 stand-in for the future NightKill ability.
    virtual std::optional<int> chooseNightKill(const GameState& state,
                                               const std::vector<int>& candidates) = 0;

    // Day exile vote: `voterId` picks whom to banish among `candidates`
    // (std::nullopt = abstain, when allowed).
    virtual std::optional<int> chooseVote(const GameState& state, int voterId,
                                          const std::vector<int>& candidates) = 0;

    // Directed/broadcast notification for UI / observers / logging (BRD §11).
    // M1 uses it as a simple broadcast log; per-player targeting comes later.
    virtual void notify(const std::string& message) { (void)message; }
};

}  // namespace ww
