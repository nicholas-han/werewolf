#include "core/game_state.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <random>

#include "core/roles/role.h"

namespace ww {

GameState::Snapshot GameState::snapshot() const {
    Snapshot s;
    s.players.reserve(players.size());
    for (const Player& p : players) s.players.push_back(p.snapshot());
    s.phase = phase;
    s.day = day;
    s.sheriffId = sheriffId;
    s.witchAntidoteAvailable = witchAntidoteAvailable;
    s.witchPoisonAvailable = witchPoisonAvailable;
    s.lastGuardedId = lastGuardedId;
    s.mechanicLearned = mechanicLearned;
    s.mechanicLearnDay = mechanicLearnDay;
    s.mechanicAntidoteAvailable = mechanicAntidoteAvailable;
    s.mechanicPoisonAvailable = mechanicPoisonAvailable;
    s.mechanicBigKnifeAvailable = mechanicBigKnifeAvailable;
    s.mechanicLastGuardedId = mechanicLastGuardedId;
    s.logSize = log.size();
    return s;
}

void GameState::restore(const Snapshot& snap) {
    // Player count is fixed across a game, so restore by index.
    for (std::size_t i = 0; i < players.size() && i < snap.players.size(); ++i) {
        players[i].restore(snap.players[i]);
    }
    phase = snap.phase;
    day = snap.day;
    sheriffId = snap.sheriffId;
    witchAntidoteAvailable = snap.witchAntidoteAvailable;
    witchPoisonAvailable = snap.witchPoisonAvailable;
    lastGuardedId = snap.lastGuardedId;
    mechanicLearned = snap.mechanicLearned;
    mechanicLearnDay = snap.mechanicLearnDay;
    mechanicAntidoteAvailable = snap.mechanicAntidoteAvailable;
    mechanicPoisonAvailable = snap.mechanicPoisonAvailable;
    mechanicBigKnifeAvailable = snap.mechanicBigKnifeAvailable;
    mechanicLastGuardedId = snap.mechanicLastGuardedId;
    if (log.size() > snap.logSize) log.resize(snap.logSize);
}

Player* GameState::find(int id) {
    for (Player& p : players) {
        if (p.id() == id) return &p;
    }
    return nullptr;
}

const Player* GameState::find(int id) const {
    for (const Player& p : players) {
        if (p.id() == id) return &p;
    }
    return nullptr;
}

std::vector<Player*> GameState::alive() {
    std::vector<Player*> result;
    for (Player& p : players) {
        if (p.isAlive()) result.push_back(&p);
    }
    return result;
}

std::vector<const Player*> GameState::alive() const {
    std::vector<const Player*> result;
    for (const Player& p : players) {
        if (p.isAlive()) result.push_back(&p);
    }
    return result;
}

int GameState::countAlive(Faction faction) const {
    int n = 0;
    for (const Player& p : players) {
        if (p.isAlive() && p.faction() == faction) ++n;
    }
    return n;
}

int GameState::countAlive(SubKind subKind) const {
    int n = 0;
    for (const Player& p : players) {
        if (p.isAlive() && p.subKind() == subKind) ++n;
    }
    return n;
}

int GameState::countAliveRole(RoleKind kind) const {
    int n = 0;
    for (const Player& p : players) {
        if (p.isAlive() && p.role().kind() == kind) ++n;
    }
    return n;
}

int GameState::countAliveOpenWolves() const {
    int n = 0;
    for (const Player& p : players) {
        if (p.isAlive() && p.faction() == Faction::Wolf &&
            p.role().kind() != RoleKind::MechanicWolf) {
            ++n;
        }
    }
    return n;
}

GameState buildInitialState(const Board& board) {
    GameState state;
    int seat = 1;
    for (const RoleSlot& slot : board.roster) {
        for (int i = 0; i < slot.count; ++i) {
            int id = seat;  // id == seat for the deterministic initial layout
            state.players.emplace_back(id, "P" + std::to_string(seat), seat,
                                       makeRole(slot.kind, board.config));
            ++seat;
        }
    }
    return state;
}

GameState buildInitialState(const Board& board, const std::vector<RoleKind>& seatRoles) {
    GameState state;
    for (std::size_t i = 0; i < seatRoles.size(); ++i) {
        const int seat = static_cast<int>(i) + 1;
        state.players.emplace_back(seat, "P" + std::to_string(seat), seat,
                                   makeRole(seatRoles[i], board.config));
    }
    return state;
}

bool seatRolesMatchRoster(const Board& board, const std::vector<RoleKind>& seatRoles) {
    std::map<RoleKind, int> need;
    for (const RoleSlot& slot : board.roster) need[slot.kind] += slot.count;
    std::map<RoleKind, int> have;
    for (RoleKind r : seatRoles) have[r] += 1;
    return need == have;
}

std::vector<RoleKind> rosterRoleList(const Board& board) {
    std::vector<RoleKind> roles;
    for (const RoleSlot& slot : board.roster) {
        for (int i = 0; i < slot.count; ++i) roles.push_back(slot.kind);
    }
    return roles;
}

std::vector<RoleKind> randomDeal(const Board& board, unsigned seed) {
    std::vector<RoleKind> roles = rosterRoleList(board);
    std::mt19937 rng(seed);
    std::shuffle(roles.begin(), roles.end(), rng);
    return roles;
}

}  // namespace ww
