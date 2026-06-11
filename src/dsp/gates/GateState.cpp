#include "GateState.hpp"
#include <cmath>
#include <algorithm>

// ── Note length fracs: fraction of a whole note × 16 steps ───────────────────
//
// Index mapping (matches NOTEVALS[8] in SequencerEngine and the NOTE_VALUE_PARAM
// labels "1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32"):
//
//  idx 0  0.5      → 8 steps   (half note)
//  idx 1  0.25     → 4 steps   (quarter note)
//  idx 2  1/6      → 2.667     (quarter-note triplet)
//  idx 3  0.125    → 2 steps   (eighth note)
//  idx 4  1/12     → 1.333     (eighth-note triplet)
//  idx 5  0.0625   → 1 step    (sixteenth note)
//  idx 6  1/24     → 0.667     (sixteenth-note triplet)   ← label was wrongly "1/32T"
//  idx 7  0.03125  → 0.5 steps (thirty-second note, minimum floor)
//
// Index 7 is the last valid entry.  The array has exactly 8 entries.
// (A ninth 1/48 entry was removed; it was unreachable and mis-labelled.)
//
// Must match NOTEVALS[8] (SequencerEngine) and the dial labels
// "1/1,1/2,1/4,1/4T,1/8,1/8T,1/16,1/32". Commit bb4cb72 reordered the labels +
// NOTEVALS to interleave triplets but left this table in the old order, desyncing
// dial position from gate length.
const float GS_NOTE_FRACS[8] = {
    1.0f, 0.5f, 0.25f, 1.f/6.f, 0.125f, 1.f/12.f, 0.0625f, 0.03125f
};

float gs_noteSteps(int nvIdx) {
    if (nvIdx < 0 || nvIdx > 7) return 1.f;
    // No whole-step floor: sub-step lengths (1/32, triplets) render exactly via
    // the per-sample gate-seconds countdown (see setGateSeconds/process).
    return GS_NOTE_FRACS[nvIdx] * 16.f;
}

// Convert a duration in 1/16-steps to a precise gate-open time in seconds and
// arm the per-sample countdown. This governs the gate VOLTAGE only; holdRemain
// (whole-step counter) is untouched so the engine's existing tick-based decision
// logic — MidNote (holdRemain>=1), hadTail/canRest (fractional prevHold) — is
// unchanged. sixteenthSec<=0 (externally-gated modes) disables sub-step timing.
void GateState::setGateSeconds(float durSteps, float sixteenthSec) {
    gateSecRemain = (sixteenthSec > 0.f) ? durSteps * sixteenthSec : -1.f;
}

// ── Core operations ───────────────────────────────────────────────────────────

// Play a new note: retrigger gate, set pitch and duration.
void GateState::triggerNote(float pitchV, int semitone, int nvIdx, float sixteenthSec) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gatePulse.trigger(1e-3f);
    gateHeld   = true;
    holdRemain = dur;
    setGateSeconds(dur, sixteenthSec);   // precise gate length (sub-step aware)
    markSemi(semitone, dur);
}

// Legato slide: pitch changes, gate stays held (no retrigger), hold extends.
// If gate was already open: extends.  If not: opens it (first note of legato run).
void GateState::slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld, float sixteenthSec) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    if (wasHeld) {
        holdRemain += dur;
        gateHeld = true;   // ensure stays open if tick() just cleared it
        if (gateSecRemain >= 0.f) gateSecRemain += dur * sixteenthSec;  // extend
        else setGateSeconds(holdRemain, sixteenthSec);
    } else {
        gatePulse.trigger(1e-3f);   // first note of legato run opens gate
        holdRemain = dur;
        gateHeld   = true;
        setGateSeconds(dur, sixteenthSec);
    }
    markSemi(semitone, dur);
}

// Legato max (knob fully right): gate always held, holdRemain = exact dur.
void GateState::slideMax(float pitchV, int semitone, int nvIdx, float sixteenthSec) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gateHeld   = true;
    holdRemain = dur;
    setGateSeconds(dur, sixteenthSec);
    markSemi(semitone, dur);
}

// Tie (same pitch): extend hold, no pitch or retrigger change.
void GateState::extendHold(int semitone, int nvIdx, float sixteenthSec) {
    float add = gs_noteSteps(nvIdx);
    holdRemain += add;
    gateHeld = true;   // ensure stays open if tick() just cleared it
    if (gateSecRemain >= 0.f) gateSecRemain += add * sixteenthSec;
    else setGateSeconds(holdRemain, sixteenthSec);
    markSemi(semitone, holdRemain);
}

// Rest: optionally extend hold (tie-across-steps), otherwise do nothing
// (let the current note ring to its natural end — holdRemain decays itself).
void GateState::rest(bool tieExtend, int nvIdx) {
    if (tieExtend && gateHeld)
        holdRemain += gs_noteSteps(nvIdx);
    // Otherwise: no-op — holdRemain counts down, gate drops cleanly
}

// Tick: call once per 1/16 step edge.  Decrements the whole-step hold counter
// (used by the engine's decision logic). When gateSecRemain is governing the
// gate (>=0), the actual gate close is handled per-sample in process() for exact
// sub-step timing, so tick() does NOT force gateHeld=false here.
void GateState::tick() {
    if (holdRemain > 0.f) {
        holdRemain -= 1.f;
        if (holdRemain <= 0.f) {
            holdRemain = 0.f;
            if (gateSecRemain < 0.f) gateHeld = false;  // no precise timer: close at edge
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

// Process: call every sample.  Returns raw gate voltage (0 or 10V).
// muted / invertGate applied by caller so this stays Rack-port-free.
float GateState::process(float sampleTime) {
    // Precise gate length: when armed (>=0), the gate-open time is counted in
    // seconds so sub-step lengths (1/32, triplets) close exactly, mid-step.
    if (gateSecRemain >= 0.f) {
        gateSecRemain -= sampleTime;
        if (gateSecRemain <= 0.f) {
            gateSecRemain = -1.f;
            gateHeld      = false;
        }
    }
    if (!gateHeld) {
        gatePulse.process(sampleTime);
        return 0.f;
    }
    // Dip the gate for 1 ms if a new note was triggered
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
    gateSecRemain = -1.f;
    currentPitchV = 0.f;
    lastSemitone  = -1;
    gatePulse.reset();
    for (int i = 0; i < 12; ++i) semiPlayRemain[i] = 0.f;
}

// ── LED helper ────────────────────────────────────────────────────────────────
// semiPlayRemain normalised to 0..1 for setBrightness (1/4 note = full bright)
float GateState::semiLedBrightness(int semitone) const {
    if (semitone < 0 || semitone >= 12) return 0.f;
    return gs_clamp(semiPlayRemain[semitone] * 0.25f, 0.f, 1.f);
}

void GateState::markSemi(int semitone, float dur) {
    if (semitone >= 0 && semitone < 12)
        semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
}
