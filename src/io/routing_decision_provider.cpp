#include "io/routing_decision_provider.h"

#include <ostream>
#include <string>

#include "core/game_state.h"
#include "core/messages.h"
#include "core/player.h"

namespace ww {

namespace {

std::string nameOf(const GameState& s, int id) {
    const Player* p = s.find(id);
    return p ? p->name() : ("#" + std::to_string(id));
}

}  // namespace

RoutingDecisionProvider::RoutingDecisionProvider(std::map<int, PlayerChannel*> channels,
                                                 std::ostream* publicLog)
    : channels_(std::move(channels)), publicLog_(publicLog) {}

PlayerChannel* RoutingDecisionProvider::ch(int id) {
    auto it = channels_.find(id);
    return it == channels_.end() ? nullptr : it->second;
}

int RoutingDecisionProvider::wolfRepresentative(const GameState& state) const {
    int fallback = -1;  // lowest-seat alive wolf of any kind (covers the lone mechanic)
    for (const Player& p : state.players) {  // players are in seat order
        if (!p.isAlive() || p.faction() != Faction::Wolf) continue;
        if (fallback == -1) fallback = p.id();
        if (p.role().kind() != RoleKind::MechanicWolf) return p.id();  // prefer an open wolf
    }
    return fallback;
}

// --- team decisions ---

std::optional<int> RoutingDecisionProvider::chooseNightKill(const GameState& state,
                                                            const std::vector<int>& candidates) {
    const int rep = wolfRepresentative(state);
    PlayerChannel* c = ch(rep);
    std::optional<int> target =
        c ? c->chooseAmong(state, AskKind::NightKill, "狼队请选择今晚的刀杀目标", candidates, true)
          : std::nullopt;

    // Privately tell the other open wolves what the team chose (the mechanic does
    // not meet the pack, so it is never told).
    const std::string note = "【狼队】今晚刀：" + (target ? nameOf(state, *target) : "空刀");
    for (const Player& p : state.players) {
        if (!p.isAlive() || p.faction() != Faction::Wolf) continue;
        if (p.role().kind() == RoleKind::MechanicWolf || p.id() == rep) continue;
        if (PlayerChannel* w = ch(p.id())) w->tell(note);
    }
    return target;
}

std::optional<int> RoutingDecisionProvider::chooseSelfDestruct(const GameState& state,
                                                               const std::vector<int>& wolfIds) {
    // A personal declaration: ask each wolf in turn; the first to confirm blows up.
    for (int w : wolfIds) {
        PlayerChannel* c = ch(w);
        if (c && c->confirm(state, AskKind::SelfDestruct, nameOf(state, w) + " 是否自爆？")) {
            return w;
        }
    }
    return std::nullopt;
}

// --- per-player choices ---

std::optional<int> RoutingDecisionProvider::chooseVote(const GameState& s, int voterId,
                                                       const std::vector<int>& c) {
    PlayerChannel* p = ch(voterId);
    return p ? p->chooseAmong(s, AskKind::Vote, "请投票放逐", c, true) : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseInspect(const GameState& s, int id,
                                                          const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::Inspect, "请查验一名玩家", c, true) : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseGuard(const GameState& s, int id,
                                                        const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::Guard, "请选择守护目标（可空守）", c, true) : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseMechanicLearn(const GameState& s, int id,
                                                               const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::MechanicLearn, "是否学习一名玩家的身份（全局一次，可不学）",
                              c, true)
             : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseMechanicBigKnife(const GameState& s, int id,
                                                                  const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::MechanicBigKnife,
                              "是否发动破盾大刀（一次性，无视守卫；可留待后用）", c, true)
             : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseWitchPoison(const GameState& s, int id,
                                                              const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::WitchPoison, "是否使用毒药（毒谁）", c, true)
             : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseHunterShot(const GameState& s, int id,
                                                             const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::HunterShot, "是否开枪带走一名玩家", c, true)
             : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseSheriffVote(const GameState& s, int id,
                                                              const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::SheriffVote, "投票选警长", c, true) : std::nullopt;
}

std::optional<int> RoutingDecisionProvider::chooseBadgeTransfer(const GameState& s, int id,
                                                               const std::vector<int>& c) {
    PlayerChannel* p = ch(id);
    return p ? p->chooseAmong(s, AskKind::BadgeTransfer, "警徽移交给谁（不选=撕毁警徽）", c, true)
             : std::nullopt;
}

// --- per-player confirms ---

bool RoutingDecisionProvider::chooseWitchSave(const GameState& s, int witchId, int knifedId) {
    PlayerChannel* p = ch(witchId);
    return p && p->confirm(s, AskKind::WitchSave,
                           "今晚 " + nameOf(s, knifedId) + " 被刀，是否使用解药？");
}

bool RoutingDecisionProvider::chooseRunForSheriff(const GameState& s, int id) {
    PlayerChannel* p = ch(id);
    return p && p->confirm(s, AskKind::RunForSheriff, "是否上警竞选警长？");
}

bool RoutingDecisionProvider::chooseWithdraw(const GameState& s, int id) {
    PlayerChannel* p = ch(id);
    return p && p->confirm(s, AskKind::Withdraw, "是否退水退出竞选？");
}

// --- composite ---

SheriffBallot RoutingDecisionProvider::chooseSheriffExileBallot(const GameState& s, int sheriffId,
                                                                const std::vector<int>& c) {
    PlayerChannel* p = ch(sheriffId);
    if (p == nullptr) return {};  // default: 归多人 PK + abstain
    if (p->confirm(s, AskKind::ConsolidateSingle,
                   "归单人（警徽 1.5 票，必须投票）？（否=归多人PK，警徽 1 票，可弃票）")) {
        std::optional<int> t =
            p->chooseAmong(s, AskKind::BallotTarget, "归单人 投给谁", c, /*allowSkip=*/false);
        if (!t && !c.empty()) t = c.front();  // 归单人 must commit
        return SheriffBallot{true, t};
    }
    std::optional<int> t = p->chooseAmong(s, AskKind::BallotTarget, "归多人PK 投给谁（可弃票）", c, true);
    return SheriffBallot{false, t};
}

SpeechDirection RoutingDecisionProvider::chooseSpeechDirection(const GameState& s, int sheriffId,
                                                               int anchorSeat, bool singleDeath) {
    PlayerChannel* p = ch(sheriffId);
    if (p == nullptr) return SpeechDirection::Left;
    const std::string from = singleDeath ? "死者座位 " : "警长座位 ";
    const bool left = p->confirm(s, AskKind::SpeechDirection,
                                 "发言从" + from + std::to_string(anchorSeat) +
                                     " 起，向左（座位号增大）？（否=向右）");
    return left ? SpeechDirection::Left : SpeechDirection::Right;
}

std::string RoutingDecisionProvider::collectSpeech(const GameState& s, int speakerId, SpeechKind kind,
                                                   int day) {
    (void)day;
    PlayerChannel* p = ch(speakerId);
    return p ? p->speak(s, kind, "请发言") : "";
}

// --- directed notices (private to one player, §11) ---

void RoutingDecisionProvider::onInspectResult(int seerId, int targetId, bool isWolf) {
    if (PlayerChannel* p = ch(seerId)) {
        p->tell("【查验结果】#" + std::to_string(targetId) + " 是 " +
                (isWolf ? "狼人（查杀）" : "好人（金水）"));
    }
}

void RoutingDecisionProvider::onPsychicResult(int psychicId, int targetId, RoleKind shownRole) {
    if (PlayerChannel* p = ch(psychicId)) {
        p->tell("【通灵结果】#" + std::to_string(targetId) + " 的身份是 " + txt::role(shownRole));
    }
}

void RoutingDecisionProvider::onHunterGunCheck(int hunterId, bool canShoot) {
    if (PlayerChannel* p = ch(hunterId)) {
        p->tell(std::string("【验枪】当前") + (canShoot ? "可开枪" : "不可开枪（带毒）"));
    }
}

void RoutingDecisionProvider::onMechanicLearnResult(int mechanicId, int targetId,
                                                    RoleKind learnedRole) {
    if (PlayerChannel* p = ch(mechanicId)) {
        p->tell("【学习结果】你学习了 #" + std::to_string(targetId) + "，学到的身份是 " +
                txt::role(learnedRole));
    }
}

// --- output ---

void RoutingDecisionProvider::notify(const std::string& message) {
    for (auto& [id, c] : channels_) {
        (void)id;
        if (c) c->tell(message);
    }
    if (publicLog_) *publicLog_ << message << "\n";
}

void RoutingDecisionProvider::notifyPlayer(int playerId, const std::string& message) {
    if (PlayerChannel* p = ch(playerId)) p->tell(message);  // private: not to the spectator log
}

void RoutingDecisionProvider::notifyModerator(const std::string& message) {
    if (publicLog_) *publicLog_ << message << "\n";  // god-view: spectator only, never to players
}

void RoutingDecisionProvider::pause(const std::string& note) {
    if (publicLog_) *publicLog_ << note << "\n";  // non-blocking: no single operator to wait on
}

}  // namespace ww
