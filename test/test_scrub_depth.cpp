// test_scrub_depth.cpp — CC_PROMPT_SCRUB32 Part B: the per-stream MA DEPTH K.
//
// K is the OTHER copula-window parameter (with slew r): both define the window a draw's
// value is computed from, so both are DRAW-FIXED, carried by the SAME CopulaWindowLatch
// and committed by the ONE consume() — Part A generalised, not a second mechanism. K is
// runtime per stream (1..64, first-K taps L2-normalised, the weight tail ZEROED so the
// full-K fixed-reduction dot product is bit-exactly unchanged and draws beyond K are
// never fetched — the cache budget law stays honest). The scrub budget follows the
// DRAW-FIXED K: maxScrubForK(64)=32, (32)=64, (16)=80; a K commit that shrinks the range
// clamps a deep offset (the ACCEPTED jump — the readout shows it).
//
// Rack-free. Compile: see test/run_all.sh ("test_scrub_depth").

#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static void marchTo(PatternEngine& pe, int64_t target) {
    while (pe.rhythmDrawCtr < target) pe.advanceRhythmDraw(+1);
    while (pe.rhythmDrawCtr > target) pe.advanceRhythmDraw(-1);
}
static void setup(PatternEngine& pe, int64_t n, float offset, float slewKnob, int depthK, int poly = 2) {
    pe.numPolyVoicesHint = poly;
    pe.seedRhythmPhilox(0.42f);
    marchTo(pe, n);
    pe.latchSlew(slewKnob, 0.f, 0.f);
    pe.latchDepth(depthK, 64, 64);
    pe.rhythmWin_.consume();
    pe.latchMix(offset, 0.f, 0.f, true, false, false);
    pe.recomputeEffectiveRhythm();
}
static bool eq(const PatternEngine& a, const PatternEngine& b, int voices = 3) {
    for (int v = 0; v < voices; ++v)
        if (std::memcmp(a.slewedRhythm[v].data(), b.slewedRhythm[v].data(), 32 * sizeof(float)) != 0)
            return false;
    return true;
}

int main() {
    std::printf("== test_scrub_depth (Part B: per-stream MA depth) ==\n");

    // ── D1 DEPTH IS DRAW-FIXED (the generalised Part A law) ───────────────────────────
    {
        PatternEngine pe; PatternInput in;
        setup(pe, 40, 3.f, 0.6f, 64);
        const int appliedK = pe.rhythmDepthApplied;
        float before[32]; std::memcpy(before, pe.slewedRhythm[0].data(), sizeof before);
        for (int k : {16, 32, 8, 4}) {                      // the live knob moves mid-phrase
            pe.latchDepth(k, 64, 64);
            pe.refreshVisualCache(in);
            CHECK(pe.rhythmDepthApplied == appliedK, "D1: mid-phrase depth latch does NOT re-bake");
        }
        CHECK(std::memcmp(before, pe.slewedRhythm[0].data(), sizeof before) == 0,
              "D1: buffers untouched by mid-phrase depth moves");
        // the boundary consumer fires on a pending DEPTH change alone (slew unchanged)
        in.locked = false;
        pe.applyPendingSeedsAndRedraw(in);
        CHECK(pe.rhythmDepthApplied == 4 && pe.rhythmWin_.phraseDepth() == 4,
              "D1: the boundary consumes the latest latched depth (one consume, both window params)");
        CHECK(std::memcmp(before, pe.slewedRhythm[0].data(), sizeof before) != 0,
              "D1: the consumed depth re-derives the buffers (K 64->4 at r=0.6: the tail mass is real)");
        // NOTE (the physics the prompt calls the budget trade being ~free at low r): a K
        // change whose dropped tail mass is below double epsilon (64->48 at r=0.6, ~1e-21
        // of the norm) re-derives BIT-IDENTICAL buffers — that is correct, not a bug.
    }

    // ── D2 THE CRAB LAW HOLDS AT ANY K: equal (n - f) => equal bits, per K ────────────
    {
        for (int k : {16, 32}) {
            PatternEngine X, Y;
            setup(X, 40, +60.f * (k == 16) + 20.f * (k == 32), 0.6f, k);   // deep offsets legal at small K
            const float fX = (k == 16) ? 60.f : 20.f;
            setup(Y, 40 - (int64_t)fX, 0.f, 0.6f, k);
            char m[80]; std::snprintf(m, sizeof m, "D2: crab law at K=%d, offset %.0f", k, fX);
            CHECK(eq(X, Y), m);
        }
    }

    // ── D3 THE ACCEPTED JUMP: a K commit that shrinks the range clamps the offset ────
    {
        PatternEngine E, R;
        setup(E, 100, +60.f, 0.6f, 16);                      // K=16: range +/-80, offset 60 legal
        PatternInput in; in.locked = false;
        E.latchDepth(64, 64, 64);                            // commit K=64 at the boundary...
        E.applyPendingSeedsAndRedraw(in);
        setup(R, 100, +32.f, 0.6f, 64);                      // ...range is +/-32 now: 60 clamps to 32
        CHECK(E.rhythmDepthApplied == 64, "D3: the K commit landed");
        CHECK(eq(E, R), "D3: the deep offset CLAMPS to the new range (the accepted jump, pinned)");
    }

    // ── D4 K=1 IS THE RAW DRAW: slew becomes irrelevant (one tap, weight 1) ──────────
    {
        PatternEngine A, B;
        setup(A, 40, 2.f, 0.9f, 1);
        setup(B, 40, 2.f, 0.1f, 1);
        CHECK(eq(A, B), "D4: at K=1 the output is r-INVARIANT (no averaging - the raw draw)");
    }

    // ── D5 UNIFORMITY AT REDUCED K: first-K L2 normalisation keeps the marginal ──────
    {
        const int NS = 1000, SPACING = 70;
        const float alphaPer = 0.01f / (32.f * 2.f);
        const float ksCrit = std::sqrt(-std::log(alphaPer / 2.f) / 2.f) / std::sqrt((float)NS);
        static float samples[32][1000];
        for (int k : {16, 32}) {
            PatternEngine pe;
            pe.numPolyVoicesHint = 0;
            pe.seedRhythmPhilox(0.42f);
            pe.latchSlew(0.7f, 0.f, 0.f);
            pe.latchDepth(k, 64, 64);
            pe.rhythmWin_.consume();
            pe.latchMix(0.f, 0.f, 0.f, true, false, false);
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
            std::printf("    K=%2d worst per-step KS=%.4f (crit %.4f)\n", k, worst, ksCrit);
            char m[64]; std::snprintf(m, sizeof m, "D5: marginal stays uniform at K=%d", k);
            CHECK(worst < ksCrit, m);
        }
    }

    if (g_fail == 0) std::printf("test_scrub_depth: ALL PASS\n");
    else             std::printf("test_scrub_depth: %d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
