#pragma once

#include <optional>
#include <string>
#include <vector>

#include "io/player_channel.h"

// BotChannel — a trivial in-process "AI" that makes legal, game-advancing moves
// so a whole match can run with no humans (BRD roadmap §3/§5 substrate). It is
// deliberately simple and deterministic; a real strategy / LLM agent is a future
// AgentChannel implementing the same interface.
//
// Policy: wolves knife the first living non-wolf; everyone votes the first living
// player that isn't themselves; all other optional actions are skipped; no one
// self-destructs, runs for sheriff, or talks.
namespace ww {

class BotChannel : public PlayerChannel {
public:
    explicit BotChannel(int ownerSeat) : owner_(ownerSeat) {}

    std::optional<int> chooseAmong(const GameState& state, AskKind kind, const std::string& prompt,
                                   const std::vector<int>& candidates, bool allowSkip) override;
    bool confirm(const GameState& state, AskKind kind, const std::string& prompt) override;
    std::string speak(const GameState& state, SpeechKind kind, const std::string& prompt) override;
    void tell(const std::string& message) override { (void)message; }

private:
    int owner_;
};

}  // namespace ww
