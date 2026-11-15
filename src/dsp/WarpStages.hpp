#pragma once
// ── dotModular::warpStages — the FEEL-TEST distribution stages: MO rail tilt + zero-density
// gap + injected atom ──────────────────────────────────────────────────────────────────────
// (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START.md + docs/design/PHASE_DISTRIBUTION_SPEC.md —
// the authorities; math per PALETTE_KUMARASWAMY_TILT_NOTE.md +
// OUTPUT_LAYER_DISTRIBUTION_MAPPING.md "THE ONE USEFUL ATOM FEATURE".)
// DISPLAYED-(voice,lane) prototype: author-and-see, NOT the full
// all-lanes/all-voices/CV-out/modulation/coupling plumbing.
//
// THREE PURE, STATELESS STAGES that compose over ANY member quantile (the palette-wide
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
//   STAGE 2 — ZERO-DENSITY GAP (PHASE_DISTRIBUTION_SPEC §4): an interval (a, b) in TILTED
//     output coordinates, 0 ≤ a ≤ b ≤ 1, whose probability mass p = F_t(b) − F_t(a) is
//     REMOVED and rescaled uniformly by 1/(1−p) — "mass is scaled uniformly, so neither
//     side is favoured, and the normalised density outside the gap is unchanged". Closed
//     forms (F_a = F_t(a), F_b = F_t(b)):
//       G(x)   = F_t(x)/(1−p)          for x < a
//              = F_a/(1−p)             for a ≤ x < b   (flat: zero density)
//              = (F_t(x)−p)/(1−p)      for x ≥ b
//       Q_G(v) = Q_t((1−p)·v)          for v ≤ F_a/(1−p)
//              = Q_t((1−p)·v + p)      for v >  F_a/(1−p)   (the jump a→b IS the gap)
//     Checks (spec): G continuous at a and b, G(1) = 1, F_a/(1−p) ≤ 1. CLAMP (spec):
//     p ≤ 1 − ε with ε = kGapEps — the clamp is on the MASS, never on the width, so
//     b = 1 stays legal whenever F_a > 0; a degenerate p ≈ 1 lands on the clamped mass
//     (valid quantiles, no NaN — spec test 3). WIDTH ZERO IS NEUTRAL: a = b gives p = 0
//     and the stage returns F_t / Q_t through its own early-outs — an implemented
//     early-out, never a reliance on the arithmetic. An INVERTED window (b < a, modulator
//     overshoot) is neutral by the same p ≤ 0 branch.
//
//   STAGE 3 — INJECTED ATOM, FREE LOCATION (INSERT/DISPLACE — the note's leaned default):
//     the GAPPED tilted member G plus a Dirac of mass m at level ℓ (spec §5 — the atom is
//     applied AFTER the gap, so an atom inside a gap survives and is not swept to an edge):
//       H(x) = (1-m)·G(x) + m·1[x ≥ ℓ]
//     whose quantile (derived, verified against the prompt's contract):
//       q0 = G(ℓ)·(1-m)
//       u < q0        → Q_G(u/(1-m))            (the continuous part below ℓ, rescaled)
//       u ∈ [q0,q0+m] → exactly ℓ             (the atom: interval width m BY CONSTRUCTION)
//       u > q0+m      → Q_G((u-m)/(1-m))      (the continuous part above ℓ, rescaled)
//     CONTRACT (each pinned by test_warp_stages / test_phase_distribution):
//       · MONOTONE — each branch is Q_G∘rescale (monotone), the atom is a flat at ℓ, and
//         the pieces meet in order.
//       · MASS — P(output == ℓ) == m exactly: the u-interval [q0, q0+m] has width m and
//         every u in it returns ℓ bit-exactly.
//       · EDGE CONTINUITY — at u=q0 the below-branch gives Q_G(G(ℓ)) = ℓ ✓ and at u=q0+m
//         the above-branch gives Q_G(G(ℓ)) = ℓ ✓: the quantile is CONTINUOUS in u (the
//         flat sits exactly where the two branches meet — no jump, no gap).
//       · SHAPE — the continuous parts keep the member's shape at total mass (1-m)
//         (insert/displace: the atom ADDS an anchor and pushes the rest out).
//     OVERLAP RULE (spec §5, accepted): if the atom plateau covers the gap's jump point
//     v* = F_a/(1−p), the plateau wins and the jump lands at the plateau edge — the
//     consequence of the user using both features. The formulas above already do this:
//     the plateau branch's width-m interval simply contains v*.
//
// NEUTRALITY (the house neutral-parameter rule): p == 0 MUST be bit-identity (R_0 is
// arithmetically t/1 — but the contract is implemented as an EARLY-OUT, never a reliance
// on the arithmetic), m == 0 MUST be bit-identity likewise, and a zero-width gap MUST be
// bit-identity (the gap stage's own p == 0 early-out). A cell with p=0, m=0 AND a zero-
// width gap evaluates the member quantile UNTOUCHED — existing patches and every pinned
// bit-identity bridge (test_resolver_warp_bridge, test_warp_f_bank, ...) stay green
// UNCHANGED.
//
// AUTHORING CLAMPS (eval-side, the clampR/CONC_MAX precedent): p is clamped into
// [-kTiltMax, +kTiltMax] BEFORE any R_p evaluation (|p| ≥ 1 can never reach the hot path
// — modulation overshoot and hand-edited JSON included), m into [0, kAtomMassMax]
// (M_MAX keeps the continuous part alive; m=1 would collapse to the pure atom), and the
// gap's (location, size) into a = clamp01(loc), b = clamp01(loc + size) — the WINDOW is
// clamped, the MASS is clamped separately at 1−ε (the spec's own wording: the clamp is on
// the mass p, not on the output width b − a). ℓ is clamped into [0,1] (the corners are
// legal — a rail atom — but the default is interior).
//
// Pure module: no Rack/nanovg/UI/jansson — plain double arithmetic only, so the stage
// functions are directly unit-testable (test_warp_stages.cpp, test_phase_distribution.cpp)
// and cost a few compares + rescales per draw on the warp rebuild path (never per audio
// sample).
//
// COMPOSITION ORDER (the spec's ruling, §3): member → tilt → gap → atom, FIXED — the gap
// lives in tilted coordinates, and the atom is applied to the GAPPED member so an atom
// inside a gap survives (the spike sits at ℓ in the zero-density interval, splitting it
// into the two flats (a,ℓ) and (ℓ,b)).

#include <cmath>

namespace dotModular::warpStages {

// ── constants ──────────────────────────────────────────────────────────────────────
constexpr double kTiltMax          = 0.95;   // P_MAX (PROVISIONAL — the note's feel-test recipe refines)
constexpr double kAtomMassMax      = 0.9;    // M_MAX: the continuous part stays alive
constexpr double kAtomLevelDefault = 0.5;    // interior default (0/1 = rail atom, allowed)
constexpr double kGapEps           = 1e-6;   // ε — the gap-mass clamp, p ≤ 1−ε (spec §4/§9: "set by test")
constexpr double kGapLocDefault    = 0.5;    // gap location default (interior; INERT at size 0)
constexpr double kGapSizeDefault   = 0.0;    // gap size default — width zero IS the neutral body

// ── eval-side authoring clamps (max/min form — the resolver's NaN convention) ───────
inline double clampTilt(double p)   { return p < -kTiltMax ? -kTiltMax : (p > kTiltMax ? kTiltMax : p); }
inline double clampMass(double m)   { return m < 0.0 ? 0.0 : (m > kAtomMassMax ? kAtomMassMax : m); }
inline double clampLevel(double l)  { return l < 0.0 ? 0.0 : (l > 1.0 ? 1.0 : l); }
inline double clampGapLoc(double a) { return a < 0.0 ? 0.0 : (a > 1.0 ? 1.0 : a); }
// The gap BOUNDS from the authored (location, size): a = clamp01(loc), b = clamp01(loc+size)
// — monotone clamps on loc+size ≥ loc keep b ≥ a for size ≥ 0; a NEGATIVE size (modulator
// overshoot) inverts the window, and the mass clamp below neutralises it (p ≤ 0 → 0).
inline void   gapBounds(double loc, double size, double& a, double& b) {
    a = clampGapLoc(loc);
    b = clampGapLoc(loc + size);
}
// The gap's probability mass p = F_b − F_a under the spec clamp (p ≤ 1−ε); a non-positive
// mass (zero width, or an inverted/negative-size window) is the NEUTRAL 0.
inline double clampedGapMass(double Fa, double Fb) {
    double p = Fb - Fa;
    if (!(p > 0.0)) return 0.0;                 // NaN-safe: !(p>0) catches NaN too
    const double cap = 1.0 - kGapEps;
    return p > cap ? cap : p;
}

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

// ── STAGE 2: the gap, in tilted output coordinates (spec §4 closed forms) ──────────
// G(x): the gapped TILTED member CDF. cdfT is the tilted member's CDF F_t (ONE call —
// the callers pass F(R_p⁻¹(·)) composed). The p==0 early-out returns cdfT(x) UNTOUCHED
// (width zero / inverted window / a gap sitting on a member flat — all bit-neutral).
template <class C>
inline double gapCdf(double x, double a, double b, C cdfT) {
    const double Fa = cdfT(a);
    const double p  = clampedGapMass(Fa, cdfT(b));
    if (p <= 0.0) return cdfT(x);               // neutral: no gap (early-out, bit-exact)
    if (x < a)    return cdfT(x) / (1.0 - p);
    if (x < b)    return Fa / (1.0 - p);        // the flat: zero density on (a, b)
    return (cdfT(x) - p) / (1.0 - p);
}

// Q_G(v): the gapped TILTED member quantile. qT/cdfT are the tilted member's quantile
// and CDF. The branch point v* = F_a/(1−p): below it the lower mass share (rescaled),
// above it the upper share — Q_G JUMPS from a to b at v*, and that jump IS the gap.
template <class Q, class C>
inline double gapQuantile(double v, double a, double b, Q qT, C cdfT) {
    const double Fa = cdfT(a);
    const double p  = clampedGapMass(Fa, cdfT(b));
    if (p <= 0.0) return qT(v);                 // neutral: no gap (early-out, bit-exact)
    const double vStar = Fa / (1.0 - p);
    if (v <= vStar) return qT((1.0 - p) * v);
    return qT((1.0 - p) * v + p);
}

// ── THE COMPOSED QUANTILE (member → tilt → gap → atom) ─────────────────────────────
// qMember(u): the member's quantile (double→double, monotone); cdfMember(x): its CDF.
// p/loc/size/ell/m are RAW authoring values — clamped INSIDE (eval-side). The neutral
// early-outs are BIT-EXACT: no atom AND no gap AND p==0 returns the member call
// untouched; single-stage neutrals keep their exact branch forms (the prompt's mandate
// — never rely on R_0 ≡ id arithmetic, never rely on a zero-width gap's arithmetic).
template <class Q, class C>
inline double invCDF(double u, double p, double loc, double size, double ell, double m,
                     Q qMember, C cdfMember) {
    p   = clampTilt(p);
    ell = clampLevel(ell);
    double a, b;
    gapBounds(loc, size, a, b);
    // The gap's neutral test WITHOUT touching the member: width zero (or inverted) has
    // p = 0 for EVERY member — the cheap store-level early-out the bit-identity pins ride.
    const bool gapOn = (b > a);
    if (clampMass(m) <= 0.0 && !gapOn) {        // atom neutral AND gap neutral: tilt only
        if (p == 0.0) return qMember(u);        // p==0 early-out: bit-identity
        return tiltMap(qMember(u), p);
    }
    // The tilted member's pair (closed forms; p==0 keeps the exact member calls).
    auto qT  = [&](double v) { return p == 0.0 ? qMember(v) : tiltMap(qMember(v), p); };
    auto cT  = [&](double x) { return cdfMember(tiltMapInv(x, p)); };
    if (clampMass(m) <= 0.0)                    // gap only: the gapped tilted quantile
        return gapQuantile(u, a, b, qT, cT);
    // The atom's placement quantile: q0 = G(ℓ)·(1-m) with G the GAPPED tilted member's
    // CDF at the atom level (spec §5) — one member CDF eval + one R_p⁻¹ (+ the gap's two).
    const double G_ell = gapCdf(ell, a, b, cT);
    const double q0 = G_ell * (1.0 - m);
    if (u < q0)                                  // below ℓ: rescale into the gapped member's
        return gapQuantile(u / (1.0 - m), a, b, qT, cT);   // lower mass share
    if (u <= q0 + m)                             // the atom: width m BY CONSTRUCTION
        return ell;
    return gapQuantile((u - m) / (1.0 - m), a, b, qT, cT); // above ℓ: the upper mass share
}

// ── the composed CDF (display) ──────────────────────────────────────────────────────
// H(x) = (1-m)·G(x) + m·1[x ≥ ℓ], G the gapped tilted member (spec §5). The riser at ℓ
// (the jump by m) and the flat on (a,b) render through the widget's CDF sampling for free.
template <class C>
inline double cdf(double x, double p, double loc, double size, double ell, double m, C cdfMember) {
    p   = clampTilt(p);
    ell = clampLevel(ell);
    double a, b;
    gapBounds(loc, size, a, b);
    double h = gapCdf(x, a, b, [&](double xx) { return cdfMember(tiltMapInv(xx, p)); });
    if (clampMass(m) > 0.0) {
        h *= (1.0 - clampMass(m));
        if (x >= ell) h += clampMass(m);
    }
    return h;
}

// ── the composed PDF (display) — the CONTINUOUS part only ───────────────────────────
// h(x) = (1-m)·f_t(x)/(1-p) OUTSIDE the gap and 0 inside it (the uniform rescale: the
// normalised density outside the gap is unchanged — spec test 6 — the 1/(1−p) factor);
// for x ≠ ℓ the Dirac m·δ(x-ℓ) is NOT a density value — the caller
// (DistributionCurveWidget's atom bar) draws it EXACTLY as a finite bar at ℓ with height
// ∝ m. Needs the member CDF for the gap's mass p (the pdf alone cannot see it).
template <class D, class C>
inline double pdf(double x, double p, double loc, double size, double m, D pdfMember, C cdfMember) {
    p = clampTilt(p);
    double a, b;
    gapBounds(loc, size, a, b);
    double part = pdfMember(tiltMapInv(x, p)) * tiltMapInvSlope(x, p);
    const double cm = clampMass(m);
    if (b > a) {
        const double pGap = clampedGapMass(cdfMember(tiltMapInv(a, p)), cdfMember(tiltMapInv(b, p)));
        if (pGap > 0.0) {
            if (x >= a && x < b) return 0.0;    // the zero-density interval
            part /= (1.0 - pGap);               // the uniform rescale, outside the gap
        }
    }
    return cm > 0.0 ? part * (1.0 - cm) : part;
}

// ── the DUAL READING's display markers (PHASE_DISTRIBUTION_SPEC §6/§8.8) ────────────
// The dual reads the SAME chain with CDF and quantile exchanged, so the K-reading's gap
// (a,b) appears on the dual as an ATOM at level v* = F_a/(1−p) with mass (b−a) (the
// quantile's jump, read as a CDF riser), and the K-reading's atom (ℓ,m) appears as the
// dual's GAP on [q0, q0+m] with q0 = G(ℓ)·(1−m) (the quantile's plateau, read as a CDF
// flat). cdfT is the tilted member's CDF, gapCdfT the GAPPED tilted member's CDF. Used
// by the display hosts to point the widget's atom bar / gap band at what the DUAL shows
// (an atom on K is a gap on DK, and a gap on K is an atom on DK — Rodney's parity rule).
// The §5 overlap corner (the atom plateau covering v*) reads approximately through these
// markers — the MATH itself is exact via the composed formulas; only the cosmetic bar
// position can drift when both features fight over the same u.
template <class C, class G>
inline void dualMarkers(double a, double b, double ell, double m, C cdfT, G gapCdfT,
                        double& outLevel, double& outMass, double& outGapLo, double& outGapHi) {
    const double Fa = cdfT(a);
    const double p  = clampedGapMass(Fa, cdfT(b));
    outLevel = Fa / (1.0 - p);                  // the dual's atom level: the jump's x
    outMass  = (b > a) ? (b - a) : 0.0;         // the jump's size = the gap's width
    const double q0 = gapCdfT(ell) * (1.0 - clampMass(m));
    outGapLo = q0;
    outGapHi = q0 + clampMass(m);
}

// ── THE EFFECTIVE WARPED CURVE (PHASE_DISTRIBUTION_SPEC §10, display-only) ──────────
// The DISTRIBUTION plot's law: the plot shows the EFFECTIVE warped distribution at the
// current SIGNED warp depth w — NOT the bare target — so it tracks the knob continuously
// through zero (the reading flips at the sign, but k = |w| → 0 kills the difference: no
// jump). With k = |w| ∈ [0,1] and T = the composed READING's quantile (K reading: the
// composed quantile; DK reading: the composed CDF — the SIGN picked it), the warp mix
//     W(u) = (1−k)·u + k·T(u)
// is a monotone [0,1]→[0,1] self-map, and ONE sweep of u over [0,1] gives both drawn
// curves EXACTLY, closed form, no bisection and no integration:
//   · CDF trace — the PARAMETRIC point (W(u), u): P(W(U) ≤ W(u)) = P(U ≤ u) = u.
//   · PDF fill  — the output density at x = W(u), from W'(u) = (1−k) + k·T'(u) and
//                 T'(u) = 1/f_T(T(u)):
//         f_W = 1 / [ (1−k) + k / f_T(T(u)) ]
//     with the division GUARDED on the spec's two edges: f_T → 0 (a gap) gives
//     f_W → 0 for ANY k > 0 (the gap opens at once); f_T → ∞ (an atom) gives
//     f_W → 1/(1−k) (the spike fades in). The asymmetry is real, not a plot artefact.
// k = 0 is the uniform: W = u (the diagonal) and f_W = 1 EXACTLY — the 0/0 at k=0 is
// never evaluated. DISPLAY-only: the engine's warp mix never calls these; the plot
// recomputes on warp-depth or parameter change (the hosts' refresh), never per sample.
inline double warpEffectiveK(float w) {
    const double k = std::fabs((double)w);
    if (!(k == k)) return 0.0;              // NaN depth: the neutral diagonal
    return k > 1.0 ? 1.0 : k;               // the axis is [−1,+1]; overshoot clamps
}
// f_T evaluated AT the target point T(u) → the warped density f_W. The floor keeps the
// division finite for every input — the display density's own "this small is a hole"
// reading (Distribution.hpp's DIST_EDGE_EPS = 1e-9, mirrored here so this module keeps
// its no-Rack/no-deps purity; the two must move together). NaN rides the gap face to 0.
inline double warpEffectivePdf(double k, double fT) {
    if (k <= 0.0) return 1.0;                        // k = 0: the uniform, exactly
    if (std::isinf(fT)) return 1.0 / (1.0 - k);      // the atom face: the spike, finite
    if (!(fT >= 1e-9)) return 0.0;                   // the gap face (NaN too): f_W = 0
    return 1.0 / ((1.0 - k) + k / fT);               // the closed form; k < 1 ⇒ denom > 0
}
// THE DUAL READING's fill (CC_FIX_PHASE_PLOT_NEGATIVE_WARP): on the warp's negative
// side the reading is the DUAL, so the mix's T is the composed CDF g — a CDF, not a
// quantile — and T'(u) = g'(u), the composed member density AT u (NOT the reciprocal
// the quantile form divides by; feeding that in was the spec gap this helper retires).
// Same chain rule, the OTHER sign of the T' term:
//     f_W = 1 / [ (1−k) + k · g'(u) ]
// with the two faces FLIPPED against the quantile form, because the features exchange:
// g' → 0 over the K-reading's gap IS the dual's ATOM — a plateau at 1/(1−k) for
// k < 1 (the delta itself rides the ATOM BAR marker exactly like the quantile side's
// atom; at k = 1 exactly that limit IS the delta, so the fill opens the hole — 0, the
// marker's job — never inf, an inf sample would poison the fill's auto-max scale);
// g' → ∞ at the K-atom IS the dual's GAP — f_W → 0, the band marker's job. k = 0 is
// still the uniform f_W = 1, never dividing. DISPLAY FLOOR 1e-6 (the quantile form's
// 1e-9 does NOT carry over, and that is load-bearing): the member pdf's own edge
// proxy rides at O(coefficient·1e-9) (Kuma(2,3) returns 6e-9 at x = 0) — above the
// quantile form's floor it would slip into the closed form and at k = 1 the 1/g'
// reads 1.67e8 — an edge spike that eats the fill's auto-max whole (the doc's "not
// a spike that overflows the axis"). A density below 1e-6 is a hole at this plot's
// resolution anyway (200 samples over [0,1]); the DISPLAY-only floor reads it so.
// DISPLAY-only: the engine's dual mix never calls this.
inline double warpEffectivePdfDual(double k, double gPrime) {
    if (k <= 0.0) return 1.0;                        // k = 0: the uniform, exactly
    if (!(gPrime >= 1e-6))                           // the gap face (NaN too): the
        return k < 1.0 ? 1.0 / (1.0 - k)             //   dual's atom — the plateau,
                       : 0.0;                        //   the marker's delta at k = 1
    if (std::isinf(gPrime)) return 0.0;              // the atom face: the dual's gap
    return 1.0 / ((1.0 - k) + k * gPrime);           // the closed form; denom >= 1−k > 0
}

} // namespace dotModular::warpStages
