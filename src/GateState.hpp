#pragma once
/**
 * GateState.hpp
 * Playback gate engine for MeloDicer.
 *
 * Owns:
 *   holdRemain    — steps remaining in current gate (1.0 = one 1/16 step)
 *   gateHeld      — gate is currently open
 *   gatePulse     — 1ms retrigger pulse for envelope re-triggers
 *   currentPitchV — current 1V/oct output voltage
 *   lastSemitone  — semitone of the last triggered note (for tie detection)
 *   semiPlayRemain[12] — per-semitone LED flash timer (steps)
 *
 * Interface:
 *   triggerNote()   — play a note (retrigger gate, update pitch)
 *   slideNote()     — legato: update pitch, extend hold, no retrigger
 *   extendHold()    — tie: extend hold, no pitch/retrigger change
 *   rest()          — optional tie-extend on rest; otherwise let ring naturally
 *   tick()          — call once per step edge; decrements hold, clears gate
 *   process()       — call every sample; ticks gatePulse, returns gate voltage
 *   reset()         — clear all state (on module reset)
 *
 * No Rack port types leak through this interface — caller reads ports,
 * calls the appropriate method, reads gateVoltage() for output.
 */

#include <rack.hpp>
#include <cmath>
#include <algorithm>


template<typename T>
static inline T gs_clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

// ── Note length helper: fraction of a whole note × 16 steps ──────────────────
// Matches MeloDicer::NOTEVALS fractions exactly.
extern const float GS_NOTE_FRACS[9];
extern float gs_noteSteps(int nvIdx);

// ── GateState ─────────────────────────────────────────────────────────────────
struct GateState {

    // ── State ─────────────────────────────────────────────────────────────────
    float  holdRemain      = 0.f;   // steps remaining (step-clock units)
    bool   gateHeld        = false;
    float  currentPitchV   = 0.f;   // 1V/oct output
    int    lastSemitone    = -1;    // for tie detection
    float  semiPlayRemain[12] = {}; // per-semitone flash timers (steps)

    rack::dsp::PulseGenerator gatePulse;  // 1ms retrigger for envelopes

    // ── Core operations ───────────────────────────────────────────────────────
    // Play a new note: retrigger gate, set pitch and duration.
    void triggerNote(float pitchV, int semitone, int nvIdx);
    // Legato slide: pitch changes, gate stays held (no retrigger), hold extends.
    // If gate was already open: extends. If not: opens it (first note of legato run).
    void slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld);
    // Legato max (knob fully right): gate always held, holdRemain = exact dur.
    void slideMax(float pitchV, int semitone, int nvIdx);
    // Tie (same pitch): extend hold, no pitch or retrigger change.
    void extendHold(int semitone, int nvIdx);
    // Rest: optionally extend hold (tie-across-steps), otherwise do nothing
    // (let the current note ring to its natural end — holdRemain decays itself).
    void rest(bool tieExtend, int nvIdx);
    // Tick: call once per 1/16 step edge. Decrements hold, closes gate if expired.
    void tick();
    // Process: call every sample. Returns raw gate voltage (0 or 10V).
    // muted / invertGate applied by caller so this stays Rack-port-free.
    float process(float sampleTime);
    // Tick gatePulse only (when not in Modes A/B — keeps timer consistent).
    void tickPulseOnly(float sampleTime);
    // Reset all playback state.
    void reset();
    // ── LED helper ────────────────────────────────────────────────────────────
    // semiPlayRemain normalised to 0..1 for setBrightness (1/4 note = full bright)
    float semiLedBrightness(int semitone) const;
    void markSemi(int semitone, float dur);
};
