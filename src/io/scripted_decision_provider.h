#pragma once

#include <deque>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "io/decision_provider.h"

// ScriptedDecisionProvider feeds pre-set answers for deterministic end-to-end
// tests (BRD §13). Each decision queue is consumed FIFO in the order the flow
// requests it; an empty queue yields the "do nothing" default (空刀 / abstain /
// don't save / don't poison / don't shoot / don't self-destruct). Notifications
// and the seer's private results are captured for assertions.
namespace ww {

class ScriptedDecisionProvider : public DecisionProvider {
public:
    std::deque<std::optional<int>> nightKills;
    std::deque<std::optional<int>> votes;
    std::deque<std::optional<int>> inspects;
    std::deque<bool> witchSaves;
    std::deque<std::optional<int>> witchPoisons;
    std::deque<std::optional<int>> hunterShots;
    std::deque<std::optional<int>> selfDestructs;
    std::deque<std::optional<int>> guards;

    // Sheriff election (BRD §7).
    std::deque<bool> runForSheriff;
    std::deque<bool> withdraws;
    std::deque<std::optional<int>> sheriffVotes;
    std::deque<SheriffBallot> sheriffExileBallots;
    std::deque<std::optional<int>> badgeTransfers;  // a nullopt entry = tear up

    std::vector<std::string> events;
    // (seerId, targetId, isWolf) captured per inspection.
    std::vector<std::tuple<int, int, bool>> inspectResults;

    std::optional<int> chooseNightKill(const GameState&,
                                       const std::vector<int>&) override {
        return popOpt(nightKills);
    }

    std::optional<int> chooseVote(const GameState&, int,
                                  const std::vector<int>&) override {
        return popOpt(votes);
    }

    std::optional<int> chooseInspect(const GameState&, int,
                                     const std::vector<int>&) override {
        return popOpt(inspects);
    }

    std::optional<int> chooseGuard(const GameState&, int,
                                   const std::vector<int>&) override {
        return popOpt(guards);
    }

    bool chooseWitchSave(const GameState&, int, int) override {
        if (witchSaves.empty()) return false;
        bool v = witchSaves.front();
        witchSaves.pop_front();
        return v;
    }

    std::optional<int> chooseWitchPoison(const GameState&, int,
                                         const std::vector<int>&) override {
        return popOpt(witchPoisons);
    }

    std::optional<int> chooseHunterShot(const GameState&, int,
                                        const std::vector<int>&) override {
        return popOpt(hunterShots);
    }

    std::optional<int> chooseSelfDestruct(const GameState&,
                                          const std::vector<int>&) override {
        return popOpt(selfDestructs);
    }

    bool chooseRunForSheriff(const GameState&, int) override {
        return popBool(runForSheriff);
    }

    bool chooseWithdraw(const GameState&, int) override { return popBool(withdraws); }

    std::optional<int> chooseSheriffVote(const GameState&, int,
                                         const std::vector<int>&) override {
        return popOpt(sheriffVotes);
    }

    SheriffBallot chooseSheriffExileBallot(const GameState&, int,
                                           const std::vector<int>&) override {
        if (sheriffExileBallots.empty()) return {};
        SheriffBallot b = sheriffExileBallots.front();
        sheriffExileBallots.pop_front();
        return b;
    }

    std::optional<int> chooseBadgeTransfer(const GameState&, int,
                                           const std::vector<int>&) override {
        return popOpt(badgeTransfers);
    }

    void onInspectResult(int seerId, int targetId, bool isWolf) override {
        inspectResults.emplace_back(seerId, targetId, isWolf);
    }

    void notify(const std::string& message) override { events.push_back(message); }

private:
    static std::optional<int> popOpt(std::deque<std::optional<int>>& q) {
        if (q.empty()) return std::nullopt;
        std::optional<int> v = q.front();
        q.pop_front();
        return v;
    }

    static bool popBool(std::deque<bool>& q) {
        if (q.empty()) return false;
        bool v = q.front();
        q.pop_front();
        return v;
    }
};

}  // namespace ww
