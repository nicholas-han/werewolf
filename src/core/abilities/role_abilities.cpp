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
    std::vector<int> candidates = aliveIds(state, owner.id());
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

void HunterShot::onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                         std::vector<PendingDeath>& out) {
    // Poison blocks the shot (BRD §2). Any poison cause among the deaths disables
    // it; the moderator would say only "无法发动技能" without the reason (§11).
    if (owner.hasDeathCause(DeathCause::Poisoned)) return;

    std::optional<int> target =
        provider.chooseHunterShot(state, owner.id(), aliveIds(state, owner.id()));
    if (target) {
        out.push_back({*target, DeathCause::Shot});
    }
}

}  // namespace ww
