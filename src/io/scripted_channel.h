#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "io/player_channel.h"

// ScriptedChannel — a PlayerChannel with pre-fed answers, for deterministic
// per-seat tests of RoutingDecisionProvider (mirrors ScriptedDecisionProvider).
// Each queue is consumed FIFO; an empty queue yields the safe default
// (skip / no / silent). Everything told to this seat is captured in `told`.
namespace ww {

class ScriptedChannel : public PlayerChannel {
public:
    std::deque<std::optional<int>> choices;  // chooseAmong answers
    std::deque<bool> confirms;               // confirm answers
    std::deque<std::string> speeches;        // speak answers
    std::vector<std::string> told;           // captured messages (private + public)

    std::optional<int> chooseAmong(const GameState&, AskKind, const std::string&,
                                   const std::vector<int>&, bool) override {
        if (choices.empty()) return std::nullopt;
        std::optional<int> v = choices.front();
        choices.pop_front();
        return v;
    }

    bool confirm(const GameState&, AskKind, const std::string&) override {
        if (confirms.empty()) return false;
        bool v = confirms.front();
        confirms.pop_front();
        return v;
    }

    std::string speak(const GameState&, SpeechKind, const std::string&) override {
        if (speeches.empty()) return "";
        std::string v = speeches.front();
        speeches.pop_front();
        return v;
    }

    void tell(const std::string& message) override { told.push_back(message); }
};

}  // namespace ww
