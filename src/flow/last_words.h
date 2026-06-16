#pragma once

#include <vector>

#include "core/enums.h"
#include "core/player.h"

// Last-words (遗言) eligibility — a single derived predicate (BRD §5.3):
//   hasLastWords = (died in Day && cause != BlownUp) || (died at Night && day == 1)
// Never special-cased per role/death; it falls out of (death phase + cause).
namespace ww {

inline bool hasLastWords(Phase deathPhase, int deathDay,
                         const std::vector<DeathCause>& causes) {
    // 自爆无遗言 — universal, regardless of phase. This also covers 自爆吞毒 (§2),
    // where the BlownUp cause is stamped over a night poison death.
    for (DeathCause c : causes) {
        if (c == DeathCause::BlownUp) return false;
    }
    // Daytime death (exile, daytime hunter shot, ...) -> last words.
    if (deathPhase == Phase::Day) return true;
    // Night death: only the first night grants last words.
    return deathDay == 1;
}

inline bool hasLastWords(const Player& p) {
    if (!p.deathPhase() || !p.deathDay()) return false;  // still alive
    return hasLastWords(*p.deathPhase(), *p.deathDay(), p.deathCauses());
}

}  // namespace ww
