#include "ClockEngine.hpp"

void ClockEngine::reset() {
    clkTrig.reset();
    timeAcc = clockTimeAcc = pulseAcc = 0.f;
    ppqnCount = stepPulseCount = sixteenthCount = 0;
    measured = externalActive = false;
    pulseEdge = stepEdge = sixteenthEdge = quarterEdge = false;
    bpm = 120.f;
}

// Unified PPQN pulse-grid clock. The grid runs at ppqn pulses-per-quarter;
// pulsesPer16th = ppqn/4 — the MUSICAL subdivision. Each accepted grid pulse sets
// pulseEdge; every PULSES_PER_STEP-th pulse also sets stepEdge (the pattern's
// advance edge — a STEP is 6 pulses, FIXED, decision 1: at 24 a step is a 1/16 so
// stepEdge coincides with sixteenthEdge bit-for-bit; at 48 it is a 1/32, so the
// step rate doubles at the same BPM); every pulsesPer16th-th pulse also sets
// sixteenthEdge (the MUSICAL 1/16 edge, and every 4th sixteenth sets quarterEdge).
// Gate lengths downstream are counted in grid pulses, so triplets and 1/32 are
// ordinary integer pulse counts — no seconds timer, no special case.
//
// External clock: ASSUMED to arrive at the standard 24 PPQN, so one incoming pulse
// = one 24-grid pulse. At ppqn 48 each incoming pulse is MULTIPLIED by 2 (ppqn/24)
// into grid pulses (CC_PROMPT_PPQN_48 stage 3). The MUSICAL 1/16 rate is preserved:
// a sixteenth still spans the same time as at 24, so sixteenthEdge fires at the same
// rate. The STEP rate DOUBLES: a step is PULSES_PER_STEP (6) grid pulses, which is a
// 1/32 at 48, so steps fire twice per sixteenth. That is intended (32 steps = one bar
// at 1/32, at the same tempo). The multiplier fixes the double-speed bug of the old
// assume-matching contract (PPQN_48_96_NOT_SAFE). A rate mismatch still just scales
// tempo; that's the user's contract. We still derive bpm by measuring the 1/16 interval
// for display + internal-fallback continuity.
void ClockEngine::process(float clockVoltage, bool clkConnected, float internalBpm, int ppqn, float sampleTime) {
    pulseEdge     = false;
    stepEdge      = false;
    sixteenthEdge = false;
    quarterEdge   = false;
    clockTimeAcc += sampleTime;

    const int p16  = pulsesPer16th(ppqn);   // grid pulses per 1/16 — MUSICAL (6/12)
    // The external-clock multiplier (stage 3): the incoming stream is assumed 24
    // PPQN, so the grid rate is ppqn/24 incoming pulses per grid pulse. 1 at 24
    // (bit-identical to the old one-pulse-one-grid contract), 2 at 48. The legacy
    // sub-24 settings keep 1 (their historic assume-matching behaviour).
    const int mult = (ppqn > 24) ? (ppqn / 24) : 1;

    auto emitPulse = [&]() {
        // OR-combine, never overwrite: at 48 the external multiplier emits TWO grid
        // pulses per incoming edge in one process() call — a step/1/16 boundary on
        // the FIRST of the pair must survive the second emit's bookkeeping.
        pulseEdge = true;
        // THE STEP EDGE (decision 1): every PULSES_PER_STEP-th grid pulse. The
        // counter runs in lockstep with ppqnCount at 24 (both wrap at 6 from the
        // same origin), so there stepEdge == sixteenthEdge exactly; at 48 it fires
        // twice per 1/16.
        if (stepPulseCount == 0) stepEdge = true;
        stepPulseCount = (stepPulseCount + 1) % PULSES_PER_STEP;
        if (ppqnCount == 0) {
            sixteenthEdge = true;
            if (sixteenthCount == 0) quarterEdge = true;
            sixteenthCount = (sixteenthCount + 1) & 3;   // mod 4
        }
        ppqnCount = (ppqnCount + 1) % std::max(1, p16);
    };

    if (clkConnected) {
        externalActive = true;
        timeAcc = 0.f;
        if (!measured) bpm = clkClamp(internalBpm, 20.f, 300.f);

        if (clkTrig.process(clockVoltage, 0.1f, 2.f)) {
            // Measure tempo on the 1/16 boundary (start of each p16 group). With
            // the multiplier the incoming edges land on ppqnCount in steps of
            // `mult`, so the ==0 probe still hits exactly once per musical 1/16
            // (every p16/mult incoming pulses) — the measured period is the real
            // 1/16 time at either PPQN.
            if (ppqnCount == 0) {
                if (measured && clockTimeAcc > 0.001f && clockTimeAcc < 10.f) {
                    float derivedBpm = (60.f * 4.f) / clockTimeAcc; // 4 sixteenths/beat
                    bpm = clkClamp(derivedBpm, 20.f, 300.f);
                    sixteenthSec = 15.f / bpm;
                }
                measured = true;
                clockTimeAcc = 0.f;
            }
            for (int m = 0; m < mult; ++m) emitPulse();   // ×ppqn/24 grid pulses per incoming pulse
        }
    } else {
        externalActive = false;
        measured       = false;
        clockTimeAcc   = 0.f;

        float nextBpm = clkClamp(internalBpm, 20.f, 300.f);
        if (std::abs(nextBpm - bpm) > 0.001f) {
            bpm = nextBpm;
            sixteenthSec = 15.f / bpm;
        }

        // Internal grid: subdivide the 1/16 into p16 grid pulses (12 at 48 —
        // exactly 2× the pulses per bar of 24; the doc's stage 4, no other change:
        // step counts per 1/16 are a STEP-domain property and the step edge above
        // fires every PULSES_PER_STEP-th pulse regardless).
        float pulseSec = sixteenthSec / std::max(1, p16);
        pulseAcc += sampleTime;
        if (pulseAcc >= pulseSec) {
            pulseAcc -= pulseSec;
            emitPulse();
        }
    }
}
