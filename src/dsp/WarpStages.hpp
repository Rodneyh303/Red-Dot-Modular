#pragma once
// ── dotModular::warpStages — the FEEL-TEST distribution stages: MO rail tilt + injected atom ────
// (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START.md — the authority; math per
// PALETTE_KUMARASWAMY_TILT_NOTE.md + OUTPUT_LAYER_DISTRIBUTION_MAPPING.md "THE ONE USEFUL
// ATOM FEATURE".) DISPLAYED-(voice,lane) prototype: author-and-see, NOT the full
// all-lanes/all-voices/CV-out/modulation/coupling plumbing.
//
// TWO PURE, STATELESS STAGES that compose over ANY member quantile (the palette-wide
// applicability of the tilt note: monotone ∘ monotone = valid quantile for every member,
// incl. Identity and the drawn PCHIP — "draw, then tilt"):
//
//   STAGE 1 — MO RAIL TILT (sample-side, the note's pinned CONVENTION):
//       R_p(t) = (1-p)·t / (1 - p·t)          (one mul + one div)
//     applied AFTER the member's invCDF: G⁻¹(u) = R_p(F⁻¹(u)).
//     R_p is EXACTLY MobiusDistribution's invCDF at a = 1-p (verified against
//     Distribution.hpp: a·u/(1-(1-a)·u) with a=1-p gives (1-p)u/(1-pu)). It is a monotone
//     self-map of [0,1] for every p<1 (fixes 0 and 1; slope ∈ [1-p, 1/(1-p)]), so it adds
//     no flat segments and no atoms — a pure density re-weighting toward the bottom rail
//     (p>0) or the top rail (p<0). Its INVERSE is itself a Möbius map (verify by hand:
//     y = (1-p)t/(1-pt) ⟺ y·(1-p+py) = ... ⟺ t = y/(1-p+py)):
//       R_p⁻¹(y) = y / (1 - p + p·y)  =  R_{-p/(1-p)}(y)  =  Mobius(1-p).cdf(y)
//     with derivative (R_p⁻¹)'(y) = (1-p)/(1-p+py)². These give the tilted member's
//     CLOSED FORMS the display needs:
//       G(x)   = F(R_p⁻¹(x));   g(x) = f(R_p⁻¹(x))·(R_p⁻¹)'(x).
//
//   STAGE 2 — INJECTED ATOM, FREE LOCATION (INSERT/DISPLACE — the note's leaned default):
//     the tilted member G plus a Dirac of mass m at level ℓ:
//       H(x) = (1-m)·G(x) + m·1[x ≥ ℓ]
//     whose quantile (derived, verified against the prompt's contract):
//       q0 = G(ℓ)·(1-m)
//       u < q0        → G⁻¹(u/(1-m))          (the continuous part below ℓ, rescaled)
//       u ∈ [q0,q0+m] → exactly ℓ             (the atom: interval width m BY CONSTRUCTION)
//       u > q0+m      → G⁻¹((u-m)/(1-m))      (the continuous part above ℓ, rescaled)
//     CONTRACT (each pinned by test_warp_stages):
//       · MONOTONE — each branch is G⁻¹∘rescale (monotone), the atom is a flat at ℓ, and
//         the pieces meet in order (below-branch ≤ ℓ ≤ above-branch since
//         u/(1-m) < G(ℓ) < (u-m)/(1-m) across the respective intervals and G⁻¹ increasing).
//       · MASS — P(output == ℓ) == m exactly: the u-interval [q0, q0+m] has width m and
//         every u in it returns ℓ bit-exactly.
//       · EDGE CONTINUITY — at u=q0 the below-branch gives G⁻¹(G(ℓ)) = ℓ ✓ and at u=q0+m
//         the above-branch gives G⁻¹(G(ℓ)) = ℓ ✓: the quantile is CONTINUOUS in u (the
//         flat sits exactly where the two branches meet — no jump, no gap).
//       · SHAPE — the continuous parts keep the member's shape at total mass (1-m)
//         (insert/displace: the atom ADDS an anchor and pushes the rest out).
//
// NEUTRALITY (the house neutral-parameter rule): p == 0 MUST be bit-identity (R_0 is
// arithmetically t/1 — but the contract is implemented as an EARLY-OUT, never a reliance
// on the arithmetic), and m == 0 MUST be bit-identity likewise. A cell with p=0 AND m=0
// evaluates the member quantile UNTOUCHED — existing patches and every pinned
// bit-identity bridge (test_resolver_warp_bridge, test_warp_f_bank, ...) stay green
// UNCHANGED.
//
// AUTHORING CLAMPS (eval-side, the clampR/CONC_MAX precedent): p is clamped into
// [-kTiltMax, +kTiltMax] BEFORE any R_p evaluation (|p| ≥ 1 can never reach the hot path
// — modulation overshoot and hand-edited JSON included) and m into [0, kAtomMassMax]
// (M_MAX keeps the continuous part alive; m=1 would collapse to the pure atom). P_MAX =
// 0.95 is PROVISIONAL per the note (feel-test refines); ℓ is clamped into [0,1] (the
// corners are legal — a rail atom — but the default is interior).
//
// Pure module: no Rack/nanovg/UI/jansson — plain double arithmetic only, so the stage
// functions are directly unit-testable (test_warp_stages.cpp) and cost one compare +
// rescale per draw on the warp rebuild path (never per audio sample).
//
// COMPOSITION ORDER (the prompt's ruling): tilt FIRST, then the atom — the atom's G is
// the TILTED member's CDF, so a rail-ward tilt visibly compounds with an atom placed
// near that rail (the note's "reinforce" relation).

namespace dotModular::warpStages {

// ── constants ──────────────────────────────────────────────────────────────────────
constexpr double kTiltMax          = 0.95;   // P_MAX (PROVISIONAL — the note's feel-test recipe refines)
constexpr double kAtomMassMax      = 0.9;    // M_MAX: the continuous part stays alive
constexpr double kAtomLevelDefault = 0.5;    // interior default (0/1 = rail atom, allowed)

// ── eval-side authoring clamps (max/min form — the resolver's NaN convention) ───────
inline double clampTilt(double p)   { return p < -kTiltMax ? -kTiltMax : (p > kTiltMax ? kTiltMax : p); }
inline double clampMass(double m)   { return m < 0.0 ? 0.0 : (m > kAtomMassMax ? kAtomMassMax : m); }
inline double clampLevel(double l)  { return l < 0.0 ? 0.0 : (l > 1.0 ? 1.0 : l); }

// ── the Möbius maps (RAW: |p| < 1 REQUIRED — the callers clamp) ─────────────────────
// R_p(t) = (1-p)t/(1-pt) — the tilt; R_p⁻¹(y) = y/(1-p+py) — its inverse (itself a
// Möbius map: R_p⁻¹ = R_{-p/(1-p)} = Mobius(1-p).cdf). One mul + one div each.
inline double tiltMap(double t, double p)    { return (1.0 - p) * t / (1.0 - p * t); }
inline double tiltMapInv(double y, double p) { return y / (1.0 - p + p * y); }
// (R_p⁻¹)'(y) = (1-p)/(1-p+py)² — the tilt's density Jacobian (the display pdf).
inline double tiltMapInvSlope(double y, double p) {
    const double d = 1.0 - p + p * y;
    return (1.0 - p) / (d * d);
}

// ── THE COMPOSED QUANTILE (member → tilt → atom) ───────────────────────────────────
// qMember(u): the member's quantile (double→double, monotone); cdfMember(x): its CDF.
// p/ell/m are RAW authoring values — clamped INSIDE (eval-side). The neutral early-outs
// are BIT-EXACT: p==0 returns the member call untouched; m==0 likewise (the prompt's
// mandate — never rely on R_0 ≡ id arithmetic).
template <class Q, class C>
inline double invCDF(double u, double p, double ell, double m, Q qMember, C cdfMember) {
    if (clampMass(m) <= 0.0) {                 // atom neutral: tilt only
        p = clampTilt(p);
        if (p == 0.0) return qMember(u);       // p==0 early-out: bit-identity
        return tiltMap(qMember(u), p);
    }
    p   = clampTilt(p);
    ell = clampLevel(ell);
    // The atom's placement quantile: q0 = G(ℓ)·(1-m) with G(ℓ) = F(R_p⁻¹(ℓ)) — the tilted
    // member's CDF at the atom level (CLOSED FORM: one member CDF eval + one R_p⁻¹).
    const double q0 = cdfMember(tiltMapInv(ell, p)) * (1.0 - m);
    if (u < q0)                                  // below ℓ: rescale into the member's
        return p == 0.0 ? qMember(u / (1.0 - m)) // lower mass share, then tilt
                        : tiltMap(qMember(u / (1.0 - m)), p);
    if (u <= q0 + m)                             // the atom: width m BY CONSTRUCTION
        return ell;
    return p == 0.0 ? qMember((u - m) / (1.0 - m))   // above ℓ: the upper mass share
                    : tiltMap(qMember((u - m) / (1.0 - m)), p);
}

// ── the composed CDF (display) ──────────────────────────────────────────────────────
// H(x) = (1-m)·F(R_p⁻¹(x)) + m·1[x ≥ ℓ]. The riser at ℓ (the jump by m) renders through
// the widget's CDF sampling for free (the vertical segment between the last x<ℓ and the
// first x≥ℓ sample).
template <class C>
inline double cdf(double x, double p, double ell, double m, C cdfMember) {
    p   = clampTilt(p);
    ell = clampLevel(ell);
    double h = cdfMember(tiltMapInv(x, p));    // the tilted member G(x)
    if (clampMass(m) > 0.0) {
        h *= (1.0 - clampMass(m));
        if (x >= ell) h += clampMass(m);
    }
    return h;
}

// ── the composed PDF (display) — the CONTINUOUS part only ───────────────────────────
// h(x) = (1-m)·f(R_p⁻¹(x))·(R_p⁻¹)'(x) for x ≠ ℓ; the Dirac m·δ(x-ℓ) is NOT a density
// value — the caller (DistributionCurveWidget's atom bar) draws it EXPLICITLY as a
// finite bar at ℓ with height ∝ m.
template <class D>
inline double pdf(double x, double p, double m, D pdfMember) {
    p = clampTilt(p);
    const double part = pdfMember(tiltMapInv(x, p)) * tiltMapInvSlope(x, p);
    const double cm = clampMass(m);
    return cm > 0.0 ? part * (1.0 - cm) : part;
}

} // namespace dotModular::warpStages
