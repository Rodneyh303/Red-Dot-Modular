#pragma once
#include <rack.hpp>
#include "../NoteValues.hpp"   // PULSES_PER_STEP — the step grid (CC_PROMPT_PPQN_48)

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
    // ppqnSetting (24/48) is the master pulse resolution. pulsesPer16th =
    // ppqn/4 (6/12) — the MUSICAL subdivision (a 1/16 is that many grid pulses);
    // the STEP is PULSES_PER_STEP (6) pulses at ANY PPQN (decision 1 — a step is
    // 6 pulses, fixed), so at 48 the step rate doubles at the same BPM.
    // The internal clock subdivides each 1/16 into that many grid pulses (at 48
    // that is 12 — exactly 2× the pulses per bar of 24, the doc's stage 4).
    // An external clock is ASSUMED to arrive at the standard 24 PPQN (one
    // incoming pulse = one 24-grid pulse); at ppqn 48 each incoming pulse is
    // MULTIPLIED ×2 (ppqn/24) into grid pulses — the doc's stage 3: a 24 PPQN
    // external clock at the same knob tempo drives the same STEP rate, on the
    // finer grid. A rate mismatch now just scales tempo as before.
    int   ppqnCount      = 0;     // counts grid pulses within a 1/16; wraps at pulsesPer16th
    int   stepPulseCount = 0;     // counts grid pulses within a STEP; wraps at PULSES_PER_STEP
    int   sixteenthCount = 0;     // counts 1/16 edges; wraps at 4 → quarter edge
    float pulseAcc       = 0.f;   // internal: seconds since last grid pulse

    // ── Outputs (valid after each call to process()) ────────────────────────
    bool  pulseEdge      = false; // one-sample pulse: one PPQN grid pulse elapsed
    bool  stepEdge       = false; // one-sample pulse: ONE STEP elapsed (PULSES_PER_STEP
                                  // grid pulses — the pattern's advance edge at any PPQN;
                                  // at 24 it coincides with sixteenthEdge bit-for-bit)
    bool  sixteenthEdge  = false; // one-sample pulse: one 1/16 has elapsed (musical)
    bool  quarterEdge    = false; // one-sample pulse: one quarter note has elapsed
    float bpm            = 120.f; // current tempo (derived or knob)

    void reset();
    void process(float clockVoltage, bool clkConnected, float internalBpm, int ppqn, float sampleTime);

    static int pulsesPer16th(int ppqn) { return (ppqn > 0 ? ppqn : 24) / 4; }
};
