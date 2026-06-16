#pragma once

#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "flow/win_condition.h"  // GameResult
#include "io/decision_provider.h"
#include "io/event_sink.h"
#include "io/json_util.h"
#include "io/player_channel.h"  // AskKind

// JsonDecisionProvider — the engine's I/O over the JSON-lines protocol
// (docs/protocol_v1.md). Each decision becomes an `ask` written to `out`, then a
// blocking read of a matching `reply` from `in`; each information output becomes
// an `event`. The rule engine is unchanged — this is purely a DecisionProvider
// implementation, standalone so RoutingDecisionProvider / Console stay untouched.
//
// The orchestrator (Python) drives this: it owns the human terminal + all AI
// brains and guarantees a legal reply per ask; on EOF / bad / illegal replies
// the engine falls back to a legal default so a game always terminates (§8).
namespace ww {

class JsonDecisionProvider : public DecisionProvider {
public:
    JsonDecisionProvider(std::istream& in, std::ostream& out, std::string boardName, unsigned seed);

    // Driver hooks (called by app/main.cpp around Game::run): the roster, the
    // private per-seat deal, and the final result.
    void emitGameStart(const GameState& state);
    void emitDeals(const GameState& state);
    void emitGameOver(GameResult result);

    // --- DecisionProvider overrides ---
    std::optional<int> chooseNightKill(const GameState&, const std::vector<int>&) override;
    std::optional<int> chooseVote(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseInspect(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseGuard(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseMechanicLearn(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseMechanicBigKnife(const GameState&, int, const std::vector<int>&) override;
    bool chooseWitchSave(const GameState&, int, int) override;
    std::optional<int> chooseWitchPoison(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseHunterShot(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseSelfDestruct(const GameState&, const std::vector<int>&) override;
    bool chooseRunForSheriff(const GameState&, int) override;
    bool chooseWithdraw(const GameState&, int) override;
    std::optional<int> chooseSheriffVote(const GameState&, int, const std::vector<int>&) override;
    SheriffBallot chooseSheriffExileBallot(const GameState&, int, const std::vector<int>&) override;
    std::optional<int> chooseBadgeTransfer(const GameState&, int, const std::vector<int>&) override;
    SpeechDirection chooseSpeechDirection(const GameState&, int, int, bool) override;
    std::string collectSpeech(const GameState&, int, SpeechKind, int) override;

    void onInspectResult(int seerId, int targetId, bool isWolf) override;
    void onPsychicResult(int psychicId, int targetId, RoleKind shownRole) override;
    void onHunterGunCheck(int hunterId, bool canShoot) override;
    void onMechanicLearnResult(int mechanicId, int targetId, RoleKind learnedRole) override;

    void notify(const std::string& message) override;
    void notifyPlayer(int playerId, const std::string& message) override;
    void notifyModerator(const std::string& message) override;
    void pause(const std::string& note) override;

private:
    std::istream& in_;
    std::ostream& out_;
    JsonEventSink sink_;
    std::string boardName_;
    unsigned seed_;
    int nextId_ = 1;
    int curDay_ = 1;
    std::string curPhase_ = "Night";

    void writeLine(const std::string& s);
    jsonu::Value readReply();
    void sync(const GameState& s);

    std::string nameOf(const GameState&, int id) const;
    std::string candidatesArray(const GameState&, const std::vector<int>&) const;
    int wolfRepresentative(const GameState&) const;

    // ask helpers (logMod=true also emits a moderator `decision` event)
    std::optional<int> askChoose(const GameState&, int seat, AskKind, const std::string& prompt,
                                 const std::vector<int>& candidates, bool allowSkip, bool logMod);
    bool askConfirm(const GameState&, int seat, AskKind, const std::string& prompt, bool logMod);
    std::string askSpeak(const GameState&, int seat, SpeechKind, const std::string& prompt);

    void emitDecision(int seat, const char* kind, std::optional<int> target);
    void emitNarration(Vis vis, std::optional<int> seat, const std::string& text);
};

}  // namespace ww
