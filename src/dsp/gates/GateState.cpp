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
// Fraction of a WHOLE note per dial position. MUST match NOTEVALS[] in
// SequencerEngine.cpp and the dial labels in MonsoonConfigurator.cpp:
//   idx 0..7 = 1/1, 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/32
// (Previously this table was shifted one slot — 1/4 played as 1/4T, and
//  1/8/1/8T/1/16/1/32 all collapsed to a single 1/16 step on the gate.)
const float GS_NOTE_FRACS[8] = {
    1.0f, 0.5f, 0.25f, 1.f/6.f, 0.125f, 1.f/12.f, 0.0625f, 0.03125f
};

float gs_noteSteps(int nvIdx) {
    if (nvIdx < 0 || nvIdx > 7) return 1.f;
    // No whole-step floor: sub-step lengths (1/32 = 0.5 steps, 1/16T = 0.667,
    // 1/8T = 1.333) are rendered as fractional gate closes by tick()/process().
    return GS_NOTE_FRACS[nvIdx] * 16.f;
}

// ── Core operations ───────────────────────────────────────────────────────────

// Helper: set holdRemain (whole steps) + pendingFrac (sub-step remainder) from a
// possibly-fractional step duration. sixteenthSec>0 enables sub-step rendering;
// if 0 (e.g. externally-gated modes) we fall back to ceiling-to-whole-step.
void GateState::setDuration(float durSteps, float sixteenthSec) {
    float whole = std::floor(durSteps + 1e-6f);
    float frac  = durSteps - whole;
    if (sixteenthSec <= 0.f) {            // no sub-step timing available
        holdRemain  = std::max(1.f, std::round(durSteps));
        pendingFrac = 0.f;
        fracGateSec = -1.f;
        return;
    }
    if (whole < 1.f) {                    // note shorter than one step
        holdRemain  = 0.f;
        pendingFrac = 0.f;
        fracGateSec = std::max(frac, 0.f) * sixteenthSec;  // open sub-step now
    } else {
        holdRemain  = whole;
        pendingFrac = (frac > 1e-6f) ? frac : 0.f;
        fracGateSec = -1.f;
    }
}

// Play a new note: retrigger gate, set pitch and duration.
void GateState::triggerNote(float pitchV, int semitone, int nvIdx, float sixteenthSec) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gatePulse.trigger(1e-3f);
    gateHeld   = true;
    setDuration(dur, sixteenthSec);
    markSemi(semitone, dur);
}

// Legato slide: pitch changes, gate stays held (no retrigger), hold extends.
// If gate was already open: extends.  If not: opens it (first note of legato run).
void GateState::slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld, float sixteenthSec) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    if (wasHeld) {
        // Extending an open gate: re-fold any pending fractional remainder into
        // the running length, then re-split. Keeps legato runs gapless while
        // still ending the final note on its true sub-step boundary.
        float running = holdRemain + pendingFrac + dur;
        setDuration(running, sixteenthSec);
        gateHeld = true;   // ensure stays open if tick() just cleared it
    } else {
        gatePulse.trigger(1e-3f);   // first note of legato run opens gate
        gateHeld = true;
        setDuration(dur, sixteenthSec);
    }
    markSemi(semitone, dur);
}

// Legato max (knob fully right): gate always held, holdRemain = exact dur.
void GateState::slideMax(float pitchV, int semitone, int nvIdx, float sixteenthSec) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gateHeld   = true;
    setDuration(dur, sixteenthSec);
    markSemi(semitone, dur);
}

// Tie (same pitch): extend hold, no pitch or retrigger change.
void GateState::extendHold(int semitone, int nvIdx, float sixteenthSec) {
    float running = holdRemain + pendingFrac + gs_noteSteps(nvIdx);
    setDuration(running, sixteenthSec);
    gateHeld = true;   // ensure stays open if tick() just cleared it
    markSemi(semitone, running);
}

// Rest: optionally extend hold (tie-across-steps), otherwise do nothing
// (let the current note ring to its natural end — holdRemain decays itself).
void GateState::rest(bool tieExtend, int nvIdx) {
    if (tieExtend && gateHeld)
        holdRemain += gs_noteSteps(nvIdx);
    // Otherwise: no-op — holdRemain counts down, gate drops cleanly
}

// Tick: call once per 1/16 step edge.  Decrements hold, closes gate if expired.
// sixteenthSec is the current step duration (for arming the fractional tail).
void GateState::tick(float sixteenthSec) {
    if (holdRemain > 0.f) {
        holdRemain -= 1.f;
        if (holdRemain <= 0.f) {
            holdRemain = 0.f;
            // Last whole step done. If there's a sub-step remainder, arm it so
            // the gate stays open a fraction of THIS step then drops mid-step.
            if (pendingFrac > 1e-6f && sixteenthSec > 0.f) {
                fracGateSec = pendingFrac * sixteenthSec;
                pendingFrac = 0.f;
            } else if (fracGateSec < 0.f) {
                gateHeld = false;            // clean whole-step close (unchanged)
            }
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
    // Fractional final-step close: if armed, hold the gate for the sub-step
    // duration then drop it mid-step (1/32, triplet tails).
    if (fracGateSec >= 0.f) {
        fracGateSec -= sampleTime;
        if (fracGateSec <= 0.f) {
            fracGateSec = -1.f;
            gateHeld    = false;
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
    fracGateSec   = -1.f;
    pendingFrac   = 0.f;
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
