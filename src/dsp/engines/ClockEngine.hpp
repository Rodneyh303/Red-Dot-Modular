#pragma once
#include <rack.hpp>

// ── Minimal clamp helper — lives here so ClockEngine.cpp does not need
//    to include MeloDicer.hpp (which would create a circular dependency).
template <typename T>
static inline T clkClamp(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

struct ClockEngine {
    rack::dsp::SchmittTrigger clkTrig;

    // ── Internal accumulator (used when no ext clock) ───────────────────────
    float timeAcc        = 0.f;   // seconds accumulated since last internal 1/16 tick

    // ── External clock period measurement ──────────────────────────────────
    float clockTimeAcc   = 0.f;   // seconds since last CLK pulse
    float sixteenthSec   = 0.125f; // cached internal clock interval
    bool  measured       = false; // true after first period has been observed
    bool  externalActive = false; // true when CLK IN is connected and producing pulses

    // ── PPQN pulse grid ─────────────────────────────────────────────────────
    // ppqnSetting (24/48/96) is the master pulse resolution. pulsesPer16th =
    // ppqn/4 (6/12/24). The internal clock subdivides each 1/16 into that many
    // pulses; an external clock is ASSUMED to arrive at exactly ppqn PPQN (one
    // incoming pulse = one grid pulse) — a rate mismatch just scales tempo.
    int   ppqnCount      = 0;     // counts grid pulses within a 1/16; wraps at pulsesPer16th
    int   sixteenthCount = 0;     // counts 1/16 edges; wraps at 4 → quarter edge
    float pulseAcc       = 0.f;   // internal: seconds since last grid pulse

    // ── Outputs (valid after each call to process()) ────────────────────────
    bool  pulseEdge      = false; // one-sample pulse: one PPQN grid pulse elapsed
    bool  sixteenthEdge  = false; // one-sample pulse: one 1/16 step has elapsed
    bool  quarterEdge    = false; // one-sample pulse: one quarter note has elapsed
    float bpm            = 120.f; // current tempo (derived or knob)

    void reset();
    void process(float clockVoltage, bool clkConnected, float internalBpm, int ppqn, float sampleTime);

    static int pulsesPer16th(int ppqn) { return (ppqn > 0 ? ppqn : 24) / 4; }
};
