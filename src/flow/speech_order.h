#pragma once

#include "io/decision_provider.h"  // SpeechDirection

// Time-seeded speaking-order helpers for the no-sheriff case (BRD M5 §随机发言).
// Pure functions so the math is testable; the caller supplies system-time values.
//
// Method (per spec):
//   - direction: (system time) % 2 decides clockwise vs counter-clockwise.
//   - first speaker (peaceful / multi-death): a second system-time reading % alive.
namespace ww {

inline SpeechDirection timeDirection(long long timeValue) {
    return (timeValue % 2 == 0) ? SpeechDirection::Left : SpeechDirection::Right;
}

inline int timeFirstSpeaker(long long timeValue, int aliveCount) {
    if (aliveCount <= 0) return 0;
    long long m = timeValue % aliveCount;
    if (m < 0) m += aliveCount;  // keep in [0, aliveCount)
    return static_cast<int>(m);
}

}  // namespace ww
