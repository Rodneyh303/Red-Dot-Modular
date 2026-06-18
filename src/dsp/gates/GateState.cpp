#include "GateState.hpp"
#include <cmath>
#include <algorithm>

// ── Note length helper ───────────────────────────────────────────────────────
// The note-value table lives in dsp/NoteValues.hpp (single source of truth).
float gs_noteSteps(int nvIdx) {
    return noteValueSteps(nvIdx);
}

// Arm the gate-close countdown from a duration in 1/16-steps, converted to PPQN
// grid pulses. ALL note lengths use this one mechanism — whole steps, triplets,
// and 1/32 alike are integer pulse counts (every value × pulsesPer16th is exact
// at 24/48/96 PPQN), so there is no whole-vs-fractional special case and no
// seconds timer racing the clock edge. holdRemain (the engine's step-unit
// DECISION counter) is untouched.
void GateState::armGate(float durSteps) {
    int p16 = (pulsesPer16th > 0) ? pulsesPer16th : 6;
    gatePulseRemain = (int)std::lround(durSteps * (float)p16);
    if (gatePulseRemain < 1) gatePulseRemain = 1;   // a sounding note is ≥1 pulse
}

// ── Core operations ───────────────────────────────────────────────────────────

void GateState::triggerNote(float pitchV, int semitone, int nvIdx) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gatePulse.trigger(1e-3f);
    gateHeld   = true;
    holdRemain = dur;
    armGate(dur);
    markSemi(semitone, dur);
}

void GateState::slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    if (wasHeld) {
        holdRemain += dur;
        gateHeld = true;            // stays open if tick() just zeroed holdRemain
        armGate(holdRemain);        // re-arm gate to the summed length (in pulses)
    } else {
        gatePulse.trigger(1e-3f);   // first note of legato run opens the gate
        holdRemain = dur;
        gateHeld   = true;
        armGate(dur);
    }
    markSemi(semitone, dur);
}

void GateState::slideMax(float pitchV, int semitone, int nvIdx) {
    float dur = gs_noteSteps(nvIdx);
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gateHeld   = true;
    holdRemain = dur;
    armGate(dur);
    markSemi(semitone, dur);
}

void GateState::extendHold(int semitone, int nvIdx) {
    float add = gs_noteSteps(nvIdx);
    holdRemain += add;
    gateHeld = true;
    armGate(holdRemain);            // re-arm to the summed length
    markSemi(semitone, holdRemain);
}

void GateState::rest(bool tieExtend, int nvIdx) {
    if (tieExtend && gateHeld) {
        holdRemain += gs_noteSteps(nvIdx);
        armGate(holdRemain);
    }
    // Otherwise: no-op — the gate's pulse countdown closes it cleanly.
}

// Tick: once per 1/16 step edge. Decrements the whole-step DECISION counter and
// the semi LED timers. Does NOT close the gate (tickPulse does, on the finer
// grid). p16 keeps the pulse resolution current for arming.
void GateState::tick(int p16) {
    if (p16 > 0) pulsesPer16th = p16;
    if (holdRemain > 0.f) {
        holdRemain -= 1.f;
        if (holdRemain <= 0.f) holdRemain = 0.f;
    }
    for (int i = 0; i < 12; ++i) {
        if (semiPlayRemain[i] > 0.f) {
            semiPlayRemain[i] -= 1.f;
            if (semiPlayRemain[i] < 0.f) semiPlayRemain[i] = 0.f;
        }
    }
}

// TickPulse: once per PPQN grid pulse. The sole gate-close mechanism.
void GateState::tickPulse() {
    if (gatePulseRemain >= 0) {
        gatePulseRemain -= 1;
        if (gatePulseRemain <= 0) {
            gatePulseRemain = -1;
            gateHeld        = false;
        }
    }
}

// Process: every sample. Only the 1ms retrigger dip remains here.
float GateState::process(float sampleTime) {
    if (!gateHeld) {
        gatePulse.process(sampleTime);
        return 0.f;
    }
    return gatePulse.process(sampleTime) ? 0.f : 10.f;
}

void GateState::tickPulseOnly(float sampleTime) {
    gatePulse.process(sampleTime);
}

void GateState::reset() {
    holdRemain      = 0.f;
    gateHeld        = false;
    gatePulseRemain = -1;
    currentPitchV   = 0.f;
    lastSemitone    = -1;
    gatePulse.reset();
    for (int i = 0; i < 12; ++i) semiPlayRemain[i] = 0.f;
}

// ── LED helper ────────────────────────────────────────────────────────────────
float GateState::semiLedBrightness(int semitone) const {
    if (semitone < 0 || semitone >= 12) return 0.f;
    return gs_clamp(semiPlayRemain[semitone] * 0.25f, 0.f, 1.f);
}

void GateState::markSemi(int semitone, float dur) {
    if (semitone >= 0 && semitone < 12)
        semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
}
