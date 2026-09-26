#pragma once
// GaussianCopula.hpp — shared copula primitives.
//
// WHY: several stages (slew, spread) need to change the RELATIONSHIP between draws without
// changing each draw's DISTRIBUTION. Linear mixing of uniforms does not: it concentrates toward
// 0.5 (see docs/design/UNIFORM_MARGINALS_COPULA_PLAN.md — an equal 7-tap average has ~1/7 the
// variance, which is the AVERAGE_POLY failure mode). Doing the combination in NORMAL space and
// mapping back keeps the marginal EXACTLY uniform while giving an exactly specified correlation.
//
// Core identity: if u_j are iid uniform and  z = SUM_j w_j * PhiInv(u_j)  with SUM_j w_j^2 == 1,
// then z ~ N(0,1) exactly, so Phi(z) ~ U(0,1) exactly. Correlation between two such combinations
// is the dot product of their weight vectors — analytic, no fitting.
//
// Header-only, no Rack dependency, deterministic, no state. Everything is a pure function of its
// inputs so callers stay counter-addressable and reversible (see DICE_SCRUB_SLEW_B2.md).
#include <cmath>
#include <cstddef>

namespace redDot {
namespace copula {

// Clamp for PhiInv inputs: Phi^-1(0) and Phi^-1(1) are infinite.
inline constexpr double U_EPS = 1e-12;

/// Standard normal CDF (EXACT, erfc form — accurate in the far tail). Used by PhiInv's Halley
/// refinement, the round-trip test, and spread's mix2, where the ~1e-15 contract matters.
/// NOT used in the slew readout (applyZ) — that uses PhiLUT (see below).
inline double Phi(double z) { return 0.5 * std::erfc(-z * 0.70710678118654752440); }

/// Interpolated lookup-table Phi for UNCACHEABLE, hot, float-precision callers (the slew readout's
/// final Phi in applyZ, and spread's mix2). Splits from the exact Phi by the caching structure:
/// cached callers (PhiInv, once per new draw) can afford the exact erfc Phi; uncacheable callers
/// (544×/window in applyZ) use this LUT. See docs/design/SLEW_COPULA_PLAN.md "Phi on the slew readout".
///
/// 4096-entry HALF-RANGE table over z in [0,6], built ONCE at first touch from the EXACT Phi, with
/// linear interpolation and symmetry Phi(-z)=1-Phi(z). Max abs err 9.3e-8 (below the 1e-7 float-lane
/// target), monotone across [-7,7] (preserves the uniform marginal), deterministic (reversal-neutral),
/// saturates to 0/1 beyond |z|>=6. NO transcendental in the hot path. r==0 short-circuits before
/// applyZ, so PhiLUT never touches bit-identity; distribution tests MUST run through it (rule 3).
struct PhiLUT {
    static constexpr int N = 4096;
    static constexpr double ZMAX = 6.0;
    static constexpr double STEP = ZMAX / N;
    static constexpr double INV_STEP = N / ZMAX;     // multiply (not divide) in the hot path
    // Built once, then read-only. A function-local static initialiser avoids a per-call `built`
    // branch (the lazy `if(!built)` check was the cause of a 2× slowdown in an earlier version).
    struct Table { float v[N + 1]; };
    static const Table& table() {
        static const Table t = []() {
            Table t;
            for (int i = 0; i <= N; ++i) t.v[i] = (float)Phi(i * STEP);   // one source of truth
            return t;
        }();
        return t;
    }
    static inline double eval(double z) {
        const double az = z < 0.0 ? -z : z;
        if (az >= ZMAX) return z < 0.0 ? 0.0 : 1.0;          // saturate
        const double fi = az * INV_STEP;                     // [0, N), multiply not divide
        const int i = (int)fi;
        const double frac = fi - (double)i;
        const Table& t = table();
        const double p = (double)t.v[i] + frac * ((double)t.v[i + 1] - (double)t.v[i]);
        return z < 0.0 ? (1.0 - p) : p;                      // Phi(-z) = 1 - Phi(z)
    }
};
inline double PhiFast(double z) { return PhiLUT::eval(z); }   // uncacheable/hot/float callers

/// Force the Phi LUT to build NOW (off the audio thread). Call from plugin.cpp init() so the
/// ~0.33 ms one-time table build (4,097 exact Phi calls) happens on the load thread, not as a
/// spike mid-block on first playback use. table() is a function-local static (thread-safe,
/// once, no static-init-order hazard) — this just touches it to trigger the init.
/// See docs/design/SLEW_COPULA_PLAN.md "LUT warm-up".
inline void warmPhiLut() { (void)PhiLUT::table(); }

/// Inverse standard normal CDF (Acklam's rational approximation, |err| < 1.15e-9),
/// refined by one Halley step against Phi so the round trip is accurate to ~1e-15.
inline double PhiInv(double p) {
    if (p <= U_EPS) p = U_EPS;
    if (p >= 1.0 - U_EPS) p = 1.0 - U_EPS;
    static const double a[6] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                                 1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[5] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                                 6.680131188771972e+01, -1.328068155288572e+01};
    static const double c[6] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                                -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[4] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
                                3.754408661907416e+00};
    const double pl = 0.02425, ph = 1.0 - pl;
    double x;
    if (p < pl) {
        const double q = std::sqrt(-2.0 * std::log(p));
        x = (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    } else if (p > ph) {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        x = -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    } else {
        const double q = p - 0.5, r = q * q;
        x = (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q /
            (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
    }
    // Halley refinement
    const double e = Phi(x) - p;
    const double v = e * 2.50662827463100050242 * std::exp(x * x * 0.5);   // e / pdf(x)
    x -= v / (1.0 + x * v * 0.5);
    return x;
}

/// Combine uniforms in normal space with pre-normalised weights (SUM w^2 must be 1).
/// Returns a uniform. Pure function of its inputs.
inline double combine(const double* u, const double* w, std::size_t n) {
    double z = 0.0;
    for (std::size_t j = 0; j < n; ++j) z += w[j] * PhiInv(u[j]);
    return Phi(z);
}

/// Two-source copula mix used by SPREAD: result correlates with `leader` at exactly rho and keeps
/// a uniform marginal. rho < 0 gives the mirror (rho = -1 => exactly 1 - leader), so the legacy
/// "1 - p" special case disappears. `own` is the voice's own (independent) draw.
inline double mix2(double own, double leader, double rho) {
    if (rho > 0.9999) return leader;
    if (rho < -0.9999) return 1.0 - leader;
    const double w[2] = {rho, std::sqrt(1.0 - rho * rho)};
    const double u[2] = {leader, own};
    return combine(u, w, 2);
}

}  // namespace copula
}  // namespace redDot
