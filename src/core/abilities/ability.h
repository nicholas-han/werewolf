#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/enums.h"

// Ability is the polymorphic base for role behaviours (BRD §8.0/§8.1). Roles are
// composed from reusable Ability components rather than a deep role hierarchy.
//
// Behavioural hooks are split into small mix-in interfaces (NightActor,
// DeathTrigger). A concrete ability inherits Ability plus whichever hooks it
// needs; the flow discovers them via dynamic_cast. New roles = new components,
// no flow/role-hierarchy surgery.
namespace ww {

class GameState;
class Player;
class DecisionProvider;

class Ability {
public:
    virtual ~Ability() = default;
    virtual std::string name() const = 0;
};

// A single death to be applied during settlement (BRD §5.2 / §4.2).
struct PendingDeath {
    int playerId;
    DeathCause cause;
};

// Shared scratchpad for the night so ordered abilities can see each other's
// choices (e.g. the witch must know who the wolves knifed, BRD §2/§5.1).
struct NightContext {
    bool wolvesActed = false;
    std::optional<int> wolfTarget;    // who the wolves chose to knife (空刀 = unset)
    std::optional<int> savedTarget;   // player rescued by the witch's antidote
    std::optional<int> poisonTarget;  // player hit by the witch's poison
};

// Mix-in for abilities that act during the night, in a defined order (§5.1).
class NightActor {
public:
    virtual ~NightActor() = default;
    virtual int nightOrder() const = 0;  // lower acts earlier
    virtual void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                            DecisionProvider& provider) = 0;
};

// Mix-in for abilities that fire when their owner dies (e.g. hunter shot, §2).
// Triggered deaths are appended to `out` and settled sequentially (§4.2).
class DeathTrigger {
public:
    virtual ~DeathTrigger() = default;
    virtual void onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                         std::vector<PendingDeath>& out) = 0;
};

}  // namespace ww
