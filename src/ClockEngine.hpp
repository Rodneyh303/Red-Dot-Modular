#pragma once
#include <rack.hpp>

struct ClockEngine {
    rack::dsp::SchmittTrigger clkTrig;

    // ── Internal accumulator (used when no ext clock) ───────────────────────
    float timeAcc        = 0.f;   // seconds accumulated since last internal 1/16 tick

    // ── External clock period measurement ──────────────────────────────────
    float clockTimeAcc   = 0.f;   // seconds since last CLK pulse
    bool  measured       = false; // true after first period has been observed
    bool  externalActive = false; // true when CLK IN is connected and producing pulses

    // ── PPQN clock divider ──────────────────────────────────────────────────
    int   ppqnCount      = 0;     // counts incoming CLK pulses; wraps at ppqn
    int   sixteenthCount = 0;     // counts 1/16 edges; wraps at 4 → quarter edge

    // ── Outputs (valid after each call to process()) ────────────────────────
    bool  sixteenthEdge  = false; // one-sample pulse: one 1/16 step has elapsed
    bool  quarterEdge    = false; // one-sample pulse: one quarter note has elapsed
    float bpm            = 120.f; // current tempo (derived or knob)

    void reset();
    void process(float clockVoltage, bool clkConnected, float internalBpm, int ppqn, float sampleTime);
};