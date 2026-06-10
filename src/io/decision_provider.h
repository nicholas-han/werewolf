#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/game_state.h"

// DecisionProvider decouples the pure-logic engine from I/O (BRD §10): whenever
// the flow needs a player decision it asks through this interface and never
// touches the console directly. Implementations: Scripted (tests), Console
// (humans), Bot (simulation).
//
// M1 exposes only the two decisions the skeleton loop needs (night kill, exile
// vote). As real role abilities arrive (M2+), this can grow into a more generic
// requestAction(Decision, ...) form; kept concrete for now for simplicity.
namespace ww {

// The sheriff's exile ballot (BRD §7.1 归票). 归单人: badge counts 1.5 and the
// sheriff MUST commit to a target. 归多人 PK: badge counts 1.0 and the sheriff
// MAY abstain (target empty).
struct SheriffBallot {
    bool consolidateSingle = false;  // true = 归单人 (1.5, must vote)
    std::optional<int> target;       // required if consolidateSingle
};

// Direction the sheriff opens the speaking order in (BRD §7.1.2). Left = toward
// the next-higher seat (wrapping), Right = toward the next-lower seat.
enum class SpeechDirection { Left, Right };

class DecisionProvider {
public:
    virtual ~DecisionProvider() = default;

    // Night: the wolf team picks a kill target among `candidates`
    // (std::nullopt = 空刀 / no kill). M1 stand-in for the future NightKill ability.
    virtual std::optional<int> chooseNightKill(const GameState& state,
                                               const std::vector<int>& candidates) = 0;

    // Day exile vote: `voterId` picks whom to banish among `candidates`
    // (std::nullopt = abstain, when allowed).
    virtual std::optional<int> chooseVote(const GameState& state, int voterId,
                                          const std::vector<int>& candidates) = 0;

    // --- M2 role-ability decisions (defaulted; impls override what they need) ---

    // Seer inspects a player (std::nullopt = skip). Result is delivered via
    // onInspectResult so only the seer learns it (BRD §2/§11).
    virtual std::optional<int> chooseInspect(const GameState& state, int seerId,
                                             const std::vector<int>& candidates) {
        (void)state; (void)seerId; (void)candidates; return std::nullopt;
    }

    // Guardian protects a player among `candidates` (already excludes last night's
    // target unless the board allows it); std::nullopt = 空守 (BRD §2, 12-player).
    virtual std::optional<int> chooseGuard(const GameState& state, int guardId,
                                           const std::vector<int>& candidates) {
        (void)state; (void)guardId; (void)candidates; return std::nullopt;
    }

    // MechanicWolf learns a living player's role (std::nullopt = don't learn this
    // night). Global once (BRD §2, psychic board).
    virtual std::optional<int> chooseMechanicLearn(const GameState& state, int mechanicId,
                                                   const std::vector<int>& candidates) {
        (void)state; (void)mechanicId; (void)candidates; return std::nullopt;
    }

    // MechanicWolf's one-shot 破盾大刀 target (std::nullopt = save it for later).
    // Only offered when it is the lone wolf and has learned a wolf (BRD §2).
    virtual std::optional<int> chooseMechanicBigKnife(const GameState& state, int mechanicId,
                                                      const std::vector<int>& candidates) {
        (void)state; (void)mechanicId; (void)candidates; return std::nullopt;
    }

    // Witch antidote: rescue the knifed player `knifedId`? Only asked while the
    // antidote is unused (BRD §2 死讯可见性).
    virtual bool chooseWitchSave(const GameState& state, int witchId, int knifedId) {
        (void)state; (void)witchId; (void)knifedId; return false;
    }

    // Witch poison: kill one player (std::nullopt = don't poison).
    virtual std::optional<int> chooseWitchPoison(const GameState& state, int witchId,
                                                 const std::vector<int>& candidates) {
        (void)state; (void)witchId; (void)candidates; return std::nullopt;
    }

    // Hunter shot on death: take one player (std::nullopt = 不翻牌/不开枪). Only
    // asked when the hunter is actually allowed to shoot (not poisoned, BRD §2).
    virtual std::optional<int> chooseHunterShot(const GameState& state, int hunterId,
                                                const std::vector<int>& candidates) {
        (void)state; (void)hunterId; (void)candidates; return std::nullopt;
    }

    // Daytime self-destruct (BRD §2): which wolf blows up (std::nullopt = none).
    virtual std::optional<int> chooseSelfDestruct(const GameState& state,
                                                  const std::vector<int>& wolfIds) {
        (void)state; (void)wolfIds; return std::nullopt;
    }

    // --- M3 sheriff-election decisions (BRD §7; defaulted) ---

    // Stand for sheriff (上警)? (BRD §7.2).
    virtual bool chooseRunForSheriff(const GameState& state, int playerId) {
        (void)state; (void)playerId; return false;
    }

    // Withdraw from the race (退水)? (BRD §7.2).
    virtual bool chooseWithdraw(const GameState& state, int candidateId) {
        (void)state; (void)candidateId; return false;
    }

    // Sheriff-election vote (std::nullopt = abstain) among `candidates` (§7.2).
    virtual std::optional<int> chooseSheriffVote(const GameState& state, int voterId,
                                                 const std::vector<int>& candidates) {
        (void)state; (void)voterId; (void)candidates; return std::nullopt;
    }

    // Sheriff's exile ballot — 归票 (BRD §7.1). Default = 归多人 PK + abstain.
    virtual SheriffBallot chooseSheriffExileBallot(const GameState& state, int sheriffId,
                                                   const std::vector<int>& candidates) {
        (void)state; (void)sheriffId; (void)candidates; return {};
    }

    // Badge handoff on the sheriff's death (BRD §7.6): new holder, or
    // std::nullopt = tear up the badge (撕毁，全局不再有警长).
    virtual std::optional<int> chooseBadgeTransfer(const GameState& state, int sheriffId,
                                                   const std::vector<int>& candidates) {
        (void)state; (void)sheriffId; (void)candidates; return std::nullopt;
    }

    // Sheriff opens the day's speaking order (BRD §7.1.2). `anchorSeat` is the
    // lone night-victim's seat when `singleDeath` is true, else the sheriff's
    // own seat. Cosmetic cue (no mechanical effect). Default = Left.
    virtual SpeechDirection chooseSpeechDirection(const GameState& state, int sheriffId,
                                                  int anchorSeat, bool singleDeath) {
        (void)state; (void)sheriffId; (void)anchorSeat; (void)singleDeath;
        return SpeechDirection::Left;
    }

    // Directed result delivered privately to the seer (BRD §11).
    virtual void onInspectResult(int seerId, int targetId, bool isWolf) {
        (void)seerId; (void)targetId; (void)isWolf;
    }

    // Directed result delivered privately to the psychic — the target's exact
    // role (BRD §2, psychic board; MechanicWolf reports its disguise).
    virtual void onPsychicResult(int psychicId, int targetId, RoleKind shownRole) {
        (void)psychicId; (void)targetId; (void)shownRole;
    }

    // Nightly private gesture to the hunter: whether a shot is currently available
    // (BRD §2 每晚验枪 / §5.1). Informational only.
    virtual void onHunterGunCheck(int hunterId, bool canShoot) {
        (void)hunterId; (void)canShoot;
    }

    // Directed result delivered privately to the MechanicWolf right after it learns
    // a player's identity (BRD §2/§11): the exact role it just acquired.
    virtual void onMechanicLearnResult(int mechanicId, int targetId, RoleKind learnedRole) {
        (void)mechanicId; (void)targetId; (void)learnedRole;
    }

    // Speech capture (BRD roadmap §4 发言记录): collect `speakerId`'s spoken words
    // during the day's speaking phase (`kind`=Statement) or on death (`kind`=
    // LastWords). Returns the text ("" = passed / silent / logging disabled).
    // Voice input is just another source feeding this same string. Default no-op
    // so non-recording providers stay fast.
    virtual std::string collectSpeech(const GameState& state, int speakerId, SpeechKind kind,
                                      int day) {
        (void)state; (void)speakerId; (void)kind; (void)day; return "";
    }

    // Pacing hook for a human moderator (BRD M5 ⑤): block until the operator is
    // ready to continue. No-op for scripted/bot providers.
    virtual void pause(const std::string& note) { (void)note; }

    // Directed/broadcast notification for UI / observers / logging (BRD §11).
    // M1 uses it as a simple broadcast log; per-player targeting comes later.
    virtual void notify(const std::string& message) { (void)message; }
};

}  // namespace ww
