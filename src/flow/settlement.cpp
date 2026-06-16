#include "flow/settlement.h"

#include <algorithm>
#include <map>
#include <string>

#include "core/messages.h"
#include "core/player.h"
#include "core/roles/role.h"
#include "flow/last_words.h"

namespace ww {

Settlement::Settlement(GameState& state, const BoardConfig& config, DecisionProvider& provider)
    : state_(state), config_(config), provider_(provider) {}

void Settlement::announceDeath(const Player& p) {
    // A self-destruct is announced as 自爆 only — any swallowed night cause stays
    // hidden (§2 自爆吞毒: the witch's poison is concealed by the blast).
    if (p.hasDeathCause(DeathCause::BlownUp)) {
        provider_.notify(txt::out(p.name(), txt::cause(DeathCause::BlownUp)));
        return;
    }
    std::string causes;
    for (DeathCause c : p.deathCauses()) {
        if (!causes.empty()) causes += "+";
        causes += txt::cause(c);
    }
    provider_.notify(txt::out(p.name(), causes));
}

void Settlement::collectLastWords(Player& dead) {
    if (!hasLastWords(dead)) return;  // §5.3 eligibility
    provider_.notify(txt::lastWordsCue(dead.name()));
    std::string lw =  // §4 发言记录: capture 遗言 for replay
        provider_.collectSpeech(state_, dead.id(), SpeechKind::LastWords, state_.day);
    state_.recordSpeech(state_.day, SpeechKind::LastWords, dead.id(), dead.seat(), std::move(lw));
    provider_.pause(txt::lastWordsPause(dead.name()));
}

void Settlement::maybeTransferBadge(Player& dead) {
    if (!state_.sheriffId || *state_.sheriffId != dead.id()) return;

    // §7.6: transfer happens at the death; the dead holder loses the badge first.
    dead.isSheriff = false;
    std::vector<int> candidates;
    for (const Player& p : state_.players) {
        if (p.isAlive()) candidates.push_back(p.id());
    }
    std::optional<int> heir = provider_.chooseBadgeTransfer(state_, dead.id(), candidates);

    Player* newHolder = heir ? state_.find(*heir) : nullptr;
    if (newHolder != nullptr && newHolder->isAlive()) {
        state_.sheriffId = newHolder->id();
        newHolder->isSheriff = true;
        provider_.notify(txt::badgeTransferred(newHolder->name()));
    } else {
        state_.sheriffId.reset();  // torn up (撕毁) -> no sheriff for the rest (§7.6)
        provider_.notify(txt::badgeDestroyed());
    }
}

std::vector<Player*> Settlement::record(const std::vector<PendingDeath>& batch) {
    std::vector<Player*> newly;
    for (const PendingDeath& pd : batch) {
        Player* p = state_.find(pd.playerId);
        if (p == nullptr) continue;
        const bool wasAlive = p->isAlive();
        p->recordDeath(pd.cause, state_.day, state_.phase);  // accumulates causes (同刀同毒)
        if (wasAlive) newly.push_back(p);
    }
    return newly;
}

GameResult Settlement::recordBatchSequential(const std::vector<PendingDeath>& batch,
                                             std::vector<Player*>& newlyOut) {
    // Group causes by player, keeping FIRST-appearance order = the night's
    // resolution order (knife pushed before poison). A 同刀同毒 player is one
    // death, positioned at its first (knife) cause.
    std::vector<int> order;
    std::map<int, std::vector<DeathCause>> causes;
    for (const PendingDeath& pd : batch) {
        if (causes.find(pd.playerId) == causes.end()) order.push_back(pd.playerId);
        causes[pd.playerId].push_back(pd.cause);
    }

    GameResult decided = GameResult::Ongoing;
    for (int id : order) {
        Player* p = state_.find(id);
        if (p == nullptr) continue;
        const bool wasAlive = p->isAlive();
        for (DeathCause c : causes[id]) p->recordDeath(c, state_.day, state_.phase);
        if (wasAlive) newlyOut.push_back(p);
        if (decided == GameResult::Ongoing) {  // §4.2: first death to decide wins
            GameResult r = evaluateWin(state_, config_);
            if (r != GameResult::Ongoing) decided = r;
        }
    }
    return decided;
}

void Settlement::announceBatch(const std::vector<Player*>& deaths,
                               const std::vector<int>& lastWordsOrder) {
    // Announce all together in seat order, so the public never sees a settlement
    // order (§5.2 公布顺序).
    std::vector<Player*> batch = deaths;
    std::sort(batch.begin(), batch.end(),
              [](const Player* a, const Player* b) { return a->seat() < b->seat(); });
    for (Player* d : batch) announceDeath(*d);

    // Last words in the requested order (random for first-night multi-death,
    // §5.2/§5.3); empty -> seat order.
    std::vector<Player*> lwOrder;
    if (lastWordsOrder.empty()) {
        lwOrder = batch;
    } else {
        for (int id : lastWordsOrder) {
            if (Player* p = state_.find(id)) lwOrder.push_back(p);
        }
    }
    for (Player* d : lwOrder) collectLastWords(*d);
}

GameResult Settlement::resolveRecorded(std::deque<Player*> worklist,
                                      const std::vector<int>& lastWordsOrder) {
    std::vector<Player*> batch(worklist.begin(), worklist.end());
    announceBatch(batch, lastWordsOrder);  // phases 1-2: announce + last words

    // Badge transfer + win check + death triggers (seat order), chaining further
    // deaths — each chained death announces + takes last words as it occurs.
    std::sort(batch.begin(), batch.end(),
              [](const Player* a, const Player* b) { return a->seat() < b->seat(); });
    std::deque<Player*> q(batch.begin(), batch.end());
    while (!q.empty()) {
        Player* dead = q.front();
        q.pop_front();

        // §4.2: if this death already decided the game, it ends NOW — no badge
        // transfer (a moot badge is never reassigned, §7.6) and no death triggers.
        if (GameResult r = evaluateWin(state_, config_); r != GameResult::Ongoing) {
            return r;
        }

        maybeTransferBadge(*dead);  // §7.6: transfer (game ongoing) before this death's skills

        for (const auto& ability : dead->role().abilities()) {
            auto* trigger = dynamic_cast<DeathTrigger*>(ability.get());
            if (trigger == nullptr) continue;
            std::vector<PendingDeath> triggered;
            trigger->onDeath(state_, *dead, provider_, triggered);
            for (const PendingDeath& td : triggered) {
                Player* t = state_.find(td.playerId);
                if (t == nullptr) continue;
                const bool wasAlive = t->isAlive();
                t->recordDeath(td.cause, state_.day, state_.phase);
                if (wasAlive) {
                    announceDeath(*t);
                    collectLastWords(*t);
                    q.push_back(t);
                }
            }
        }
    }
    return GameResult::Ongoing;
}

GameResult Settlement::apply(std::vector<PendingDeath> batch) {
    std::vector<Player*> newly = record(batch);
    std::deque<Player*> worklist(newly.begin(), newly.end());
    return resolveRecorded(std::move(worklist));
}

}  // namespace ww
