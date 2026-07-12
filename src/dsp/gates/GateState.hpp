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
#include "../NoteValues.hpp"


template<typename T>
static inline T gs_clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

// ── Note length helper ───────────────────────────────────────────────────────
// Note-value data lives in dsp/NoteValues.hpp. gs_noteSteps wraps noteValueSteps.
extern float gs_noteSteps(int nvIdx);

// ── GateState ─────────────────────────────────────────────────────────────────
struct GateState {

    // ── State ─────────────────────────────────────────────────────────────────
    float  holdRemain      = 0.f;   // whole-step hold counter (engine DECISIONS:
                                    // MidNote/canRest/hadTail). Unchanged: step units.
    bool   gateHeld        = false;
    // Gate-open length counted in PPQN GRID PULSES — the single mechanism that
    // closes the gate for ALL note lengths (whole, triplet, 1/32 alike: each is
    // just an integer pulse count = noteSteps * pulsesPer16th). Replaces the old
    // seconds timer (gateSecRemain) + its whole-vs-fractional special-casing.
    // >=0 while governing the gate; decremented once per grid pulse in tickPulse().
    int    gatePulseRemain = -1;
    int    pulsesPer16th   = 6;     // grid resolution (24/48/96 PPQN → 6/12/24);
                                    // set by the engine each tick from the clock.
    float  currentPitchV   = 0.f;   // 1V/oct output
    int    lastSemitone    = -1;    // for tie detection
    float  semiPlayRemain[12] = {}; // per-semitone flash timers (steps)

    // ── Leading-edge legato instrument (STEP 1: computed, UNUSED by gate logic) ──
    // In the leading-edge model a note commits AT ITS ONSET to hold its gate forward
    // into the next note (a "slur lead"). This flag records that intended commitment
    // for the CURRENT note, computed at trigger time from the note's OWN legato draw
    // AND the grid-alignment constraint (only integer-step lengths can lead — see
    // LEGATO_TIE_REDESIGN.md). In step 1 it is set but NOT consulted by the gate/decision
    // path (the join still decides exactly as today); step 2 will make the join consume it.
    bool   slurForward     = false; // this note intends to hold forward (leading-edge)

    // ── Articulation type (per voice) — the single source of truth the display reads ──
    // Set by the three articulation methods: triggerNote → Single (a fresh attack, including an
    // opt-out re-strike INSIDE a slur), slideNote → Legato (slid to a new pitch, no retrigger),
    // extendHold → Tie (held same pitch). MidNote/hold leave it unchanged (the note continues).
    // This is what the lantern reads per voice (voices[i].gs.lastNoteType) — and, later, what a
    // poly TIE/LEGATO output jack emits — so per-voice legato is exposed exactly like mono's,
    // rather than inferred from a 1ms gate dip that the display can't recover. Values match the
    // lantern's colour convention: Single (blue) / Tie (violet) / Legato (teal).
    enum class NoteType : uint8_t { Single = 0, Tie = 1, Legato = 2 };
    NoteType lastNoteType = NoteType::Single;

    rack::dsp::PulseGenerator gatePulse;  // 1ms retrigger for envelopes

    // ── Core operations ───────────────────────────────────────────────────────
    // Arm the gate-close pulse countdown from a duration in 1/16-steps.
    void armGate(float durSteps);   // gatePulseRemain = round(durSteps * pulsesPer16th)
    void triggerNote(float pitchV, int semitone, int nvIdx);
    void slideNote(float pitchV, int semitone, int nvIdx, bool wasHeld);
    void slideMax(float pitchV, int semitone, int nvIdx);
    void extendHold(int semitone, int nvIdx);
    void rest(bool tieExtend, int nvIdx);
    // Tick: call once per 1/16 step edge. Decrements the whole-step DECISION
    // counter (holdRemain) + semi LED timers. Does NOT close the gate (that's
    // tickPulse on the finer grid). `p16` keeps pulsesPer16th current.
    void tick(int p16 = 0);
    // TickPulse: call once per PPQN grid pulse. Decrements gatePulseRemain and
    // closes the gate when it reaches 0. This is the sole gate-close mechanism.
    void tickPulse();
    // Process: call every sample. Returns raw gate voltage (0 or 10V); only the
    // 1ms retrigger dip lives here now.
    float process(float sampleTime);
    void tickPulseOnly(float sampleTime);
    void reset();
    // ── LED helper ────────────────────────────────────────────────────────────
    // semiPlayRemain normalised to 0..1 for setBrightness (1/4 note = full bright)
    float semiLedBrightness(int semitone) const;
    void markSemi(int semitone, float dur);
};
