// test_phase_distribution.cpp — the PHASE_DISTRIBUTION_SPEC acceptance battery
// (docs/design/PHASE_DISTRIBUTION_SPEC.md — "Tests (must pass before the implementation
// is accepted): 1..8", Rodney-decided). Compile: see test/run_all.sh (header-only,
// rack-free — the stage math lives in WarpStages.hpp, the dual reading in WarpF.hpp).
//
// The eight spec tests, mapped 1:1, plus the generalized-dual pins (Rodney: "For each
// of our distributions ... add the dual ... Atom dual is a gap and vice versa"):
//   T1  G continuous, non-decreasing, G(1)=1 over a grid of (α, β, p, a, b).
//   T2  Width zero reproduces the untouched distribution BIT-EXACTLY (the early-outs,
//       member → tilt → atom included — the pre-gap pins keep their exact forms).
//   T3  a=0 / b=1 give valid quantiles; the ε clamp engages only when p ≈ 1.
//   T4  Round trip Q(G(x)) == x outside the gap (float tolerance).
//   T5  The quantile jump is at v* = F_a/(1−p), landing at a then b.
//   T6  The rescale is uniform: the density outside the gap is f_t/(1−p), 0 inside.
//   T7  An atom inside a gap sits at ℓ (splitting the gap); a plateau covering the
//       jump point wins (the §5 overlap rule).
//   T8  The DUAL reading — reached by the warp depth's NEGATIVE side (as amended:
//       "we go back to making only the original a menu choice. But we make the warp
//       knob bidirectional ... Negative blends uniform and the dual distribution"):
//       an atom on K is a gap on DK and a gap on K is an atom on DK, numerically, at
//       several parameter sets; the swap identity holds for EVERY palette member
//       (dual.cdf == member.invCDF, dual.invCDF == member.cdf), the bank reaches the
//       dual by the SIGN of w over a BARE member (the dual's quantile IS the composed
//       CDF at w = −1), and the dual envelope persists.
//   §10 The EFFECTIVE warped curve — the DISTRIBUTION plot's law (display-only):
//       W(u) = (1−k)u + k·T(u), k = |w|, T the composed READING's target (K: the
//       composed quantile; DK: the composed CDF — the SIGN picked it); the CDF
//       trace the PARAMETRIC point (W(u), u), the fill the READING's guarded
//       closed form — K: f_W = 1/[(1−k) + k/f_T(T(u))], DK: f_W = 1/[(1−k) + k·g'(u)]
//       (CC_FIX_PHASE_PLOT_NEGATIVE_WARP: the dual's T is a CDF, so T' = g'(u),
//       the composed member density AT u — NOT the reciprocal). Gates: k=0 the
//       diagonal with f_W = 1; continuous through w = 0 (both readings O(k) off
//       the diagonal); W monotone; no inf/NaN over the (member, k, reading,
//       feature) grid; the guarded faces (K: gap → 0 for any k > 0, atom →
//       1/(1−k); DK: the exchange flipped — g' → 0 the 1/(1−k) plateau, g' → ∞
//       the hole; the uniform never divides).
//
// Header-only, Rack-free (jansson via the test/jansson.h shim); registered in
// test/run_all.sh.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "WarpF.hpp"                    // the bank + the member wrappers + the dual
#include "SandsModifierResolver.hpp"    // the resolver + the bank's composed mix

using dotModular::WarpBank;
using dotModular::WarpF;
namespace ws = dotModular::warpStages;

static int g_pass = 0, g_fail = 0;
static void ok(bool cond, const char* what) {
    if (cond) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static bool sameBits(double a, double b) {
    uint64_t ra, rb;
    std::memcpy(&ra, &a, sizeof ra);
    std::memcpy(&rb, &b, sizeof rb);
    return ra == rb;
}

int main() {
    WarpBank bank;   // owns the drawn-curve pool the roster references
    const dotModular::MonotoneCurve cA =
        dotModular::MonotoneCurve::fromDistribution(dotModular::KumaraswamyDistribution(2, 3), 9);
    const int p0 = bank.putCurve(cA);
    const dotModular::MonotoneCurve* pool = bank.pool;

    // The member roster: every palette alternative (the generalized dual's sweep) —
    // the same shape test_warp_stages uses.
    struct Member { const char* name; WarpF f; bool strict; };
    const Member roster[] = {
        { "identity",     dotModular::IdentityWarp{},                 true  },
        { "triangular",   dotModular::TriangularWarp{0.3},            true  },
        { "power5",       dotModular::PowerWarp{5.0},                 true  },
        { "revpow5",      dotModular::ReversedPowerWarp{5.0},         true  },
        { "kuma-steep",   dotModular::KumaraswamyWarp{5.0, 25.8},     true  },
        { "kuma-2-3",     dotModular::KumaraswamyWarp{2.0, 3.0},      true  },
        { "mobius4",      dotModular::MobiusWarp{4.0},                true  },
        { "logit-sig",    dotModular::LogitSigmoidWarp{2.0, -0.5},    true  },
        { "logit-norm",   dotModular::LogitNormalWarp{0.5, 0.4},      true  },
        { "trunc-norm",   dotModular::TruncNormalWarp{0.5, 0.2},      true  },
        { "drawn-A",      dotModular::DrawnWarp{(uint16_t)p0},        true  },
    };
    const int R = (int)(sizeof roster / sizeof roster[0]);

    // The member pair lambdas (the same shape warpedValue composes over).
    auto mQ = [&](const WarpF& f, double u) { return dotModular::invCDF(f, u, pool); };
    auto mC = [&](const WarpF& f, double x) { return dotModular::cdf(f, x, pool); };
    auto mP = [&](const WarpF& f, double x) { return dotModular::pdf(f, x, pool); };
    // The composed K-reading pieces over a member (gap args = authored loc/size).
    auto Kq = [&](const WarpF& f, double u, double p, double loc, double size, double ell, double m) {
        return ws::invCDF(u, p, loc, size, ell, m,
                          [&](double uu) { return mQ(f, uu); },
                          [&](double xx) { return mC(f, xx); });
    };
    auto Kc = [&](const WarpF& f, double x, double p, double loc, double size, double ell, double m) {
        return ws::cdf(x, p, loc, size, ell, m, [&](double xx) { return mC(f, xx); });
    };
    auto Kp = [&](const WarpF& f, double x, double p, double loc, double size, double m) {
        return ws::pdf(x, p, loc, size, m,
                       [&](double xx) { return mP(f, xx); },
                       [&](double xx) { return mC(f, xx); });
    };

    std::printf("phase distribution (gap + dual): %d roster members\n", R);

    // ── T1: G continuous, non-decreasing, G(1) = 1 over a grid of (α, β, p, a, b) ──
    {
        const double alphas[] = {0.5, 1.0, 2.0, 5.0};
        const double betas[]  = {0.5, 1.0, 3.0};
        const double ps[]     = {-0.8, 0.0, 0.8};
        struct Win { double loc, size; };
        const Win wins[] = {{0.2, 0.3}, {0.0, 0.3}, {0.5, 0.5}, {0.3, 0.0}, {0.1, 0.05}};
        bool mono = true, g1 = true, cont = true;
        for (double al : alphas) for (double be : betas) {
            const WarpF f = dotModular::KumaraswamyWarp{al, be};
            for (double p : ps) for (const Win& w : wins) {
                double prev = -1e18; bool first = true;
                for (int i = 0; i <= 2000; ++i) {
                    const double x = (double)i / 2000.0;
                    const double g = Kc(f, x, p, w.loc, w.size, 0.5, 0.0);
                    if (!first && g < prev - 1e-15) mono = false;   // non-decreasing
                    prev = g; first = false;
                }
                if (std::fabs(Kc(f, 1.0, p, w.loc, w.size, 0.5, 0.0) - 1.0) > 1e-12) g1 = false;
                // continuity at the window edges (the LEFT limit meets the value)
                const double a = ws::clampGapLoc(w.loc), b = ws::clampGapLoc(w.loc + w.size);
                const double eps = 1e-9;
                if (a > 0.0 && a > eps) {
                    const double gl = Kc(f, a - eps, p, w.loc, w.size, 0.5, 0.0);
                    const double gr = Kc(f, a,       p, w.loc, w.size, 0.5, 0.0);
                    if (std::fabs(gl - gr) > 1e-6) cont = false;
                }
                if (b < 1.0) {
                    const double gl = Kc(f, b - eps, p, w.loc, w.size, 0.5, 0.0);
                    const double gr = Kc(f, b,       p, w.loc, w.size, 0.5, 0.0);
                    // the clamp's leftover jump (p clamped BELOW the true mass) is the
                    // documented degenerate — the grid keeps p ≤ 1−ε comfortably, so
                    // continuity must hold here
                    if (std::fabs(gl - gr) > 1e-6) cont = false;
                }
            }
        }
        ok(mono, "T1: G non-decreasing over the (alpha,beta,p,a,b) grid (2001-pt sweeps)");
        ok(g1,   "T1: G(1) == 1 across the grid (1e-12)");
        ok(cont, "T1: G continuous at the window edges across the grid (1e-6 at 1e-9 off-edge)");
    }

    // ── T2: width zero reproduces the untouched distribution BIT-EXACTLY ────────────
    {
        // (i) the STAGE level: composed(loc=0.5, size=0) == the pre-gap composition,
        // bit-for-bit, for every roster member × tilt × atom (the pre-gap form is
        // reproduced inline from the pinned tilt/atom formulas).
        bool all = true;
        for (int r = 0; r < R && all; ++r) {
            const WarpF& f = roster[r].f;
            for (double p : {0.0, 0.4, -0.6}) {
                for (double m : {0.0, 0.3}) {
                    for (int i = 0; i <= 500 && all; ++i) {
                        const double u = (double)i / 500.0;
                        const double got = Kq(f, u, p, 0.5, 0.0, 0.5, m);
                        // the PRE-GAP reference (the pinned member → tilt → atom chain):
                        const double pc = ws::clampTilt(p), mc = ws::clampMass(m);
                        double want;
                        if (mc <= 0.0) want = pc == 0.0 ? mQ(f, u) : ws::tiltMap(mQ(f, u), pc);
                        else {
                            const double q0 = mC(f, ws::tiltMapInv(0.5, pc)) * (1.0 - mc);
                            if (u < q0)          want = pc == 0.0 ? mQ(f, u / (1.0 - mc)) : ws::tiltMap(mQ(f, u / (1.0 - mc)), pc);
                            else if (u <= q0 + mc) want = 0.5;
                            else                 want = pc == 0.0 ? mQ(f, (u - mc) / (1.0 - mc)) : ws::tiltMap(mQ(f, (u - mc) / (1.0 - mc)), pc);
                        }
                        if (!sameBits(got, want)) {
                            std::printf("  (T2) broke on %s p=%g m=%g u=%g\n", roster[r].name, p, m, u);
                            all = false;
                        }
                    }
                }
            }
        }
        ok(all, "T2: width-zero gap == the pre-gap composition BIT-EXACT (roster x tilt x atom)");
        // (ii) the BANK level: a size-0 store leaves warpedValue on the pre-gap path —
        // the neutral early-out now carries the gapSize==0 term.
        {
            WarpBank b;
            const int i0 = WarpBank::index(2, 3);
            b.warpF[i0] = dotModular::KumaraswamyWarp{2.0, 3.0};
            b.warpW[i0] = 1.f; b.warpTilt[i0] = 0.3f; b.warpAtomMass[i0] = 0.25f;
            b.warpGapLoc[i0] = 0.5f;   // the authored default; SIZE 0 = neutral
            bool bankOK = true;
            for (int k = 0; k <= 500; ++k) {
                const double u = (double)k / 500.0;
                const double pc = (double)b.warpTilt[i0], mc = (double)b.warpAtomMass[i0];
                const double q0 = mC(b.warpF[i0], ws::tiltMapInv(0.5, pc)) * (1.0 - mc);
                double want;
                if (u < q0)            want = ws::tiltMap(mQ(b.warpF[i0], u / (1.0 - mc)), pc);
                else if (u <= q0 + mc) want = 0.5;
                else                   want = ws::tiltMap(mQ(b.warpF[i0], (u - mc) / (1.0 - mc)), pc);
                if (!sameBits(b.warpedValue(2, 3, u, false), want)) bankOK = false;
            }
            ok(bankOK, "T2: bank with gapSize==0 composes the pre-gap chain BIT-EXACT (stages live)");
        }
    }

    // ── T3: a=0 / b=1 give valid quantiles; the clamp engages only when p ≈ 1 ──────
    {
        bool valid = true, clampOnly = true;
        const double ps[] = {-0.8, 0.0, 0.8};
        for (double p : ps) {
            // the full-span window (0,1) on a mid member: every quantile finite in [0,1]
            const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
            for (int i = 0; i <= 2000; ++i) {
                const double u = (double)i / 2000.0;
                const double q = Kq(f, u, p, 0.0, 1.0, 0.5, 0.0);
                if (!(std::isfinite(q) && q >= 0.0 && q <= 1.0)) valid = false;
            }
            // the mass clamp itself: mid gaps pass through EXACTLY; only p ≈ 1 clamps
            // (IEEE-exact halves — 0.75−0.25 and 0.5−0.0 are both exactly 0.5)
            if (ws::clampedGapMass(0.25, 0.75) != 0.5) clampOnly = false;
            if (ws::clampedGapMass(0.0, 0.5) != 0.5) clampOnly = false;
            if (!(ws::clampedGapMass(0.0, 1.0) == 1.0 - ws::kGapEps)) clampOnly = false;
            if (!(ws::clampedGapMass(1e-9, 1.0) == 1.0 - ws::kGapEps)) clampOnly = false;
            if (ws::clampedGapMass(0.999, 1.0) != 0.0010000000000000009) clampOnly = false;
            // and the clamped window still yields finite quantiles (the degenerate face)
            for (int i = 0; i <= 2000; ++i) {
                const double u = (double)i / 2000.0;
                const double q = Kq(f, u, p, 0.0, 1.0, 0.5, 0.0);
                if (!std::isfinite(q)) valid = false;
            }
        }
        ok(valid,     "T3: a=0,b=1 quantiles finite in [0,1] across tilts (the clamp's degenerate included)");
        ok(clampOnly, "T3: the mass clamp passes mid gaps exactly and caps only p≈1 at 1-eps");
    }

    // ── T4: round trip Q(G(x)) == x for x outside the gap ──────────────────────────
    {
        bool rt = true;
        const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
        const double a = 0.25, b = 0.55;   // the window under test
        for (double p : {0.0, 0.5, -0.5}) {
            for (int i = 0; i <= 2000; ++i) {
                const double x = (double)i / 2000.0;
                // the round trip holds on [0,a] ∪ (b,1]: the CDF is FLAT on the closed
                // [a,b] (G(b) = F_a/(1−p) = v*, and Q(v*) = a — the removed mass is
                // unreachable by construction; b itself is only a limit point)
                if (x > a && x <= b) continue;
                const double g = Kc(f, x, p, a, b - a, 0.5, 0.0);
                const double q = Kq(f, g, p, a, b - a, 0.5, 0.0);
                if (std::fabs(q - x) > 1e-9) { rt = false; std::printf("  (T4) x=%g -> %g (p=%g)\n", x, q, p); }
            }
        }
        ok(rt, "T4: Q(G(x)) == x outside the closed gap, 1e-9 (the flat is unreachable by design)");
    }

    // ── T5: the jump in Q_G is at v* = F_a/(1−p), landing at a then b ──────────────
    {
        bool at = true, land = true;
        const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
        for (double p : {0.0, 0.5, -0.5}) {
            const double a = 0.3, b = 0.6;
            // the tilted member's CDF/quantile pair (the gap lives in TILTED coords)
            auto cT = [&](double x) { return mC(f, ws::tiltMapInv(x, ws::clampTilt(p))); };
            auto qT = [&](double v) { return ws::tiltMap(mQ(f, v), ws::clampTilt(p)); };
            const double Fa = cT(a), pr = ws::clampedGapMass(Fa, cT(b));
            const double vStar = Fa / (1.0 - pr);
            const double eps = 1e-12;
            const double ql = ws::gapQuantile(vStar - eps, a, b, qT, cT);
            const double qr = ws::gapQuantile(vStar + eps, a, b, qT, cT);
            if (std::fabs(ql - a) > 1e-9) at = false;    // lands at a from below
            if (std::fabs(qr - b) > 1e-9) land = false;  // jumps to b above
        }
        ok(at,   "T5: Q_G(v*-eps) == a (the jump's left landing)");
        ok(land, "T5: Q_G(v*+eps) == b (the jump lands at b — the gap IS the jump)");
    }

    // ── T6: the rescale is uniform — density outside the gap is f_t/(1−p), 0 inside ─
    {
        bool ratio = true, hole = true;
        const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
        for (double p : {0.0, 0.5, -0.5}) {
            const double a = 0.25, b = 0.55;
            auto cT = [&](double x) { return mC(f, ws::tiltMapInv(x, ws::clampTilt(p))); };
            const double pr = ws::clampedGapMass(cT(a), cT(b));
            for (int i = 0; i <= 400; ++i) {
                const double x = (double)i / 400.0;
                if (x >= a && x < b) {
                    if (Kp(f, x, p, a, b - a, 0.0) != 0.0) hole = false;   // the zero-density interval
                    continue;
                }
                const double withGap = Kp(f, x, p, a, b - a, 0.0);
                const double bare    = mP(f, ws::tiltMapInv(x, ws::clampTilt(p)))
                                     * ws::tiltMapInvSlope(x, ws::clampTilt(p));
                if (std::fabs(withGap - bare / (1.0 - pr)) > 1e-9 * (1.0 + bare)) ratio = false;
            }
        }
        ok(hole,  "T6: the density is EXACTLY 0 inside the gap");
        ok(ratio, "T6: the density outside is f_t(x)/(1-p) — the UNIFORM rescale (neither side favoured)");
    }

    // ── T7: an atom inside a gap sits at ℓ; a plateau covering the jump point wins ──
    {
        bool sits = true, splits = true, wins = true;
        const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
        const double a = 0.3, b = 0.7, p = 0.0;
        const double ell = 0.5, m = 0.2;   // ℓ inside (a,b)
        auto cT = [&](double x) { return mC(f, x); };
        const double G_ell = ws::gapCdf(ell, a, b, cT);
        const double q0 = G_ell * (1.0 - m);
        // (i) the plateau IS [q0, q0+m] at ℓ: every u in it returns ℓ BIT-EXACTLY
        for (int i = 0; i <= 400; ++i) {
            const double u = q0 + (m * (double)i / 400.0);
            if (Kq(f, u, p, a, b - a, ell, m) != ell) sits = false;
        }
        // (ii) the atom SPLITS the gap: the quantile below q0 lands ≤ a, above q0+m ≥ b —
        // it never visits (a,ℓ) or (ℓ,b)
        for (int i = 1; i < 1000; ++i) {
            const double u = q0 * (double)i / 1000.0;
            if (Kq(f, u, p, a, b - a, ell, m) > a + 1e-9) splits = false;
            const double u2 = q0 + m + (1.0 - q0 - m) * (double)i / 1000.0;
            if (Kq(f, u2, p, a, b - a, ell, m) < b - 1e-9) splits = false;
        }
        // (iii) the OVERLAP RULE: the plateau covers the jump point v* → Q(v*) == ℓ
        // (the plateau wins; the jump lands at the plateau edges). v* = F_a/(1−p) —
        // which IS gapCdf(a) (the flat value); an atom with ℓ ∈ [a,b) has
        // G(ℓ) = v*, so q0 = v*·(1−m) and the plateau [q0, q0+m] ALWAYS covers v*.
        {
            const double vStar = ws::gapCdf(a, a, b, cT);   // = Fa/(1−p), the flat value
            if (!(q0 <= vStar && vStar <= q0 + m)) wins = false;   // this setup must overlap
            if (Kq(f, vStar, p, a, b - a, ell, m) != ell) wins = false;
        }
        ok(sits,   "T7: the atom inside the gap sits at ℓ BIT-EXACTLY across [q0, q0+m]");
        ok(splits, "T7: the atom splits the gap — below-branch lands ≤ a, above-branch ≥ b");
        ok(wins,   "T7: the plateau covers the jump point and WINS (Q(v*) == ℓ, the §5 rule)");
    }

    // ── T8: the DUAL reading — atom↔gap exchange, the swap identity, the bank path ──
    {
        // (0) THE SWAP IDENTITY for EVERY member (Rodney: "apply to each"): the dual's
        // quantile IS the member's CDF and vice versa — bit-exact through the visitors.
        bool swap = true;
        for (int r = 0; r < R; ++r) {
            const WarpF dk = dotModular::dualWrap(roster[r].f);
            if (!dotModular::isDualWarp(dk)) {
                // the identity is REFUSED (its dual is itself) — the wrap must be a
                // no-op, not a second uniform entry
                if (dk != roster[r].f) swap = false;
                continue;
            }
            for (int i = 0; i <= 200; ++i) {
                const double x = (double)i / 200.0;
                if (!sameBits(dotModular::invCDF(dk, x, pool), mC(roster[r].f, x))) swap = false;
                if (!sameBits(dotModular::cdf(dk, x, pool),    mQ(roster[r].f, x))) swap = false;
            }
        }
        ok(swap, "T8: dual.cdf == member.invCDF and dual.invCDF == member.cdf BIT-EXACT (whole roster)");
        // (1) the DUAL OF THE UNIFORM IS THE UNIFORM (no second identity entry)
        {
            const WarpF wrapped = dotModular::dualWrap(dotModular::IdentityWarp{});
            const WarpF plain{dotModular::IdentityWarp{}};
            if (wrapped != plain)
                ok(false, "T8: dualWrap refuses the identity (the uniform is its own dual)");
            else ++g_pass;
        }
        // (2) atom → gap: K's atom (ℓ,m) reads as DK's flat on [q0, q0+m] — the DK's
        // CDF (= K's composed quantile) is CONSTANT there, at ℓ. Several parameter sets.
        bool a2g = true;
        struct AtomSet { double ell, m; };
        const AtomSet asets[] = {{0.5, 0.2}, {0.3, 0.45}, {0.7, 0.1}};
        const WarpF asetMembers[] = {dotModular::KumaraswamyWarp{2.0, 3.0},
                                     dotModular::PowerWarp{2.0},
                                     dotModular::TriangularWarp{0.5}};
        for (const WarpF& f : asetMembers) {
            for (double p : {0.0, 0.4}) {
                for (const AtomSet& s : asets) {
                    // K's composed quantile == the DK's CDF (the reading swap). The
                    // placement quantile q0 = G(ℓ)·(1−m) uses the PRE-ATOM gapped CDF
                    // G (spec §5) — with the gap neutral here, G(ℓ) = F_t(ℓ).
                    auto DKcdf = [&](double x) { return Kq(f, x, p, 0.5, 0.0, s.ell, s.m); };
                    const double G_ell = mC(f, ws::tiltMapInv(s.ell, ws::clampTilt(p)));
                    const double q0 = G_ell * (1.0 - s.m);
                    const double c1 = DKcdf(q0 + 0.1 * s.m);
                    const double c2 = DKcdf(q0 + 0.5 * s.m);
                    const double c3 = DKcdf(q0 + 0.9 * s.m);
                    if (!(c1 == s.ell && c2 == s.ell && c3 == s.ell)) a2g = false;
                }
            }
        }
        ok(a2g, "T8: an atom on K reads as a GAP on DK — the DK cdf is flat at ℓ across [q0,q0+m]");
        // (3) gap → atom: K's gap (a,b) reads as DK's atom at v* = F_a/(1−p) of mass
        // (b−a): the DK's CDF (= K's composed quantile) JUMPS by (b−a) at v*, and the
        // marker helpers report exactly that pair.
        bool g2a = true, markers = true;
        for (double p : {0.0, 0.4, -0.4}) {
            const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
            const double a = 0.3, b = 0.6;
            auto cT = [&](double x) { return mC(f, ws::tiltMapInv(x, ws::clampTilt(p))); };
            auto qT = [&](double v) { return ws::tiltMap(mQ(f, v), ws::clampTilt(p)); };
            const double pr = ws::clampedGapMass(cT(a), cT(b));
            const double vStar = cT(a) / (1.0 - pr);
            const double eps = 1e-12;
            const double lo = ws::gapQuantile(vStar - eps, a, b, qT, cT);
            const double hi = ws::gapQuantile(vStar + eps, a, b, qT, cT);
            if (!(std::fabs(lo - a) < 1e-9 && std::fabs(hi - b) < 1e-9
                  && std::fabs((hi - lo) - (b - a)) < 1e-9)) g2a = false;
            double lv, mv, glo, ghi;
            ws::dualMarkers(a, b, 0.5, 0.0, cT,
                            [&](double x) { return ws::gapCdf(x, a, b, cT); }, lv, mv, glo, ghi);
            if (!(std::fabs(lv - vStar) < 1e-9 && std::fabs(mv - (b - a)) < 1e-9)) markers = false;
        }
        ok(g2a,     "T8: a gap on K reads as an ATOM on DK — the DK cdf jumps by (b-a) at v*");
        ok(markers, "T8: dualMarkers report the dual's atom (level v*, mass b-a) exactly");
        // (4) the BANK reaches the dual by the SIGN: a BARE member cell at w=−1 ==
        //     warpStages::cdf(u) over that member — bit-exact — and with the gap live,
        //     the SAME gap params drive both readings (one store, two readings, one
        //     sign apart); w=+1 stays the composed QUANTILE (the reading the menu
        //     authors).
        {
            WarpBank b;
            const int i0 = WarpBank::index(1, 4);
            b.warpF[i0] = dotModular::PowerWarp{2.0};   // BARE member — the sign does the reading
            b.warpTilt[i0] = 0.2f; b.warpGapLoc[i0] = 0.35f; b.warpGapSize[i0] = 0.2f;
            const WarpF member = dotModular::PowerWarp{2.0};
            bool bankDual = true, bankPlain = true;
            b.warpW[i0] = -1.f;
            for (int k = 0; k <= 400; ++k) {
                const double u = (double)k / 400.0;
                const double pc = (double)b.warpTilt[i0], lc = (double)b.warpGapLoc[i0],
                             sc = (double)b.warpGapSize[i0];
                const double want = ws::cdf(u, pc, lc, sc, 0.5, 0.0,
                                            [&](double xx) { return mC(member, xx); });
                if (!sameBits(b.warpedValue(1, 4, u, false), want)) bankDual = false;
            }
            ok(bankDual, "T8: the bank's cell at w=-1 == the composed CDF AT u BIT-EXACT (gap live)");
            b.warpW[i0] = 1.f;
            for (int k = 0; k <= 400; ++k) {
                const double u = (double)k / 400.0;
                const double pc = (double)b.warpTilt[i0], lc = (double)b.warpGapLoc[i0],
                             sc = (double)b.warpGapSize[i0];
                const double want = ws::invCDF(u, pc, lc, sc, 0.5, 0.0,
                                               [&](double uu) { return mQ(member, uu); },
                                               [&](double xx) { return mC(member, xx); });
                if (!sameBits(b.warpedValue(1, 4, u, false), want)) bankPlain = false;
            }
            ok(bankPlain, "T8: the same cell at w=+1 == the composed QUANTILE BIT-EXACT (one sign apart)");
        }
        // (5) the dual envelope PERSISTS at the codec level (the legacy writer's form —
        //     the bank's parse migrates it, pinned in test_warp_f_bank S8c).
        {
            WarpF dk = dotModular::dualWrap(dotModular::KumaraswamyWarp{2.0, 3.0});
            json_t* j = dotModular::warpToJson(dk);
            WarpF back = dotModular::warpFromJson(j);
            json_decref(j);
            ok(back == dk, "T8: the dual round-trips the slot JSON codec (type+member exact)");
            // the drawn-member dual round-trips its POOL REFERENCE
            WarpF dd = dotModular::dualWrap(dotModular::DrawnWarp{(uint16_t)p0});
            json_t* j2 = dotModular::warpToJson(dd);
            WarpF back2 = dotModular::warpFromJson(j2);
            json_decref(j2);
            ok(back2 == dd, "T8: a dual-drawn round-trips (the pool ref rides the member)");
            // the dual PDF is finite everywhere (the house proxy at the member's holes)
            bool pdfFin = true;
            for (int i = 0; i <= 400; ++i) {
                const double x = (double)i / 400.0;
                if (!std::isfinite(dotModular::pdf(dk, x, pool))) pdfFin = false;
            }
            ok(pdfFin, "T8: the dual's reciprocal density is finite across [0,1] (the edge proxy)");
        }
        // (6) THE EXCHANGE AT THE BANK LEVEL (spec test 8's parenthetical — "reached at
        //     the bank level by w = −1 over the bare member"): a BARE-member cell with a
        //     LIVE ATOM, gap neutral — at w=−1 the bank's reading (the dual's quantile =
        //     the composed CDF H) JUMPS the gap [q0, q0+m] at u = ℓ (the atom IS the
        //     dual's gap — the jump spans exactly dualMarkers' [q0, q0+m]); at w=+1 the
        //     SAME cell reads the atom plateau AT ℓ (bit-exact). One store, one sign
        //     apart — nothing re-authored.
        {
            WarpBank b;
            const int i0 = WarpBank::index(3, 1);
            b.warpF[i0] = dotModular::TriangularWarp{0.5};   // BARE member — the sign does the reading
            b.warpTilt[i0] = 0.f;                            // clean q0 arithmetic
            b.warpAtomLevel[i0] = 0.5f; b.warpAtomMass[i0] = 0.3f;
            const WarpF member = dotModular::TriangularWarp{0.5};
            // THE CELL'S OWN ARITHMETIC: the store is FLOAT — the reference mass is the
            // float-roundtripped value the engine reads, never the literal 0.3
            const double mCell = (double)b.warpAtomMass[i0];
            const double q0 = mC(member, 0.5) * (1.0 - mCell);   // G(ℓ)(1−m), the gap neutral
            bool jumps = true, plateau = true;
            b.warpW[i0] = -1.f;
            // just below ℓ the dual's quantile lands AT the gap's LEFT edge q0; just
            // above ℓ it lands AT the RIGHT edge q0+m — the gap [q0, q0+m] is the
            // K-atom, seen through the bank
            const double lo = b.warpedValue(3, 1, 0.5 - 1e-12, false);
            const double hi = b.warpedValue(3, 1, 0.5 + 1e-12, false);
            if (std::fabs(lo - q0) > 1e-9) jumps = false;
            if (std::fabs(hi - (q0 + mCell)) > 1e-9) jumps = false;
            // the edges ARE dualMarkers' pair for this cell
            double lv, mv, glo, ghi;
            auto cT = [&](double x) { return mC(member, x); };
            ws::dualMarkers(0.5, 0.5, 0.5, mCell, cT,
                            [&](double x) { return ws::gapCdf(x, 0.5, 0.5, cT); }, lv, mv, glo, ghi);
            if (!(std::fabs(glo - lo) < 1e-9 && std::fabs(ghi - hi) < 1e-9)) jumps = false;
            ok(jumps, "T8: the bank at w=-1 with a live atom JUMPS the dual's gap [q0,q0+m] at u=ell (the exchange, bank level)");
            b.warpW[i0] = 1.f;
            for (int k = 0; k <= 100; ++k) {
                const double u = q0 + (0.3 * (double)k / 100.0);   // across [q0, q0+m]
                if (b.warpedValue(3, 1, u, false) != 0.5) plateau = false;   // flat AT ell
            }
            ok(plateau, "T8: the SAME cell at w=+1 reads the atom plateau AT ell BIT-EXACT (one sign apart)");
        }
    }

    // ── §10: the EFFECTIVE warped curve — the DISTRIBUTION plot's law (display-only) ──
    // One sweep of u: W(u) = (1−k)u + k·T(u), k = |w|, T the composed READING's
    // target (K: the composed quantile; DK: the composed CDF); the CDF trace the
    // PARAMETRIC point (W(u), u) — no bisection, no integration — and the fill the
    // READING's guarded closed form: K 1/[(1−k) + k/f_T(T(u))], DK
    // 1/[(1−k) + k·g'(u)] (CC_FIX_PHASE_PLOT_NEGATIVE_WARP). The CC prompt's gates:
    // continuous through w = 0 (no jump at the zero crossing, EITHER sign); k = 0
    // the diagonal and f_W = 1; W monotone; no inf/NaN over the (member, k,
    // reading, feature) grid.
    {
        const double ks[] = {0.0, 0.25, 0.5, 0.75, 1.0};
        // The composed READING pair over a member with tilt, gap AND atom LIVE (the
        // grid's features): K reading T = the composed quantile with the fill factor
        // f(T(u)) — the composed pdf AT the target point (the quantile form's
        // T' = 1/f); DK reading T = the composed CDF with the fill factor g'(u) —
        // the composed pdf AT u (the CDF form's T' = g'; the reciprocal density the
        // old plot fed the quantile form was exactly the spec gap this fix retires).
        auto K_T   = [&](const WarpF& f, double u) { return Kq(f, u, 0.3, 0.4, 0.15, 0.5, 0.2); };
        auto K_fT  = [&](const WarpF& f, double t) { return Kp(f, t, 0.3, 0.4, 0.15, 0.2); };
        auto DK_T  = [&](const WarpF& f, double u) { return Kc(f, u, 0.3, 0.4, 0.15, 0.5, 0.2); };
        auto DK_gp = [&](const WarpF& f, double u) { return Kp(f, u, 0.3, 0.4, 0.15, 0.2); };
        bool diag = true, fw1 = true, mono = true, fin = true, k1face = true, cross = true;
        bool gapFace = true, atomFace = true, uniformFace = true;
        for (int r = 0; r < R; ++r) {
            const WarpF& f = roster[r].f;
            for (int reading = 0; reading < 2; ++reading) {
                auto T   = [&](double u) { return reading == 0 ? K_T(f, u)  : DK_T(f, u); };
                auto fac = [&](double u) { return reading == 0 ? K_fT(f, K_T(f, u)) : DK_gp(f, u); };
                for (double k : ks) {
                    double prev = -1e18;
                    for (int i = 0; i <= 500; ++i) {
                        const double u = (double)i / 500.0;
                        const double t = T(u);
                        const double x = (1.0 - k) * u + k * t;
                        if (k == 0.0 && !sameBits(x, u)) diag = false;   // the diagonal, BIT-EXACT
                        const double fw0 = reading == 0 ? ws::warpEffectivePdf(0.0, fac(u))
                                                        : ws::warpEffectivePdfDual(0.0, fac(u));
                        if (k == 0.0 && !sameBits(fw0, 1.0)) fw1 = false;   // the uniform, BIT-EXACT
                        if (x < prev - 1e-12) mono = false;              // W monotone (T is)
                        prev = x;
                        const double fw = reading == 0 ? ws::warpEffectivePdf(k, fac(u))
                                                       : ws::warpEffectivePdfDual(k, fac(u));
                        if (!(std::isfinite(fw) && fw >= 0.0)) fin = false;   // no inf/NaN, ever
                    }
                    // the chain fixes 0 and 1: W(0) = 0 and W(1) = 1 (the Sklar edges)
                    if (std::fabs(k * T(0.0)) > 1e-12) fin = false;
                    if (std::fabs((1.0 - k) * 1.0 + k * T(1.0) - 1.0) > 1e-12) fin = false;
                }
            }
        }
        // the k = 1 face: f_W == f_T (the target's own density) wherever the floor
        // isn't riding (inside the live gap the closed form hands over to the guard)
        {
            const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
            for (int i = 1; i < 500; ++i) {
                const double t = K_T(f, (double)i / 500.0);
                const double fTv = K_fT(f, t);
                if (fTv < 1e-9) continue;
                if (std::fabs(ws::warpEffectivePdf(1.0, fTv) - fTv) > 1e-15 * fTv) k1face = false;
            }
            // the DUAL's own k = 1 face (the fix): the CDF form's closed form at k = 1
            // is 1/g' — the composed density's reciprocal, the SAME division the helper
            // writes (bit-exact modulo the guard; the 1e-6 display floor skips what the
            // helper itself would face)
            for (int i = 1; i < 500; ++i) {
                const double gp = DK_gp(f, (double)i / 500.0);
                if (gp < 1e-6) continue;   // under the display floor the guard hands over
                if (std::fabs(ws::warpEffectivePdfDual(1.0, gp) - 1.0 / gp) > 1e-15 * (1.0 / gp)) k1face = false;
            }
        }
        // the ZERO CROSSING is continuous: both readings converge to the diagonal as
        // k → 0 — |W_±k(u) − u| ≤ k (T, u ∈ [0,1]) and the two SIDES differ by at most
        // 2k — O(k), never a jump, so the drawn curve tracks the knob through zero
        {
            const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
            for (double eps : {1e-2, 1e-3, 1e-4}) {
                for (int i = 0; i <= 200; ++i) {
                    const double u = (double)i / 200.0;
                    const double wPos = (1.0 - eps) * u + eps * K_T(f, u);
                    const double wNeg = (1.0 - eps) * u + eps * DK_T(f, u);
                    if (std::fabs(wPos - u) > eps + 1e-12 || std::fabs(wNeg - u) > eps + 1e-12) cross = false;
                    if (std::fabs(wPos - wNeg) > 2.0 * eps + 1e-12) cross = false;
                }
            }
        }
        // the GUARDED faces (the helper's contract, direct — the widget divides through
        // it): the gap opens AT ONCE (f_W == 0 EXACTLY for any k > 0, the floor
        // included), the atom fades in (→ 1/(1−k), finite even at f_T = ∞), and the
        // k = 0 uniform NEVER divides (f_W = 1 for ANY f_T — the 0/0 is never reached)
        for (double k : {0.25, 0.5, 0.75}) {
            if (ws::warpEffectivePdf(k, 0.0) != 0.0) gapFace = false;
            if (ws::warpEffectivePdf(k, 1e-12) != 0.0) gapFace = false;   // under the floor
            if (!(std::fabs(ws::warpEffectivePdf(k, 1e12) - 1.0 / (1.0 - k)) < 1e-6)) atomFace = false;
            if (ws::warpEffectivePdf(k, (double)INFINITY) != 1.0 / (1.0 - k)) atomFace = false;
        }
        // k = 1 rides the k1face pin (f_W == f_T there — the 1/(1−k) limit is k < 1's
        // law); the inf CALL at k = 1 must still not NaN (the atom branch reads first)
        if (!std::isinf(ws::warpEffectivePdf(1.0, (double)INFINITY))) atomFace = false;
        for (double fT : {0.0, 1e-12, 1.0, 1e12, (double)INFINITY, (double)NAN})
            if (ws::warpEffectivePdf(0.0, fT) != 1.0) uniformFace = false;
        // the DUAL form's guarded faces (the fix — FLIPPED with the exchange):
        // g' = 0 is the dual's ATOM — the 1/(1−k) plateau for k < 1 (EXACT,
        // under-floor included), the marker's delta at k = 1 exactly (0 — never inf,
        // an inf sample would poison the fill's auto-max); g' = inf is the dual's
        // GAP — the hole, 0 EXACTLY; NaN rides the gap face; the uniform never divides
        bool dualGapFace = true, dualAtomFace = true, dualUniformFace = true;
        for (double k : {0.25, 0.5, 0.75}) {
            if (ws::warpEffectivePdfDual(k, 0.0) != 1.0 / (1.0 - k)) dualGapFace = false;
            if (ws::warpEffectivePdfDual(k, 1e-12) != 1.0 / (1.0 - k)) dualGapFace = false;   // under the floor
            if (ws::warpEffectivePdfDual(k, (double)NAN) != 1.0 / (1.0 - k)) dualGapFace = false;
            if (ws::warpEffectivePdfDual(k, (double)INFINITY) != 0.0) dualAtomFace = false;
        }
        if (ws::warpEffectivePdfDual(1.0, 0.0) != 0.0) dualGapFace = false;          // the delta → the marker
        if (ws::warpEffectivePdfDual(1.0, 1e-12) != 0.0) dualGapFace = false;
        // the EDGE-PROXY regression (the floor's own bug): the member pdf's edge proxy
        // rides at O(1e-9)·coeff — BELOW the 1e-6 display floor — so at k = 1 it must
        // face to the hole (0), not 1/6e-9 = 1.67e8 (the spike that ate the fill's
        // auto-max); at k < 1 it faces to the plateau like any under-floor g'
        if (ws::warpEffectivePdfDual(1.0, 6e-9) != 0.0) dualGapFace = false;
        if (ws::warpEffectivePdfDual(0.5, 6e-9) != 2.0) dualGapFace = false;
        for (double gp : {0.0, 1e-12, 1.0, 1e12, (double)INFINITY, (double)NAN})
            if (ws::warpEffectivePdfDual(0.0, gp) != 1.0) dualUniformFace = false;
        ok(diag,        "§10: k=0 draws the diagonal — W(u) == u BIT-EXACT (whole roster, both readings)");
        ok(fw1,         "§10: k=0 draws f_W == 1 — the uniform, the 0/0 never evaluated (BIT-EXACT)");
        ok(mono,        "§10: W(u) is MONOTONE over the sweep (roster x k x reading, features live)");
        ok(fin,         "§10: f_W finite and >= 0 over the whole grid; W(0)=0, W(1)=1 (no inf/NaN)");
        ok(k1face,      "§10: k=1 gives f_W == f_T (K) and 1/g' (DK) — each reading its own density, off the floor");
        ok(cross,       "§10: the zero crossing is continuous — both readings O(k) off the diagonal, <= 2k apart");
        ok(gapFace,     "§10: the gap face — f_W == 0 EXACTLY for any k > 0 (the gap opens at once)");
        ok(atomFace,    "§10: the atom face — f_W -> 1/(1-k), finite even at f_T = inf (the spike fades in)");
        ok(uniformFace, "§10: the uniform face — k=0 returns 1 for ANY f_T incl. 0, inf, NaN");
        ok(dualGapFace, "§10: the DUAL gap face — g'=0 the dual's ATOM: the 1/(1-k) plateau (k<1), the marker's delta at k=1 (never inf)");
        ok(dualAtomFace,"§10: the DUAL atom face — g'=inf the dual's GAP: f_W == 0 EXACTLY for any k > 0");
        ok(dualUniformFace, "§10: the DUAL uniform face — k=0 returns 1 for ANY g' incl. 0, inf, NaN");
    }

    // ── §10b: the NEGATIVE-WARP fix's gates (CC_FIX_PHASE_PLOT_NEGATIVE_WARP) ──────
    // The plot's negative side must BLEND, not switch: (1) the plotted curve changes
    // continuously with the SIGNED w and w = −0.01 is near the diagonal, not the full
    // dual (the symptom — the old gate failed the sign and drew the bare target);
    // (2) the trace IS the engine's blend at the same (w, u), both signs; (3) the
    // fill is a density — integrates to ~1 over the trace for the loadouts whose
    // sampled law has no jump connectors (the trapezoid bridges a sampled x-JUMP
    // with a sliver the true law doesn't carry — the features ride the markers).
    {
        const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};   // a strong S — the full dual is far off the diagonal
        const double wSweep[] = {-1.0, -0.5, -0.2, -0.05, -0.01, 0.0, 0.01, 0.05, 0.2, 0.5, 1.0};
        // the §10 READING pair at this member (the grid's feature loadout, tilt+gap+atom live)
        auto K_T   = [&](double u) { return Kq(f, u, 0.3, 0.4, 0.15, 0.5, 0.2); };
        auto K_fac = [&](double u) { return Kp(f, K_T(u), 0.3, 0.4, 0.15, 0.2); };
        auto DK_T  = [&](double u) { return Kc(f, u, 0.3, 0.4, 0.15, 0.5, 0.2); };
        auto DK_gp = [&](double u) { return Kp(f, u, 0.3, 0.4, 0.15, 0.2); };
        // THE SIGN SWEEP: max_u|W(u)−u| = |w|·D_reading (T is depth-independent — only
        // the SIGN picks it), so within one side it is exactly Lipschitz-1 in w, and
        // across zero |a·D−b·E| ≤ a+b = |w_a−w_b| — continuous through the crossing.
        // The fill rides along, finite and >= 0 at every sampled w (both helpers).
        bool cont = true, fillFin = true, nearDiag = true, dualFar = true;
        double prevMax = -1.0, prevW = 0.0;
        for (double w : wSweep) {
            const double k = ws::warpEffectiveK((float)w);
            const bool dual = (w < 0.0);
            auto T   = [&](double u) { return dual ? DK_T(u) : K_T(u); };
            auto fac = [&](double u) { return dual ? DK_gp(u) : K_fac(u); };
            double maxDev = 0.0;
            for (int i = 0; i <= 400; ++i) {
                const double u = (double)i / 400.0;
                const double dev = std::fabs((1.0 - k) * u + k * T(u) - u);
                if (dev > maxDev) maxDev = dev;
                const double fw = dual ? ws::warpEffectivePdfDual(k, fac(u))
                                       : ws::warpEffectivePdf(k, fac(u));
                if (!(std::isfinite(fw) && fw >= 0.0)) fillFin = false;
            }
            if (prevMax >= 0.0 && std::fabs(maxDev - prevMax) > std::fabs(w - prevW) + 1e-12) cont = false;
            prevMax = maxDev; prevW = w;
        }
        // w = −0.01 is NEAR THE DIAGONAL — ≤ k off it — while the FULL dual (what the
        // old draw snapped to) really is far off at this loadout, so the pin means
        // something
        {
            const double k = ws::warpEffectiveK(-0.01f);
            double maxDev = 0.0, dualDev = 0.0;
            for (int i = 0; i <= 400; ++i) {
                const double u = (double)i / 400.0;
                if (std::fabs((1.0 - k) * u + k * DK_T(u) - u) > maxDev) maxDev = std::fabs((1.0 - k) * u + k * DK_T(u) - u);
                if (std::fabs(DK_T(u) - u) > dualDev) dualDev = std::fabs(DK_T(u) - u);
            }
            if (maxDev > 0.01 + 1e-12) nearDiag = false;
            if (dualDev < 0.05) dualFar = false;
        }
        ok(cont,     "§10b: the sign sweep is CONTINUOUS — max|W−u| is Lipschitz in w across {−1…+1}, both readings");
        ok(fillFin,  "§10b: the fill is finite and >= 0 at every sampled w of the sign sweep (both helpers, features live)");
        ok(nearDiag, "§10b: w = −0.01 plots NEAR THE DIAGONAL (≤ k off it) — the negative side blends, it does not switch");
        ok(dualFar,  "§10b: the full dual at this loadout is far off the diagonal (> 0.05) — the −0.01 pin means something");

        // THE TRACE IS THE ENGINE'S OWN BLEND: the widget's xs at each sampled w equals
        // WarpBank::warpedValue at the same (w, u) — both signs, the cell's stage
        // features LIVE, the composition on the cell's FLOAT-roundtripped values (the
        // store's own arithmetic, not the literals)
        {
            WarpBank b;
            const int i0 = WarpBank::index(4, 2);
            b.warpF[i0] = f;
            b.warpTilt[i0] = 0.3f; b.warpGapLoc[i0] = 0.4f; b.warpGapSize[i0] = 0.15f;
            b.warpAtomLevel[i0] = 0.5f; b.warpAtomMass[i0] = 0.2f;
            const double pc = (double)b.warpTilt[i0], lc = (double)b.warpGapLoc[i0],
                         sc = (double)b.warpGapSize[i0], al = (double)b.warpAtomLevel[i0],
                         mc = (double)b.warpAtomMass[i0];
            bool traceEng = true;
            for (double w : {-1.0, -0.5, -0.2, -0.05, 0.0, 0.05, 0.2, 0.5, 1.0}) {
                b.warpW[i0] = (float)w;
                const double k = ws::warpEffectiveK((float)w);
                auto T = [&](double u) { return w < 0.0 ? Kc(b.warpF[i0], u, pc, lc, sc, al, mc)
                                                        : Kq(b.warpF[i0], u, pc, lc, sc, al, mc); };
                for (int i = 0; i <= 200; ++i) {
                    const double u = (double)i / 200.0;
                    if (std::fabs((1.0 - k) * u + k * T(u) - b.warpedValue(4, 2, u)) > 1e-12) traceEng = false;
                }
            }
            ok(traceEng, "§10b: the trace equals the ENGINE blend — xs == warpedValue at the same (w, u), both signs, features live");
        }

        // THE FILL IS A DENSITY (trapezoid over the trace, both readings) — the
        // parametric sweep's own law ∫f_W(W(u))·W'(u) du = 1. Pinned on the loadouts
        // whose SAMPLED law is clean: bare member and tilt-only (the tilt window is a
        // bijection — no jumps, no flats), and the DUAL with the gap live for k < 1
        // (the composed CDF is continuous, its flat spread at 1/(1−k) — the fill is
        // the exact pushforward). The jump-carrying loadouts (K+gap's quantile jump,
        // the atom's delta/plateau) pin finiteness only — the trapezoid bridges a
        // sampled x-jump with a sliver the true law doesn't carry, and the features
        // themselves ride the bar/band markers.
        {
            const int N = 2000;
            auto integral = [&](auto Tfn, auto facFn, bool dualForm, double k) {
                double acc = 0.0, px = 0.0, pf = -1.0;
                for (int i = 0; i <= N; ++i) {
                    const double u = (double)i / (double)N;
                    const double x = (1.0 - k) * u + k * Tfn(u);
                    const double fw = dualForm ? ws::warpEffectivePdfDual(k, facFn(u))
                                               : ws::warpEffectivePdf(k, facFn(u));
                    if (i > 0) acc += 0.5 * (pf + fw) * (x - px);
                    px = x; pf = fw;
                }
                return acc;
            };
            bool integ1 = true, integTilt = true, integGapDual = true;
            for (double k : {0.25, 0.5, 0.75, 1.0}) {
                // BARE member: K factor f(Q(u)), DK factor g'(u) = f(u) — both smooth
                if (std::fabs(integral([&](double u){ return mQ(f, u); },
                                       [&](double u){ return mP(f, mQ(f, u)); }, false, k) - 1.0) > 0.01) integ1 = false;
                if (std::fabs(integral([&](double u){ return mC(f, u); },
                                       [&](double u){ return mP(f, u); }, true, k) - 1.0) > 0.01) integ1 = false;
                // TILT only (0.3): the window is a monotone bijection — still clean
                if (std::fabs(integral([&](double u){ return Kq(f, u, 0.3, 0.5, 0.0, 0.5, 0.0); },
                                       [&](double u){ return Kp(f, Kq(f, u, 0.3, 0.5, 0.0, 0.5, 0.0), 0.3, 0.5, 0.0, 0.0); }, false, k) - 1.0) > 0.01) integTilt = false;
                if (std::fabs(integral([&](double u){ return Kc(f, u, 0.3, 0.5, 0.0, 0.5, 0.0); },
                                       [&](double u){ return Kp(f, u, 0.3, 0.5, 0.0, 0.0); }, true, k) - 1.0) > 0.01) integTilt = false;
                // DUAL with the GAP live (loc 0.4, size 0.15, no atom), k < 1: the fill
                // is the exact pushforward — integrates to 1
                if (k < 1.0 && std::fabs(integral([&](double u){ return Kc(f, u, 0.0, 0.4, 0.15, 0.5, 0.0); },
                                                  [&](double u){ return Kp(f, u, 0.0, 0.4, 0.15, 0.0); }, true, k) - 1.0) > 0.02) integGapDual = false;
            }
            ok(integ1,        "§10b: the fill integrates to ~1 — bare member, both readings (trapezoid over the trace, tight)");
            ok(integTilt,     "§10b: the fill integrates to ~1 — tilt-only loadout, both readings (the window is a bijection)");
            ok(integGapDual,  "§10b: the fill integrates to ~1 — the DUAL with the gap live, k < 1 (the exact pushforward)");
        }
    }

    if (g_fail == 0) { std::printf("ALL PASS (%d checks)\n", g_pass); return 0; }
    std::printf("%d FAILURE(S) of %d checks\n", g_fail, g_fail + g_pass);
    return 1;
}
