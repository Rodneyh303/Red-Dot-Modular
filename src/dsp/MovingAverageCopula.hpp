#pragma once
// MovingAverageCopula.hpp — SLEW as a moving-average Gaussian copula.
//
// INTENT (Rodney): slew controls how far the next draw may move from the last one.
// REQUIREMENTS: stateless / reversible, preserve the lane's distribution, controlled motion.
//
//   r == 0  -> return the legacy draw u_n BIT-IDENTICALLY (no migration, no behaviour change)
//   r  > 0  -> z_n = SUM_{j<K} w_j(r) * PhiInv(u_{n-j}),  w_j proportional to r^j,  SUM w^2 == 1
//              out = Phi(z_n)  -> EXACTLY uniform -> feeds the existing lane quantile unchanged
//
// Weights are geometric and L2-normalised, so (GaussianCopula.hpp):
//   * every output is exactly uniform for every r;
//   * lag-m correlation is the analytic dot product SUM_j w_j*w_{j+m} — no fitting, no measurement.
// As K grows, lag-1 -> r, so the knob reads as "correlation with the previous draw".
//
// WHY NOT the alternatives (all rejected, see docs/design/UNIFORM_MARGINALS_COPULA_PLAN.md):
//   * linear average of the window  -> concentrates toward 0.5 (variance ~1/K): AVERAGE_POLY's bug
//   * clamping a variance-preserving linear mix -> piles mass at 0 and 1
//   * truncating the window near the origin     -> under-weights edges, breaks pure-fn-of-position
//   * AR(1) / recursive slew -> carries state: unstable backward, and degenerate at rho = 0
//
// KEY PROPERTY: r enters ONLY at readout, never the draw chain. The chain therefore reverses
// exactly under ANY slew modulation — no constant-slew requirement. Output replay on reverse is
// exact iff r(n) is itself reproducible at step n (e.g. lane-driven, not live CV).
//
// Header-only, no state, no Rack dependency. Caller supplies the window of raw uniforms.
#include <cmath>
#include <cstddef>
#include "GaussianCopula.hpp"
#include "../third_party/simd/simd_compat.hpp"   // AVX2 __m256d for sumZ SIMD path

namespace redDot {

class MovingAverageCopula {
public:
    static constexpr std::size_t K = 64;     ///< window length (taps)
    static constexpr double R_MAX = 0.97;    ///< r clamp: geometric weights degenerate as r -> 1

    /// Fill w (K entries) with L2-normalised geometric weights for r. w[0] is the newest draw.
    /// SUM w^2 == 1 exactly (up to rounding), which is what makes the output uniform.
    static void weights(double r, double* w) {
        r = clampR(r);
        double s2 = 0.0, p = 1.0;
        for (std::size_t j = 0; j < K; ++j) { w[j] = p; s2 += p * p; p *= r; }
        const double inv = 1.0 / std::sqrt(s2);
        for (std::size_t j = 0; j < K; ++j) w[j] *= inv;
    }

    /// Cache of L2-normalised weights for a given r.  Slew r is phrase-fixed (the slew-phrase
    /// fix landed), so within one recomputeEffective* call — which loops over all 16 steps ×
    /// lanes × voices × 2 windows — weights(r, w) was recomputed every sumZ invocation with
    /// the SAME r.  This struct caches w[K] + the r it was built for; callers update(r) once
    /// per phrase (or per recompute if r changed) then pass w to the sumZ(z, w) overload.
    /// Bit-identical: the weights and the dot product are the same computation, just hoisted.
    struct WeightCache {
        double w[K];
        double cachedR = 999.0;   ///< invalid sentinel (no valid r is 999)
        bool matches(double r) const { return r == cachedR; }
        void update(double r) { weights(r, w); cachedR = r; }
        void invalidate() { cachedR = 999.0; }
    };

    /// Apply slew at one position. u holds the window NEWEST FIRST: u[0] = u_n, u[j] = u_{n-j}.
    /// r == 0 returns u[0] unchanged (bit-identical legacy path).
    /// r < 0 creates anti-correlation (alternating-sign weights → hocket/interlock).
    static double apply(const double* u, double r) {
        if (r == 0.0) return u[0];                // exact passthrough, and NaN-safe
        double w[K];
        weights(r, w);
        return copula::combine(u, w, K);
    }
    /// Apply slew from a window of PRE-COMPUTED normal scores z (= PhiInv(u), newest first),
    /// skipping the per-call PhiInv. Use this when the caller caches PhiInv(u) alongside each
    /// raw draw (it is a pure function of the draw ⇒ reversal-neutral). r == 0 returns Phi(z[0])
    /// — NOT the raw uniform — so callers on the r==0 bit-identity path must short-circuit to
    /// the raw u[0] themselves (see PatternEngine::patternXAt). For r > 0 this is bit-identical
    /// to apply(u, r): combine does z' = Σ w_j·PhiInv(u_j) = Σ w_j·z[j], then Phi(z').
    // The weighted normal-space sum s = Σ w_j·z_j WITHOUT the final Phi. Used by the SCRUB blend
    // (Phase 2): recomputeEffective* blends two adjacent windows' s values in normal space, then
    // applies PhiFast ONCE — blending the uniform outputs would be a linear blend of uniforms
    // (the distortion Phase 2 eliminates). r==0 returns z[0] (the cached PhiInv of the raw draw).
    static double sumZ(const double* z, double r) {
        if (r == 0.0) return z[0];
        double w[K];
        weights(r, w);
        return sumZ(z, w);
    }
    /// sumZ with PRE-COMPUTED weights (from a WeightCache).  Skips the weights() call — the
    /// dot product only.  Caller must ensure w was built for the correct r and that r != 0
    /// (the r==0 early-out is in the sumZ(z, r) wrapper; callers on the r==0 path short-
    /// circuit before reaching here).
    static double sumZ(const double* z, const double* w) {
        // 4-wide lane-independent accumulation with FIXED reduction order.
        // No FMA (separate mul+add = 2 roundings, matches scalar — cross-platform determinism).
        // Two tiers: AVX1 (4-wide __m256d) > scalar (4-wide double).
        // AVX2 adds nothing for double mul/add (only integer 256-bit + gather).
        // Each lane does accN += w[j+N] * z[j+N] (one mul, one add, two roundings).
        // Horizontal sum: (lane0 + lane1) + (lane2 + lane3) — fixed everywhere.
        // Bit-identical across native AVX1 (x86 -mavx), SIMDe NEON (arm64), and scalar.
        #if defined(__AVX__) || defined(__aarch64__) || defined(__ARM_NEON)
        __m256d acc = _mm256_setzero_pd();
        for (std::size_t j = 0; j < K; j += 4) {
            __m256d zj = _mm256_loadu_pd(&z[j]);
            __m256d wj = _mm256_loadu_pd(&w[j]);
            acc = _mm256_add_pd(acc, _mm256_mul_pd(zj, wj));  // NO FMA — 2 roundings
        }
        double tmp[4];
        _mm256_storeu_pd(tmp, acc);
        return (tmp[0] + tmp[1]) + (tmp[2] + tmp[3]);
        #else
        double lane0 = 0.0, lane1 = 0.0, lane2 = 0.0, lane3 = 0.0;
        for (std::size_t j = 0; j < K; j += 4) {
            lane0 += w[j]   * z[j];
            lane1 += w[j+1] * z[j+1];
            lane2 += w[j+2] * z[j+2];
            lane3 += w[j+3] * z[j+3];
        }
        return (lane0 + lane1) + (lane2 + lane3);
        #endif
    }
    // applyZ = PhiFast(sumZ(...)) — the full copula readout (uniform output). Used by patternXAt
    // (non-scrub callers) and the r==0 short-circuit shape. Phase 2's scrub blend uses sumZ +
    // its own PhiFast so the two windows share one Phi.
    static double applyZ(const double* z, double r) {
        return copula::PhiFast(sumZ(z, r));
    }
    /// applyZ with PRE-COMPUTED weights (from a WeightCache).  = PhiFast(sumZ(z, w)).
    static double applyZ(const double* z, const double* w) {
        return copula::PhiFast(sumZ(z, w));
    }

    /// Analytic lag-m correlation of the output series for a constant r (dot product of the
    /// weight vector with itself shifted by m). Tests compare the empirical value against this.
    static double lagCorr(double r, std::size_t m = 1) {
        if (m >= K) return 0.0;
        r = clampR(r);
        if (r == 0.0) return (m == 0) ? 1.0 : 0.0;
        double w[K];
        weights(r, w);
        double acc = 0.0;
        for (std::size_t j = 0; j + m < K; ++j) acc += w[j] * w[j + m];
        return acc;
    }

    /// Clamp r to [-R_MAX, R_MAX]. Negative r is valid (anti-correlation); the geometric
    /// weights alternate in sign, producing a lag-1 correlation of the same magnitude but
    /// opposite polarity. R_MAX bounds |r| because geometric weights degenerate as |r| → 1.
    static double clampR(double r) { return r < -R_MAX ? -R_MAX : (r > R_MAX ? R_MAX : r); }
};

}  // namespace redDot
