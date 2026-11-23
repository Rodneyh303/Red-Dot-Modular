// ScrubOffsetCore — Rack-free math for the +/-32 bidirectional draw-offset "jog wheel"
// (CC_PROMPT_SCRUB32_BIDIR_SLEW_PHRASE.md). The offset is in DRAWS, not steps: each unit
// is one full draw of the moving-average Gaussian copula, so +/-32 spans up to ~64
// phrases. This header holds ONLY the pure position arithmetic both the engine (the six
// scrub sites in PatternEngine) and the widget (ScrubKnobT) call, so the floor-vs-
// truncate rule that makes NEGATIVE offsets correct is proven once and shared.
//
// No rack:: and no engine includes — unit-tested headless (test_scrub_offset).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace redDot { namespace scrub {

// The span: |offset| <= SCRUB_K draws; 2*SCRUB_K+1 integer positions (was 6).
constexpr int SCRUB_K          = 32;
constexpr int SCRUB_POSITIONS  = 2 * SCRUB_K + 1;   // 65
// The copula window (MovingAverageCopula::K) a read averages over — kept here so the
// cache-prune reasoning (K + SCRUB_K + margin) has one source. Mirror of the engine's K.
constexpr int COPULA_K         = 64;

// ── param <-> integer position ──
// The knob param is normalized [-1, 1]; a position k maps to k/SCRUB_K, which is EXACT
// in float for every k in [-32, 32] (k/32: a power-of-two denominator). f = 0 is neutral
// (the live read, the most recent draw) and sits at the centre of the range.
inline float positionToParam(int pos) {
    int p = std::min(std::max(pos, -SCRUB_K), SCRUB_K);
    return (float)p / (float)SCRUB_K;
}
inline int paramToPositionInt(float v) {
    if (!std::isfinite(v)) return 0;
    int p = (int)std::lround(v * (float)SCRUB_K);
    return std::min(std::max(p, -SCRUB_K), SCRUB_K);
}

// Snap a normalized param to the nearest integer position, output EXACT k/32. Idempotent;
// ties (x.5 positions) resolve away from zero via lround's round-half-away. Used in the
// widget's onDragEnd (snap-on-release mode) BEFORE the undo ParamChange is pushed.
inline float snapToIntegerPosition(float v) {
    return positionToParam(paramToPositionInt(v));
}

// ── continuous scrub position -> (integer draw offset, fraction) ──
// s is the continuous signed offset in [-SCRUB_K, SCRUB_K] (knob base + CV). The split
// MUST use floor, not truncation: for s = -3.25, floor gives f = -4, frac = 0.75 (frac
// always in [0,1)); truncation toward zero would give f = -3, frac = -0.25 — a negative
// fraction that corrupts the brownian-bridge blend. This is the whole reason the core
// exists.
struct Split { int f = 0; float frac = 0.f; };
inline Split splitScrub(float s) {
    Split out;
    if (!std::isfinite(s)) return out;                         // {0, 0}
    float c = std::min(std::max(s, (float)-SCRUB_K), (float)SCRUB_K);
    float ff = std::floor(c);
    out.f    = (int)ff;
    out.frac = c - ff;                                         // in [0, 1)
    if (out.frac < 0.f) out.frac = 0.f;                        // guard fp edge at exact integers
    if (out.frac >= 1.f) out.frac = 0.f;                       // (c==integer): frac exactly 0
    return out;
}

// Map a normalized mix/CV value [-1, 1] to a continuous scrub position, clamped to the span.
inline float scrubPositionFromMix(float mixNorm) {
    float m = std::isfinite(mixNorm) ? std::min(std::max(mixNorm, -1.f), 1.f) : 0.f;
    return m * (float)SCRUB_K;
}

// ── the two bracketing draw indices for counter N and integer offset f ──
// The read blends the draw at N-f (the "hi" window, nearer the counter) with N-f-1 (the
// "lo" window, the bridge key) at frac. For f > 0 this reads BEHIND the counter; for
// f < 0 it reads AHEAD (N + |f|) — Philox makes future draw indices computable. The two
// indices always differ by exactly 1, in BOTH directions, which is what the OU bridge
// assumes.
inline int64_t drawIndexHi(int64_t N, int f) { return N - f; }
inline int64_t drawIndexLo(int64_t N, int f) { return N - f - 1; }   // the bridge key

// The prune half-span a draw cache must keep to cover any offset in [-SCRUB_K, SCRUB_K]
// plus the copula window and a margin (the symmetric BIDIRECTIONAL rule).
inline int cacheHalfSpan(int margin = 16) { return COPULA_K + SCRUB_K + std::max(0, margin); }

// ── THE CACHE BUDGET: moving-average depth TRADES AGAINST jog range ──────────
// (Rodney 2026-11-23.) The cache half-span is a FIXED allocation of draw entries, and it is
// spent on two things: the copula window (K draws) and the jog offset range (SCRUB each side).
// So if the MOVING-AVERAGE PERIOD becomes user-selectable and the user picks a SHORTER one,
// the freed entries become EXTRA JOG RANGE at no cache cost:
//     K + scrub + margin <= kCacheHalfSpan
// With the current allocation (112) and margin 16 the K+scrub budget is 96:
//     K = 64 -> scrub +/-32   (today's default)
//     K = 32 -> scrub +/-64   (Rodney's example: halving the average doubles the shuttle)
//     K = 16 -> scrub +/-80
// A shorter average is less smoothing / less temporal correlation but a longer reach; a longer
// average is more correlated but a shorter reach. That is a real musical trade, not a limitation.
constexpr int kCacheHalfSpan = COPULA_K + SCRUB_K + 16;   // 112 — the fixed entry allocation

// The jog range available for a chosen moving-average period K (clamped non-negative).
inline int maxScrubForK(int K, int margin = 16, int halfSpan = kCacheHalfSpan) {
    const int k = std::max(0, K);
    return std::max(0, halfSpan - k - std::max(0, margin));
}
// The inverse: the longest moving-average period that still affords a given jog range.
inline int maxKForScrub(int scrub, int margin = 16, int halfSpan = kCacheHalfSpan) {
    const int sc = std::max(0, scrub);
    return std::max(0, halfSpan - sc - std::max(0, margin));
}
// Does a (K, scrub) pair fit the allocation?
inline bool budgetFits(int K, int scrub, int margin = 16, int halfSpan = kCacheHalfSpan) {
    return std::max(0, K) + std::max(0, scrub) + std::max(0, margin) <= halfSpan;
}
// When K changes the jog range changes with it: re-clamp a live offset into the new range
// (same shape as the viewport's re-clamp — keep the position where it is if it still fits).
inline int clampOffsetForK(int offset, int K, int margin = 16, int halfSpan = kCacheHalfSpan) {
    const int lim = maxScrubForK(K, margin, halfSpan);
    return std::min(std::max(offset, -lim), lim);
}

// PARAM-REPRESENTATION CONSEQUENCE (flag for the engine/widget wiring): positionToParam above
// normalises by the FIXED SCRUB_K (k/32 — exact because 32 is a power of two). Once the range is
// VARIABLE, k/range is exact only when `range` is a power of two (64 is, 80 is not). So a variable
// jog range argues for storing the param as the OFFSET DIRECTLY (range [-maxScrub, +maxScrub],
// integer values exactly representable) instead of a normalised [-1,1]. Then exactness no longer
// depends on the range. Decide this BEFORE wiring the param, not after.

}} // namespace redDot::scrub
