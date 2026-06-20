#pragma once

#include <string>
#include <vector>

#include "core/enums.h"

// Board = a data-driven configuration (BRD §3): role composition + rule
// switches + win rule. Adding a new board should mostly be adding data.
namespace ww {

// Per-role rule parameters (BRD §3, "板子相关机制变量"): mechanics owned by a
// specific role rather than the global flow. Grouped into one sub-struct per role
// so a board configures only the roles it uses and BoardConfig stays readable as
// roles grow. New role with tunable rules -> add a struct here, nest it below.

// Hunter (§2): death causes that forbid the death-triggered shot.
struct HunterRules {
    std::vector<DeathCause> shotBlockedBy = {DeathCause::Poisoned};
};

// WolfGun (§2): like the hunter, but a self-destruct (自爆) also forbids the shot.
struct WolfGunRules {
    std::vector<DeathCause> shotBlockedBy = {DeathCause::Poisoned, DeathCause::BlownUp};
};

struct MechanicWolfRules {
    // What the mechanic's learned 守卫 does to a poison aimed at its protected
    // target (BRD §3). Default = the canonical reflect (kills the poisoner).
    PoisonReflect poisonReflect = PoisonReflect::ReflectToPoisoner;
    // 破盾大刀 piercing (§2): by default the one-shot big knife ignores BOTH the
    // guard and the witch's antidote (and stays hidden from the witch). Turn either
    // off to let a守护/解药 stop it (and, for the antidote, surface it to the witch).
    bool bigKnifePiercesGuard = true;
    bool bigKnifePiercesAntidote = true;
    // Death causes that forbid the learned-hunter's shot (§2). Includes 自爆 because
    // the mechanic is a wolf and a self-destruct never shoots.
    std::vector<DeathCause> learnedHunterShotBlockedBy = {DeathCause::Poisoned,
                                                          DeathCause::BlownUp};
};

// Rule switches (BRD §3 config table). Defaults match the 9-player board.
// Flat fields below are global flow switches ("通用机制变量"); per-role tunables
// live in the nested *Rules structs.
struct BoardConfig {
    bool sheriffEnabled = true;
    WinRule winRule = WinRule::KillSide;
    WitchSelfRescue witchSelfRescue = WitchSelfRescue::Never;
    bool witchBothPotionsSameNight = false;
    bool blownUpEnabled = true;
    bool abstainAllowed = true;
    ExileTieRule exileTieRule = ExileTieRule::RunoffThenNoExile;
    bool guardConsecutiveSameTarget = false;  // guardian may NOT protect the same player twice (§2)
    HunterRules hunter;                        // §2 per-role: hunter shot-block causes
    WolfGunRules wolfGun;                      // §2 per-role: wolf-gun shot-block causes
    MechanicWolfRules mechanic;                // §2/§3 per-role: mechanic-wolf learned-ability rules
};

// One entry of the roster: `count` players of `kind`.
struct RoleSlot {
    RoleKind kind;
    int count;
};

struct Board {
    std::string name;
    std::vector<RoleSlot> roster;
    BoardConfig config;

    // Total number of players = sum of roster counts.
    int totalPlayers() const;
};

// First board: 9-player Seer/Witch/Hunter (BRD §3).
// 3 Werewolf + 3 Civilian + Seer + Witch + Hunter.
Board makeBoard9_SeerWitchHunter();

// Second board: 12-player Guard + WolfGun (BRD §3).
// 3 Werewolf + WolfGun + Seer + Witch + Hunter + Guardian + 4 Civilian.
Board makeBoard12_GuardWolfGun();

// Third board: 12-player Psychic + MechanicWolf (BRD §3).
// 3 Werewolf + MechanicWolf + Psychic + Witch + Hunter + Guardian + 4 Civilian.
Board makeBoard12_PsychicMechanic();

}  // namespace ww
