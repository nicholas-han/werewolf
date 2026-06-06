#include "core/game_state.h"

#include "core/roles/role.h"

namespace ww {

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

}  // namespace ww
