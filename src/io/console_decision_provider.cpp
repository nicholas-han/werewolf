#include "io/console_decision_provider.h"

#include <algorithm>
#include <cctype>
#include <istream>
#include <ostream>
#include <string>

#include "core/game_state.h"
#include "core/messages.h"
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
    out_ << "  可选:";
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
        out_ << prompt << "（直接回车=不选）\n";
        listCandidates(state, candidates);
        out_ << "> ";
        std::optional<std::string> line = readLine();
        if (!line) return std::nullopt;       // EOF -> treat as no choice
        if (line->empty()) return std::nullopt;
        int v = 0;
        if (!parseInt(*line, v)) {
            out_ << "  ！请输入数字\n";
            continue;
        }
        if (std::find(candidates.begin(), candidates.end(), v) == candidates.end()) {
            out_ << "  ！不是有效选项，请重输\n";
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
            out_ << "  ！必须选择一个\n";
            continue;
        }
        int v = 0;
        if (!parseInt(*line, v) ||
            std::find(candidates.begin(), candidates.end(), v) == candidates.end()) {
            out_ << "  ！不是有效选项，请重输\n";
            continue;
        }
        return v;
    }
}

bool ConsoleDecisionProvider::promptYesNo(const std::string& prompt) {
    for (;;) {
        out_ << prompt << "（y/n）\n> ";
        std::optional<std::string> line = readLine();
        if (!line) return false;  // EOF -> no
        if (line->empty()) continue;
        char c = static_cast<char>(std::tolower((*line)[0]));
        if (c == 'y') return true;
        if (c == 'n') return false;
        out_ << "  ！请输入 y 或 n\n";
    }
}

std::optional<int> ConsoleDecisionProvider::chooseNightKill(const GameState& state,
                                                            const std::vector<int>& candidates) {
    return promptOptional("【夜晚】狼人请选择刀杀目标", state, candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseVote(const GameState& state, int voterId,
                                                       const std::vector<int>& candidates) {
    return promptOptional("【白天】" + nameOf(state, voterId) + " 请投票放逐", state, candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseInspect(const GameState& state, int seerId,
                                                          const std::vector<int>& candidates) {
    return promptOptional("【夜晚】预言家 " + nameOf(state, seerId) + " 请查验", state, candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseGuard(const GameState& state, int guardId,
                                                        const std::vector<int>& candidates) {
    return promptOptional("【夜晚】守卫 " + nameOf(state, guardId) + " 请守护（可空守）", state,
                          candidates);
}

bool ConsoleDecisionProvider::chooseWitchSave(const GameState& state, int witchId, int knifedId) {
    return promptYesNo("【夜晚】女巫 " + nameOf(state, witchId) + "，今晚 " + nameOf(state, knifedId) +
                       " 被刀，是否使用解药？");
}

std::optional<int> ConsoleDecisionProvider::chooseWitchPoison(const GameState& state, int witchId,
                                                              const std::vector<int>& candidates) {
    return promptOptional("【夜晚】女巫 " + nameOf(state, witchId) + " 是否使用毒药（毒谁）", state,
                          candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseHunterShot(const GameState& state, int shooterId,
                                                             const std::vector<int>& candidates) {
    // Serves any death-triggered shot (hunter / wolfgun); name the shooter's role.
    const Player* p = state.find(shooterId);
    const std::string role = p ? txt::role(p->role().kind()) : "枪手";
    return promptOptional("【死亡结算】" + role + " " + nameOf(state, shooterId) + " 是否开枪带人",
                          state, candidates);
}

std::optional<int> ConsoleDecisionProvider::chooseSelfDestruct(const GameState& state,
                                                               const std::vector<int>& wolfIds) {
    // Self-destruct is a spontaneous *player* declaration, not a daily moderator
    // question. Until a player/declaration channel exists, the console never
    // offers it (the engine logic remains, exercised by scripted tests).
    (void)state;
    (void)wolfIds;
    return std::nullopt;
}

bool ConsoleDecisionProvider::chooseRunForSheriff(const GameState& state, int playerId) {
    return promptYesNo("【竞选】" + nameOf(state, playerId) + " 是否上警？");
}

bool ConsoleDecisionProvider::chooseWithdraw(const GameState& state, int candidateId) {
    return promptYesNo("【竞选】" + nameOf(state, candidateId) + " 是否退水？");
}

std::optional<int> ConsoleDecisionProvider::chooseSheriffVote(const GameState& state, int voterId,
                                                              const std::vector<int>& candidates) {
    return promptOptional("【竞选】" + nameOf(state, voterId) + " 投票选警长", state, candidates);
}

SheriffBallot ConsoleDecisionProvider::chooseSheriffExileBallot(const GameState& state,
                                                                int sheriffId,
                                                                const std::vector<int>& candidates) {
    out_ << "【白天】警长 " << nameOf(state, sheriffId) << " 归票。\n";
    if (promptYesNo("  归单人（警徽 1.5 票，必须投票）？（n = 归多人PK，警徽 1 票，可弃票）")) {
        int target = promptRequired("  归单人 投给谁", state, candidates);
        return SheriffBallot{true, target};
    }
    std::optional<int> target = promptOptional("  归多人PK 投给谁", state, candidates);
    return SheriffBallot{false, target};
}

std::optional<int> ConsoleDecisionProvider::chooseBadgeTransfer(const GameState& state,
                                                                int sheriffId,
                                                                const std::vector<int>& candidates) {
    out_ << "【死亡结算】警长 " << nameOf(state, sheriffId) << " 出局。\n";
    return promptOptional("  警徽移交给谁（直接回车=撕毁警徽）", state, candidates);
}

SpeechDirection ConsoleDecisionProvider::chooseSpeechDirection(const GameState& state,
                                                               int sheriffId, int anchorSeat,
                                                               bool singleDeath) {
    out_ << "【白天】警长 " << nameOf(state, sheriffId) << " 决定发言顺序。\n";
    out_ << "  从" << (singleDeath ? "死者" : "警长") << "座位 " << anchorSeat
         << " 开始；方向 L=座位号增大，R=座位号减小\n";
    return promptYesNo("  向左（座位号增大）？（n = 向右）") ? SpeechDirection::Left
                                                            : SpeechDirection::Right;
}

void ConsoleDecisionProvider::onInspectResult(int seerId, int targetId, bool isWolf) {
    out_ << "【私密→预言家 #" << seerId << "】#" << targetId << " 是 "
         << (isWolf ? "狼人（查杀）" : "好人（金水）") << "\n";
}

void ConsoleDecisionProvider::notify(const std::string& message) {
    out_ << message << "\n";
}

void ConsoleDecisionProvider::pause(const std::string& note) {
    out_ << note << "（按回车继续）";
    readLine();  // block until Enter (or EOF)
    out_ << "\n";
}

}  // namespace ww
