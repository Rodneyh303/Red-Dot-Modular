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
// and 1/32 alike are integer pulse counts (every value resolves to an exact
// integer pulse count at 24 and at 48 — noteValuePulses), so there is no
// whole-vs-fractional special case and no seconds timer racing the clock edge.
// The step-unit hold (holdRemain) is armed from the SAME pulse count / 6.
void GateState::armGate(float durSteps) {
    int p16 = (pulsesPer16th > 0) ? pulsesPer16th : 6;
    gatePulseRemain = (int)std::lround(durSteps * (float)p16);
    if (gatePulseRemain < 1) gatePulseRemain = 1;   // a sounding note is ≥1 pulse
}

// Arm from the note table's own exact pulse count (CC_PROMPT_PPQN_48): no
// rounding at all — the MUSICAL duration in grid pulses (a 1/16 is 6 pulses at
// 24 and 12 at 48, the same TIME either way).
void GateState::armGatePulses(int pulses) {
    gatePulseRemain = (pulses < 1) ? 1 : pulses;   // a sounding note is ≥1 pulse
}

// ── Core operations ──────────────────────────────────────────────────────
// The note→hold conversion, ONE place: the note's MUSICAL pulse count drives the
// gate width, and the SAME count / PULSES_PER_STEP drives the step-unit hold +
// LED marks (a step is 6 pulses, FIXED — at 24 the division reproduces the old
// 1/16-unit floats bit-for-bit; at 48 the holds read the doubled step rate).
static inline float noteHoldSteps(int nvIdx, int ppqn) {
    return (float)noteValuePulses(nvIdx, ppqn) / (float)PULSES_PER_STEP;
}
static inline int noteHoldPulses(int nvIdx, int ppqn) {
    return noteValuePulses(nvIdx, ppqn);
}

void GateState::triggerNote(float pitchV, int semitone, int nvIdx, bool isQuant) {
    const int   pp  = (ppqn > 0) ? ppqn : 24;
    const int   p   = noteHoldPulses(nvIdx, pp);
    const float dur = noteHoldSteps(nvIdx, pp);   // STEP units
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gatePulse.trigger(1e-3f);
    gateHeld   = true;
    holdRemain = dur;
    armGatePulses(p);
    markSemi(semitone, dur, isQuant);
    lastNoteType = NoteType::Single;        // a fresh attack (incl. an opt-out re-strike)
}

// THE ONE JOIN (the tie/legato collapse). Replaces the old slideNote/extendHold
// pair, whose only differences were (a) extendHold did not write currentPitchV,
// (b) it did not write lastSemitone, (c) it passed the SUMMED holdRemain to
// markSemi instead of this note's dur, and (d) the label. (a) was a latent
// octave-discard bug — benign ONLY because the caller's test happened to be the
// strict one; (b) was a provable no-op (the tie test includes sem == heldSem);
// (c) is a real LED rule and is kept, derived from the same predicate; (d) is now
// derived. Writing the drawn pitch unconditionally makes the discard class
// UNREPRESENTABLE: on a tie the drawn pitch is within kPitchEps of the held one
// by construction, so the stored value moves by at most ~0.12 cents and moves
// TOWARDS the value actually drawn.
GateState::NoteType GateState::continueNote(float pitchV, int semitone, int nvIdx,
                                            bool wasHeld, bool isQuant) {
    const int   pp  = (ppqn > 0) ? ppqn : 24;
    const int   p   = noteHoldPulses(nvIdx, pp);
    const float dur = noteHoldSteps(nvIdx, pp);   // STEP units
    // The label, derived BEFORE the write (the compare needs the held values).
    // `wasHeld &&` is deliberate but currently REDUNDANT: tie is read only inside
    // the wasHeld branch below, so a cold open is Single by the branch, not by the
    // predicate (test_continue_note's M2 survives structurally for that reason).
    // It is kept because it is what would hold if this ever returned the bool.
    const bool tie = wasHeld && isSameAsHeld(pitchV, semitone);
    currentPitchV = pitchV;        // ALWAYS — never discard the drawn octave
    lastSemitone  = semitone;      // ALWAYS — a no-op on a tie, by the predicate
    if (wasHeld) {
        holdRemain += dur;
        gateHeld = true;            // stays open if tick() just zeroed holdRemain
        armGatePulses((int)std::lround(holdRemain * (float)PULSES_PER_STEP));
                                    // re-arm gate to the summed length (exact pulses)
        lastNoteType = tie ? NoteType::Tie : NoteType::Legato;
        // LED rule, same predicate: a TIE is ONE sustained note, so its fader stays
        // lit for the whole remaining hold; a SLIDE lights the NEW degree's fader for
        // that note's own length (feeding the summed hold there would smear every
        // fader in a legato run).
        markSemi(semitone, tie ? holdRemain : dur, isQuant);
    } else {
        gatePulse.trigger(1e-3f);   // first note of legato run opens the gate
        holdRemain = dur;
        gateHeld   = true;
        armGatePulses(p);
        lastNoteType = NoteType::Single;    // this note retriggers → a fresh attack
        markSemi(semitone, dur, isQuant);
    }
    return lastNoteType;
}

void GateState::slideMax(float pitchV, int semitone, int nvIdx, bool isQuant) {
    const int   pp  = (ppqn > 0) ? ppqn : 24;
    const int   p   = noteHoldPulses(nvIdx, pp);
    const float dur = noteHoldSteps(nvIdx, pp);   // STEP units
    currentPitchV = pitchV;
    lastSemitone  = semitone;
    gateHeld   = true;
    holdRemain = dur;
    armGatePulses(p);
    markSemi(semitone, dur, isQuant);
    lastNoteType = NoteType::Legato;        // LegatoMax = forced slide, no retrigger
}

void GateState::rest(bool tieExtend, int nvIdx) {
    if (tieExtend && gateHeld) {
        const int pp = (ppqn > 0) ? ppqn : 24;
        holdRemain += noteHoldSteps(nvIdx, pp);       // STEP units
        armGatePulses((int)std::lround(holdRemain * (float)PULSES_PER_STEP));
    }
    // Otherwise: no-op — the gate's pulse countdown closes it cleanly.
}

// Tick: once per STEP edge (a step = PULSES_PER_STEP grid pulses, fixed — at 24
// that is the 1/16 edge, at 48 the 1/32). Decrements the step-unit DECISION
// counter and the semi LED timers. Does NOT close the gate (tickPulse does, on
// the finer grid). p16 keeps the MUSICAL pulse resolution + the derived ppqn
// current for arming.
void GateState::tick(int p16) {
    if (p16 > 0) { pulsesPer16th = p16; ppqn = p16 * 4; }
    if (holdRemain > 0.f) {
        holdRemain -= 1.f;
        if (holdRemain <= 0.f) holdRemain = 0.f;
    }
    // Fade ALL per-degree flash timers (MAXN, not 12) — else Micro-24 degrees 12..23 that play never
    // decay and their faders stay lit red (the marks are set MAXN-wide in markSemi). At N=12 the tail
    // is always 0, so this is behaviourally identical there.
    for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) {
        if (semiPlayRemain[i] > 0.f) {
            semiPlayRemain[i] -= 1.f;
            if (semiPlayRemain[i] < 0.f) semiPlayRemain[i] = 0.f;
        }
        if (semiQuantPlayRemain[i] > 0.f) {   // FADER_SEQ_QUANT_COLOURS (green flash decay)
            semiQuantPlayRemain[i] -= 1.f;
            if (semiQuantPlayRemain[i] < 0.f) semiQuantPlayRemain[i] = 0.f;
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
    slurForward     = false;
    gatePulse.reset();
    for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) { semiPlayRemain[i] = 0.f; semiQuantPlayRemain[i] = 0.f; }
}

// ── LED helper ────────────────────────────────────────────────────────────────
// Degree index bound is MAXN (24) now, not 12 — a Micro-24 degree 12..23 flashes its own fader. At
// N=12 nothing ever marks a degree >= 12, so this is behaviourally identical there.
float GateState::semiLedBrightness(int semitone) const {
    if (semitone < 0 || semitone >= dotModular::TuningTable::MAXN) return 0.f;
    return gs_clamp(semiPlayRemain[semitone] * 0.25f, 0.f, 1.f);
}

float GateState::semiQuantLedBrightness(int semitone) const {
    if (semitone < 0 || semitone >= dotModular::TuningTable::MAXN) return 0.f;
    return gs_clamp(semiQuantPlayRemain[semitone] * 0.25f, 0.f, 1.f);
}

void GateState::markSemi(int semitone, float dur, bool isQuant) {
    if (semitone >= 0 && semitone < dotModular::TuningTable::MAXN) {
        if (isQuant) semiQuantPlayRemain[semitone] = std::max(semiQuantPlayRemain[semitone], dur);
        else         semiPlayRemain[semitone]      = std::max(semiPlayRemain[semitone], dur);
    }
}
