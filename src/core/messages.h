#pragma once

#include <string>

#include "core/enums.h"

// Centralised moderator-facing text (BRD M5). Single source of truth for the
// Chinese judge narration so wording lives in one place (and tests key on the
// same functions instead of brittle literals). Decision prompts that are purely
// console-local live in ConsoleDecisionProvider; everything the engine narrates
// through notify() comes from here.
namespace ww::txt {

inline std::string role(RoleKind r) {
    switch (r) {
        case RoleKind::Werewolf: return "狼人";
        case RoleKind::Seer: return "预言家";
        case RoleKind::Witch: return "女巫";
        case RoleKind::Hunter: return "猎人";
        case RoleKind::Civilian: return "平民";
        case RoleKind::Guardian: return "守卫";
        case RoleKind::WolfGun: return "狼枪";
        case RoleKind::Psychic: return "通灵师";
        case RoleKind::MechanicWolf: return "机械狼";
    }
    return "?";
}

inline std::string cause(DeathCause c) {
    switch (c) {
        case DeathCause::Killed: return "狼杀";
        case DeathCause::Poisoned: return "毒杀";
        case DeathCause::Exiled: return "放逐";
        case DeathCause::Shot: return "枪杀";
        case DeathCause::BlownUp: return "自爆";
    }
    return "?";
}

// --- Phase banners & night narration ---
inline std::string nightBanner(int day) {
    return "=== 第 " + std::to_string(day) + " 夜 ===  天黑请闭眼";
}
inline std::string dayBanner(int day) {
    return "=== 第 " + std::to_string(day) + " 天 ===  天亮了";
}
inline std::string openEyes(const std::string& roleName) { return roleName + "请睁眼"; }
inline std::string closeEyes(const std::string& roleName) { return roleName + "请闭眼"; }

// --- Death announcement & last words ---
inline std::string announceHeader() { return "【公布昨夜死讯】"; }
inline std::string peacefulNight() { return "【公布昨夜死讯】平安夜，无人死亡"; }
// Said before naming multiple night deaths so the announce order leaks nothing
// (§5.2): deaths are simultaneous and then listed by seat number ascending.
inline std::string deathOrderDisclaimer() { return "（死亡顺序不分先后）"; }
inline std::string out(const std::string& name, const std::string& causes) {
    return name + " 出局（" + causes + "）";
}
// Death announced WITHOUT a cause (§11): a night death is just「出局」so nobody can
// tell knife from poison; only public-event causes (放逐/自爆/枪杀) are ever named.
inline std::string outNoCause(const std::string& name) { return name + " 出局"; }
inline std::string lastWordsCue(const std::string& name) { return "  → " + name + " 可发表遗言"; }
inline std::string lastWordsPause(const std::string& name) { return "请 " + name + " 发表遗言"; }
inline std::string announcePause() { return "准备公布昨夜情况"; }

// --- Day / speech / vote ---
inline std::string speechPhase() { return "【发言阶段】请依次发言"; }
inline std::string speakingOrder(const std::string& names) { return "发言顺序：" + names; }
inline std::string voteTransition() { return "【发言结束，进入放逐投票】"; }
inline std::string voteHeader() { return "【放逐投票】"; }
inline std::string firstRoundVotes(const std::string& body) { return "  首轮票数：" + body; }
inline std::string runoffVotes(const std::string& body) { return "  决胜轮票数：" + body; }
// Individual ballots revealed together AFTER collection (§6): "P1→P5、P3 弃票、…".
inline std::string voteBallots(const std::string& body) { return "  投票详情：" + body; }
inline std::string exiled(const std::string& name) {
    return "  放逐结果：" + name + " 票数最高，被放逐";
}
inline std::string exiledRunoff(const std::string& name) { return "  放逐结果：" + name + " 被放逐"; }
inline std::string firstRoundTie(const std::string& names) {
    return "  首轮平票（" + names + "）→ 进入决胜轮";
}
inline std::string noVotesNoExile() { return "  无人得票 → 本轮无人出局"; }
inline std::string runoffStillTie() { return "  决胜轮仍平票 → 本轮无人出局"; }

// --- Sheriff election (§7) ---
inline std::string electionBegin() { return "【警长竞选】开始"; }
inline std::string electionDeferred() { return "【警长竞选】顺延，仅投票（无发言）"; }
inline std::string sheriffCandidates(const std::string& names) { return "上警候选人：" + names; }
// Announced after blind registration (§7.2): who stood for sheriff (like the vote reveal).
inline std::string sheriffRunners(const std::string& names) { return "  上警竞选警长：" + names; }
inline std::string sheriffVoteHeader() { return "【警长投票】（仅未上警玩家投票）"; }
inline std::string sheriffVotes(const std::string& body) { return "  警长票数：" + body; }
inline std::string sheriffRunoffTie(const std::string& names) {
    return "  警长竞选平票（" + names + "）→ 进入决胜轮";
}
inline std::string sheriffRunoffVotes(const std::string& body) {
    return "  警长决胜轮票数：" + body;
}
inline std::string autoSheriff(const std::string& name) {
    return "仅 " + name + " 一人竞选，自动当选";
}
inline std::string becomesSheriff(const std::string& name) { return name + " 当选警长"; }
inline std::string noSheriffNobodyRan() { return "无人上警，本局无警长"; }
inline std::string badgeLostEveryoneRan() { return "全员上警，警徽流失，本局无警长"; }
inline std::string noSheriffAllWithdrew() { return "全员退水，本局无警长"; }
inline std::string badgeLostTie() { return "竞选平票，警徽流失，本局无警长"; }
inline std::string badgeTransferred(const std::string& name) { return "警徽移交给 " + name; }
inline std::string badgeDestroyed() { return "警徽被撕毁，本局不再有警长"; }

// --- Result ---
inline std::string resultTown() { return "游戏结束：好人胜利"; }
inline std::string resultWolf() { return "游戏结束：狼人胜利"; }

// --- Speech log / replay (BRD §4 发言记录) ---
inline std::string speechKind(SpeechKind k) {
    return k == SpeechKind::LastWords ? "遗言" : "发言";
}
inline std::string transcriptHeader() { return "=== 复盘：发言记录 ==="; }
inline std::string transcriptEmpty() { return "（本局无发言记录）"; }
inline std::string transcriptDay(int day) {
    return "── 第 " + std::to_string(day) + " 天 ──";
}
inline std::string transcriptLine(int seat, const std::string& name, SpeechKind kind,
                                  const std::string& text) {
    return "  [" + std::to_string(seat) + "] " + name + "（" + speechKind(kind) + "）：" + text;
}

}  // namespace ww::txt
