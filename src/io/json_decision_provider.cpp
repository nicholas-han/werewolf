#include "io/json_decision_provider.h"

#include <algorithm>
#include <map>
#include <string>

#include "core/game_state.h"
#include "core/messages.h"
#include "core/player.h"

namespace ww {

namespace {

const char* askKindName(AskKind k) {
    switch (k) {
        case AskKind::NightKill: return "NightKill";
        case AskKind::SelfDestruct: return "SelfDestruct";
        case AskKind::Vote: return "Vote";
        case AskKind::RunoffVote: return "RunoffVote";
        case AskKind::Inspect: return "Inspect";
        case AskKind::Guard: return "Guard";
        case AskKind::WitchSave: return "WitchSave";
        case AskKind::WitchPoison: return "WitchPoison";
        case AskKind::HunterShot: return "HunterShot";
        case AskKind::MechanicLearn: return "MechanicLearn";
        case AskKind::MechanicBigKnife: return "MechanicBigKnife";
        case AskKind::RunForSheriff: return "RunForSheriff";
        case AskKind::Withdraw: return "Withdraw";
        case AskKind::SheriffVote: return "SheriffVote";
        case AskKind::ConsolidateSingle: return "ConsolidateSingle";
        case AskKind::BallotTarget: return "BallotTarget";
        case AskKind::BadgeTransfer: return "BadgeTransfer";
        case AskKind::SpeechDirection: return "SpeechDirection";
    }
    return "Unknown";
}

const char* speechKindName(SpeechKind k) {
    switch (k) {
        case SpeechKind::Statement: return "Statement";
        case SpeechKind::LastWords: return "LastWords";
        case SpeechKind::WolfChat: return "WolfChat";
    }
    return "Statement";
}

bool contains(const std::vector<int>& v, int x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

}  // namespace

JsonDecisionProvider::JsonDecisionProvider(std::istream& in, std::ostream& out,
                                           std::string boardName, unsigned seed)
    : in_(in), out_(out), sink_(out), boardName_(std::move(boardName)), seed_(seed) {}

void JsonDecisionProvider::writeLine(const std::string& s) {
    out_ << s << "\n";
    out_.flush();
}

void JsonDecisionProvider::sync(const GameState& s) {
    curDay_ = s.day;
    curPhase_ = (s.phase == Phase::Night) ? "Night" : "Day";
}

std::string JsonDecisionProvider::nameOf(const GameState& s, int id) const {
    const Player* p = s.find(id);
    return p ? p->name() : ("#" + std::to_string(id));
}

std::string JsonDecisionProvider::candidatesArray(const GameState& s,
                                                  const std::vector<int>& ids) const {
    std::string out = "[";
    bool first = true;
    for (int id : ids) {
        if (!first) out += ",";
        first = false;
        jsonu::Obj o;
        o.num("seat", id).str("name", nameOf(s, id));
        out += o.dump();
    }
    out += "]";
    return out;
}

int JsonDecisionProvider::wolfRepresentative(const GameState& state) const {
    int fallback = -1;  // lowest-seat alive wolf of any kind (covers the lone mechanic)
    for (const Player& p : state.players) {  // players are in seat order
        if (!p.isAlive() || p.faction() != Faction::Wolf) continue;
        if (fallback == -1) fallback = p.id();
        if (p.role().kind() != RoleKind::MechanicWolf) return p.id();  // prefer an open wolf
    }
    return fallback;
}

jsonu::Value JsonDecisionProvider::readReply() {
    std::string line;
    while (std::getline(in_, line)) {
        if (line.empty()) continue;
        std::optional<jsonu::Value> v = jsonu::parse(line);
        if (!v) continue;
        const jsonu::Value* t = v->get("t");
        if (t && t->isStr() && t->s == "reply") return *v;
        // ignore any other line type (the orchestrator only sends replies)
    }
    return jsonu::Value{};  // Null = EOF / no reply
}

void JsonDecisionProvider::emitNarration(Vis vis, std::optional<int> seat, const std::string& text) {
    Event e;
    e.vis = vis;
    e.seat = seat;
    e.etype = (vis == Vis::Moderator) ? "status" : "narration";
    e.text = text;
    e.day = curDay_;
    e.phase = curPhase_;
    sink_.emit(e);
}

void JsonDecisionProvider::emitDecision(int seat, const char* kind, std::optional<int> target) {
    jsonu::Obj d;
    d.num("seat", seat).str("kind", kind);
    if (target) d.num("target", *target);
    else d.null("target");
    Event e;
    e.vis = Vis::Moderator;
    e.etype = "decision";
    e.dataJson = d.dump();
    e.day = curDay_;
    e.phase = curPhase_;
    e.text = std::string("【上帝】#") + std::to_string(seat) + " " + kind +
             (target ? (" -> #" + std::to_string(*target)) : "（无/跳过）");
    sink_.emit(e);
}

std::optional<int> JsonDecisionProvider::askChoose(const GameState& s, int seat, AskKind kind,
                                                   const std::string& prompt,
                                                   const std::vector<int>& candidates,
                                                   bool allowSkip, bool logMod) {
    sync(s);
    // Vote-like kinds must always cast a real ballot so the game keeps progressing
    // (mirrors BotChannel) even on a dead orchestrator; other skippable kinds abstain.
    const bool voteLike = (kind == AskKind::Vote || kind == AskKind::RunoffVote ||
                           kind == AskKind::SheriffVote || kind == AskKind::BallotTarget);
    auto firstNonSelf = [&]() -> std::optional<int> {
        for (int id : candidates) {
            if (id != seat) return id;
        }
        return candidates.front();
    };
    auto fallback = [&]() -> std::optional<int> {
        if (candidates.empty()) return std::nullopt;
        if (voteLike) return firstNonSelf();
        return allowSkip ? std::nullopt : firstNonSelf();
    };
    if (candidates.empty()) return std::nullopt;

    const int id = nextId_++;
    jsonu::Obj a;
    a.str("t", "ask").num("id", id).num("seat", seat).str("qtype", "choose")
        .str("kind", askKindName(kind)).str("prompt", prompt)
        .num("day", curDay_).str("phase", curPhase_)
        .raw("candidates", candidatesArray(s, candidates)).boolean("allowSkip", allowSkip);
    writeLine(a.dump());

    jsonu::Value r = readReply();
    std::optional<int> result;
    const jsonu::Value* c = r.get("choice");
    if (r.isNull() || !c) {
        result = fallback();  // EOF / missing field -> fallback (vote-like still votes)
    } else if (c->isNull()) {
        result = allowSkip ? std::nullopt : fallback();  // explicit abstain, honored when allowed
    } else if (c->isInt() && contains(candidates, static_cast<int>(c->i))) {
        result = static_cast<int>(c->i);
    } else {
        result = fallback();  // illegal value -> fallback (not a silent abstain)
    }
    if (logMod) emitDecision(seat, askKindName(kind), result);
    return result;
}

bool JsonDecisionProvider::askConfirm(const GameState& s, int seat, AskKind kind,
                                      const std::string& prompt, bool logMod) {
    sync(s);
    const int id = nextId_++;
    jsonu::Obj a;
    a.str("t", "ask").num("id", id).num("seat", seat).str("qtype", "confirm")
        .str("kind", askKindName(kind)).str("prompt", prompt)
        .num("day", curDay_).str("phase", curPhase_);
    writeLine(a.dump());

    jsonu::Value r = readReply();
    const jsonu::Value* d = r.get("decision");
    const bool decision = (d && d->isBool()) ? d->b : false;  // default false (conservative)
    if (logMod) {
        jsonu::Obj dd;
        dd.num("seat", seat).str("kind", askKindName(kind)).boolean("value", decision);
        Event e;
        e.vis = Vis::Moderator;
        e.etype = "decision";
        e.dataJson = dd.dump();
        e.day = curDay_;
        e.phase = curPhase_;
        e.text = std::string("【上帝】#") + std::to_string(seat) + " " + askKindName(kind) + "=" +
                 (decision ? "是" : "否");
        sink_.emit(e);
    }
    return decision;
}

std::string JsonDecisionProvider::askSpeak(const GameState& s, int seat, SpeechKind sk,
                                           const std::string& prompt) {
    sync(s);
    const int id = nextId_++;
    jsonu::Obj a;
    a.str("t", "ask").num("id", id).num("seat", seat).str("qtype", "speak")
        .str("kind", speechKindName(sk)).str("prompt", prompt)
        .num("day", curDay_).str("phase", curPhase_);
    writeLine(a.dump());

    jsonu::Value r = readReply();
    const jsonu::Value* tv = r.get("text");
    return (tv && tv->isStr()) ? tv->s : std::string();
}

// --- driver hooks ---

void JsonDecisionProvider::emitGameStart(const GameState& state) {
    std::string seats = "[";
    bool first = true;
    for (const Player& p : state.players) {
        if (!first) seats += ",";
        first = false;
        jsonu::Obj o;
        o.num("seat", p.seat()).str("name", p.name());
        seats += o.dump();
    }
    seats += "]";
    jsonu::Obj g;
    g.str("t", "game_start").str("protocol", "1.0").str("board", boardName_)
        .num("seed", static_cast<long long>(seed_)).raw("seats", seats);
    writeLine(g.dump());
}

void JsonDecisionProvider::emitDeals(const GameState& state) {
    std::vector<int> openWolves;
    for (const Player& p : state.players) {
        if (p.faction() == Faction::Wolf && p.role().kind() != RoleKind::MechanicWolf) {
            openWolves.push_back(p.seat());
        }
    }
    for (const Player& p : state.players) {
        const RoleKind rk = p.role().kind();
        jsonu::Obj d;
        d.str("role", std::string(to_string(rk)))
            .str("faction", std::string(to_string(p.faction())))
            .str("subKind", std::string(to_string(p.subKind())));

        std::string text = "你的身份是：" + txt::role(rk);
        if (rk == RoleKind::MechanicWolf) {
            text += "（你是机械狼，不与狼队见面；其他狼出局后可独立行动）";
        } else if (p.faction() == Faction::Wolf) {
            std::string mates = "[";
            bool first = true;
            text += "；你的狼队友：";
            for (int seat : openWolves) {
                if (seat == p.seat()) continue;
                if (!first) mates += ",";
                first = false;
                mates += std::to_string(seat);
                text += "P" + std::to_string(seat) + " ";
            }
            mates += "]";
            d.raw("teammates", mates);
        }
        Event e;
        e.vis = Vis::Private;
        e.seat = p.id();
        e.etype = "deal";
        e.text = text;
        e.dataJson = d.dump();
        e.day = 1;
        e.phase = "Night";
        sink_.emit(e);
    }
}

void JsonDecisionProvider::emitGameOver(GameResult result) {
    jsonu::Obj g;
    g.str("t", "game_over").str("result", std::string(to_string(result)));
    writeLine(g.dump());
}

// --- team decisions ---

std::optional<int> JsonDecisionProvider::chooseNightKill(const GameState& state,
                                                         const std::vector<int>& candidates) {
    sync(state);
    // Living "open" wolves vote; the mechanic acts alone (not via the team vote).
    std::vector<int> wolves;
    for (const Player& p : state.players) {  // seat order
        if (p.isAlive() && p.faction() == Faction::Wolf &&
            p.role().kind() != RoleKind::MechanicWolf) {
            wolves.push_back(p.id());
        }
    }

    std::optional<int> target;
    if (wolves.empty()) {
        // Lone wolf (e.g. the mechanic once its pack is gone): a single pick.
        const int rep = wolfRepresentative(state);
        if (rep != -1) {
            target = askChoose(state, rep, AskKind::NightKill, "请选择今晚的刀杀目标",
                               candidates, true, false);
        }
    } else {
        // Secret ballot (BRD §2 AI 狼刀决议): each open wolf privately votes a target
        // (弃票 = 倾向空刀); the strict-max target is killed; a tie or no votes = 空刀.
        std::map<int, int> tally;
        for (int w : wolves) {
            std::optional<int> v = askChoose(state, w, AskKind::NightKill,
                                             "狼队投票：今晚刀谁？（可弃票=倾向空刀）", candidates,
                                             true, false);
            if (v) tally[*v]++;
            emitDecision(w, "NightKillVote", v);  // god-view: each wolf's secret ballot
        }
        int best = -1, bestCount = 0, leaders = 0;
        for (const auto& [t, c] : tally) {
            if (c > bestCount) { best = t; bestCount = c; leaders = 1; }
            else if (c == bestCount) { ++leaders; }
        }
        target = (bestCount > 0 && leaders == 1) ? std::optional<int>(best) : std::nullopt;
    }

    // God-view: the team's resolved knife; then privately tell every open wolf.
    const int recorder = wolves.empty() ? wolfRepresentative(state) : wolves.front();
    if (recorder != -1) emitDecision(recorder, "NightKill", target);
    const std::string note = "【狼队】今晚刀：" + (target ? nameOf(state, *target) : std::string("空刀"));
    for (int w : wolves) emitNarration(Vis::Private, w, note);
    return target;
}

std::optional<int> JsonDecisionProvider::chooseSelfDestruct(const GameState& state,
                                                            const std::vector<int>& wolfIds) {
    for (int w : wolfIds) {
        if (askConfirm(state, w, AskKind::SelfDestruct, nameOf(state, w) + " 是否自爆？", true)) {
            return w;
        }
    }
    return std::nullopt;
}

// --- per-player choices ---

std::optional<int> JsonDecisionProvider::chooseVote(const GameState& s, int v,
                                                    const std::vector<int>& c) {
    return askChoose(s, v, AskKind::Vote, "请投票放逐", c, true, false);
}

std::optional<int> JsonDecisionProvider::chooseInspect(const GameState& s, int id,
                                                       const std::vector<int>& c) {
    return askChoose(s, id, AskKind::Inspect, "请查验一名玩家", c, true, true);
}

std::optional<int> JsonDecisionProvider::chooseGuard(const GameState& s, int id,
                                                     const std::vector<int>& c) {
    return askChoose(s, id, AskKind::Guard, "请选择守护目标（可空守）", c, true, true);
}

std::optional<int> JsonDecisionProvider::chooseMechanicLearn(const GameState& s, int id,
                                                             const std::vector<int>& c) {
    return askChoose(s, id, AskKind::MechanicLearn, "是否学习一名玩家的身份（全局一次，可不学）", c,
                     true, true);
}

std::optional<int> JsonDecisionProvider::chooseMechanicBigKnife(const GameState& s, int id,
                                                                const std::vector<int>& c) {
    return askChoose(s, id, AskKind::MechanicBigKnife, "是否发动破盾大刀（一次性，无视守卫；可留待后用）",
                     c, true, true);
}

std::optional<int> JsonDecisionProvider::chooseWitchPoison(const GameState& s, int id,
                                                           const std::vector<int>& c) {
    return askChoose(s, id, AskKind::WitchPoison, "是否使用毒药（毒谁）", c, true, true);
}

std::optional<int> JsonDecisionProvider::chooseHunterShot(const GameState& s, int id,
                                                          const std::vector<int>& c) {
    return askChoose(s, id, AskKind::HunterShot, "是否开枪带走一名玩家", c, true, true);
}

std::optional<int> JsonDecisionProvider::chooseSheriffVote(const GameState& s, int id,
                                                           const std::vector<int>& c) {
    return askChoose(s, id, AskKind::SheriffVote, "投票选警长", c, true, false);
}

std::optional<int> JsonDecisionProvider::chooseBadgeTransfer(const GameState& s, int id,
                                                             const std::vector<int>& c) {
    return askChoose(s, id, AskKind::BadgeTransfer, "警徽移交给谁（不选=撕毁警徽）", c, true, true);
}

// --- per-player confirms ---

bool JsonDecisionProvider::chooseWitchSave(const GameState& s, int witchId, int knifedId) {
    return askConfirm(s, witchId, AskKind::WitchSave,
                      "今晚 " + nameOf(s, knifedId) + " 被刀，是否使用解药？", true);
}

bool JsonDecisionProvider::chooseRunForSheriff(const GameState& s, int id) {
    return askConfirm(s, id, AskKind::RunForSheriff, "是否上警竞选警长？", false);
}

bool JsonDecisionProvider::chooseWithdraw(const GameState& s, int id) {
    return askConfirm(s, id, AskKind::Withdraw, "是否退水退出竞选？", false);
}

// --- composite ---

SheriffBallot JsonDecisionProvider::chooseSheriffExileBallot(const GameState& s, int sheriffId,
                                                             const std::vector<int>& c) {
    if (askConfirm(s, sheriffId, AskKind::ConsolidateSingle,
                   "归单人（警徽 1.5 票，必须投票）？（否=归多人PK，警徽 1 票，可弃票）", false)) {
        std::optional<int> t =
            askChoose(s, sheriffId, AskKind::BallotTarget, "归单人 投给谁", c, false, false);
        if (!t && !c.empty()) t = c.front();
        return SheriffBallot{true, t};
    }
    std::optional<int> t =
        askChoose(s, sheriffId, AskKind::BallotTarget, "归多人PK 投给谁（可弃票）", c, true, false);
    return SheriffBallot{false, t};
}

SpeechDirection JsonDecisionProvider::chooseSpeechDirection(const GameState& s, int sheriffId,
                                                            int anchorSeat, bool singleDeath) {
    const std::string from = singleDeath ? "死者座位 " : "警长座位 ";
    const bool left =
        askConfirm(s, sheriffId, AskKind::SpeechDirection,
                   "发言从" + from + std::to_string(anchorSeat) + " 起，向左（座位号增大）？（否=向右）",
                   false);
    return left ? SpeechDirection::Left : SpeechDirection::Right;
}

std::string JsonDecisionProvider::collectSpeech(const GameState& s, int speakerId, SpeechKind kind,
                                                int /*day*/) {
    std::string text = askSpeak(s, speakerId, kind, "请发言");
    if (!text.empty()) {
        jsonu::Obj d;
        d.num("speaker", speakerId).str("kind", speechKindName(kind));
        Event e;
        e.vis = Vis::Public;
        e.etype = "speech";
        e.dataJson = d.dump();
        e.day = curDay_;
        e.phase = curPhase_;
        e.text = nameOf(s, speakerId) + "：" + text;
        sink_.emit(e);
    }
    return text;
}

std::string JsonDecisionProvider::collectWolfChat(const GameState& s, int speakerId,
                                                  const std::vector<int>& openWolfIds) {
    sync(s);
    std::string text = askSpeak(s, speakerId, SpeechKind::WolfChat, "狼队私聊，请发言");
    if (!text.empty()) {
        jsonu::Obj d;
        d.num("speaker", speakerId).str("kind", "WolfChat");
        const std::string disp = nameOf(s, speakerId) + "（狼队）：" + text;
        // Private to each open wolf (incl. the speaker, so every wolf's view holds it);
        // never public. The orchestrator can dedupe identical lines for the god-script.
        for (int w : openWolfIds) {
            Event e;
            e.vis = Vis::Private;
            e.seat = w;
            e.etype = "speech";
            e.dataJson = d.dump();
            e.day = curDay_;
            e.phase = curPhase_;
            e.text = disp;
            sink_.emit(e);
        }
    }
    return text;
}

// --- directed private results (§11) ---

void JsonDecisionProvider::onInspectResult(int seerId, int targetId, bool isWolf) {
    jsonu::Obj d;
    d.str("kind", "Inspect").num("target", targetId).boolean("isWolf", isWolf);
    Event e;
    e.vis = Vis::Private;
    e.seat = seerId;
    e.etype = "result_private";
    e.dataJson = d.dump();
    e.day = curDay_;
    e.phase = curPhase_;
    e.text = "【查验结果】#" + std::to_string(targetId) + " 是 " +
             (isWolf ? "狼人（查杀）" : "好人（金水）");
    sink_.emit(e);
}

void JsonDecisionProvider::onPsychicResult(int psychicId, int targetId, RoleKind shownRole) {
    jsonu::Obj d;
    d.str("kind", "Psychic").num("target", targetId).str("shownRole", std::string(to_string(shownRole)));
    Event e;
    e.vis = Vis::Private;
    e.seat = psychicId;
    e.etype = "result_private";
    e.dataJson = d.dump();
    e.day = curDay_;
    e.phase = curPhase_;
    e.text = "【通灵结果】#" + std::to_string(targetId) + " 的身份是 " + txt::role(shownRole);
    sink_.emit(e);
}

void JsonDecisionProvider::onHunterGunCheck(int hunterId, bool canShoot) {
    jsonu::Obj d;
    d.str("kind", "GunCheck").boolean("canShoot", canShoot);
    Event e;
    e.vis = Vis::Private;
    e.seat = hunterId;
    e.etype = "result_private";
    e.dataJson = d.dump();
    e.day = curDay_;
    e.phase = curPhase_;
    e.text = std::string("【验枪】当前") + (canShoot ? "可开枪" : "不可开枪（带毒）");
    sink_.emit(e);
}

void JsonDecisionProvider::onMechanicLearnResult(int mechanicId, int targetId, RoleKind learnedRole) {
    jsonu::Obj d;
    d.str("kind", "Learn").num("target", targetId).str("learnedRole", std::string(to_string(learnedRole)));
    Event e;
    e.vis = Vis::Private;
    e.seat = mechanicId;
    e.etype = "result_private";
    e.dataJson = d.dump();
    e.day = curDay_;
    e.phase = curPhase_;
    e.text = "【学习结果】你学习了 #" + std::to_string(targetId) + "，学到的身份是 " + txt::role(learnedRole);
    sink_.emit(e);
}

// --- output ---

void JsonDecisionProvider::notify(const std::string& message) {
    emitNarration(Vis::Public, std::nullopt, message);
}

void JsonDecisionProvider::notifyPlayer(int playerId, const std::string& message) {
    emitNarration(Vis::Private, playerId, message);
}

void JsonDecisionProvider::notifyModerator(const std::string& message) {
    emitNarration(Vis::Moderator, std::nullopt, message);
}

void JsonDecisionProvider::pause(const std::string& note) {
    if (!note.empty()) emitNarration(Vis::Moderator, std::nullopt, note);
}

}  // namespace ww
