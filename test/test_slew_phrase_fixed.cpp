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

    std::cout << std::string(50, '=') << "\n"
              << (s_fail == 0 ? "\033[32m" : "\033[31m")
              << s_pass << " passed, " << s_fail << " failed\033[0m\n";
    return s_fail == 0 ? 0 : 1;
}
