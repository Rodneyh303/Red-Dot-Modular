#include "ClockEngine.hpp"
#include "MeloDicer.hpp"

void ClockEngine::reset() {
    clkTrig.reset();
    timeAcc = clockTimeAcc = 0.f;
    ppqnCount = sixteenthCount = 0;
    measured = externalActive = false;
    sixteenthEdge = quarterEdge = false;
    bpm = 120.f;
}

void ClockEngine::process(float clockVoltage, bool clkConnected, float internalBpm, int ppqn, float sampleTime) {
    sixteenthEdge = false;
    quarterEdge   = false;
    clockTimeAcc += sampleTime;

    if (clkConnected) {
        externalActive = true;
        timeAcc = 0.f;
        if (!measured) bpm = clampv(internalBpm, 20.f, 300.f);

        if (clkTrig.process(clockVoltage, 0.1f, 2.f)) {
            if (ppqnCount == 0) {
                sixteenthEdge = true;
                
                // Stabilize BPM by measuring the full 1/16th note interval instead of a single pulse
                if (measured && clockTimeAcc > 0.001f && clockTimeAcc < 10.f) {
                    float derivedBpm = (60.f * 4.f) / clockTimeAcc; // 4 sixteenths per beat
                    bpm = clampv(derivedBpm, 20.f, 300.f);
                }
                measured = true;
                clockTimeAcc = 0.f;

                if (sixteenthCount == 0) quarterEdge = true;
                sixteenthCount = (sixteenthCount + 1) & 3; // mod 4
            }
            ppqnCount = (ppqnCount + 1) % std::max(1, ppqn);
        }
    } else {
        externalActive   = false;
        measured         = false;
        clockTimeAcc     = 0.f;
        ppqnCount        = 0;
        bpm              = clampv(internalBpm, 20.f, 300.f);

        float sixteenthSec = (4.f * 60.f / bpm) / 16.f;
        timeAcc += sampleTime;
        if (timeAcc >= sixteenthSec) {
            timeAcc       -= sixteenthSec;
            sixteenthEdge  = true;
            if (sixteenthCount == 0) quarterEdge = true;
            sixteenthCount = (sixteenthCount + 1) & 3;
        }
    }
}