#pragma once
// QuantiserPartitionCore — the quantiser as a WEIGHTED PARTITION of the input voltage axis.
//
// WHY (docs/design/QUANTISER_WEIGHTED_PARTITION.md; the qmix "a 1% scale degree sounds ~half
// the time" bug). The old SequencerEngine::quantize gives each active degree a fixed capture
// radius = w/12 and, when the CV lands outside every radius (most of the axis), falls back to
// the NEAREST active degree by distance with the WEIGHT IGNORED. So a faint degree wins every
// CV for which it is merely nearest — a big slice for a low degree with no close neighbour.
//
// The Vermona Mode-C semantics ("the faders define your scale; higher fader = wider
// quantization range") only make sense as a PARTITION with no gaps: each active degree claims
// a slice of the axis proportional to its fader, RELATIVE within the active set. The boundary
// between two adjacent active-degree instances sits proportional to their weights:
//     boundary = c_a + (c_b - c_a) * w_a / (w_a + w_b)
// The stronger neighbour pushes the boundary toward the weaker, claiming more range. Adjacent
// instances share endpoints, so the axis tiles — no gaps, no fallback. A 1% degree beside a
// 100% degree claims ~1% of the gap, so it sounds ~1%.
//
// OCTAVE WRAP is automatic here: instances are enumerated ACROSS octaves (center = o + pos)
// and the winner's octave falls out of which slice vIn lands in — the doc's "seam boundary
// with a +/-1 delta" is just the boundary between the top instance of one octave and the
// bottom of the next. CLAMP: only instances whose center is in [vLo,vHi] are considered (as
// the old code skipped out-of-range candidates), so the lowest/highest in-range instance
// absorbs the tail — e.g. 0 V -> the lowest active degree at octave 0.
//
// UNIFORM WEIGHTS reduce to plain nearest-degree snapping (every boundary becomes the
// midpoint), which is byte-identical to the old quantize for the default 12-TET / equal-
// weight case — the regime the goldens encode. Only non-uniform weights change.
//
// Rack-free, header-only, no allocation (fixed scratch, bounded by kMaxInstances).

#include <cstddef>
#include <array>

namespace redDot {

struct QuantResult {
    int   index  = 0;     // index into the positions[]/weights[] arrays (the chosen active degree)
    int   octave = 0;     // the octave the winning instance sits in (center = octave + positions[index])
    float volts  = 0.f;   // octave + positions[index], already within [vLo,vHi]
    // THE WRAP MEMO (octave-wrap ruling, 2026-11-28): the octave shift the in-range remap applied
    // to fold a margin winner back into [vLo,vHi]. 0 for every mid-range win (the remap is a no-op
    // there), +1 when a below-range instance was folded UP, -1 when an above-range instance was
    // folded DOWN. volts/octave hold the FOLDED absolute answer (the both-external case must live
    // in range); wrapDelta lets a mixed row recover the unfolded intent: the UNFOLDED octave is
    // octave - wrapDelta, and a generated register applies the fold's inverse (register + ... -
    // wrapDelta... see voicePitch) so "just below the bottom C" sounds as the B just below the
    // register, not the B at its top. At equal weights the wrap boundary sits exactly ON the range
    // edge, so a margin instance never wins and wrapDelta is always 0 — the goldens' regime.
    int   wrapDelta = 0;
};

// positions[i] : within-octave voltage of active degree i, in [0,1) (s/12 for 12-TET, else
//                the tuning's degreeVolts(s)).
// weights[i]   : the fader weight, > 0 (a w==0 degree is OUT of the scale and must not be in
//                these arrays — the caller's active list already excludes it).
// n            : number of active degrees, >= 1.
// vIn          : the input voltage (any value; outside [vLo,vHi] the extreme instance absorbs it).
// [vLo,vHi]    : the reachable output range (the engine clamps to [0,5]).
//
// Returns the winning active-degree index, its octave, and volts. Never allocates; if n<=0 it
// returns a zeroed result (the caller guarantees n>=1 — an empty scale is handled upstream).
inline QuantResult weightedQuantize(const float* positions, const float* weights, int n,
                                    float vIn, float vLo = 0.f, float vHi = 5.f) {
    QuantResult out;
    if (n <= 0) return out;

    // Enumerate every active-degree INSTANCE in [vLo-1, vHi+1] — ONE OCTAVE OF MARGIN each
    // side — sorted by center. The margin is the fix for the tail bug (Rodney's feel-test:
    // the lowest/highest nonzero degree picked far above its fader): without margin the
    // lowest in-range instance's lower bound was -inf and the highest's upper bound +inf, so
    // each absorbed the whole unweighted tail ([vLo, firstCenter) / (lastCenter, vHi]) — a
    // full degree's worth of range regardless of its weight. With margin, the edge in-range
    // instances get a REAL weighted boundary against their wrap-neighbour (the same degree an
    // octave away), so a faint edge degree claims only its proportional sliver and the rest
    // wraps to the neighbour. A winner landing on a margin (out-of-range) instance is remapped
    // to the nearest in-range octave of that degree below (octave wrap at the seam — the doc's
    // intended +/-1 behaviour). Equal weights are byte-identical: the wrap boundary is the
    // midpoint, which for a degree and its octave-neighbour lands exactly on vLo/vHi, so no
    // in-range vIn ever maps to a margin instance (the goldens' regime is untouched).
    static constexpr int kMaxInstances = 24 * 8;
    std::array<float, kMaxInstances> cCenter{};
    std::array<int,   kMaxInstances> cIndex{};
    std::array<int,   kMaxInstances> cOct{};
    int     m = 0;

    const int oLo = (int)vLo - 1;
    const int oHi = (int)vHi + 1;
    const float encLo = vLo - 1.f;   // enumeration window (one octave margin each side)
    const float encHi = vHi + 1.f;
    for (int o = oLo; o <= oHi; ++o) {
        for (int i = 0; i < n; ++i) {
            if (weights[i] <= 0.f) continue;   // SELF-PROTECT: a muted degree is not in the
                                               // partition at all (don't trust the caller to
                                               // pre-filter). Real neighbours then border each
                                               // other directly and split their gap by weight;
                                               // and no boundary can ever divide 0/(0+0)=NaN.
            const float center = (float)o + positions[i];
            if (center < encLo || center > encHi) continue;
            if (m >= kMaxInstances) break;
            // insertion into the sorted-by-center arrays (m is tiny; insertion sort is fine)
            int k = m;
            while (k > 0 && cCenter[k-1] > center) {
                cCenter[k] = cCenter[k-1]; cIndex[k] = cIndex[k-1]; cOct[k] = cOct[k-1]; --k;
            }
            cCenter[k] = center; cIndex[k] = i; cOct[k] = o; ++m;
        }
    }

    if (m == 0) {
        // No instance enumerated. Two sub-cases:
        int fb = -1;
        for (int i = 0; i < n; ++i) { if (weights[i] > 0.f) { fb = i; break; } }
        if (fb < 0) {
            // EVERY weight <= 0 (all-muted scale reaching the core). CHROMATIC PASSTHROUGH —
            // no scale means no quantisation — matching the caller's own all-muted cold path
            // (activeSemiCount==0). No division, no NaN; the degenerate-scale fallback the
            // caller never has to make (Rodney's "no nonzero faders" case).
            out.index  = 0;
            out.volts  = vIn < vLo ? vLo : (vIn > vHi ? vHi : vIn);
            out.octave = (int)std::floor(out.volts);
            return out;
        }
        // An active degree exists but none landed in the enumeration window (range narrower
        // than the degree spacing — not reachable with the default >=2-octave range, kept as a
        // safety net): snap to the nearest instance of the first active degree, clamped.
        out.index = fb;
        int   bestO = (int)vIn;
        float bestD = 1e30f;
        for (int o = oLo; o <= oHi; ++o) {
            const float c = (float)o + positions[fb];
            const float d = (vIn > c) ? (vIn - c) : (c - vIn);
            if (d < bestD) { bestD = d; bestO = o; }
        }
        out.octave = bestO;
        float v = (float)bestO + positions[fb];
        out.volts = v < vLo ? vLo : (v > vHi ? vHi : v);
        return out;
    }

    // Find the instance whose weighted slice contains vIn. Slices tile the axis: instance k
    // owns [lb_k, ub_k), lb_0 = -inf, ub_{m-1} = +inf, ub_k = weighted boundary to k+1. So the
    // winner is the first k with vIn < ub_k (the last instance catches everything above).
    int win = m - 1;
    for (int k = 0; k < m - 1; ++k) {
        const float wA = weights[cIndex[k]];
        const float wB = weights[cIndex[k+1]];
        // boundary proportional to weights. Both are > 0 (the w<=0 skip above guarantees it),
        // but guard the division anyway so the core never emits a NaN boundary: equal split
        // (midpoint) is the safe tie-break if a zero-sum pair ever reaches here.
        const float wsum = wA + wB;
        const float frac = (wsum > 0.f) ? (wA / wsum) : 0.5f;
        const float ub = cCenter[k] + (cCenter[k+1] - cCenter[k]) * frac;
        if (vIn < ub) { win = k; break; }
    }

    out.index  = cIndex[win];
    out.octave = cOct[win];
    out.volts  = cCenter[win];

    // Remap a margin (out-of-range) winner to the nearest IN-RANGE octave of the SAME degree
    // (the octave wrap at the seam). The degree the LED lights is preserved; only the octave
    // folds into [vLo,vHi]. Equal weights never reach here (the wrap boundary sits on the
    // range edge), so the goldens are untouched.
    const float pos = positions[out.index];
    while (out.volts < vLo) { out.octave += 1; out.wrapDelta += 1; out.volts = (float)out.octave + pos; }
    while (out.volts > vHi) { out.octave -= 1; out.wrapDelta -= 1; out.volts = (float)out.octave + pos; }
    if (out.volts < vLo) out.volts = vLo;   // degenerate (range < one degree spacing): clamp
    if (out.volts > vHi) out.volts = vHi;
    return out;
}

} // namespace redDot
