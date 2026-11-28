// test_scrub_bidir.cpp — CC_PROMPT_SCRUB32 Part B: the scrub is a BIDIRECTIONAL +/-32
// signed draw offset (offset-direct: the latched value IS the offset), floor-split, with
// f < 0 reading AHEAD of the counter (Philox is addressable at any signed index). The six
// recompute sites share ONE split path (PatternEngine::scrubSplit -> ScrubOffsetCore), so
// they cannot drift.
//
// THE CRAB-CANON LAW (G1): the read depends ONLY on (seed, n - f) — counter minus offset.
// Two engines at different counters and different offsets with equal n - f are BITWISE
// equal, including offsets of opposite SIGN and counters reached by REVERSE rolls. This is
// what lets two Monsoons align a canon by offset alone.
//
// Rack-free. Compile: see test/run_all.sh ("test_scrub_bidir").

#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// Advance a stream's draw counter to an absolute position via +1/-1 rolls.
static void marchTo(PatternEngine& pe, int64_t target) {
    while (pe.rhythmDrawCtr < target) pe.advanceRhythmDraw(+1);
    while (pe.rhythmDrawCtr > target) pe.advanceRhythmDraw(-1);
}

// Fresh engine at counter n, scrub offset s, slew r (consumed), recomputed.
static void setup(PatternEngine& pe, int64_t n, float offset, float slewKnob, int poly = 2) {
    pe.numPolyVoicesHint = poly;
    pe.seedRhythmPhilox(0.42f);
    marchTo(pe, n);
    pe.latchSlew(slewKnob, 0.f, 0.f);
    pe.rhythmWin_.consume();
    pe.latchMix(offset, 0.f, 0.f, true, false, false);
    pe.recomputeEffectiveRhythm();
}

static bool buffersEqual(const PatternEngine& a, const PatternEngine& b, int voices) {
    for (int v = 0; v < voices; ++v)
        if (std::memcmp(a.slewedRhythm[v].data(), b.slewedRhythm[v].data(), 32 * sizeof(float)) != 0)
            return false;
    return true;
}

int main() {
    std::printf("== test_scrub_bidir (Part B) ==\n");

    // ── G1 MIRROR / CRAB-CANON: equal (n - f) => bitwise equal, any sign, any route ──
    {
        struct Case { int64_t nX; float fX; int64_t nY; float fY; const char* tag; };
        const Case cases[] = {
            { 40,  +5.f,  38,  +3.f,  "both behind"            },
            { 40,  +5.f,  30,  -5.f,  "behind vs AHEAD"        },   // 35 == 35
            { 40, +32.f,  -8, -16.f,  "full back vs ahead, negative counter" },
            { 40,   0.f,  72, +32.f,  "live vs full-range back" },
            { 40,  -7.f,  40,  -7.f,  "identical ahead"        },
        };
        for (float slewKnob : {0.f, 0.6f}) {                        // r==0 linear AND copula paths
            for (const Case& c : cases) {
                PatternEngine X, Y;
                setup(X, c.nX, c.fX, slewKnob);
                setup(Y, c.nY, c.fY, slewKnob);
                char m[128];
                std::snprintf(m, sizeof m, "G1 slew=%.1f %s: n-f %lld==%lld -> bitwise equal",
                              slewKnob, c.tag, (long long)(c.nX - (int64_t)c.fX),
                              (long long)(c.nY - (int64_t)c.fY));
                CHECK(buffersEqual(X, Y, 3), m);
            }
        }
        // the REVERSE route: Y reaches its counter by overshooting forward then rolling back
        // (the draw index steps -1 per reverse boundary) — same cell, same bits.
        PatternEngine X, Y;
        setup(X, 40, +5.f, 0.6f);
        Y.numPolyVoicesHint = 2; Y.seedRhythmPhilox(0.42f);
        for (int k = 0; k < 60; ++k) Y.advanceRhythmDraw(+1);       // forward past it...
        for (int k = 0; k < 25; ++k) Y.advanceRhythmDraw(-1);       // ...reverse back to 35
        Y.latchSlew(0.6f, 0.f, 0.f); Y.rhythmWin_.consume();
        Y.latchMix(0.f, 0.f, 0.f, true, false, false);
        Y.recomputeEffectiveRhythm();
        CHECK(buffersEqual(X, Y, 3), "G1 reverse route: forward+reverse rolls land the same bits");
    }

    // ── G2 NEGATIVE-f FLOOR SPLIT, behavioural reference (r==0 linear blend) ──────────
    // s = -3.25 -> f = -4, frac = 0.75: the blend sits between the f=-4 and f=-3 windows.
    // Reference = the SAME lerp formula applied to two integer-offset engines' buffers.
    {
        PatternEngine E, A, B;
        setup(E, 40, -3.25f, 0.f);
        setup(A, 40, -4.f,   0.f);
        setup(B, 40, -3.f,   0.f);
        const float frac = 0.75f;
        bool ok = true;
        for (int v = 0; v < 3 && ok; ++v)
            for (int i = 0; i < 32 && ok; ++i) {
                const float ref = A.slewedRhythm[v][i] + frac * (B.slewedRhythm[v][i] - A.slewedRhythm[v][i]);
                ok = (E.slewedRhythm[v][i] == ref);
            }
        CHECK(ok, "G2 s=-3.25 == lerp(f=-4, f=-3, 0.75) bitwise (floor split, not truncation)");
    }

    // ── G3 f=0 IDENTITY: offset 0 at slew 0 is the raw current draw ───────────────────
    {
        PatternEngine E;
        setup(E, 40, 0.f, 0.f);
        PatternEngine::RhythmDraw d{};
        E.patternRhythmAt(E.rhythmDrawCtr, 0.f, d);
        bool ok = true;
        for (int i = 0; i < 32 && ok; ++i) ok = (E.slewedRhythm[0][i] == d.rhythm[0][i]);
        CHECK(ok, "G3 offset 0, slew 0 == the raw draw at N bitwise (the live read)");
    }

    // ── G4 SNAP law spot checks (core-level law lives in test_scrub_offset) ───────────
    {
        using namespace redDot::scrub;
        CHECK(snapToIntegerPosition(snapToIntegerPosition(13.4f)) == snapToIntegerPosition(13.4f),
              "G4 snap is idempotent");
        CHECK(snapToIntegerPosition(-27.f) == -27.f, "G4 integers are fixed points (exact floats)");
    }

    // ── G5 CACHE: a +/-32 round trip revisits the SAME bits (prune never corrupts) ────
    {
        PatternEngine E;
        setup(E, 100, +32.f, 0.6f);
        float first[32]; std::memcpy(first, E.slewedRhythm[0].data(), sizeof first);
        E.latchMix(-32.f, 0.f, 0.f, true, false, false); E.recomputeEffectiveRhythm();
        E.latchMix(+32.f, 0.f, 0.f, true, false, false); E.recomputeEffectiveRhythm();
        CHECK(std::memcmp(first, E.slewedRhythm[0].data(), sizeof first) == 0,
              "G5 +32 -> -32 -> +32 lands the first visit's bits (cache miss recomputes identically)");
    }

    // ── G6 HELD-FRAC MARGINAL UNIFORMITY: a fractional offset is still uniform per step ──
    // The bridge read is a genuine intermediate draw-state, not an output crossfade: the
    // marginal stays U(0,1) at any frac. 1000 samples spaced 70 > K apart; family-wise 1%
    // criterion over 32 steps x 6 held positions (the mutation named in the prompt — a
    // crossfade of OUTPUTS instead of the z-space bridge — breaks exactly this at frac!=0).
    {
        const float holds[6] = {3.15f, 3.4f, 3.6f, 3.85f, -3.4f, -3.85f};
        const int   NS = 1000, SPACING = 70;
        const float alphaPer = 0.01f / (32.f * 6.f);
        const float ksCrit = std::sqrt(-std::log(alphaPer / 2.f) / 2.f) / std::sqrt((float)NS);
        static float samples[32][1000];
        for (float hold : holds) {
            PatternEngine pe;
            pe.numPolyVoicesHint = 0;
            pe.seedRhythmPhilox(0.42f);
            pe.latchSlew(0.7f, 0.f, 0.f); pe.rhythmWin_.consume();
            pe.latchMix(hold, 0.f, 0.f, true, false, false);
            for (int n = 0; n < NS; ++n) {
                for (int a = 0; a < SPACING; ++a) pe.advanceRhythmDraw(+1);
                pe.recomputeEffectiveRhythm();
                for (int st = 0; st < 32; ++st) samples[st][n] = pe.slewedRhythm[0][st];
            }
            float worst = 0.f;
            for (int st = 0; st < 32; ++st) {
                std::sort(samples[st], samples[st] + NS);
                float d = 0.f;
                for (int i = 0; i < NS; ++i) {
                    const float u = samples[st][i];
                    d = std::max(d, std::max(std::fabs((i + 1) / (float)NS - u),
                                             std::fabs(u - i / (float)NS)));
                }
                worst = std::max(worst, d);
            }
            std::printf("    hold=%+.2f worst per-step KS=%.4f (crit %.4f)\n", hold, worst, ksCrit);
            char m[96]; std::snprintf(m, sizeof m, "G6 held frac at %+.2f stays uniform per step", hold);
            CHECK(worst < ksCrit, m);
        }
    }

    // ── BENCH (print; assert only a generous bound) ───────────────────────────────────
    {
        PatternEngine E; setup(E, 1000, 0.f, 0.6f, 15);             // 16 voices
        auto ms = [](auto fn){ auto t0=std::chrono::steady_clock::now(); fn();
            return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count(); };
        double stepMs = ms([&]{ E.latchMix(1.f,0,0,true,false,false);  E.recomputeEffectiveRhythm(); });
        double jumpMs = ms([&]{ E.latchMix(-31.f,0,0,true,false,false); E.recomputeEffectiveRhythm(); });
        std::printf("    bench (16 voices, rhythm stream): +/-1 step %.3f ms, 32-jump cold %.3f ms\n",
                    stepMs, jumpMs);
        CHECK(jumpMs < 50.0, "BENCH: a 32-position cold jump stays under a generous 50 ms headless bound");
    }

    if (g_fail == 0) std::printf("test_scrub_bidir: ALL PASS\n");
    else             std::printf("test_scrub_bidir: %d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
