#pragma once

#include <string>

#include "core/game_state.h"
#include "core/messages.h"
#include "core/player.h"

// Replay formatting for the speech log (BRD roadmap §4 发言记录 / 复盘). Reads the
// recorded speeches off the truth layer and renders a day-grouped transcript.
namespace ww {

inline std::string formatTranscript(const GameState& state) {
    std::string out = txt::transcriptHeader() + "\n";
    if (state.speeches.empty()) {
        out += txt::transcriptEmpty() + "\n";
        return out;
    }
    int curDay = -1;
    for (const GameState::SpeechEntry& e : state.speeches) {
        if (e.day != curDay) {
            out += txt::transcriptDay(e.day) + "\n";
            curDay = e.day;
        }
        const Player* p = state.find(e.speakerId);
        const std::string name = p ? p->name() : ("#" + std::to_string(e.speakerId));
        out += txt::transcriptLine(e.seat, name, e.kind, e.text) + "\n";
    }
    return out;
}

}  // namespace ww
