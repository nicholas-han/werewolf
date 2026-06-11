#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/enums.h"

// PlayerChannel — one independent participant in a game (BRD §10/§11, roadmap §3).
//
// The engine talks to ONE DecisionProvider; RoutingDecisionProvider fans every
// per-player request out to that player's PlayerChannel. A channel is just a
// transport for "ask this participant something" / "tell this participant
// something" — it does not know the rules. Implementations:
//   - ScriptedChannel  : pre-fed answers (tests)
//   - BotChannel       : a trivial in-process AI making legal moves
//   - (future) AgentChannel / NetworkChannel : an LLM agent / a remote client
//
// Because every seat is just a PlayerChannel, the same engine drives humans,
// bots, and AI agents with no changes — the AI-agent milestone is a new channel.
namespace ww {

class GameState;

// What is being asked, so a bot/agent can branch on intent without parsing the
// human-facing prompt string (which is only for terminal/UI channels).
enum class AskKind {
    NightKill,         // wolf team's knife (asked of the team representative)
    SelfDestruct,      // does this wolf blow up?
    Vote,              // exile vote (round 1)
    RunoffVote,        // exile vote (runoff)
    Inspect,           // seer / psychic / learned-psychic check
    Guard,             // guardian / learned-guard protect
    WitchSave,         // use the antidote on the knifed player?
    WitchPoison,       // poison whom?
    HunterShot,        // death-triggered shot
    MechanicLearn,     // learn a player's identity
    MechanicBigKnife,  // fire the one-shot 破盾大刀
    RunForSheriff,     // stand for sheriff?
    Withdraw,          // withdraw from the race?
    SheriffVote,       // vote for sheriff
    ConsolidateSingle, // sheriff 归单人 (1.5) vs 归多人 PK?
    BallotTarget,      // sheriff's ballot target
    BadgeTransfer,     // hand the badge to whom (skip = tear up)?
    SpeechDirection    // open the speaking order to the left? (else right)
};

class PlayerChannel {
public:
    virtual ~PlayerChannel() = default;

    // Pick one player id among `candidates` (std::nullopt = skip/abstain when
    // `allowSkip`). `prompt` is human-facing text; `kind` is the machine intent.
    virtual std::optional<int> chooseAmong(const GameState& state, AskKind kind,
                                           const std::string& prompt,
                                           const std::vector<int>& candidates, bool allowSkip) = 0;

    // A yes/no decision.
    virtual bool confirm(const GameState& state, AskKind kind, const std::string& prompt) = 0;

    // Free-form speech ("" = passed); voice transcription feeds the same string.
    virtual std::string speak(const GameState& state, SpeechKind kind, const std::string& prompt) = 0;

    // Deliver a directed/broadcast message to this participant (private notice,
    // or a public event echoed to everyone's view).
    virtual void tell(const std::string& message) = 0;
};

}  // namespace ww
