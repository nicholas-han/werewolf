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
            r->addAbility(std::make_unique<HunterShot>());
            return r;
        }
        case RoleKind::Civilian:
            return std::make_unique<Role>(kind, "Civilian", Faction::Town, SubKind::Civilian);
    }
    // Unreachable for valid enum values.
    return std::make_unique<Role>(kind, "Unknown", Faction::Town, SubKind::Civilian);
}

}  // namespace ww
