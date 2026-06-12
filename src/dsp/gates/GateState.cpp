#include "GateState.hpp"
#include <cmath>
#include <algorithm>

// ── Note length helper ───────────────────────────────────────────────────────
// The note-value table now lives in dsp/NoteValues.hpp (single source of truth,
// shared with SequencerEngine's PPQN gating and the dial labels). gs_noteSteps
// is a thin wrapper over noteValueSteps() so existing call sites are unchanged.
float gs_noteSteps(int nvIdx) {
    return noteValueSteps(nvIdx);
}

// Arm the precise per-sample gate countdown from a duration in 1/16-steps, using
// the step length stashed by tick() (curStepSec). Governs the gate VOLTAGE only;
// holdRemain (whole-step counter) is untouched so the engine's tick-based
// decision logic — MidNote (holdRemain>=1), hadTail/canRest (fractional
// prevHold) — is unchanged. curStepSec<=0 (externally-gated modes, before any
// tick) disables sub-step timing and the gate falls back to whole-step close.
void GateState::armGate(float durSteps) {
    gateSecRemain = (curStepSec > 0.f) ? durSteps * curStepSec : -1.f;
}

// ── Core operations ───────────────────────────────────────────────────────────

// Play a new note: retrigger gate, set pitch and duration.
void GateState::triggerNote(float pitchV, int semitone, int nvIdx) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gatePulse.trigger(1e-3f);
    gateHeld   = true;
    holdRemain = dur;
    armGate(dur);   // precise gate length (sub-step aware)
    markSemi(semitone, dur);
}

// Legato slide: pitch changes, gate stays held (no retrigger), hold extends.
// If gate was already open: extends.  If not: opens it (first note of legato run).
void GateState::slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    if (wasHeld) {
        holdRemain += dur;
        gateHeld = true;   // ensure stays open if tick() just cleared it
        if (gateSecRemain >= 0.f && curStepSec > 0.f) gateSecRemain += dur * curStepSec;  // extend
        else armGate(holdRemain);
    } else {
        gatePulse.trigger(1e-3f);   // first note of legato run opens gate
        holdRemain = dur;
        gateHeld   = true;
        armGate(dur);
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
    armGate(dur);
    markSemi(semitone, dur);
}

// Tie (same pitch): extend hold, no pitch or retrigger change.
void GateState::extendHold(int semitone, int nvIdx) {
    float add = gs_noteSteps(nvIdx);
    holdRemain += add;
    gateHeld = true;   // ensure stays open if tick() just cleared it
    if (gateSecRemain >= 0.f && curStepSec > 0.f) gateSecRemain += add * curStepSec;
    else armGate(holdRemain);
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
void GateState::tick(float sixteenthSec) {
    if (sixteenthSec > 0.f) curStepSec = sixteenthSec;   // for sub-step gate timing
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
    curStepSec    = 0.f;
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
