#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/enums.h"
#include "core/roles/role.h"

// Player is the actor (BRD §8.1). It owns a Role (composition) and carries
// status + universal, role-agnostic concerns. Voting/speaking/sheriff-running
// are driven by the flow layer, not by the role.
namespace ww {

class Player {
public:
    Player(int id, std::string name, int seat, std::unique_ptr<Role> role)
        : id_(id), name_(std::move(name)), seat_(seat), role_(std::move(role)) {}

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    int seat() const { return seat_; }

    Role& role() { return *role_; }
    const Role& role() const { return *role_; }
    Faction faction() const { return role_->faction(); }
    SubKind subKind() const { return role_->subKind(); }

    bool isAlive() const { return status_ == Status::Alive; }
    Status status() const { return status_; }
    const std::vector<DeathCause>& deathCauses() const { return deathCauses_; }
    std::optional<int> deathDay() const { return deathDay_; }
    std::optional<Phase> deathPhase() const { return deathPhase_; }

    // Records a death cause. A player may accumulate several causes (BRD §1/§5.2,
    // e.g. knifed + poisoned the same night). The first lethal cause flips the
    // player to Out and stamps the death day + phase (used for last-words
    // eligibility, §5.3); later causes are appended only.
    void recordDeath(DeathCause cause, int day, Phase phase = Phase::Night) {
        deathCauses_.push_back(cause);
        if (status_ == Status::Alive) {
            status_ = Status::Out;
            deathDay_ = day;
            deathPhase_ = phase;
        }
    }

    bool hasDeathCause(DeathCause cause) const {
        return std::find(deathCauses_.begin(), deathCauses_.end(), cause) !=
               deathCauses_.end();
    }

    // Captures/restores the mutable per-game fields for sandbox rollback (BRD
    // §4.4). The role/abilities are fixed for the whole game, so they are NOT
    // part of the snapshot — only this changing state is.
    struct Snapshot {
        Status status;
        std::vector<DeathCause> deathCauses;
        std::optional<int> deathDay;
        std::optional<Phase> deathPhase;
        bool isSheriff;
        bool guardedTonight;
        bool poisonedTonight;
    };
    Snapshot snapshot() const {
        return Snapshot{status_,   deathCauses_,    deathDay_,        deathPhase_,
                        isSheriff, guardedTonight,  poisonedTonight};
    }
    void restore(const Snapshot& s) {
        status_ = s.status;
        deathCauses_ = s.deathCauses;
        deathDay_ = s.deathDay;
        deathPhase_ = s.deathPhase;
        isSheriff = s.isSheriff;
        guardedTonight = s.guardedTonight;
        poisonedTonight = s.poisonedTonight;
    }

    // --- Persistent / transient per-game flags ---
    // Badge holder (BRD §7). Single sheriff at any time is enforced via
    // GameState::sheriffId; this mirror flag is convenience for the flow layer.
    bool isSheriff = false;

    // Per-night transient markers, reset by the flow each night (M1+).
    bool guardedTonight = false;
    bool poisonedTonight = false;

private:
    int id_;
    std::string name_;
    int seat_;
    std::unique_ptr<Role> role_;
    Status status_ = Status::Alive;
    std::vector<DeathCause> deathCauses_;
    std::optional<int> deathDay_;
    std::optional<Phase> deathPhase_;
};

}  // namespace ww
