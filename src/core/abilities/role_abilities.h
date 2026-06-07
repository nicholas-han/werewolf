#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/abilities/ability.h"

// Concrete role abilities (BRD §2). Each is a small composable component:
//   NightKill         — wolves' shared nightly kill (§5.1)
//   Inspect           — seer's nightly check
//   WitchPotions      — antidote + poison with the §2 constraints
//   Protect           — guardian's nightly protect (§2, 12-player board)
//   DeathTriggerShoot — death-triggered shot, parameterised by blocked causes
//                       (hunter={Poisoned}; wolfgun={Poisoned,BlownUp})
namespace ww {

// Wolves act as a team: the first wolf processed makes the one kill decision
// (guarded by NightContext::wolvesActed); the rest are no-ops.
class NightKill : public Ability, public NightActor {
public:
    std::string name() const override { return "NightKill"; }
    int nightOrder() const override { return 10; }
    std::string nightCue() const override { return "狼人"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

class Inspect : public Ability, public NightActor {
public:
    std::string name() const override { return "Inspect"; }
    int nightOrder() const override { return 20; }
    std::string nightCue() const override { return "预言家"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

class WitchPotions : public Ability, public NightActor {
public:
    explicit WitchPotions(WitchSelfRescue selfRescue, bool bothPotionsSameNight)
        : selfRescue_(selfRescue), bothPotionsSameNight_(bothPotionsSameNight) {}

    std::string name() const override { return "WitchPotions"; }
    int nightOrder() const override { return 30; }
    std::string nightCue() const override { return "女巫"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;

private:
    WitchSelfRescue selfRescue_;
    bool bothPotionsSameNight_;
};

// Guardian's nightly protect (§2). Acts before the wolves; records the target in
// NightContext and updates GameState's last-guarded for the "no two nights in a
// row" rule. The dawn knife-survival formula (§5.2) lives in the flow.
class Protect : public Ability, public NightActor {
public:
    explicit Protect(bool allowConsecutiveSameTarget)
        : allowConsecutive_(allowConsecutiveSameTarget) {}

    std::string name() const override { return "Protect"; }
    int nightOrder() const override { return 5; }  // before the wolves
    std::string nightCue() const override { return "守卫"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;

private:
    bool allowConsecutive_;
};

// Death-triggered shot, reused by Hunter and WolfGun (§2). `blocked` lists the
// death causes that forbid the shot (hunter: {Poisoned}; wolfgun: {Poisoned,
// BlownUp}). The shot itself always resolves in daytime via the settlement.
class DeathTriggerShoot : public Ability, public DeathTrigger {
public:
    DeathTriggerShoot(std::string name, std::vector<DeathCause> blocked)
        : name_(std::move(name)), blocked_(std::move(blocked)) {}

    std::string name() const override { return name_; }
    void onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                 std::vector<PendingDeath>& out) override;

private:
    std::string name_;
    std::vector<DeathCause> blocked_;
};

}  // namespace ww
