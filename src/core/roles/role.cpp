#include "core/roles/role.h"

#include <string>

namespace ww {

std::unique_ptr<Role> makeRole(RoleKind kind) {
    switch (kind) {
        case RoleKind::Werewolf:
            return std::make_unique<Role>(kind, "Werewolf", Faction::Wolf, SubKind::None);
        case RoleKind::Seer:
            return std::make_unique<Role>(kind, "Seer", Faction::Town, SubKind::PowerRole);
        case RoleKind::Witch:
            return std::make_unique<Role>(kind, "Witch", Faction::Town, SubKind::PowerRole);
        case RoleKind::Hunter:
            return std::make_unique<Role>(kind, "Hunter", Faction::Town, SubKind::PowerRole);
        case RoleKind::Civilian:
            return std::make_unique<Role>(kind, "Civilian", Faction::Town, SubKind::Civilian);
    }
    // Unreachable for valid enum values.
    return std::make_unique<Role>(kind, "Unknown", Faction::Town, SubKind::Civilian);
}

}  // namespace ww
