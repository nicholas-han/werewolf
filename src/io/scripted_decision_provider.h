#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "io/decision_provider.h"

// ScriptedDecisionProvider feeds pre-set answers for deterministic end-to-end
// tests (BRD §13). Decisions are consumed FIFO in the exact order the flow
// requests them:
//   - night kills: one per night (default std::nullopt = 空刀 when exhausted);
//   - votes: requested per voter in ascending seat order among the alive
//     (default std::nullopt = abstain when exhausted).
// Notifications are captured into `events` for assertions.
namespace ww {

class ScriptedDecisionProvider : public DecisionProvider {
public:
    std::deque<std::optional<int>> nightKills;
    std::deque<std::optional<int>> votes;
    std::vector<std::string> events;

    std::optional<int> chooseNightKill(const GameState&,
                                       const std::vector<int>&) override {
        if (nightKills.empty()) return std::nullopt;
        auto v = nightKills.front();
        nightKills.pop_front();
        return v;
    }

    std::optional<int> chooseVote(const GameState&, int /*voterId*/,
                                  const std::vector<int>&) override {
        if (votes.empty()) return std::nullopt;
        auto v = votes.front();
        votes.pop_front();
        return v;
    }

    void notify(const std::string& message) override { events.push_back(message); }
};

}  // namespace ww
