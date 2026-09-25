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

namespace redDot {

class MovingAverageCopula {
public:
    static constexpr std::size_t K = 64;     ///< window length (taps)
    static constexpr double R_MAX = 0.97;    ///< r clamp: geometric weights degenerate as r -> 1

    /// Fill `w` (K entries) with L2-normalised geometric weights for r. w[0] is the newest draw.
    /// SUM w^2 == 1 exactly (up to rounding), which is what makes the output uniform.
    static void weights(double r, double* w) {
        r = clampR(r);
        double s2 = 0.0, p = 1.0;
        for (std::size_t j = 0; j < K; ++j) { w[j] = p; s2 += p * p; p *= r; }
        const double inv = 1.0 / std::sqrt(s2);
        for (std::size_t j = 0; j < K; ++j) w[j] *= inv;
    }

    /// Apply slew at one position. `u` holds the window NEWEST FIRST: u[0] = u_n, u[j] = u_{n-j}.
    /// r == 0 returns u[0] unchanged (bit-identical legacy path).
    static double apply(const double* u, double r) {
        if (!(r > 0.0)) return u[0];              // exact legacy passthrough, and NaN-safe
        double w[K];
        weights(r, w);
        return copula::combine(u, w, K);
    }

    /// Analytic lag-m correlation of the output series for a constant r (dot product of the
    /// weight vector with itself shifted by m). Tests compare the empirical value against this.
    static double lagCorr(double r, std::size_t m = 1) {
        if (m >= K) return 0.0;
        r = clampR(r);
        if (!(r > 0.0)) return (m == 0) ? 1.0 : 0.0;
        double w[K];
        weights(r, w);
        double acc = 0.0;
        for (std::size_t j = 0; j + m < K; ++j) acc += w[j] * w[j + m];
        return acc;
    }

    static double clampR(double r) { return r < 0.0 ? 0.0 : (r > R_MAX ? R_MAX : r); }
};

}  // namespace redDot
