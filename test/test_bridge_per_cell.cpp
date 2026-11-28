// test_bridge_per_cell.cpp — feat/bridge-per-cell-xi: INDEPENDENT bridge innovations per cell.
//
// The parent branch (feat/scrub32-bidir) drew ONE ξ per interval pair, shared by every
// (step, lane, voice) cell of a recompute. Marginals stayed exactly N(0,1), but at
// FRACTIONAL scrub positions every cell shared one bridge innovation: cross-cell
// correlation of weight v = sqrtV² that integer positions do not have. This branch keys
// ξ per cell — PhiInv(bridgePhilox.atUniform(pairIndex·kXiStride + fi)) — memoised in the
// ZWindowCache beside z0/z1 so the hot path (z-hit, frac moving) stays a float load.
//
// X1 is the A/B discriminator: it FAILS on the parent branch (shared ξ ⇒ cross-cell corr
// ≈ v ≈ large) and PASSES here (corr ≈ 0). X2–X4 pin determinism and the three fill paths
// (miss-fill / lazy z-hit fill / ForVoice partial fill) against each other bitwise.
// BENCH answers "is live recompute fast enough under scrub/jog": it times the z-hit
// (xi-hit) wiggle path and the cold-jump path at 16 voices, plus the raw ns/draw.
//
// Rack-free. Compile: see test/run_all.sh ("test_bridge_per_cell").

#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "VoiceResolver.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static void marchTo(PatternEngine& pe, int64_t target) {
    while (pe.rhythmDrawCtr < target) pe.advanceRhythmDraw(+1);
    while (pe.rhythmDrawCtr > target) pe.advanceRhythmDraw(-1);
}

static void setup(PatternEngine& pe, int64_t n, float offset, float slewKnob, int poly = 2) {
    pe.numPolyVoicesHint = poly;
    pe.seedRhythmPhilox(0.42f);
    marchTo(pe, n);
    pe.latchSlew(slewKnob, 0.f, 0.f);
    pe.rhythmWin_.consume();
    pe.latchMix(offset, 0.f, 0.f, true, false, false);
    pe.recomputeEffectiveRhythm();
}

// All four rhythm-stream lanes, voices 0..voices-1, bitwise.
static bool lanesEqual(const PatternEngine& a, const PatternEngine& b, int voices) {
    const size_t B = 32 * sizeof(float);
    for (int v = 0; v < voices; ++v) {
        if (std::memcmp(a.slewedRhythm[v].data(),    b.slewedRhythm[v].data(),    B)) return false;
        if (std::memcmp(a.slewedAccent[v].data(),    b.slewedAccent[v].data(),    B)) return false;
        if (std::memcmp(a.slewedVariation[v].data(), b.slewedVariation[v].data(), B)) return false;
        if (std::memcmp(a.slewedLegato[v].data(),    b.slewedLegato[v].data(),    B)) return false;
    }
    return true;
}

static float pearson(const float* x, const float* y, int n) {
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    for (int i = 0; i < n; ++i) { sx += x[i]; sy += y[i]; sxx += (double)x[i]*x[i]; syy += (double)y[i]*y[i]; sxy += (double)x[i]*y[i]; }
    const double n_ = n;
    const double cov = sxy/n_ - (sx/n_)*(sy/n_);
    const double vx  = sxx/n_ - (sx/n_)*(sx/n_);
    const double vy  = syy/n_ - (sy/n_)*(sy/n_);
    return (float)(cov / std::sqrt(vx*vy));
}

int main() {
    std::printf("== test_bridge_per_cell (independent bridge innovations) ==\n");

    // ── X1 CROSS-CELL INDEPENDENCE AT A HELD FRACTIONAL POSITION (the A/B discriminator) ──
    // Hold offset +0.5 (f=0, frac=0.5), slew 0.7; one sample per pair, pairs spaced 70 > K
    // apart so the z windows are independent across samples. Cells with independent z's:
    //   pair 1: same lane, different steps  (rhythm[0][3]  vs rhythm[0][11])
    //   pair 2: same step,  different lanes (rhythm[0][5]  vs variation[0][5])
    // Parent (shared ξ): corr ≈ v = sqrtV² = (1−r)/(1+r) at frac=0.5 (uniform-space ≈ 0.95·v),
    // so SMALL r discriminates hardest — slew 0.3 keeps v large.
    // This branch: ξ independent per cell ⇒ corr ≈ 0.  n=1500 ⇒ sd(corr) ≈ 0.026; the
    // acceptance bound 0.10 is ~4σ above independence and far below the shared-ξ value.
    {
        // The shared-ξ corr the parent would show (meaningfulness guard): recompute v from
        // the engine's own kernel at r = slewKnobToR(0.7), frac = 0.5.
        const double r  = (double)PatternEngine::slewKnobToR(0.3f);   // small r = LARGE shared-xi signature: v=(1-r)/(1+r)
        const double aR = std::fabs(r), om = (r < 0) ? 3.14159265358979323846 : 0.0;
        const double rhoA = std::pow(aR, 0.5) * std::cos(om * 0.5);
        const double rhoB = rhoA;
        const double det  = 1.0 - r * r;
        const double a    = (rhoA - r * rhoB) / det, b = (rhoB - r * rhoA) / det;
        const double v    = 1.0 - (rhoA * a + rhoB * b);
        std::printf("    r=%.4f  shared-xi would give cross-cell corr ~ v=%.3f\n", r, v);
        CHECK(v > 0.25, "X1 guard: shared-xi corr target is large enough to discriminate");

        const int NS = 1500, SPACING = 70;
        static float uA[NS], uB[NS], uC[NS], uD[NS];
        PatternEngine pe;
        pe.numPolyVoicesHint = 0;
        pe.seedRhythmPhilox(0.42f);
        pe.latchSlew(0.3f, 0.f, 0.f); pe.rhythmWin_.consume();
        pe.latchMix(0.5f, 0.f, 0.f, true, false, false);
        for (int n = 0; n < NS; ++n) {
            for (int k = 0; k < SPACING; ++k) pe.advanceRhythmDraw(+1);
            pe.recomputeEffectiveRhythm();
            uA[n] = pe.slewedRhythm[0][3];  uB[n] = pe.slewedRhythm[0][11];
            uC[n] = pe.slewedRhythm[0][5];  uD[n] = pe.slewedVariation[0][5];
        }
        const float cSteps = pearson(uA, uB, NS);
        const float cLanes = pearson(uC, uD, NS);
        std::printf("    corr(step3, step11)=%.4f  corr(rhythm5, variation5)=%.4f  (bound 0.10)\n",
                    cSteps, cLanes);
        CHECK(std::fabs(cSteps) < 0.10f, "X1 cross-STEP independence at frac=0.5 (parent shared-xi FAILS this)");
        CHECK(std::fabs(cLanes) < 0.10f, "X1 cross-LANE independence at frac=0.5 (parent shared-xi FAILS this)");
    }

    // ── X2 REVISIT DETERMINISM: xi is a pure function of (seed, pair, cell) ──────────────
    // Leave a fractional position, come back: the cache refills from Philox replay — bitwise.
    {
        PatternEngine A;
        setup(A, 50, +3.5f, 0.6f);
        PatternEngine ref;                       // snapshot via an identical twin
        setup(ref, 50, +3.5f, 0.6f);
        A.latchMix(-10.25f, 0.f, 0.f, true, false, false); A.recomputeEffectiveRhythm();
        A.latchMix(+17.75f, 0.f, 0.f, true, false, false); A.recomputeEffectiveRhythm();
        A.latchMix(+3.5f,   0.f, 0.f, true, false, false); A.recomputeEffectiveRhythm();
        CHECK(lanesEqual(A, ref, 3), "X2 revisit of a fractional position replays identical bits");
    }

    // ── X3 THE THREE FILL PATHS AGREE BITWISE ────────────────────────────────────────────
    // A: integer fill (+3.0, no xi drawn) -> ForVoice lazy draws (mono, then V2) -> full
    //    z-hit lazy fill. B: direct full miss-fill at +3.4. Every route = same bits.
    {
        PatternEngine A, B;
        setup(A, 50, +3.0f, 0.6f);               // z filled at f=3, frac=0 -> NO xi drawn yet
        setup(B, 50, +3.4f, 0.6f);               // miss path: z + xi filled together
        A.latchMix(+3.4f, 0.f, 0.f, true, false, false);
        A.recomputeEffectiveRhythmForVoice(dotModular::VoiceResolver::mono());   // lazy xi, mono cells only
        CHECK(!std::memcmp(A.slewedRhythm[0].data(), B.slewedRhythm[0].data(), 32*sizeof(float)) &&
              !std::memcmp(A.slewedAccent[0].data(), B.slewedAccent[0].data(), 32*sizeof(float)),
              "X3 ForVoice(mono) lazy xi == full miss-fill, bitwise");
        A.recomputeEffectiveRhythmForVoice(dotModular::VoiceResolver::slot(2)); // lazy xi, V2 cells
        CHECK(!std::memcmp(A.slewedRhythm[1].data(), B.slewedRhythm[1].data(), 32*sizeof(float)),
              "X3 ForVoice(V2) lazy xi == full miss-fill, bitwise");
        A.recomputeEffectiveRhythm();            // z-hit full pass: completes xi, sets xiValid
        CHECK(lanesEqual(A, B, 3), "X3 full z-hit lazy fill == full miss-fill, bitwise");
        // and now the xi-HIT path (cache read) against a fresh miss-fill at the same spot:
        A.latchMix(+3.8f, 0.f, 0.f, true, false, false); A.recomputeEffectiveRhythm();  // xi-hit
        PatternEngine C;
        setup(C, 50, +3.8f, 0.6f);                                                      // miss
        CHECK(lanesEqual(A, C, 3), "X3 xi-HIT (cached) == miss-fill (drawn), bitwise");
    }

    // ── X4 INTEGER POSITIONS DRAW NOTHING: frac==0 leaves xiValid false, bits == raw ─────
    // (The crab-canon and raw-identity laws at integer offsets are pinned by test_scrub_bidir
    // G1/G3, which run on this branch; here we pin that the integer path never touches xi.)
    {
        PatternEngine A;
        setup(A, 40, +5.f, 0.6f);
        CHECK(!A.rhythmZCacheMru().xiValid, "X4 integer offset fills z but draws no xi (sqrtV==0)");
        A.latchMix(+5.5f, 0.f, 0.f, true, false, false); A.recomputeEffectiveRhythm();
        CHECK(A.rhythmZCacheMru().xiValid, "X4 first fractional recompute lazily fills xi on the z-hit");
    }

    // ── BENCH: the scrub/jog cost question (print; assert only generous bounds) ──────────
    {
        PatternEngine pe;
        setup(pe, 100, +3.1f, 0.6f, 15);         // 16 voices, fractional: worst realistic UI case
        using clk = std::chrono::steady_clock;

        // hot path: frac wiggle inside one pair (z-hit + xi-hit) — the live-jog steady state
        {
            volatile float sink = 0.f;
            const auto t0 = clk::now();
            const int IT = 400;
            for (int i = 0; i < IT; ++i) {
                const float off = 3.1f + 0.8f * (float)(i % 7) / 7.f;
                pe.latchMix(off, 0.f, 0.f, true, false, false);
                pe.recomputeEffectiveRhythm();
                sink += pe.slewedRhythm[0][0];
            }
            const double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count() / IT;
            std::printf("    BENCH hit-path (z-hit+xi-hit, 16 voices): %.3f ms/recompute\n", ms);
            CHECK(ms < 10.0, "BENCH hot wiggle path under 10ms (generous CI bound)");
            (void)sink;
        }
        // cold path: pair-crossing jumps (z-miss, xi drawn per cell inside the miss fill)
        {
            volatile float sink = 0.f;
            const auto t0 = clk::now();
            const int IT = 60;
            for (int i = 0; i < IT; ++i) {
                const float off = (i % 2) ? +16.5f : -16.5f;
                pe.latchMix(off, 0.f, 0.f, true, false, false);
                pe.recomputeEffectiveRhythm();
                sink += pe.slewedRhythm[0][0];
            }
            const double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count() / IT;
            std::printf("    BENCH miss-path (33-draw jump, 16 voices, xi fill included): %.3f ms/recompute\n", ms);
            CHECK(ms < 50.0, "BENCH cold jump path under 50ms (generous CI bound)");
            (void)sink;
        }
        // raw per-cell draw cost: one keyed Philox + PhiInv (the quantity the cache saves)
        {
            volatile double sink = 0.0;
            const auto t0 = clk::now();
            const int N = 1 << 20;
            for (int i = 0; i < N; ++i)
                sink += redDot::copula::PhiInv((double)pe.bridgeRhythmPhilox.atUniform((uint64_t)i));
            const double ns = std::chrono::duration<double, std::nano>(clk::now() - t0).count() / N;
            std::printf("    BENCH raw xi draw (Philox+PhiInv): %.1f ns/cell  (~2048 cells = %.3f ms uncached)\n",
                        ns, ns * 2048.0 / 1e6);
            (void)sink;
        }
    }

    if (g_fail == 0) std::printf("ALL PASS\n");
    else             std::printf("%d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
