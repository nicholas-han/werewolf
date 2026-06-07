#include "core/roles/role.h"

#include <string>

#include "core/abilities/role_abilities.h"

namespace ww {

std::unique_ptr<Role> makeRole(RoleKind kind, const BoardConfig& config) {
    switch (kind) {
        case RoleKind::Werewolf: {
            auto r = std::make_unique<Role>(kind, "Werewolf", Faction::Wolf, SubKind::None);
            r->addAbility(std::make_unique<NightKill>());
            return r;
        }
        case RoleKind::Seer: {
            auto r = std::make_unique<Role>(kind, "Seer", Faction::Town, SubKind::PowerRole);
            r->addAbility(std::make_unique<Inspect>());
            return r;
        }
        case RoleKind::Witch: {
            auto r = std::make_unique<Role>(kind, "Witch", Faction::Town, SubKind::PowerRole);
            r->addAbility(std::make_unique<WitchPotions>(config.witchSelfRescue,
                                                         config.witchBothPotionsSameNight));
            return r;
        }
        case RoleKind::Hunter: {
            auto r = std::make_unique<Role>(kind, "Hunter", Faction::Town, SubKind::PowerRole);
            r->addAbility(std::make_unique<DeathTriggerShoot>(
                "HunterShot", std::vector<DeathCause>{DeathCause::Poisoned}));
            r->addAbility(std::make_unique<HunterGunCheck>());  // nightly 验枪 gesture
            return r;
        }
        case RoleKind::Guardian: {
            auto r = std::make_unique<Role>(kind, "Guardian", Faction::Town, SubKind::PowerRole);
            r->addAbility(std::make_unique<Protect>(config.guardConsecutiveSameTarget));
            return r;
        }
        case RoleKind::WolfGun: {
            auto r = std::make_unique<Role>(kind, "WolfGun", Faction::Wolf, SubKind::None);
            r->addAbility(std::make_unique<NightKill>());
            r->addAbility(std::make_unique<DeathTriggerShoot>(
                "WolfGunShot",
                std::vector<DeathCause>{DeathCause::Poisoned, DeathCause::BlownUp}));
            return r;
        }
        case RoleKind::Psychic: {
            auto r = std::make_unique<Role>(kind, "Psychic", Faction::Town, SubKind::PowerRole);
            r->addAbility(std::make_unique<PsychicInspect>());
            return r;
        }
        case RoleKind::MechanicWolf: {
            auto r = std::make_unique<Role>(kind, "MechanicWolf", Faction::Wolf, SubKind::None);
            r->addAbility(std::make_unique<MechanicLearn>());
            r->addAbility(std::make_unique<MechanicLoneKill>());
            // Learned-active components — each is inert until the matching role is
            // learned (and from the next night). Composed once; no runtime mutation.
            r->addAbility(std::make_unique<MechanicLearnedInspect>());
            r->addAbility(std::make_unique<MechanicLearnedWitch>(config.witchBothPotionsSameNight));
            r->addAbility(std::make_unique<MechanicLearnedProtect>(config.guardConsecutiveSameTarget));
            r->addAbility(std::make_unique<MechanicLearnedShoot>());
            return r;
        }
        case RoleKind::Civilian:
            return std::make_unique<Role>(kind, "Civilian", Faction::Town, SubKind::Civilian);
    }
    // Unreachable for valid enum values.
    return std::make_unique<Role>(kind, "Unknown", Faction::Town, SubKind::Civilian);
}

}  // namespace ww
