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

    // Self-rescue policy (BRD §2). First board: Never.
    // Never: never; FirstNightOnly: only night 1; Always: every night (§2).
    const bool selfRescueAllowed =
        selfRescue_ == WitchSelfRescue::Always ||
        (selfRescue_ == WitchSelfRescue::FirstNightOnly && state.day == 1);

    // Offer the antidote on `target`, honoring the self-rescue policy. On accept,
    // record the save, spend the antidote, and mark the night. Returns whether saved.
    // Shared by the normal knife and the (configurably saveable) 破盾大刀 below.
    auto trySave = [&](int target) -> bool {
        if (target == owner.id() && !selfRescueAllowed) return false;  // cannot self-rescue
        if (!provider.chooseWitchSave(state, owner.id(), target)) return false;
        ctx.savedTarget = target;
        state.witchAntidoteAvailable = false;
        savedThisNight = true;
        return true;
    };

    // Antidote vs the NORMAL knife (BRD §2 死讯可见性). The 破盾大刀 is handled below.
    if (state.witchAntidoteAvailable && ctx.wolfTarget.has_value()) {
        const int knifed = *ctx.wolfTarget;
        if (!trySave(knifed) && knifed == owner.id() && !selfRescueAllowed) {
            // She is the knifed one but cannot self-rescue: still tell her she was
            // knifed (BRD §2 死讯可见性; e.g. a guard may secretly save her).
            // Directed — only the witch may learn this (§11).
            provider.notifyPlayer(owner.id(), "【女巫】你今晚被刀（无法自救）");
        }
    }

    // 破盾大刀 antidote: by default the big knife pierces the antidote and stays
    // hidden (§2), so it is not surfaced. When the board forbids piercing, offer its
    // target like a normal knife — one antidote, so only if she has not saved yet.
    if (!bigKnifePiercesAntidote_ && !savedThisNight && state.witchAntidoteAvailable &&
        ctx.bigKnifeTarget.has_value()) {
        trySave(*ctx.bigKnifeTarget);
    }

    // Poison: blocked this night if she saved and the board forbids two potions
    // in one night (BRD §2, witchBothPotionsSameNight=false on first board).
    const bool poisonBlocked = savedThisNight && !bothPotionsSameNight_;
    if (state.witchPoisonAvailable && !poisonBlocked) {
        // Poison may target anyone alive, INCLUDING the witch herself (自毒, §2).
        std::optional<int> target = provider.chooseWitchPoison(state, owner.id(), aliveIds(state));
        if (target) {
            ctx.poisonTarget = *target;
            ctx.poisonSourceId = owner.id();
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

void PsychicInspect::actAtNight(NightContext& /*ctx*/, GameState& state, Player& owner,
                                DecisionProvider& provider) {
    std::optional<int> target = provider.chooseInspect(state, owner.id(), aliveIds(state));
    if (!target) return;
    const Player* t = state.find(*target);
    if (t == nullptr) return;
    // Disguise: inspecting the MechanicWolf returns its learned role (or, if it has
    // not learned yet, MechanicWolf itself). Everyone else shows their true role.
    RoleKind shown = t->role().kind();
    if (shown == RoleKind::MechanicWolf) {
        shown = state.mechanicLearned.value_or(RoleKind::MechanicWolf);
    }
    provider.onPsychicResult(owner.id(), *target, shown);
}

void MechanicLearn::actAtNight(NightContext& /*ctx*/, GameState& state, Player& owner,
                               DecisionProvider& provider) {
    if (state.mechanicLearned.has_value()) return;  // global once
    std::optional<int> target =
        provider.chooseMechanicLearn(state, owner.id(), aliveIds(state, owner.id()));
    if (!target) return;
    const Player* t = state.find(*target);
    if (t == nullptr) return;
    state.mechanicLearned = t->role().kind();  // disguise takes effect immediately
    state.mechanicLearnDay = state.day;        // active abilities start the next night
    provider.onMechanicLearnResult(owner.id(), *target, *state.mechanicLearned);  // private (§11)

    // Witch: copy the witch's *current* remaining potions (BRD §2 — she acts first,
    // so anything already spent tonight is gone). Independent of the real witch.
    if (state.mechanicLearned == RoleKind::Witch) {
        state.mechanicAntidoteAvailable = state.witchAntidoteAvailable;
        state.mechanicPoisonAvailable = state.witchPoisonAvailable;
    }
    // Any wolf role grants the one-shot 破盾大刀 (usable once it becomes lone, §2).
    if (t->faction() == Faction::Wolf) {
        state.mechanicBigKnifeAvailable = true;
    }
}

void MechanicLoneKill::actAtNight(NightContext& ctx, GameState& state, Player& owner,
                                  DecisionProvider& provider) {
    // Only knifes once every *other* wolf is out (the team's NightKill is gone).
    for (const Player& p : state.players) {
        if (p.isAlive() && p.faction() == Faction::Wolf && p.id() != owner.id()) return;
    }
    if (ctx.wolvesActed) return;  // safety: don't double-set if a team kill happened
    ctx.wolvesActed = true;
    ctx.wolfTarget = provider.chooseNightKill(state, aliveIds(state));  // 普通刀 (守卫可挡)

    // One-shot 破盾大刀 (from learning a wolf): usable only once lone and only from
    // the night after learning (§2). Adds a second kill that ignores the guard.
    if (state.mechanicBigKnifeAvailable && state.mechanicAbilitiesActive()) {
        if (std::optional<int> big =
                provider.chooseMechanicBigKnife(state, owner.id(), aliveIds(state))) {
            ctx.bigKnifeTarget = *big;
            state.mechanicBigKnifeAvailable = false;
        }
    }
}

void MechanicLearnedInspect::actAtNight(NightContext& /*ctx*/, GameState& state, Player& owner,
                                        DecisionProvider& provider) {
    if (state.mechanicLearned != RoleKind::Psychic || !state.mechanicAbilitiesActive()) return;
    std::optional<int> target = provider.chooseInspect(state, owner.id(), aliveIds(state));
    if (!target) return;
    const Player* t = state.find(*target);
    if (t == nullptr) return;
    RoleKind shown = t->role().kind();
    if (shown == RoleKind::MechanicWolf) {
        shown = state.mechanicLearned.value_or(RoleKind::MechanicWolf);
    }
    provider.onPsychicResult(owner.id(), *target, shown);
}

void MechanicLearnedWitch::actAtNight(NightContext& ctx, GameState& state, Player& owner,
                                      DecisionProvider& provider) {
    if (state.mechanicLearned != RoleKind::Witch || !state.mechanicAbilitiesActive()) return;
    bool saved = false;
    if (state.mechanicAntidoteAvailable && ctx.wolfTarget.has_value()) {
        if (provider.chooseWitchSave(state, owner.id(), *ctx.wolfTarget)) {
            ctx.mechSavedTarget = *ctx.wolfTarget;
            state.mechanicAntidoteAvailable = false;
            saved = true;
        }
    }
    const bool poisonBlocked = saved && !bothPotionsSameNight_;
    if (state.mechanicPoisonAvailable && !poisonBlocked) {
        // Learned-witch poison may also target anyone alive, incl. self (§2 自毒).
        if (std::optional<int> target =
                provider.chooseWitchPoison(state, owner.id(), aliveIds(state))) {
            ctx.mechPoisonTarget = *target;
            ctx.mechPoisonSourceId = owner.id();
            state.mechanicPoisonAvailable = false;
        }
    }
}

void MechanicLearnedProtect::actAtNight(NightContext& ctx, GameState& state, Player& owner,
                                        DecisionProvider& provider) {
    if (state.mechanicLearned != RoleKind::Guardian || !state.mechanicAbilitiesActive()) return;
    std::vector<int> candidates;
    for (const Player& p : state.players) {
        if (!p.isAlive()) continue;
        if (!allowConsecutive_ && state.mechanicLastGuardedId &&
            *state.mechanicLastGuardedId == p.id()) {
            continue;
        }
        candidates.push_back(p.id());
    }
    std::optional<int> target = provider.chooseGuard(state, owner.id(), candidates);
    ctx.mechanicGuardTarget = target;
    state.mechanicLastGuardedId = target;
}

void MechanicLearnedShoot::onDeath(GameState& state, Player& owner, DecisionProvider& provider,
                                   std::vector<PendingDeath>& out) {
    if (state.mechanicLearned != RoleKind::Hunter || !state.mechanicAbilitiesActive()) return;
    // A blocked death cause forbids the shot (§2, board-configurable: 毒 + 自爆).
    for (DeathCause c : blocked_) {
        if (owner.hasDeathCause(c)) return;
    }
    if (std::optional<int> target =
            provider.chooseHunterShot(state, owner.id(), aliveIds(state, owner.id()))) {
        out.push_back({*target, DeathCause::Shot});
    }
}

void HunterGunCheck::actAtNight(NightContext& ctx, GameState& /*state*/, Player& owner,
                                DecisionProvider& provider) {
    // The hunter cannot shoot if he will die poisoned tonight — by the real witch
    // OR the mechanic's learned witch (§2). A mechanic learned-guard on the hunter
    // saves him from poison (he survives -> can shoot) UNLESS the board's reflect
    // mode is None, in which case the guard blocks nothing — mirroring the dawn rule
    // in game.cpp. This runs after all poison is set (night order 43, §3).
    const int self = owner.id();
    const bool guardSaves = poisonReflect_ != PoisonReflect::None &&
                            ctx.mechanicGuardTarget && *ctx.mechanicGuardTarget == self;
    const bool poisonedByWitch = ctx.poisonTarget && *ctx.poisonTarget == self;
    const bool poisonedByMechWitch = ctx.mechPoisonTarget && *ctx.mechPoisonTarget == self;
    const bool diesPoisoned = (poisonedByWitch || poisonedByMechWitch) && !guardSaves;
    // If the board does not block the shot on poison at all, the hunter can always
    // shoot tonight (poison is the only night-determined block); otherwise he can
    // shoot iff he will not die poisoned.
    const bool canShoot = !poisonBlocksShot_ || !diesPoisoned;
    provider.onHunterGunCheck(self, canShoot);
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
