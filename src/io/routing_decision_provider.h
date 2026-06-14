#pragma once

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "io/decision_provider.h"
#include "io/player_channel.h"

// RoutingDecisionProvider — turns the engine's single DecisionProvider into
// per-player channels (BRD §10/§11, roadmap §3 多人). Every player-scoped request
// is routed to that player's PlayerChannel; public events broadcast to all;
// god-view goes only to an optional spectator log. The rule engine is unchanged.
//
// Team decisions: the wolves' nightly knife is decided by ONE representative (the
// lowest-seat alive wolf, preferring an "open" wolf over the MechanicWolf); the
// other open wolves are privately told the chosen target. Self-destruct is asked
// of each wolf individually (it is a personal declaration).
namespace ww {

class RoutingDecisionProvider : public DecisionProvider {
public:
    // `channels` maps player id -> that player's channel (non-owning). `publicLog`
    // (optional) receives public + moderator narration for a spectator view.
    explicit RoutingDecisionProvider(std::map<int, PlayerChannel*> channels,
                                     std::ostream* publicLog = nullptr);

    // --- team decisions ---
    std::optional<int> chooseNightKill(const GameState&, const std::vector<int>&) override;
    std::optional<int> chooseSelfDestruct(const GameState&, const std::vector<int>&) override;

    // --- per-player choices ---
    std::optional<int> chooseVote(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseInspect(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseGuard(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseMechanicLearn(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseMechanicBigKnife(const GameState&, int,
                                              const std::vector<int>&) override;
    std::optional<int> chooseWitchPoison(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseHunterShot(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseSheriffVote(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseBadgeTransfer(const GameState&, int, const std::vector<int>&) override;

    // --- per-player confirms ---
    bool chooseWitchSave(const GameState&, int, int) override;
    bool chooseRunForSheriff(const GameState&, int) override;
    bool chooseWithdraw(const GameState&, int) override;

    // --- composite ---
    SheriffBallot chooseSheriffExileBallot(const GameState&, int, const std::vector<int>&) override;
    SpeechDirection chooseSpeechDirection(const GameState&, int, int, bool) override;
    std::string collectSpeech(const GameState&, int, SpeechKind, int) override;

    // --- directed notices ---
    void onInspectResult(int seerId, int targetId, bool isWolf) override;
    void onPsychicResult(int psychicId, int targetId, RoleKind shownRole) override;
    void onHunterGunCheck(int hunterId, bool canShoot) override;
    void onMechanicLearnResult(int mechanicId, int targetId, RoleKind learnedRole) override;

    // --- output ---
    void notify(const std::string& message) override;            // public -> everyone
    void notifyPlayer(int playerId, const std::string& message) override;  // private -> one
    void notifyModerator(const std::string& message) override;   // god-view -> spectator only
    void pause(const std::string& note) override;                // non-blocking note to spectator

private:
    std::map<int, PlayerChannel*> channels_;
    std::ostream* publicLog_;

    PlayerChannel* ch(int id);
    // Lowest-seat alive wolf, preferring open wolves over the MechanicWolf.
    int wolfRepresentative(const GameState&) const;
};

}  // namespace ww
