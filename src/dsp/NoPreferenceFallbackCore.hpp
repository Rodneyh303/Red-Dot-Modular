#pragma once
// ── No-preference fallback core (fallback-ladder ruling, 2026-11-28) ─────────────────────────────
// ONE degenerate rule for the whole quantiser/generative family: when every weight in the governing
// set is zero ("all faders down"), the user has expressed NO PREFERENCE, and the engine behaves
// exactly as if every MEMBER of the governing set had been set to the same fader height. The
// substitution happens at the single point where weights enter each path; everything downstream —
// the pairwise weighted partition for the quantiser (QuantiserPartitionCore.hpp), warp/distribution
// distortion of the pick draw for generated notes — operates unchanged on the substituted uniform
// weights. Uniform weights through the pairwise partition ARE exact nearest-note snapping (equal
// weights ⇒ midpoint boundaries — the goldens' byte-identical regime), so "snap to the nearest note
// of the selected scale" needs no second algorithm.
//
// The governing SET comes from the membership channel, never from the weights (weight is LOUDNESS,
// membership is SCALE — ENABLED_MASK_BUILD_BRIEF). A member with zero weight is a legitimate
// fallback target. Sources of membership:
//   • Monsoon 12-TET path: ScaleManager::activeScaleMask — the context-menu selected scale
//     (Chromatic is the default: the arbiter's no-selection fall-through is all-12 = 0xFFF,
//     byte-identical to selecting Chromatic, and the menu now shows it checked).
//   • Micro path (Colonnades / Colonnades Duo): TuningTable::enabled[] — the dmtune/authored scale
//     within the tuning; the all-true default = the whole tuning (12-TET Micro-12 / 24-TET Micro-24
//     at constructor defaults). Rodney's Duo ladder (dmtune scale → tuning → default tuning) is the
//     DATA degenerating, not code: the engine just reads (cents[], enabled[]).
//   • Sikit retunes POSITIONS only (degreeVolts); the set stays Monsoon's.
//
// CONTRACT (pinned in test_no_preference_fallback.cpp): all-weights-zero is byte-identical to the
// user manually setting every member's fader to the same value. Degree ORDER never implies pitch
// order (a Micro tuning may be non-monotone in degree index): the quantiser core sorts candidate
// instances by voltage, and the generative CDF walk is order-free, so these helpers preserve degree
// indexing and impose no ordering of their own.
//
// Rack-free on purpose: unit-testable without Rack (CLAUDE.md). Header-only, no allocation.

namespace redDot {

// True when no weight in w[0..n) is positive (zeros and negatives both count as "down").
inline bool allWeightsZero(const float* w, int n) {
    for (int i = 0; i < n; ++i)
        if (w[i] > 0.f) return false;
    return true;
}

// Build the quantiser's active degree set from (weights, membership).
//   • Normal case: every degree with weight > 0 enters with its weight (caller pre-clamps).
//   • No-preference case (no positive weight): every MEMBER enters with uniform weight 1.
//   • Defensive rung (FB6): if the membership set is itself empty — the module-side
//     never-all-off guard failed, or a stale enabled[] — ALL n degrees enter with weight 1.
//     The quantiser must always have a target; silence is never the quantiser's call.
// members == nullptr means "all n degrees are members". Returns the count written to
// outList/outWeight (parallel arrays, caller-sized to n). n <= 0 returns 0.
inline int buildActiveDegreeSet(const float* weights, const bool* members, int n,
                                int* outList, float* outWeight) {
    if (n <= 0) return 0;
    int m = 0;
    for (int s = 0; s < n; ++s) {
        if (weights[s] > 0.f) {
            outList[m] = s;
            outWeight[m] = weights[s];
            ++m;
        }
    }
    if (m > 0) return m;                     // normal weighted regime — byte-identical to legacy
    for (int s = 0; s < n; ++s) {            // no preference: uniform over the membership set
        if (members && !members[s]) continue;
        outList[m] = s;
        outWeight[m] = 1.f;
        ++m;
    }
    if (m > 0) return m;
    for (int s = 0; s < n; ++s) {            // FB6: empty membership — uniform over everything
        outList[m] = s;
        outWeight[m] = 1.f;
        ++m;
    }
    return m;
}

// Generative-path substitution, in place: if all of w[0..n) are down, set every member's weight
// to 1 (uniform) and every non-member's to 0. Runs UPSTREAM of the pick, so warp / distribution
// distortion of the draw r still shapes the uniform pick exactly as it would shape real equal
// faders. Same FB6 defensive rung: an empty membership set substitutes uniform over ALL n.
// Returns true when a substitution was made (callers may want to know; displays do not change —
// the fader params themselves are never written).
inline bool substituteUniformWeights(float* w, const bool* members, int n) {
    if (n <= 0 || !allWeightsZero(w, n)) return false;
    bool any = false;
    if (members) {
        for (int i = 0; i < n; ++i) any = any || members[i];
    } else {
        any = true;
    }
    for (int i = 0; i < n; ++i)
        w[i] = (!members || !any || members[i]) ? 1.f : 0.f;
    return true;
}

} // namespace redDot
