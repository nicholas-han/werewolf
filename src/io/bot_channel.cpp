#include "io/bot_channel.h"

#include "core/game_state.h"
#include "core/player.h"

namespace ww {

std::optional<int> BotChannel::chooseAmong(const GameState& state, AskKind kind,
                                           const std::string& /*prompt*/,
                                           const std::vector<int>& candidates, bool allowSkip) {
    if (candidates.empty()) return std::nullopt;

    switch (kind) {
        case AskKind::NightKill: {
            // Knife the first living non-wolf (advances the game toward an end).
            for (int id : candidates) {
                const Player* p = state.find(id);
                if (p && p->faction() != Faction::Wolf) return id;
            }
            return candidates.front();
        }
        case AskKind::Vote:
        case AskKind::RunoffVote:
        case AskKind::SheriffVote:
        case AskKind::BallotTarget: {
            // Vote for the first candidate that isn't me.
            for (int id : candidates) {
                if (id != owner_) return id;
            }
            return allowSkip ? std::nullopt : std::optional<int>(candidates.front());
        }
        default:
            // Inspect / Guard / Poison / Shot / Learn / BigKnife / BadgeTransfer:
            // a placeholder bot skips when allowed.
            return allowSkip ? std::nullopt : std::optional<int>(candidates.front());
    }
}

bool BotChannel::confirm(const GameState& /*state*/, AskKind /*kind*/, const std::string& /*prompt*/) {
    return false;  // never save / run for sheriff / withdraw / self-destruct / 归单人
}

std::string BotChannel::speak(const GameState& /*state*/, SpeechKind /*kind*/,
                              const std::string& /*prompt*/) {
    return "";  // bots stay silent for now
}

}  // namespace ww
