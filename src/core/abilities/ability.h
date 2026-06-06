#pragma once

#include <string>

// Ability is the polymorphic base for all role behaviours (BRD §8.0/§8.1).
//
// Design: roles are composed from a set of reusable Ability components rather
// than a deep role inheritance chain. Concrete abilities (NightKill, Inspect,
// SaveAndPoison, DeathTriggerShoot, SelfExplode, ...) arrive in M2; M0 only
// establishes the base interface so Role can hold a vector of them.
namespace ww {

class Ability {
public:
    virtual ~Ability() = default;

    // Human-readable identifier, mainly for logging / tests.
    virtual std::string name() const = 0;
};

}  // namespace ww
