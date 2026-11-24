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

    // Enumerate every active-degree INSTANCE whose center is in [vLo,vHi], sorted by center.
    // octave range: center = o + pos, pos in [0,1), so o spans floor(vLo)-1 .. ceil(vHi)+1 and
    // we filter. kMaxInstances bounds it: up to 24 degrees over ~6 octaves of [0,5].
    static constexpr int kMaxInstances = 24 * 8;
    std::array<float, kMaxInstances> cCenter{};
    std::array<int,   kMaxInstances> cIndex{};
    std::array<int,   kMaxInstances> cOct{};
    int     m = 0;

    const int oLo = (int)vLo - 1;
    const int oHi = (int)vHi + 1;
    for (int o = oLo; o <= oHi; ++o) {
        for (int i = 0; i < n; ++i) {
            const float center = (float)o + positions[i];
            if (center < vLo || center > vHi) continue;
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
        // No instance in range (degenerate: range narrower than the degree spacing). Fall back
        // to the nearest instance of degree 0 across octaves, clamped.
        out.index = 0;
        int   bestO = (int)vIn;
        float bestD = 1e30f;
        for (int o = oLo; o <= oHi; ++o) {
            const float c = (float)o + positions[0];
            const float d = (vIn > c) ? (vIn - c) : (c - vIn);
            if (d < bestD) { bestD = d; bestO = o; }
        }
        out.octave = bestO;
        float v = (float)bestO + positions[0];
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
        // boundary proportional to weights; wA+wB > 0 since weights are > 0.
        const float ub = cCenter[k] + (cCenter[k+1] - cCenter[k]) * (wA / (wA + wB));
        if (vIn < ub) { win = k; break; }
    }

    out.index  = cIndex[win];
    out.octave = cOct[win];
    out.volts  = cCenter[win];   // already within [vLo,vHi] by construction
    return out;
}

} // namespace redDot
