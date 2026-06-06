#pragma once

#include <string_view>

// Core enumerations for the werewolf engine (BRD §1, §3).
namespace ww {

// Camp / faction. Town subdivides further via SubKind.
enum class Faction { Wolf, Town };

// Sub-classification. Meaningful for Town (神 PowerRole / 民 Civilian).
// Wolves use SubKind::None.
enum class SubKind { None, PowerRole, Civilian };

// Alive / out of the game.
enum class Status { Alive, Out };

// Cause of death (BRD §1). A player may accumulate several (e.g. Killed+Poisoned).
enum class DeathCause { Killed, Poisoned, Exiled, Shot, BlownUp };

// Coarse game phase. Sub-steps are handled by the flow layer (M1+).
enum class Phase { Night, Day };

// Concrete roles in the first board (BRD §3).
enum class RoleKind { Werewolf, Seer, Witch, Hunter, Civilian };

// --- Board config option enums (BRD §3) ---

// Win rule (BRD §4).
enum class WinRule { KillSide, KillAll };

// Witch self-rescue policy (BRD §2). First board: Never.
enum class WitchSelfRescue { Never, FirstNightOnly, Always };

// Exile-vote tie handling (BRD §6).
enum class ExileTieRule { RunoffThenNoExile };

// --- Small to_string helpers for logging / test output ---

constexpr std::string_view to_string(Faction f) {
    switch (f) {
        case Faction::Wolf: return "Wolf";
        case Faction::Town: return "Town";
    }
    return "?";
}

constexpr std::string_view to_string(SubKind s) {
    switch (s) {
        case SubKind::None: return "None";
        case SubKind::PowerRole: return "PowerRole";
        case SubKind::Civilian: return "Civilian";
    }
    return "?";
}

constexpr std::string_view to_string(Status s) {
    switch (s) {
        case Status::Alive: return "Alive";
        case Status::Out: return "Out";
    }
    return "?";
}

constexpr std::string_view to_string(DeathCause c) {
    switch (c) {
        case DeathCause::Killed: return "Killed";
        case DeathCause::Poisoned: return "Poisoned";
        case DeathCause::Exiled: return "Exiled";
        case DeathCause::Shot: return "Shot";
        case DeathCause::BlownUp: return "BlownUp";
    }
    return "?";
}

constexpr std::string_view to_string(RoleKind r) {
    switch (r) {
        case RoleKind::Werewolf: return "Werewolf";
        case RoleKind::Seer: return "Seer";
        case RoleKind::Witch: return "Witch";
        case RoleKind::Hunter: return "Hunter";
        case RoleKind::Civilian: return "Civilian";
    }
    return "?";
}

}  // namespace ww
