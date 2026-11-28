// test_slew_phrase_fixed.cpp — CC_PROMPT_SCRUB32 Part A: slew is a copula-WINDOW parameter,
// consumed ONLY at the draw boundary (the roll / the boundary slew-consumer), never from a
// mid-phrase mix/scrub move. The engine's own record of which slew is baked into the buffers
// is *SlewApplied; the draw-fixed committed value is the phrase slew (CopulaWindowLatch).
//
// RED FIRST: on the pre-fix engine the recompute reads the LIVE *SlewLatched, so a mid-phrase
// mix move that misses the z-cache pulls the live slew in — T1 fails (7/8 changes). GREEN after
// the fix: the recompute reads the phrase-fixed value, so mid-phrase moves cannot change it; the
// boundary consumer is the ONE place it advances.
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include <iostream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstring>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _m; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _m = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); if(!_m.empty()) std::cout << " — " << _m; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)

// Seed a stream, advance a few draws, and bake a baseline (the roll-equivalent): a full
// recompute with slew S0 and a fixed mix, so *SlewApplied == clamp(S0) and the phrase is S0.
static void bake(PatternEngine& pe, float slew0, float mix0) {
    pe.seedRhythmPhilox(0.42f);
    for (int k = 0; k < 5; ++k) pe.advanceRhythmDraw(+1);
    pe.latchSlew(slew0, 0.f, 0.f);                      // r>0 -> the copula path is live
    pe.rhythmWin_.consume();                            // the roll-equivalent: phrase <- slew0 (NON-ZERO)
    pe.latchMix(mix0, 0.f, 0.f, true, false, false);
    pe.recomputeEffectiveRhythm();                      // bakes slew0 into the buffers, *Applied = slew0
}

int main() {
    std::cout << "\033[1mSlew phrase-fixed (Part A) Tests\033[0m\n" << std::string(50, '=') << "\n";

    const float slews[8] = {0.60f,0.65f,0.55f,0.70f,0.50f,0.62f,0.58f,0.68f};
    const float mixes[8] = {0.31f,0.33f,0.30f,0.35f,0.28f,0.34f,0.29f,0.36f};

    // T1 — THE LEAK IS GONE: mid-phrase slew+mix modulation (NO roll) must not move the baked slew.
    TEST("mid-phrase slew+mix modulation does NOT change rhythmSlewApplied (0/8)", {
        PatternEngine pe; PatternInput in;
        bake(pe, 0.60f, 0.30f);
        const float appliedAtBake = pe.rhythmSlewApplied;
        int changes = 0; float prev = pe.rhythmSlewApplied;
        for (int k = 0; k < 8; ++k) {
            pe.latchSlew(slews[k], 0.f, 0.f);                      // slew MOVES
            pe.latchMix(mixes[k], 0.f, 0.f, true, false, false);   // mix MOVES (enters for-voice branch)
            pe.refreshVisualCache(in);                             // the leak path
            if (pe.rhythmSlewApplied != prev) { ++changes; prev = pe.rhythmSlewApplied; }
        }
        EXPECT(changes == 0);                       // pre-fix: 7
        EXPECT(pe.rhythmSlewApplied == appliedAtBake);
    });

    // T2 — CONSUMED EXACTLY ONCE AT THE BOUNDARY: after a mid-phrase slew change (held), a phrase
    // boundary that does NOT redraw consumes it through the boundary slew-consumer branch, to the
    // LATEST latched value — and the phrase now equals that value.
    TEST("a phrase boundary consumes the latched slew exactly once (to the latest value)", {
        PatternEngine pe; PatternInput in;
        bake(pe, 0.60f, 0.30f);
        const float held = pe.rhythmSlewApplied;
        // Mid-phrase: move slew several times (mix held so no mix-branch work); slew stays held.
        for (int k = 0; k < 4; ++k) { pe.latchSlew(slews[k], 0.f, 0.f); pe.refreshVisualCache(in); }
        EXPECT(pe.rhythmSlewApplied == held);               // still held mid-phrase
        const float latest = 0.44f;
        pe.latchSlew(latest, 0.f, 0.f);                     // the value live at the boundary
        // A boundary with no pending roll/seed and mode 0 -> no redraw -> the slew-consumer branch.
        in.locked = false;
        pe.applyPendingSeedsAndRedraw(in);
        EXPECT(std::fabs(pe.rhythmSlewApplied - latest) < 1e-6f);   // consumed, once, to the latest
    });

    // T3 — MIX-ONLY (FRAC) CHANGE DOES NOT RECOMPUTE ALL VOICES: with r!=0, the z-cache warm, and
    // the mix move kept WITHIN one integer scrub position f (frac-only -> z-cache hits), the
    // for-voice path runs for the displayed voice only; a non-displayed poly voice's slewed buffer
    // is untouched. (A mix move that CROSSES f legitimately recomputes all voices — the scrub
    // position moved — so this test holds f fixed to isolate the no-full-recompute property.)
    TEST("mix-frac-only mid-phrase change leaves a non-displayed voice's buffer untouched", {
        PatternEngine pe; PatternInput in;
        pe.numPolyVoicesHint = 2;                 // V1(displayed, mono)=0, plus V2
        bake(pe, 0.60f, 0.20f);                   // mix 0.20 -> s=1.2 -> f=1
        pe.recomputeEffectiveRhythm();            // warm all voices + the z-cache at (N, f=1, r)
        float before[16];
        for (int i = 0; i < 16; ++i) before[i] = pe.slewedRhythm[1][i];   // V2 == slot 1
        pe.latchMix(0.28f, 0.f, 0.f, true, false, false);  // 0.28 -> s=1.68 -> f=1 (same f, frac only)
        pe.refreshVisualCache(in);                         // z-cache HIT -> for-voice(mono) only
        for (int i = 0; i < 16; ++i)
            EXPECT(std::memcmp(&pe.slewedRhythm[1][i], &before[i], sizeof(float)) == 0);
    });

    // T4 — THE DIFFERENTIAL (prompt tests 1+5 in one law): an engine whose live slew latch is
    // moved mid-phrase must stay BITWISE equal, in every voice's slewed buffer, to a reference
    // engine whose latch was never moved — through mix moves that stay within one scrub
    // position AND moves that cross integer positions (z-cache hit and miss paths both).
    // This kills the whole failure class at once: ANY recompute path reading *SlewLatched
    // instead of the phrase value diverges (the ForVoice mutation), and a fractional-position
    // blend whose two windows used different r diverges at the blended steps (same-r law —
    // the z-cache key (N, f, r) guarantee, asserted behaviourally).
    TEST("differential: mid-phrase slew latches never perturb any voice's buffers (hit + miss paths)", {
        PatternEngine A, B; PatternInput in;
        A.numPolyVoicesHint = 3; B.numPolyVoicesHint = 3;
        bake(A, 0.60f, 0.20f); bake(B, 0.60f, 0.20f);
        A.recomputeEffectiveRhythm(); B.recomputeEffectiveRhythm();
        // mix walk: frac-only moves (f=1 throughout: 0.20->0.28), then integer crossings
        // (0.35 -> f=2, 0.05 -> f=0), then back between — cache hits AND misses.
        const float walk[7] = {0.24f, 0.28f, 0.35f, 0.05f, 0.33f, 0.21f, 0.30f};
        for (int k = 0; k < 7; ++k) {
            A.latchSlew(slews[k], 0.f, 0.f);                      // A's live latch MOVES
            A.latchMix(walk[k], 0.f, 0.f, true, false, false);
            B.latchMix(walk[k], 0.f, 0.f, true, false, false);    // B: identical mix, latch still
            A.refreshVisualCache(in); B.refreshVisualCache(in);
            for (int v = 0; v < 3; ++v)
                EXPECT(std::memcmp(A.slewedRhythm[v].data(), B.slewedRhythm[v].data(), 32 * sizeof(float)) == 0);
            EXPECT(A.rhythmSlewApplied == B.rhythmSlewApplied);
        }
    });

    // T5 — UNIFORMITY GUARD (prompt test 4): the pre-distribution slewed output stays marginally
    // uniform PER STEP at every slew. 1000 INDEPENDENT samples per step: draw indices spaced 70
    // apart (> K = 64 — consecutive draws share window cells and are autocorrelated, which
    // inflates KS; same thinning rationale as test_GaussianCopula). The seed is fixed, so this
    // is deterministic — a failure is a finding, never flakiness. Documents that the harm of a
    // mid-phrase slew would be the JOINT law (the window), never the marginal.
    //
    // CRITICAL VALUE — family-wise, NOT the prompt's single-test 0.043: we assert the WORST of
    // 16 steps x 6 slews = 96 KS statistics, and the max of 96 independent 5%-level tests
    // exceeds the single-test critical value in nearly every run by pure order statistics
    // (measured: worst-of-96 = 0.050 at seed 0.42 and 0.058 at seed 0.13, different steps and
    // slews each time — moving, i.e. statistical, not systematic). Sidak/Bonferroni at a 1%
    // FAMILY level over 96 comparisons: alpha_per = 0.01/96, c = sqrt(-ln(alpha_per/2)/2)
    // ~= 2.22, crit ~= 0.0702 at n = 1000. A real marginal defect (a biased step) lands far
    // beyond that; the two seeds' maxima pass with a third of the interval to spare.
    TEST("per-step KS stays under the family-wise 1% critical value for slew in {-1,-0.6,0,0.3,0.7,1}", {
        const float rs[6] = {-1.f, -0.6f, 0.f, 0.3f, 0.7f, 1.f};
        const int   NS = 1000, SPACING = 70;
        const float alphaPer = 0.01f / 96.f;                  // 1% family-wise over 16 steps x 6 slews
        const float ksCrit = std::sqrt(-std::log(alphaPer / 2.f) / 2.f) / std::sqrt((float)NS); // ~0.0702
        static float samples[16][1000];
        for (float r : rs) {
            PatternEngine pe;
            pe.seedRhythmPhilox(0.42f);
            pe.latchSlew(r, 0.f, 0.f);
            pe.rhythmWin_.consume();                           // phrase <- r (the roll-equivalent)
            pe.latchMix(0.f, 0.f, 0.f, true, false, false);    // f = 0: the live window
            for (int n = 0; n < NS; ++n) {
                for (int a = 0; a < SPACING; ++a) pe.advanceRhythmDraw(+1);
                pe.recomputeEffectiveRhythm();
                for (int st = 0; st < 16; ++st) samples[st][n] = pe.slewedRhythm[0][st];
            }
            float worst = 0.f; int worstStep = -1;
            for (int st = 0; st < 16; ++st) {
                std::sort(samples[st], samples[st] + NS);
                float d = 0.f;
                for (int i = 0; i < NS; ++i) {
                    float u = samples[st][i];
                    d = std::max(d, std::max(std::fabs((i + 1) / (float)NS - u),
                                             std::fabs(u - i / (float)NS)));
                }
                if (d > worst) { worst = d; worstStep = st; }
            }
            std::printf("    r=%+.1f worst per-step KS=%.4f (step %d, crit %.4f)\n",
                        r, worst, worstStep, ksCrit);
            EXPECT(worst < ksCrit);
        }
    });

    std::cout << std::string(50, '=') << "\n"
              << (s_fail == 0 ? "\033[32m" : "\033[31m")
              << s_pass << " passed, " << s_fail << " failed\033[0m\n";
    return s_fail == 0 ? 0 : 1;
}
