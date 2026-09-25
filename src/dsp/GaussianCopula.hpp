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

/// Standard normal CDF. erfc is accurate in the far tail, unlike 0.5*(1+erf(x/sqrt2)).
inline double Phi(double z) { return 0.5 * std::erfc(-z * 0.70710678118654752440); }

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
