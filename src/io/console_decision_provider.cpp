#include "io/console_decision_provider.h"

#include <algorithm>
#include <cctype>
#include <istream>
#include <ostream>
#include <string>

#include "core/game_state.h"
#include "core/player.h"

namespace ww {

namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

bool parseInt(const std::string& s, int& out) {
    try {
        size_t pos = 0;
        int v = std::stoi(s, &pos);
        if (pos != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

ConsoleDecisionProvider::ConsoleDecisionProvider(std::istream& in, std::ostream& out)
    : in_(in), out_(out) {}

std::string ConsoleDecisionProvider::nameOf(const GameState& state, int id) {
    const Player* p = state.find(id);
    return p ? p->name() : ("#" + std::to_string(id));
}

void ConsoleDecisionProvider::listCandidates(const GameState& state,
                                             const std::vector<int>& candidates) {
    out_ << "  options:";
    for (int id : candidates) out_ << " " << id << "(" << nameOf(state, id) << ")";
    out_ << "\n";
}

std::optional<std::string> ConsoleDecisionProvider::readLine() {
    std::string line;
    if (!std::getline(in_, line)) return std::nullopt;
    return trim(line);
}

std::optional<int> ConsoleDecisionProvider::promptOptional(const std::string& prompt,
                                                           const GameState& state,
                                                           const std::vector<int>& candidates) {
    for (;;) {
        out_ << prompt << " (blank = none)\n";
        listCandidates(state, candidates);
        out_ << "> ";
        std::optional<std::string> line = readLine();
        if (!line) return std::nullopt;       // EOF -> treat as no choice
        if (line->empty()) return std::nullopt;
        int v = 0;
        if (!parseInt(*line, v)) {
            out_ << "  ! not a number, try again\n";
            continue;
        }
        if (std::find(candidates.begin(), candidates.end(), v) == candidates.end()) {
            out_ << "  ! not a valid option, try again\n";
            continue;
        }
        return v;
    }
}

int ConsoleDecisionProvider::promptRequired(const std::string& prompt, const GameState& state,
                                            const std::vector<int>& candidates) {
    for (;;) {
        out_ << prompt << "\n";
        listCandidates(state, candidates);
        out_ << "> ";
        std::optional<std::string> line = readLine();
        if (!line) return candidates.empty() ? -1 : candidates.front();  // EOF fallback
        if (line->empty()) {
            out_ << "  ! a choice is required\n";
            continue;
        }
        int v = 0;
        if (!parseInt(*line, v) ||
            std::find(candidates.begin(), candidates.end(), v) == candidates.end()) {
            out_ << "  ! not a valid option, try again\n";
            continue;
        }
        return v;
    }
}

bool ConsoleDecisionProvider::promptYesNo(const std::string& prompt) {
    for (;;) {
        out_ << prompt << " (y/n)\n> ";
        std::optional<std::string> line = readLine();
        if (!line) return false;  // EOF -> no
        if (line->empty()) continue;
        char c = static_cast<char>(std::tolower((*line)[0]));
        if (c == 'y') return true;
        if (c == 'n') return false;
        out_ << "  ! please answer y or n\n";
    }
}

std::optional<int> ConsoleDecisionProvider::chooseNightKill(const GameState& state,
                                                            const std::vector<int>& candidates) {
    return promptOptional("[Night] Werewolves, choose a kill target", state, candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseVote(const GameState& state, int voterId,
                                                       const std::vector<int>& candidates) {
    return promptOptional("[Day] " + nameOf(state, voterId) + ", vote to exile", state, candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseInspect(const GameState& state, int seerId,
                                                          const std::vector<int>& candidates) {
    return promptOptional("[Night] Seer " + nameOf(state, seerId) + ", inspect whom", state,
                          candidates);
}

bool ConsoleDecisionProvider::chooseWitchSave(const GameState& state, int witchId, int knifedId) {
    return promptYesNo("[Night] Witch " + nameOf(state, witchId) + ", " + nameOf(state, knifedId) +
                       " was knifed. Use the antidote?");
}

std::optional<int> ConsoleDecisionProvider::chooseWitchPoison(const GameState& state, int witchId,
                                                              const std::vector<int>& candidates) {
    return promptOptional("[Night] Witch " + nameOf(state, witchId) + ", poison whom", state,
                          candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseHunterShot(const GameState& state, int hunterId,
                                                             const std::vector<int>& candidates) {
    return promptOptional("[Death] Hunter " + nameOf(state, hunterId) + ", shoot whom", state,
                          candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseSelfDestruct(const GameState& state,
                                                               const std::vector<int>& wolfIds) {
    return promptOptional("[Day] Any wolf self-destruct? Enter the wolf's seat", state, wolfIds);
}

bool ConsoleDecisionProvider::chooseRunForSheriff(const GameState& state, int playerId) {
    return promptYesNo("[Election] " + nameOf(state, playerId) + ", run for sheriff?");
}

bool ConsoleDecisionProvider::chooseWithdraw(const GameState& state, int candidateId) {
    return promptYesNo("[Election] " + nameOf(state, candidateId) + ", withdraw from the race?");
}

std::optional<int> ConsoleDecisionProvider::chooseSheriffVote(const GameState& state, int voterId,
                                                              const std::vector<int>& candidates) {
    return promptOptional("[Election] " + nameOf(state, voterId) + ", vote for sheriff", state,
                          candidates);
}

SheriffBallot ConsoleDecisionProvider::chooseSheriffExileBallot(const GameState& state,
                                                                int sheriffId,
                                                                const std::vector<int>& candidates) {
    out_ << "[Day] Sheriff " << nameOf(state, sheriffId) << " 归票.\n";
    if (promptYesNo("  归单人 (badge = 1.5, must vote)? (n = 归多人PK, badge = 1, may abstain)")) {
        int target = promptRequired("  归单人 target", state, candidates);
        return SheriffBallot{true, target};
    }
    std::optional<int> target = promptOptional("  归多人PK vote (blank = abstain)", state, candidates);
    return SheriffBallot{false, target};
}

std::optional<int> ConsoleDecisionProvider::chooseBadgeTransfer(const GameState& state,
                                                                int sheriffId,
                                                                const std::vector<int>& candidates) {
    out_ << "[Death] Sheriff " << nameOf(state, sheriffId) << " is out.\n";
    return promptOptional("  Transfer the badge to whom (blank = tear it up)", state, candidates);
}

SpeechDirection ConsoleDecisionProvider::chooseSpeechDirection(const GameState& state,
                                                               int sheriffId, int anchorSeat,
                                                               bool singleDeath) {
    out_ << "[Day] Sheriff " << nameOf(state, sheriffId) << " sets the speaking order.\n";
    out_ << "  start from " << (singleDeath ? "the dead player's" : "the sheriff's")
         << " seat " << anchorSeat << "; direction L = toward higher seats, R = toward lower\n";
    return promptYesNo("  go Left (higher seats)? (n = Right)") ? SpeechDirection::Left
                                                                : SpeechDirection::Right;
}

void ConsoleDecisionProvider::onInspectResult(int seerId, int targetId, bool isWolf) {
    out_ << "[Private->Seer #" << seerId << "] #" << targetId << " is "
         << (isWolf ? "a WEREWOLF (查杀)" : "GOOD (金水)") << "\n";
}

void ConsoleDecisionProvider::notify(const std::string& message) {
    out_ << message << "\n";
}

void ConsoleDecisionProvider::pause(const std::string& note) {
    out_ << note << " [press Enter] ";
    readLine();  // block until Enter (or EOF)
}

}  // namespace ww
