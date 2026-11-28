// test_slew_leak_probe.cpp — INVESTIGATION (CC_PROMPT_SCRUB32 Part A, step 1).
// NOT a fix and NOT a permanent pin — a probe that reproduces, through the REAL engine
// path (latchSlew/latchMix + refreshVisualCache), the mid-phrase slew leak the prompt
// describes, and prints the count so the report is a measurement, not an assumption.
//
// Contract (SLEW_COPULA_PLAN / PatternEngine.hpp ~1362): slew is a copula-WINDOW parameter,
// consumed ONLY at the draw boundary (the roll). It MUST be constant across a phrase. So
// `rhythmSlewApplied` (the engine's own record of which slew is baked into the buffers)
// must change ONLY at a roll (drawCtr advance), never from a mid-phrase mix/scrub move.
//
// MECHANISM under test (verified by reading, 2026-11-24): refreshVisualCache's mix branch
// calls recomputeEffectiveRhythmForVoice, which reads the LIVE rhythmSlewLatched; a changed
// slew misses the z-cache (keyed on r) and falls through to the FULL recomputeEffectiveRhythm,
// which sets rhythmSlewApplied = the live slew — mid-phrase, with NO roll.
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include <iostream>
#include <cstdio>

int main() {
    std::printf("== test_slew_leak_probe (Part A investigation) ==\n");

    const float slews[8] = {0.60f,0.65f,0.55f,0.70f,0.50f,0.62f,0.58f,0.68f};
    const float mixes[8] = {0.31f,0.33f,0.30f,0.35f,0.28f,0.34f,0.29f,0.36f};

    auto bakeBaseline = [](PatternEngine& pe) {
        // The ROLL-equivalent: a full recompute bakes the current slew and records *Applied.
        pe.latchSlew(0.60f, 0.f, 0.f);                 // r>0 -> the copula path is live
        pe.latchMix(0.30f, 0.f, 0.f, true, false, false);
        pe.recomputeEffectiveRhythm();
    };

    // ── CASE A: slew-only modulation mid-phrase (NO roll, mix held) ──
    int changesA = 0;
    {
        PatternEngine pe;
        pe.seedRhythmPhilox(0.42f);
        for (int k = 0; k < 5; ++k) pe.advanceRhythmDraw(+1);
        PatternInput in;
        bakeBaseline(pe);
        float prev = pe.rhythmSlewApplied;
        for (int k = 0; k < 8; ++k) {
            pe.latchSlew(slews[k], 0.f, 0.f);          // slew MOVES
            pe.latchMix(0.30f, 0.f, 0.f, true, false, false);  // mix HELD at the baked value
            pe.refreshVisualCache(in);                 // drawCtr==applied & mix==applied -> no branch
            if (pe.rhythmSlewApplied != prev) { ++changesA; prev = pe.rhythmSlewApplied; }
        }
    }

    // ── CASE B: slew AND mix modulated mid-phrase (NO roll) ──
    int changesB = 0;
    {
        PatternEngine pe;
        pe.seedRhythmPhilox(0.42f);
        for (int k = 0; k < 5; ++k) pe.advanceRhythmDraw(+1);
        PatternInput in;
        bakeBaseline(pe);
        float prev = pe.rhythmSlewApplied;
        for (int k = 0; k < 8; ++k) {
            pe.latchSlew(slews[k], 0.f, 0.f);          // slew MOVES
            pe.latchMix(mixes[k], 0.f, 0.f, true, false, false);  // mix MOVES (enters for-voice branch)
            pe.refreshVisualCache(in);
            if (pe.rhythmSlewApplied != prev) { ++changesB; prev = pe.rhythmSlewApplied; }
        }
    }

    std::printf("CASE A (slew-only, mix held)   : rhythmSlewApplied changed %d / 8 cycles  (contract: 0)\n", changesA);
    std::printf("CASE B (slew + mix modulated)  : rhythmSlewApplied changed %d / 8 cycles  (contract: 0)\n", changesB);
    std::printf("LEAK %s: a mid-phrase mix move %s pull the live slew into the buffers.\n",
                changesB > 0 ? "REPRODUCED" : "absent",
                changesB > 0 ? "DID" : "did not");
    // ASSERTING PIN (upgraded with the Part A fix, CC_PROMPT_SCRUB32): this file began as a
    // print-only probe — it MEASURED the leak on the pre-fix engine (CASE B: 7/8, the whole
    // red half of red/green, through the public API so it compiles on both sides). With the
    // CopulaWindowLatch wiring in, the contract is law and the probe asserts it: a mid-phrase
    // slew and/or mix move must never change the applied slew. If this goes red, the live
    // latch is leaking into a recompute path again.
    int fails = 0;
    if (changesA != 0) { std::printf("  FAIL: CASE A leaked (%d changes)\n", changesA); ++fails; }
    if (changesB != 0) { std::printf("  FAIL: CASE B leaked (%d changes)\n", changesB); ++fails; }
    return fails ? 1 : 0;
}
