#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/abilities/ability.h"
#include "core/board.h"
#include "core/enums.h"

// Role is a lightweight metadata holder + a set of composed abilities
// (BRD §8.0/§8.1). It is deliberately flat: no deep inheritance. Faction /
// SubKind are data; behaviour lives in Ability components.
namespace ww {

class Role {
public:
    Role(RoleKind kind, std::string name, Faction faction, SubKind subKind)
        : kind_(kind), name_(std::move(name)), faction_(faction), subKind_(subKind) {}

    RoleKind kind() const { return kind_; }
    const std::string& name() const { return name_; }
    Faction faction() const { return faction_; }
    SubKind subKind() const { return subKind_; }

    const std::vector<std::unique_ptr<Ability>>& abilities() const { return abilities_; }
    void addAbility(std::unique_ptr<Ability> ability) {
        abilities_.push_back(std::move(ability));
    }

private:
    RoleKind kind_;
    std::string name_;
    Faction faction_;
    SubKind subKind_;
    std::vector<std::unique_ptr<Ability>> abilities_;
};

// Builds a role: metadata (faction / subKind / name) + composed abilities
// (BRD §8). `config` parameterises ability behaviour (e.g. witch potion rules);
// it defaults to the standard 9-player config.
std::unique_ptr<Role> makeRole(RoleKind kind, const BoardConfig& config = {});

}  // namespace ww
