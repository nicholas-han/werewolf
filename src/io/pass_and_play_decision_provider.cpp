#include "io/pass_and_play_decision_provider.h"

#include <istream>
#include <ostream>
#include <string>

namespace ww {

PassAndPlayDecisionProvider::PassAndPlayDecisionProvider(std::istream& in, std::ostream& out)
    : ConsoleDecisionProvider(in, out), in_(in), out_(out) {}

void PassAndPlayDecisionProvider::clearScreen() {
    out_ << "\033[2J\033[3J\033[H";  // clear screen + scrollback + cursor home
}

void PassAndPlayDecisionProvider::waitEnter() {
    std::string line;
    std::getline(in_, line);
}

void PassAndPlayDecisionProvider::ensureTurn(const std::string& who, int key) {
    if (turnActive_ && turnKey_ == key) return;  // same player's turn continues
    endTurn();                                    // close any other open turn
    out_ << "\n======================================\n";
    out_ << "请将设备交给【" << who << "】，其他人请勿偷看。\n准备好后按回车。";
    waitEnter();
    clearScreen();
    turnActive_ = true;
    turnKey_ = key;
}

void PassAndPlayDecisionProvider::endTurn() {
    if (!turnActive_) return;
    out_ << "\n（看完请按回车，交回设备）";
    waitEnter();
    clearScreen();
    turnActive_ = false;
}

void PassAndPlayDecisionProvider::privateAnnounce(int playerId, const std::string& message) {
    ensureTurn("玩家 P" + std::to_string(playerId), playerId);
    out_ << message << "\n";
}

// --- private decisions ---

std::optional<int> PassAndPlayDecisionProvider::chooseNightKill(const GameState& s,
                                                               const std::vector<int>& c) {
    ensureTurn("狼队", kWolfTeam);
    return ConsoleDecisionProvider::chooseNightKill(s, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseVote(const GameState& s, int voter,
                                                          const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(voter), voter);
    return ConsoleDecisionProvider::chooseVote(s, voter, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseInspect(const GameState& s, int id,
                                                             const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(id), id);
    return ConsoleDecisionProvider::chooseInspect(s, id, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseGuard(const GameState& s, int id,
                                                           const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(id), id);
    return ConsoleDecisionProvider::chooseGuard(s, id, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseMechanicLearn(const GameState& s, int id,
                                                                   const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(id), id);
    return ConsoleDecisionProvider::chooseMechanicLearn(s, id, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseMechanicBigKnife(const GameState& s, int id,
                                                                      const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(id), id);
    return ConsoleDecisionProvider::chooseMechanicBigKnife(s, id, c);
}

bool PassAndPlayDecisionProvider::chooseWitchSave(const GameState& s, int witch, int knifed) {
    ensureTurn("玩家 P" + std::to_string(witch), witch);
    return ConsoleDecisionProvider::chooseWitchSave(s, witch, knifed);
}

std::optional<int> PassAndPlayDecisionProvider::chooseWitchPoison(const GameState& s, int witch,
                                                                 const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(witch), witch);
    return ConsoleDecisionProvider::chooseWitchPoison(s, witch, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseHunterShot(const GameState& s, int shooter,
                                                                const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(shooter), shooter);
    return ConsoleDecisionProvider::chooseHunterShot(s, shooter, c);
}

bool PassAndPlayDecisionProvider::chooseRunForSheriff(const GameState& s, int id) {
    ensureTurn("玩家 P" + std::to_string(id), id);
    return ConsoleDecisionProvider::chooseRunForSheriff(s, id);
}

bool PassAndPlayDecisionProvider::chooseWithdraw(const GameState& s, int id) {
    ensureTurn("玩家 P" + std::to_string(id), id);
    return ConsoleDecisionProvider::chooseWithdraw(s, id);
}

std::optional<int> PassAndPlayDecisionProvider::chooseSheriffVote(const GameState& s, int voter,
                                                                 const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(voter), voter);
    return ConsoleDecisionProvider::chooseSheriffVote(s, voter, c);
}

SheriffBallot PassAndPlayDecisionProvider::chooseSheriffExileBallot(const GameState& s, int sheriff,
                                                                   const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(sheriff), sheriff);
    return ConsoleDecisionProvider::chooseSheriffExileBallot(s, sheriff, c);
}

std::optional<int> PassAndPlayDecisionProvider::chooseBadgeTransfer(const GameState& s, int sheriff,
                                                                   const std::vector<int>& c) {
    ensureTurn("玩家 P" + std::to_string(sheriff), sheriff);
    return ConsoleDecisionProvider::chooseBadgeTransfer(s, sheriff, c);
}

SpeechDirection PassAndPlayDecisionProvider::chooseSpeechDirection(const GameState& s, int sheriff,
                                                                  int anchorSeat, bool singleDeath) {
    ensureTurn("玩家 P" + std::to_string(sheriff), sheriff);
    return ConsoleDecisionProvider::chooseSpeechDirection(s, sheriff, anchorSeat, singleDeath);
}

// --- private notices (shown during the actor's open turn) ---

void PassAndPlayDecisionProvider::onInspectResult(int seerId, int targetId, bool isWolf) {
    ensureTurn("玩家 P" + std::to_string(seerId), seerId);
    ConsoleDecisionProvider::onInspectResult(seerId, targetId, isWolf);
}

void PassAndPlayDecisionProvider::onPsychicResult(int psychicId, int targetId, RoleKind shownRole) {
    ensureTurn("玩家 P" + std::to_string(psychicId), psychicId);
    ConsoleDecisionProvider::onPsychicResult(psychicId, targetId, shownRole);
}

void PassAndPlayDecisionProvider::onHunterGunCheck(int hunterId, bool canShoot) {
    ensureTurn("玩家 P" + std::to_string(hunterId), hunterId);
    ConsoleDecisionProvider::onHunterGunCheck(hunterId, canShoot);
}

void PassAndPlayDecisionProvider::onMechanicLearnResult(int mechanicId, int targetId,
                                                        RoleKind learnedRole) {
    ensureTurn("玩家 P" + std::to_string(mechanicId), mechanicId);
    ConsoleDecisionProvider::onMechanicLearnResult(mechanicId, targetId, learnedRole);
}

// --- public output ---

void PassAndPlayDecisionProvider::notify(const std::string& message) {
    endTurn();  // never show public text while a private hand-off is open
    ConsoleDecisionProvider::notify(message);
}

void PassAndPlayDecisionProvider::pause(const std::string& note) {
    endTurn();
    ConsoleDecisionProvider::pause(note);
}

}  // namespace ww
