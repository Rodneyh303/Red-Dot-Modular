#include "ClockEngine.hpp"

void ClockEngine::reset() {
    clkTrig.reset();
    timeAcc = clockTimeAcc = pulseAcc = 0.f;
    ppqnCount = sixteenthCount = 0;
    measured = externalActive = false;
    pulseEdge = sixteenthEdge = quarterEdge = false;
    bpm = 120.f;
}

// Unified PPQN pulse-grid clock. The grid runs at `ppqn` pulses-per-quarter;
// pulsesPer16th = ppqn/4. Each accepted grid pulse sets pulseEdge; every
// pulsesPer16th-th pulse also sets sixteenthEdge (and every 4th sixteenth sets
// quarterEdge). Gate lengths downstream are counted in grid pulses, so triplets
// and 1/32 are ordinary integer pulse counts — no seconds timer, no special case.
//
// External clock: ASSUMED to arrive at exactly `ppqn` PPQN — one incoming pulse =
// one grid pulse. A rate mismatch (e.g. menu says 24, cable is 48) simply scales
// tempo; that's the user's contract. We still derive bpm by measuring the 1/16
// interval for display + internal-fallback continuity.
void ClockEngine::process(float clockVoltage, bool clkConnected, float internalBpm, int ppqn, float sampleTime) {
    pulseEdge     = false;
    sixteenthEdge = false;
    quarterEdge   = false;
    clockTimeAcc += sampleTime;

    const int p16 = pulsesPer16th(ppqn);   // grid pulses per 1/16 (6/12/24)

    auto emitPulse = [&]() {
        pulseEdge = true;
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
            // Measure tempo on the 1/16 boundary (start of each p16 group).
            if (ppqnCount == 0) {
                if (measured && clockTimeAcc > 0.001f && clockTimeAcc < 10.f) {
                    float derivedBpm = (60.f * 4.f) / clockTimeAcc; // 4 sixteenths/beat
                    bpm = clkClamp(derivedBpm, 20.f, 300.f);
                    sixteenthSec = 15.f / bpm;
                }
                measured = true;
                clockTimeAcc = 0.f;
            }
            emitPulse();   // one incoming pulse = one grid pulse
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

        // Internal grid: subdivide the 1/16 into p16 pulses.
        float pulseSec = sixteenthSec / std::max(1, p16);
        pulseAcc += sampleTime;
        if (pulseAcc >= pulseSec) {
            pulseAcc -= pulseSec;
            emitPulse();
        }
    }
}
