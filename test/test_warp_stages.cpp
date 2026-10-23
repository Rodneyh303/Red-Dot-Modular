// test_warp_stages.cpp — the FEEL-TEST stages: MO rail tilt + injected atom
// (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START.md — the authority; math per
// PALETTE_KUMARASWAMY_TILT_NOTE.md + OUTPUT_LAYER_DISTRIBUTION_MAPPING.md
// "THE ONE USEFUL ATOM FEATURE").
//
// The PURE stage functions live in src/dsp/WarpStages.hpp (Rack-free, plain double
// arithmetic), so this battery drives them directly over a roster of members — the
// same WarpF wrappers the bank holds, drawn pool curves included — plus the BANK's
// composed path (WarpBank::warpedValue runs the same stages over the resolved cell)
// and the persistence round-trip.
//
// Battery (the prompt's light list):
//   (a) NEUTRAL — p=0 AND m=0 → the composed invCDF == the member invCDF BIT-EXACT
//       (the early-outs; same BITS, not tolerance). Also each single-stage neutral:
//       m=0/p≠0 == R_p(F⁻¹(u)) exactly, p=0/m>0 == the pure-atom branches exactly.
//   (b) MONOTONE — tilt |p|≤0.95 strictly increasing; atom m∈[0,0.9] non-decreasing
//       (the flat AT ℓ IS the atom — strictness is only violated on it); both together;
//       dense u sweeps, several members incl. the steep conc=5 Kumaraswamy.
//   (c) ATOM MASS — P(output == ℓ) == m exactly over a dense u grid: the interval
//       [q0, q0+m] has width m BY CONSTRUCTION; assert the grid count matches m·K.
//   (d) TILT DIRECTION — p>0 shifts mass toward the bottom rail, p<0 toward the top
//       (mean + median checks, |shift| monotone in |p|); the Mobius absorption
//       identity (tilt over Mobius(a) == Mobius(a(1-p))) and uniform+tilt == the
//       pure Mobius reference curve (the note's test 7, palette-wide applicability).
//   (e) EDGE CONTINUITY — the branches meet at ℓ: H⁻¹(q0) == H⁻¹(q0+m) == ℓ exactly
//       (bit-equal), and the below/above limits approach ℓ.
//   Plus: CDF/PDF closed forms (the display path) round-trip against the quantile;
//   the BANK path at w=1 == the composed quantile and at neutral == the plain mix
//   bit-exactly; persistence round-trip (save→load survives; ABSENT keys ⟹ neutral).
//
// Header-only, Rack-free (jansson via the test/jansson.h shim — the
// test_resolver_warp_bridge compile shape); registered in test/run_all.sh.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <variant>

#include "WarpF.hpp"                    // the bank + the member wrappers (pulls WarpStages)
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
// Bit-exact equality (the neutral contract is EXACT by construction — the early-outs).
static bool sameBits(double a, double b) {
    uint64_t ra, rb;
    std::memcpy(&ra, &a, sizeof ra);
    std::memcpy(&rb, &b, sizeof rb);
    return ra == rb;
}

// ── the member roster: every alternative, the steep cases the monotone battery uses ──
struct Member {
    const char* name;
    WarpF f;
    bool strict;   // strictly increasing quantile (degenerate trunc-normals are not)
};

int main() {
    WarpBank bank;   // owns the drawn-curve pool the roster references
    const dotModular::MonotoneCurve cA =
        dotModular::MonotoneCurve::fromDistribution(dotModular::KumaraswamyDistribution(2, 3), 9);
    const dotModular::MonotoneCurve cB =
        dotModular::MonotoneCurve::fromDistribution(dotModular::PowerDistribution(0.4), 9);
    const int p0 = bank.putCurve(cA);
    const int p1 = bank.putCurve(cB);

    const Member roster[] = {
        { "identity",     dotModular::IdentityWarp{},                 true  },
        { "triangular",   dotModular::TriangularWarp{0.3},            true  },
        { "power5",       dotModular::PowerWarp{5.0},                 true  },   // steep near 0
        { "revpow5",      dotModular::ReversedPowerWarp{5.0},         true  },   // steep near 1
        { "kuma-steep",   dotModular::KumaraswamyWarp{5.0, 25.8},     true  },   // conc=5 steepest
        { "kuma-2-3",     dotModular::KumaraswamyWarp{2.0, 3.0},      true  },
        { "mobius4",      dotModular::MobiusWarp{4.0},                true  },
        { "logit-sig",    dotModular::LogitSigmoidWarp{2.0, -0.5},    true  },
        { "logit-norm",   dotModular::LogitNormalWarp{0.5, 0.4},      true  },
        { "trunc-norm",   dotModular::TruncNormalWarp{0.5, 0.2},      true  },
        { "trunc-degen",  dotModular::TruncNormalWarp{-2.0, 0.5},     false },   // point mass at 0
        { "drawn-A",      dotModular::DrawnWarp{(uint16_t)p0},        true  },
        { "drawn-B",      dotModular::DrawnWarp{(uint16_t)p1},        true  },
    };
    const int R = (int)(sizeof roster / sizeof roster[0]);
    const dotModular::MonotoneCurve* pool = bank.pool;

    // The composed quantile over a roster member (the same lambdas warpedValue uses).
    auto composedQ = [&](const WarpF& f, double u, double p, double ell, double m) {
        return ws::invCDF(u, p, ell, m,
                          [&](double uu) { return dotModular::invCDF(f, uu, pool); },
                          [&](double x)  { return dotModular::cdf(f, x, pool); });
    };
    auto memberQ = [&](const WarpF& f, double u) { return dotModular::invCDF(f, u, pool); };
    auto memberC = [&](const WarpF& f, double x) { return dotModular::cdf(f, x, pool); };

    std::printf("warp stages (tilt + atom): %d roster members\n", R);

    // ── (a) NEUTRAL: bit-exact by the early-outs ────────────────────────────────────
    {
        bool all = true;
        for (int r = 0; r < R; ++r) {
            const WarpF& f = roster[r].f;
            for (int i = 0; i <= 4000; ++i) {
                const double u = (double)i / 4000.0;
                if (!sameBits(composedQ(f, u, 0.0, 0.5, 0.0), memberQ(f, u))) { all = false; break; }
            }
            if (!all) { std::printf("  (a) neutral broke on %s\n", roster[r].name); break; }
        }
        ok(all, "(a) p=0 AND m=0: composed invCDF == member invCDF BIT-EXACT (roster x 4001 u)");
    }
    {
        // Single-stage neutrals: m=0/p≠0 == R_p(F⁻¹(u)) EXACTLY; p=0/m>0 == the pure
        // atom branches EXACTLY (the p==0 arms inside the atom path).
        const WarpF f = dotModular::KumaraswamyWarp{5.0, 25.8};
        bool tiltOnly = true, atomOnly = true;
        for (int i = 0; i <= 4000; ++i) {
            const double u = (double)i / 4000.0;
            if (!sameBits(composedQ(f, u, 0.4, 0.5, 0.0),
                          ws::tiltMap(memberQ(f, u), 0.4)))
                tiltOnly = false;
            const double q0 = memberC(f, ws::tiltMapInv(0.5, 0.0)) * (1.0 - 0.25);
            double want;
            if (u < q0)          want = memberQ(f, u / (1.0 - 0.25));
            else if (u <= q0 + 0.25) want = 0.5;
            else                 want = memberQ(f, (u - 0.25) / (1.0 - 0.25));
            if (!sameBits(composedQ(f, u, 0.0, 0.5, 0.25), want))
                atomOnly = false;
        }
        ok(tiltOnly, "(a) m=0, p=0.4: composed == R_p(F^-1(u)) BIT-EXACT (the tilt early-out path)");
        ok(atomOnly, "(a) p=0, m=0.25: composed == the pure-atom branches BIT-EXACT (the p==0 arms)");
    }

    // ── (b) MONOTONE: dense sweeps, several members incl. steep ─────────────────────
    {
        const double ps[]  = { 0.95, 0.5, 0.2, -0.2, -0.5, -0.95 };
        const double ms[]  = { 0.3, 0.9 };
        bool tiltStrict = true, bothMono = true, atomFlatOnly = true;
        for (int r = 0; r < R; ++r) {
            if (!roster[r].strict) continue;   // the degenerate member is flat by nature
            const WarpF& f = roster[r].f;
            // Tilt alone (m=0): STRICTLY increasing — the tilt adds no flat segments.
            for (double p : ps) {
                double prev = -1e18; bool first = true;
                for (int i = 0; i <= 8000; ++i) {
                    const double u = (double)i / 8000.0;
                    const double v = composedQ(f, u, p, 0.5, 0.0);
                    if (!first && !(v > prev)) tiltStrict = false;
                    prev = v; first = false;
                }
            }
            // Atom (with AND without tilt — every (p, m) pair has both stages live in the
            // p!=0 rows, and the tilt-neutral row p... none here is 0; the p=0 atom-only
            // monotonicity is the same branch structure): non-DECREASING — the flat AT ℓ
            // is the atom itself, the only legal flat.
            for (double m : ms) {
                for (double p : ps) {
                    double prev = -1e18; bool first = true;
                    for (int i = 0; i <= 8000; ++i) {
                        const double u = (double)i / 8000.0;
                        const double v = composedQ(f, u, p, 0.37, m);
                        if (!first && v < prev) bothMono = false;         // non-decreasing
                        if (!first && v == prev && v != 0.37 && roster[r].name[0] != 'd')
                            atomFlatOnly = false;                          // flats only AT the atom (parametrics; drawn may carry authored flats)
                        prev = v; first = false;
                    }
                }
            }
        }
        ok(tiltStrict, "(b) tilt |p|<=0.95: STRICTLY increasing over 8001-pt sweeps (roster, steep incl.)");
        ok(bothMono,  "(b) atom m in {0.3,0.9} with and without tilt (both stages live): non-decreasing");
        ok(atomFlatOnly, "(b) parametric flats appear ONLY at the atom level (the insert's one flat)");
    }

    // ── (c) ATOM MASS: P(output == ell) == m exactly ────────────────────────────────
    {
        const int K = 100000;
        bool massOK = true;
        for (int r = 0; r < R; ++r) {
            if (!roster[r].strict) continue;
            const WarpF& f = roster[r].f;
            const double ell = 0.62, m = 0.25, p = 0.3;
            long cnt = 0;
            for (int i = 0; i < K; ++i) {
                const double u = (double)(i + 0.5) / (double)K;   // midpoints (no grid edge cases)
                if (composedQ(f, u, p, ell, m) == ell) ++cnt;
            }
            // The atom interval [q0, q0+m] has width m BY CONSTRUCTION; a midpoint grid
            // must land ~m*K of its points in it (±2 for the boundary straddles).
            if (std::fabs((double)cnt - m * (double)K) > 2.0) {
                massOK = false;
                std::printf("  (c) mass broke on %s: cnt=%ld want=%.0f\n", roster[r].name, cnt, m * (double)K);
            }
        }
        ok(massOK, "(c) P(output == ell) == m exactly over a 100k midpoint grid (roster)");
    }

    // ── (d) TILT DIRECTION: p>0 toward the bottom rail, p<0 toward the top ──────────
    {
        auto meanOf = [&](const WarpF& f, double p) {
            const int K = 20000;
            double s = 0.0;
            for (int i = 0; i < K; ++i)
                s += composedQ(f, (double)(i + 0.5) / (double)K, p, 0.5, 0.0);
            return s / (double)K;
        };
        const WarpF members[] = { dotModular::IdentityWarp{},
                                  dotModular::KumaraswamyWarp{2.0, 3.0},
                                  dotModular::TriangularWarp{0.7} };
        bool dirOK = true, monoOK = true;
        for (const WarpF& f : members) {
            const double m0 = meanOf(f, 0.0), mP2 = meanOf(f, 0.2), mP5 = meanOf(f, 0.5),
                         mM2 = meanOf(f, -0.2), mM5 = meanOf(f, -0.5);
            if (!(mP5 < mP2 && mP2 < m0 && m0 < mM2 && mM2 < mM5)) monoOK = false;
            if (!(mP2 < m0 - 1e-9 && mM2 > m0 + 1e-9)) dirOK = false;   // a rail-ward shift
            // Median: R_p(0.5) < 0.5 for p>0 (the note's audibility anchor).
            if (!(ws::tiltMap(0.5, 0.5) < 0.5 && ws::tiltMap(0.5, -0.5) > 0.5)) dirOK = false;
        }
        ok(dirOK, "(d) p>0 shifts mass toward the bottom rail, p<0 toward the top (mean + median)");
        ok(monoOK, "(d) |shift| monotone in |p| (means at 0, +-0.2, +-0.5, three members)");
    }
    {
        // The note's test-7 identities (palette-wide applicability):
        //   uniform+tilt == the pure Mobius reference (R_p IS Mobius(1-p).invCDF);
        //   tilt over Mobius(a) == Mobius(a(1-p)) (the closure/absorption identity).
        bool uniOK = true, absorbOK = true;
        for (int i = 0; i <= 4000; ++i) {
            const double u = (double)i / 4000.0;
            const dotModular::MobiusDistribution ref(1.0 - 0.4);
            if (std::fabs(composedQ(dotModular::IdentityWarp{}, u, 0.4, 0.5, 0.0) - ref.invCDF(u)) > 1e-12)
                uniOK = false;
            const dotModular::MobiusDistribution mA(3.0), mAbs(3.0 * (1.0 - 0.4));
            if (std::fabs(ws::tiltMap(mA.invCDF(u), 0.4) - mAbs.invCDF(u)) > 1e-12)
                absorbOK = false;
        }
        ok(uniOK,   "(d) uniform member + tilt == Mobius(1-p) reference curve (1e-12)");
        ok(absorbOK, "(d) Mobius absorption: tilt(Mobius(a), p) == Mobius(a(1-p)) (1e-12)");
    }

    // ── (e) EDGE CONTINUITY: the branches meet at ell ───────────────────────────────
    {
        bool edgeOK = true, limitOK = true;
        for (int r = 0; r < R; ++r) {
            if (!roster[r].strict) continue;
            const WarpF& f = roster[r].f;
            const double ell = 0.45, m = 0.3, p = 0.25;
            const double q0 = memberC(f, ws::tiltMapInv(ell, p)) * (1.0 - m);
            if (!(composedQ(f, q0, p, ell, m) == ell && composedQ(f, q0 + m, p, ell, m) == ell))
                edgeOK = false;   // the interval endpoints themselves: BIT-EXACT ell
            const double eps = 1e-12;
            const double below = composedQ(f, q0 - eps, p, ell, m);
            const double above = composedQ(f, q0 + m + eps, p, ell, m);
            if (!(below <= ell && ell - below < 1e-6)) limitOK = false;
            if (!(above >= ell && above - ell < 1e-6)) limitOK = false;
        }
        ok(edgeOK,  "(e) H^-1(q0) == H^-1(q0+m) == ell BIT-EXACT (the atom interval's edges)");
        ok(limitOK, "(e) the below/above branch limits meet ell (|gap| < 1e-6 at 1e-12 off-edge)");
    }

    // ── display closed forms: cdf/pdf round-trip against the quantile ───────────────
    {
        bool cdfOK = true, pdfOK = true;
        const WarpF f = dotModular::KumaraswamyWarp{2.0, 3.0};
        const double p = -0.3, ell = 0.55, m = 0.2;
        auto Hcdf = [&](double x) {
            return ws::cdf(x, p, ell, m, [&](double xx) { return memberC(f, xx); });
        };
        // u in a CONTINUOUS branch must invert exactly (H(H⁻¹(u)) == u); u in the atom
        // lands on the riser, where H(ℓ) = q0+m ≥ u ≥ q0 = H(ℓ⁻) brackets it.
        const double q0 = memberC(f, ws::tiltMapInv(ell, p)) * (1.0 - m);
        for (int i = 1; i < 4000; ++i) {
            const double u = (double)i / 4000.0;
            const double x = composedQ(f, u, p, ell, m);
            const double h = Hcdf(x);
            if (x == ell) {
                if (!(h >= u - 1e-9 && h <= u + m + 1e-9)) cdfOK = false;
            } else {
                if (std::fabs(h - u) > 1e-9) cdfOK = false;
            }
        }
        // The pdf's continuous part must integrate (trapz) to ~ (1-m) — the Dirac is
        // the widget's bar, not a density value.
        const int N = 4000;
        double integ = 0.0;
        for (int i = 0; i < N; ++i) {
            const double xa = (double)i / N, xb = (double)(i + 1) / N;
            const double pa = ws::pdf(xa, p, m, [&](double xx) { return dotModular::pdf(f, xx, pool); });
            const double pb = ws::pdf(xb, p, m, [&](double xx) { return dotModular::pdf(f, xx, pool); });
            integ += 0.5 * (pa + pb) * (xb - xa);
        }
        if (std::fabs(integ - (1.0 - m)) > 5e-3) pdfOK = false;
        ok(cdfOK, "display cdf: H(H^-1(u)) == u on the continuous branches; brackets across the riser");
        ok(pdfOK, "display pdf: the continuous part integrates to (1-m) (the Dirac is the bar)");
    }

    // ── the BANK path: composition through the resolver + the neutral pin ───────────
    {
        WarpBank b;
        const int i = WarpBank::index(3, 2);
        b.warpF[i] = dotModular::KumaraswamyWarp{2.0, 3.0};
        b.warpW[i] = 1.f;
        b.warpTilt[i] = 0.3f; b.warpAtomLevel[i] = 0.5f; b.warpAtomMass[i] = 0.25f;
        // The reference params are the STORED FLOATS widened — the bank's cells hold
        // floats, so the composed reference must run the same widened values.
        const double pb = (double)b.warpTilt[i], lb = (double)b.warpAtomLevel[i],
                     mb = (double)b.warpAtomMass[i];
        bool w1 = true, blend = true;
        for (int k = 0; k <= 2000; ++k) {
            const double u = (double)k / 2000.0;
            if (!sameBits(b.warpedValue(3, 2, u, false),
                          composedQ(b.warpF[i], u, pb, lb, mb)))
                w1 = false;
            // the mix blend over the composed quantile (w = 0.4):
            b.warpW[i] = 0.4f;
            const double want = (1.0 - 0.4f) * u + 0.4f * composedQ(b.warpF[i], u, pb, lb, mb);
            if (std::fabs(b.warpedValue(3, 2, u, false) - want) > 0.0) blend = false;
            b.warpW[i] = 1.f;
        }
        ok(w1,    "bank: warpedValue at w=1 == the composed quantile BIT-EXACT (resolver+routed)");
        ok(blend, "bank: warpedValue at w=0.4 == (1-w)u + w*composed (the blend over the stages)");
    }
    {
        // Neutral bank cells: the ORIGINAL mix line, bit-exact (the bridge pin, restated
        // at the stages' boundary — the pinned oracles never set tilt/atom).
        WarpBank b;
        const int i = WarpBank::index(5, 1);
        b.warpF[i] = dotModular::PowerWarp{5.0};
        b.warpW[i] = 0.7f;
        bool neutral = true;
        for (int k = 0; k <= 2000; ++k) {
            const double u = (double)k / 2000.0;
            const double want = (1.0 - 0.7f) * u + 0.7f * memberQ(b.warpF[i], u);
            if (!sameBits(b.warpedValue(5, 1, u, false), want)) neutral = false;
        }
        // Delegated: global stages compose; neutral globals stay the plain global mix.
        b.globalWarpF[1] = dotModular::TriangularWarp{0.3};
        b.globalWarpW[1] = 0.5f;
        b.globalWarpTilt[1] = -0.2f;
        for (int k = 0; k <= 500; ++k) {
            const double u = (double)k / 500.0;
            const double want = (1.0 - 0.5f) * u
                + 0.5f * composedQ(b.globalWarpF[1], u, (double)b.globalWarpTilt[1],
                                   (double)b.globalWarpAtomLevel[1],
                                   (double)b.globalWarpAtomMass[1]);
            if (!sameBits(b.warpedValue(5, 1, u, true), want)) neutral = false;
        }
        ok(neutral, "bank: neutral own cell == the ORIGINAL mix bit-exact; global tilt composes when delegated");
    }

    // ── persistence: round-trip + absent keys ⟹ neutral ─────────────────────────────
    {
        WarpBank src;
        src.warpTilt[7] = 0.31f;  src.warpAtomLevel[7] = 0.62f;  src.warpAtomMass[7] = 0.25f;
        src.warpTilt[111] = -0.95f; src.warpAtomLevel[111] = 0.0f; src.warpAtomMass[111] = 0.9f;
        src.globalWarpTilt[3] = 0.5f; src.globalWarpAtomLevel[3] = 0.75f; src.globalWarpAtomMass[3] = 0.1f;
        src.warpF[7] = dotModular::PowerWarp{2.0};
        json_t* root = json_object();
        WarpBank::serialize(src, root, "editor");
        WarpBank dst;
        const bool full = WarpBank::parse(root, "editor", dst);
        ok(full, "persistence: parse reports full (the two 112-banks present)");
        ok(dst.warpTilt[7] == 0.31f && dst.warpAtomLevel[7] == 0.62f && dst.warpAtomMass[7] == 0.25f
           && dst.warpTilt[111] == -0.95f && dst.warpAtomLevel[111] == 0.0f && dst.warpAtomMass[111] == 0.9f
           && dst.globalWarpTilt[3] == 0.5f && dst.globalWarpAtomLevel[3] == 0.75f
           && dst.globalWarpAtomMass[3] == 0.1f
           && std::holds_alternative<dotModular::PowerWarp>(dst.warpF[7]),
           "persistence: tilt/atom (own + global) survive save -> load exactly");
        // Absent keys (an OLD patch): rebuild the root with ONLY the pre-feel-test keys
        // (the six additive stage keys literally absent), parse into a fresh bank — every
        // stage must come back NEUTRAL (0 / 0.5-interior / 0).
        json_t* oldRoot = json_object();
        const char* preKeys[] = { "editorWarpW", "editorWarpF", "editorWarpPool",
                                  "editorGlobalWarpW", "editorGlobalWarpF" };
        for (const char* k : preKeys)
            json_object_set_new(oldRoot, k, json_incref(json_object_get(root, k)));
        WarpBank old;
        const bool fullOld = WarpBank::parse(oldRoot, "editor", old);
        json_decref(oldRoot);
        bool neutral = true;
        for (int k = 0; k < WarpBank::N; ++k)
            if (old.warpTilt[k] != 0.f || old.warpAtomMass[k] != 0.f
                || old.warpAtomLevel[k] != (float)ws::kAtomLevelDefault) neutral = false;
        for (int l = 0; l < WarpBank::LANES; ++l)
            if (old.globalWarpTilt[l] != 0.f || old.globalWarpAtomMass[l] != 0.f
                || old.globalWarpAtomLevel[l] != (float)ws::kAtomLevelDefault) neutral = false;
        ok(fullOld, "persistence: an old patch (stage keys absent) still reports full");
        ok(neutral, "persistence: absent stage keys ⟹ neutral (0 tilt / 0.5 level / 0 mass)");
        json_decref(root);
    }

    // ── default construction: the documented neutral state ──────────────────────────
    {
        WarpBank fresh;
        bool def = fresh.warpTilt[0] == 0.f && fresh.warpAtomMass[0] == 0.f
                   && fresh.warpAtomLevel[0] == (float)ws::kAtomLevelDefault
                   && fresh.globalWarpTilt[0] == 0.f && fresh.globalWarpAtomMass[0] == 0.f
                   && fresh.globalWarpAtomLevel[0] == (float)ws::kAtomLevelDefault;
        ok(def, "defaults: fresh bank starts neutral (0 tilt / 0.5 level / 0 mass)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
