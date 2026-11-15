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
//   T8  The DUAL reading: an atom on K is a gap on DK and a gap on K is an atom on DK,
//       numerically, at several parameter sets; the swap identity holds for EVERY
//       palette member (dual.cdf == member.invCDF, dual.invCDF == member.cdf), the
//       bank composes the dual as the chained-CDF quantile, and the dual persists.
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
        // (4) the BANK composes the dual as the chained-CDF quantile: a dual cell at
        // w=1 == warpStages::cdf(u) over the BARE member — bit-exact — and with the
        // gap live, the SAME gap params drive both readings (one store, two readings).
        {
            WarpBank b;
            const int i0 = WarpBank::index(1, 4);
            b.warpF[i0] = dotModular::dualWrap(dotModular::PowerWarp{2.0});
            b.warpW[i0] = 1.f;
            b.warpTilt[i0] = 0.2f; b.warpGapLoc[i0] = 0.35f; b.warpGapSize[i0] = 0.2f;
            const WarpF member = dotModular::PowerWarp{2.0};
            bool bankDual = true;
            for (int k = 0; k <= 400; ++k) {
                const double u = (double)k / 400.0;
                const double pc = (double)b.warpTilt[i0], lc = (double)b.warpGapLoc[i0],
                             sc = (double)b.warpGapSize[i0];
                const double want = ws::cdf(u, pc, lc, sc, 0.5, 0.0,
                                            [&](double xx) { return mC(member, xx); });
                if (!sameBits(b.warpedValue(1, 4, u, false), want)) bankDual = false;
            }
            ok(bankDual, "T8: the bank's dual cell at w=1 == the composed CDF AT u BIT-EXACT (gap live)");
        }
        // (5) the dual PERSISTS: the envelope round-trips through the slot codec.
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
    }

    if (g_fail == 0) { std::printf("ALL PASS (%d checks)\n", g_pass); return 0; }
    std::printf("%d FAILURE(S) of %d checks\n", g_fail, g_fail + g_pass);
    return 1;
}
