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

    // --- M2 role-ability decisions (defaulted; impls override what they need) ---

    // Seer inspects a player (std::nullopt = skip). Result is delivered via
    // onInspectResult so only the seer learns it (BRD §2/§11).
    virtual std::optional<int> chooseInspect(const GameState& state, int seerId,
                                             const std::vector<int>& candidates) {
        (void)state; (void)seerId; (void)candidates; return std::nullopt;
    }

    // Witch antidote: rescue the knifed player `knifedId`? Only asked while the
    // antidote is unused (BRD §2 死讯可见性).
    virtual bool chooseWitchSave(const GameState& state, int witchId, int knifedId) {
        (void)state; (void)witchId; (void)knifedId; return false;
    }

    // Witch poison: kill one player (std::nullopt = don't poison).
    virtual std::optional<int> chooseWitchPoison(const GameState& state, int witchId,
                                                 const std::vector<int>& candidates) {
        (void)state; (void)witchId; (void)candidates; return std::nullopt;
    }

    // Hunter shot on death: take one player (std::nullopt = 不翻牌/不开枪). Only
    // asked when the hunter is actually allowed to shoot (not poisoned, BRD §2).
    virtual std::optional<int> chooseHunterShot(const GameState& state, int hunterId,
                                                const std::vector<int>& candidates) {
        (void)state; (void)hunterId; (void)candidates; return std::nullopt;
    }

    // Daytime self-destruct (BRD §2): which wolf blows up (std::nullopt = none).
    virtual std::optional<int> chooseSelfDestruct(const GameState& state,
                                                  const std::vector<int>& wolfIds) {
        (void)state; (void)wolfIds; return std::nullopt;
    }

    // Directed result delivered privately to the seer (BRD §11).
    virtual void onInspectResult(int seerId, int targetId, bool isWolf) {
        (void)seerId; (void)targetId; (void)isWolf;
    }

    // Directed/broadcast notification for UI / observers / logging (BRD §11).
    // M1 uses it as a simple broadcast log; per-player targeting comes later.
    virtual void notify(const std::string& message) { (void)message; }
};

}  // namespace ww
