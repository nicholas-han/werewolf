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

// Psychic's nightly check (§2, psychic board): reveals the target's exact role
// (via onPsychicResult). For the MechanicWolf it returns its disguise.
class PsychicInspect : public Ability, public NightActor {
public:
    std::string name() const override { return "PsychicInspect"; }
    int nightOrder() const override { return 50; }  // acts last on the psychic board
    std::string nightCue() const override { return "通灵师"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

// MechanicWolf — learn one living player's role, once per game (§2). Phase 1
// records the disguise only; the learned active abilities arrive in a later phase.
class MechanicLearn : public Ability, public NightActor {
public:
    std::string name() const override { return "MechanicLearn"; }
    int nightOrder() const override { return 38; }
    std::string nightCue() const override { return "机械狼"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

// MechanicWolf — innate solo knife once every other wolf is out (§2).
class MechanicLoneKill : public Ability, public NightActor {
public:
    std::string name() const override { return "MechanicLoneKill"; }
    int nightOrder() const override { return 40; }
    std::string nightCue() const override { return "机械狼"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

// --- MechanicWolf learned-active abilities (BRD §2; live from the night after
//     learning, gated by GameState::mechanicAbilitiesActive()). Each is a no-op
//     unless the mechanic learned the matching role. ---

// Learned 通灵师: nightly inspect of a player's exact role.
class MechanicLearnedInspect : public Ability, public NightActor {
public:
    std::string name() const override { return "MechanicLearnedInspect"; }
    int nightOrder() const override { return 41; }
    std::string nightCue() const override { return "机械狼"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
};

// Learned 女巫: independent antidote/poison stock (copied at learn time).
class MechanicLearnedWitch : public Ability, public NightActor {
public:
    explicit MechanicLearnedWitch(bool bothPotionsSameNight)
        : bothPotionsSameNight_(bothPotionsSameNight) {}
    std::string name() const override { return "MechanicLearnedWitch"; }
    int nightOrder() const override { return 42; }
    std::string nightCue() const override { return "机械狼"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;

private:
    bool bothPotionsSameNight_;
};

// Learned 守卫: protect with the mechanic-only poison reflect.
class MechanicLearnedProtect : public Ability, public NightActor {
public:
    explicit MechanicLearnedProtect(bool allowConsecutiveSameTarget)
        : allowConsecutive_(allowConsecutiveSameTarget) {}
    std::string name() const override { return "MechanicLearnedProtect"; }
    int nightOrder() const override { return 6; }  // alongside the real guard
    std::string nightCue() const override { return "机械狼"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;

private:
    bool allowConsecutive_;
};

// Learned 猎人: death-triggered shot, blocked by poison (reuses the hunter rule).
class MechanicLearnedShoot : public Ability, public DeathTrigger {
public:
    std::string name() const override { return "MechanicLearnedShoot"; }
    void onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                 std::vector<PendingDeath>& out) override;
};

// Hunter's nightly "can I shoot?" gesture (§2/§5.1, all boards): a private cue to
// the hunter on whether a shot is currently available (i.e. not being poisoned
// this night). Informational only — the actual block lives in DeathTriggerShoot.
class HunterGunCheck : public Ability, public NightActor {
public:
    std::string name() const override { return "HunterGunCheck"; }
    int nightOrder() const override { return 35; }  // after the witch's poison
    std::string nightCue() const override { return "猎人"; }
    void actAtNight(NightContext& ctx, GameState& state, Player& owner,
                    DecisionProvider& provider) override;
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
