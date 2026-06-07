#include "core/abilities/role_abilities.h"

#include <algorithm>
#include <vector>

#include "core/game_state.h"
#include "core/player.h"
#include "io/decision_provider.h"

namespace ww {

namespace {

// Living player ids, optionally excluding one (e.g. the actor itself).
std::vector<int> aliveIds(const GameState& state, int exclude = -1) {
    std::vector<int> ids;
    for (const Player& p : state.players) {
        if (p.isAlive() && p.id() != exclude) ids.push_back(p.id());
    }
    return ids;
}

}  // namespace

void NightKill::actAtNight(NightContext& ctx, GameState& state, Player& /*owner*/,
                           DecisionProvider& provider) {
    if (ctx.wolvesActed) return;  // team decision is made once
    ctx.wolvesActed = true;
    ctx.wolfTarget = provider.chooseNightKill(state, aliveIds(state));
}

void Inspect::actAtNight(NightContext& /*ctx*/, GameState& state, Player& owner,
                         DecisionProvider& provider) {
    // The seer may inspect anyone alive, including themselves (rules-allowed,
    // even if nobody actually would).
    std::vector<int> candidates = aliveIds(state);
    std::optional<int> target = provider.chooseInspect(state, owner.id(), candidates);
    if (!target) return;
    const Player* t = state.find(*target);
    if (t == nullptr) return;
    const bool isWolf = (t->faction() == Faction::Wolf);
    provider.onInspectResult(owner.id(), *target, isWolf);  // private to the seer (§11)
}

void WitchPotions::actAtNight(NightContext& ctx, GameState& state, Player& owner,
                              DecisionProvider& provider) {
    bool savedThisNight = false;

    // Antidote: only offered while unused, and the witch learns the knifed
    // player only in that case (BRD §2 死讯可见性).
    if (state.witchAntidoteAvailable && ctx.wolfTarget.has_value()) {
        const int knifed = *ctx.wolfTarget;
        // Self-rescue policy (BRD §2). First board: Never.
        const bool selfRescueAllowed = (selfRescue_ != WitchSelfRescue::Never);
        if (knifed != owner.id() || selfRescueAllowed) {
            if (provider.chooseWitchSave(state, owner.id(), knifed)) {
                ctx.savedTarget = knifed;
                state.witchAntidoteAvailable = false;
                savedThisNight = true;
            }
        } else {
            // She is the knifed one but cannot self-rescue: still tell her she was
            // knifed (BRD §2 死讯可见性; e.g. a guard may secretly save her).
            provider.notify("【女巫】你今晚被刀（无法自救）");
        }
    }

    // Poison: blocked this night if she saved and the board forbids two potions
    // in one night (BRD §2, witchBothPotionsSameNight=false on first board).
    const bool poisonBlocked = savedThisNight && !bothPotionsSameNight_;
    if (state.witchPoisonAvailable && !poisonBlocked) {
        std::optional<int> target =
            provider.chooseWitchPoison(state, owner.id(), aliveIds(state, owner.id()));
        if (target) {
            ctx.poisonTarget = *target;
            state.witchPoisonAvailable = false;
        }
    }
}

void Protect::actAtNight(NightContext& ctx, GameState& state, Player& owner,
                         DecisionProvider& provider) {
    // Candidates: anyone alive (incl. self / 空守 via std::nullopt), minus last
    // night's target unless the board allows guarding the same player twice (§2).
    std::vector<int> candidates;
    for (const Player& p : state.players) {
        if (!p.isAlive()) continue;
        if (!allowConsecutive_ && state.lastGuardedId && *state.lastGuardedId == p.id()) continue;
        candidates.push_back(p.id());
    }
    std::optional<int> target = provider.chooseGuard(state, owner.id(), candidates);
    ctx.guardTarget = target;
    state.lastGuardedId = target;  // 空守 -> nullopt -> next night unrestricted
}

void DeathTriggerShoot::onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                                std::vector<PendingDeath>& out) {
    // A blocked death cause forbids the shot (BRD §2: hunter blocked by poison;
    // wolfgun also by self-destruct). The moderator only says "无法发动技能"
    // without the reason (§11).
    for (DeathCause c : blocked_) {
        if (owner.hasDeathCause(c)) return;
    }
    std::optional<int> target =
        provider.chooseHunterShot(state, owner.id(), aliveIds(state, owner.id()));
    if (target) {
        out.push_back({*target, DeathCause::Shot});
    }
}

}  // namespace ww
