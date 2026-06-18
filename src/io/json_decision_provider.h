#pragma once

#include <chrono>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
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

// Background-reader plumbing for the soft `ask` timeout (defined in the .cpp).
struct JsonReplyChannel;

class JsonDecisionProvider : public DecisionProvider {
public:
    JsonDecisionProvider(std::istream& in, std::ostream& out, std::string boardName, unsigned seed);
    ~JsonDecisionProvider() override;

    // Soft per-`ask` timeout (docs/protocol_v1.md §8). After emitting an `ask`, the
    // engine waits at most this long for the matching `reply`; on timeout it falls
    // back to the same legal default used for malformed/EOF replies so a game can
    // never hang on a dead/silent orchestrator. <= 0 disables the timeout (wait
    // indefinitely, the pre-timeout behavior). Default 600s.
    void setAskTimeout(std::chrono::milliseconds timeout) { askTimeout_ = timeout; }

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
    std::string collectWolfChat(const GameState&, int, const std::vector<int>&, int) override;

    void onInspectResult(int seerId, int targetId, bool isWolf) override;
    void onPsychicResult(int psychicId, int targetId, RoleKind shownRole) override;
    void onHunterGunCheck(int hunterId, bool canShoot) override;
    void onMechanicLearnResult(int mechanicId, int targetId, RoleKind learnedRole) override;

    void notify(const std::string& message) override;
    void notifyPlayer(int playerId, const std::string& message) override;
    void notifyModerator(const std::string& message) override;
    void pause(const std::string& note) override;
    void onPhaseEnter(const GameState& state) override { sync(state); }

private:
    std::istream& in_;
    std::ostream& out_;
    JsonEventSink sink_;
    std::string boardName_;
    unsigned seed_;
    int nextId_ = 1;
    int curDay_ = 1;
    std::string curPhase_ = "Night";

    // Soft-timeout machinery. A single background thread drains `in_` into
    // `replyCh_` so the main (engine) thread can wait for a reply with a deadline
    // instead of blocking forever in getline. `timedOut_` records whether the last
    // readReply() fell back because of a timeout (vs EOF / a real reply).
    std::chrono::milliseconds askTimeout_{std::chrono::seconds(600)};
    std::shared_ptr<JsonReplyChannel> replyCh_;
    std::thread reader_;
    bool readerStarted_ = false;
    bool timedOut_ = false;

    void writeLine(const std::string& s);
    void ensureReader();        // lazily spawn the background stdin reader
    jsonu::Value readReply();   // blocks up to askTimeout_; Null = no usable reply
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
    // Moderator-only note that `kind` for `seat` fell back due to a reply timeout
    // (§8). Emitted alongside the normal fallback so the god-script records why.
    void emitTimeoutFallback(int seat, const char* kind);
};

}  // namespace ww
