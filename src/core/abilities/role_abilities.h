#pragma once

#include <string>
#include <vector>

#include "core/abilities/ability.h"

// Concrete M2 abilities (BRD §2). Each is a small composable component:
//   NightKill   — wolves' shared nightly kill (§5.1 step 1)
//   Inspect     — seer's nightly check (§5.1 step 2)
//   WitchPotions— antidote + poison with the §2 constraints (§5.1 step 3)
//   HunterShot  — death-triggered shot, blocked by poison (§2)
namespace ww {

// Wolves act as a team: the first wolf processed makes the one kill decision
// (guarded by NightContext::wolvesActed); the rest are no-ops.
class NightKill : public Ability, public NightActor {
public:
    std::string name() const override { return "NightKill"; }
    int nightOrder() const override { return 10; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

class Inspect : public Ability, public NightActor {
public:
    std::string name() const override { return "Inspect"; }
    int nightOrder() const override { return 20; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

class WitchPotions : public Ability, public NightActor {
public:
    explicit WitchPotions(WitchSelfRescue selfRescue, bool bothPotionsSameNight)
        : selfRescue_(selfRescue), bothPotionsSameNight_(bothPotionsSameNight) {}

    std::string name() const override { return "WitchPotions"; }
    int nightOrder() const override { return 30; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;

private:
    WitchSelfRescue selfRescue_;
    bool bothPotionsSameNight_;
};

class HunterShot : public Ability, public DeathTrigger {
public:
    std::string name() const override { return "HunterShot"; }
    void onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                 std::vector<PendingDeath>& out) override;
};

}  // namespace ww
