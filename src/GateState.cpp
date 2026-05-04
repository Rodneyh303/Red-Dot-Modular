#include "GateState.hpp"
#include <cmath>
#include <algorithm>

// ── Note length helper: fraction of a whole note × 16 steps ──────────────────
const float GS_NOTE_FRACS[9] = {
    0.5f, 0.25f, 1.f/6.f, 0.125f, 1.f/12.f, 0.0625f, 1.f/24.f, 0.03125f, 1.f/48.f
};

float gs_noteSteps(int nvIdx) {
    if (nvIdx < 0 || nvIdx > 8) return 1.f;
    return std::max(0.5f, GS_NOTE_FRACS[nvIdx] * 16.f);
}

// ── Core operations ───────────────────────────────────────────────────────

// Play a new note: retrigger gate, set pitch and duration.
void GateState::triggerNote(float pitchV, int semitone, int nvIdx) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gatePulse.trigger(1e-3f);
    gateHeld   = true;
    holdRemain = dur;
    markSemi(semitone, dur);
}

// Legato slide: pitch changes, gate stays held (no retrigger), hold extends.
// If gate was already open: extends. If not: opens it (first note of legato run).
void GateState::slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    if (wasHeld) {
        holdRemain += dur;
        gateHeld = true; // Ensure it stays open if tick() just cleared it
    } else {
        gatePulse.trigger(1e-3f);   // first note of legato run opens gate
        holdRemain = dur;
        gateHeld   = true;
    }
    markSemi(semitone, dur);
}

// Legato max (knob fully right): gate always held, holdRemain = exact dur.
void GateState::slideMax(float pitchV, int semitone, int nvIdx) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gateHeld   = true;
    holdRemain = dur;
    markSemi(semitone, dur);
}

// Tie (same pitch): extend hold, no pitch or retrigger change.
void GateState::extendHold(int semitone, int nvIdx) {
    holdRemain += gs_noteSteps(nvIdx);
    gateHeld = true; // Ensure it stays open if tick() just cleared it
    markSemi(semitone, holdRemain);
}

// Rest: optionally extend hold (tie-across-steps), otherwise do nothing
// (let the current note ring to its natural end — holdRemain decays itself).
void GateState::rest(bool tieExtend, int nvIdx) {
    if (tieExtend && gateHeld)
        holdRemain += gs_noteSteps(nvIdx);
    // Otherwise: no-op — holdRemain counts down, gate drops cleanly
}

// Tick: call once per 1/16 step edge. Decrements hold, closes gate if expired.
void GateState::tick() {
    if (holdRemain > 0.f) {
        holdRemain -= 1.f;
        if (holdRemain <= 0.f) {
            holdRemain = 0.f;
            gateHeld   = false;
        }
    }

    // Decay semiPlay timers on each tick
    for (int i = 0; i < 12; ++i) {
        if (semiPlayRemain[i] > 0.f) {
            semiPlayRemain[i] -= 1.f;
            if (semiPlayRemain[i] < 0.f) semiPlayRemain[i] = 0.f;
        }
    }
}

// Process: call every sample. Returns raw gate voltage (0 or 10V).
// muted / invertGate applied by caller so this stays Rack-port-free.
float GateState::process(float sampleTime) {
    if (!gateHeld) {
        gatePulse.process(sampleTime);
        return 0.f;
    }
    // Dip the gate for 1ms if a new note was triggered
    return gatePulse.process(sampleTime) ? 0.f : 10.f;
}

// Tick gatePulse only (when not in Modes A/B — keeps timer consistent).
void GateState::tickPulseOnly(float sampleTime) {
    gatePulse.process(sampleTime);
}

// Reset all playback state.
void GateState::reset() {
    holdRemain    = 0.f;
    gateHeld      = false;
    currentPitchV = 0.f;
    lastSemitone  = -1;
    gatePulse.reset();
    for (int i = 0; i < 12; ++i) semiPlayRemain[i] = 0.f;
}

// ── LED helper ────────────────────────────────────────────────────────────
// semiPlayRemain normalised to 0..1 for setBrightness (1/4 note = full bright)
float GateState::semiLedBrightness(int semitone) const {
    if (semitone < 0 || semitone >= 12) return 0.f;
    return gs_clamp(semiPlayRemain[semitone] * 0.25f, 0.f, 1.f);
}

void GateState::markSemi(int semitone, float dur) {
    if (semitone >= 0 && semitone < 12)
        semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
}