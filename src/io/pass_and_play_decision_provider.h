#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "io/console_decision_provider.h"

// PassAndPlayDecisionProvider — single-device multiplayer (BRD roadmap §3, M10).
//
// One screen, program is the moderator. Whenever a *private* moment is due (a
// player's secret decision or a secret notice), it hands the device over: clears
// the screen, asks everyone else to look away, shows only that player's content,
// then clears again before play continues. Public output (death announcements,
// vote tallies, day/night banners) is shown to everyone.
//
// This delivers §11 per-player secrecy on one device without any networking, and
// naturally hides the MechanicWolf's actions (others never see that screen).
namespace ww {

class PassAndPlayDecisionProvider : public ConsoleDecisionProvider {
public:
    PassAndPlayDecisionProvider(std::istream& in, std::ostream& out);

    // Privately reveal something to one player (e.g. their dealt role at setup).
    void privateAnnounce(int playerId, const std::string& message);

    // Private decisions — each wrapped in a hand-off to the acting player/team.
    std::optional<int> chooseNightKill(const GameState&, const std::vector<int>&) override;
    std::optional<int> chooseVote(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseInspect(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseGuard(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseMechanicLearn(const GameState&, int,
                                           const std::vector<int>&) override;
    std::optional<int> chooseMechanicBigKnife(const GameState&, int,
                                              const std::vector<int>&) override;
    bool chooseWitchSave(const GameState&, int, int) override;
    std::optional<int> chooseWitchPoison(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseHunterShot(const GameState&, int, const std::vector<int>&) override;
    bool chooseRunForSheriff(const GameState&, int) override;
    bool chooseWithdraw(const GameState&, int) override;
    std::optional<int> chooseSheriffVote(const GameState&, int, const std::vector<int>&) override;
    SheriffBallot chooseSheriffExileBallot(const GameState&, int,
                                           const std::vector<int>&) override;
    std::optional<int> chooseBadgeTransfer(const GameState&, int, const std::vector<int>&) override;
    SpeechDirection chooseSpeechDirection(const GameState&, int, int, bool) override;

    // Private notices — shown during the actor's hand-off.
    void onInspectResult(int seerId, int targetId, bool isWolf) override;
    void onPsychicResult(int psychicId, int targetId, RoleKind shownRole) override;
    void onHunterGunCheck(int hunterId, bool canShoot) override;
    void onMechanicLearnResult(int mechanicId, int targetId, RoleKind learnedRole) override;

    // Public output — ends any open private hand-off first, then shows to all.
    void notify(const std::string& message) override;
    void pause(const std::string& note) override;

private:
    std::istream& in_;
    std::ostream& out_;
    bool turnActive_ = false;
    int turnKey_ = 0;  // player id of the current private turn, or kWolfTeam

    static constexpr int kWolfTeam = -1;

    void clearScreen();
    void waitEnter();
    void ensureTurn(const std::string& who, int key);  // open a hand-off if needed
    void endTurn();                                     // close the current hand-off
};

}  // namespace ww
