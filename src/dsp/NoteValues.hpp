#pragma once
//
// NoteValues.hpp — SINGLE SOURCE OF TRUTH for the note-length dial.
//
// The eight note-length positions were previously duplicated across five places,
// all index-aligned and all kept in sync by hand:
//   • NOTEVALS[] (.fraction + .allowedPPQN)  in SequencerEngine
//   • GS_NOTE_FRACS[]                         in GateState
//   • the configSwitch labels                in MonsoonConfigurator
//   • the context-menu labels                in MonsoonWidget
//   • a stale comment                        in SequencerEngine
// Editing one and forgetting another desynced dial position from gate length
// (commit bb4cb72 reordered NOTEVALS + labels but missed GS_NOTE_FRACS, so e.g.
// 1/4 played as 1/4T and 1/8/1/8T/1/16/1/32 collapsed to one step).
//
// This header defines the list ONCE. Every consumer reads from NOTE_VALUES so a
// reorder or relabel can only ever happen in one spot.
//
// Index → musical value (DOTTED BUILD, spec v3 — 13 entries): the ENGINE table in
// STRICT duration order (longest → shortest, verified strictly decreasing), and a
// separate DIAL_ORDER display permutation so the knob reads in GROUPED musical
// families (base, triplet, dotted-of-next). 1/16T joins (0.6667 steps — resolves
// at the 24-PPQN baseline, 4 pulses, safe; a triplet → own mask bit 7):
//    ENGINE:  0=1/1  1=1/2.  2=1/2  3=1/4.  4=1/4  5=1/8.  6=1/4T  7=1/8
//             8=1/16. 9=1/8T 10=1/16 11=1/16T 12=1/32
//    DIAL:    1/1, 1/2., 1/2, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16., 1/16, 1/16T, 1/32
// (engine durations in 1/16 steps: 16, 12, 8, 6, 4, 3, 2.667, 2, 1.5, 1.333, 1,
//  0.6667, 0.5 — strictly decreasing; the "shorter" comparisons route through
//  noteShorterOf, the fraction authority, regardless of order.)
//
// allowedPPQN is a bitmask of the clock resolutions at which the value is
// reachable: 1 = PPQN 1, 2 = PPQN 4, 4 = PPQN 24, 8 = PPQN 48 (CC_PROMPT_PPQN_48:
// at 48 every one of the 13 values resolves to an integer pulse count —
// fraction·4·48 — so all 13 carry bit 8).

#include <cmath>   // std::lround (noteValuePulses)

// ── THE STEP GRID (CC_PROMPT_PPQN_48, decision 1: "A STEP IS 6 PULSES, FIXED") ──
// The musical division is DERIVED FROM PPQN; the pattern's STEP never is. One
// STEP = PULSES_PER_STEP grid pulses at ANY PPQN (at 24 a step is a 1/16; at 48
// it is a 1/32 — the step rate doubles at the same BPM). The pulses-per-
// sixteenth (PPQN/4: 6 at 24, 12 at 48) is for MUSICAL durations only (gate
// widths, the note-value table, musical regions). Every STEP-indexed consumer
// (snap, held cells, step boundaries, the advance, the lane walks) uses
// PULSES_PER_STEP; only note-value / musical-duration maths use PPQN/4.
constexpr int PULSES_PER_STEP = 6;

// The PPQN setting → its allowedPPQN bit (1=PPQN1, 2=PPQN4, 4=PPQN24, 8=PPQN48).
// 96 is out of scope (CC_PROMPT_PPQN_48 DO-NOT) and maps to 8's neighbour slot
// only if ever added; unknown values read as PPQN24's bit (the historic hardcode).
inline int ppqnBitOf(int ppqn) {
    if (ppqn >= 48) return 8;
    if (ppqn == 24) return 4;
    if (ppqn == 4)  return 2;
    return 1;
}

struct NoteValue {
    float       fraction;     // fraction of a whole note (×16 ⇒ length in 1/16 steps)
    int         allowedPPQN;  // bitmask: 1=PPQN1, 2=PPQN4, 4=PPQN24
    const char* label;        // dial / menu label
    // The noteVariationMask bit that gates this value's participation in
    // varyNoteIndex's window (DOTTED BUILD step 1: the FLAG that replaces the old
    // magic index tests in allowed(), so the table order can change freely).
    // -1 = a straight value — always in the window. Today: 0=1/4T, 1=1/8T, 2=1/32.
    int         variationBit;
};

constexpr int NUM_NOTE_VALUES = 13;

// The one and only table — STRICT duration order, longest → shortest.
constexpr NoteValue NOTE_VALUES[NUM_NOTE_VALUES] = {
    {1.0f,        1|2|4|8, "1/1",  -1},
    {0.75f,       1|2|4|8, "1/2.",  3},   //  1 — dotted half,      12 steps (integer)
    {0.5f,        1|2|4|8, "1/2",  -1},
    {0.375f,      1|2|4|8, "1/4.",  4},   //  3 — dotted quarter,    6 steps (integer)
    {0.25f,       1|2|4|8, "1/4",  -1},
    {0.1875f,     2|4|8,   "1/8.",  5},   //  5 — dotted eighth,     3 steps (integer)
    {1.0f/6.0f,   4|8,     "1/4T",  0},
    {0.125f,      2|4|8,   "1/8",  -1},
    {0.09375f,    4|8,     "1/16.", 6},   //  8 — dotted sixteenth,  1.5 steps (FRACTIONAL, PPQN24 only)
    {1.0f/12.0f,  4|8,     "1/8T",  1},
    {0.0625f,     2|4|8,   "1/16", -1},
    {1.0f/24.0f,  4|8,     "1/16T", 7},   // 11 — sixteenth triplet, 0.6667 steps (4 pulses @ 24 PPQN — safe)
    {0.03125f,    4|8,     "1/32",  2},
};

// The DIAL display permutation (the spec's grouped reading: families together —
// base, triplet, then the next family's dotted): the ENGINE table stays in strict
// duration order. The NOTE_VALUE PARAM stores the DIAL SLOT (0..NUM-1) — the knob's
// detents, labels, and CV sweep all walk THIS order — and the ONE seam
// (ParameterManager::getNoteValue) translates slot → engine index. DIAL_SLOT_OF is
// the inverse (engine index → dial slot), used by the patch remap.
constexpr int DIAL_ORDER[NUM_NOTE_VALUES]   = { 0, 1, 2, 3, 4, 6, 5, 7, 9, 8, 10, 11, 12 };
constexpr int DIAL_SLOT_OF[NUM_NOTE_VALUES] = { 0, 1, 2, 3, 4, 6, 5, 7, 9, 8, 10, 11, 12 };
static_assert(DIAL_ORDER[DIAL_SLOT_OF[5]] == 5 && DIAL_ORDER[DIAL_SLOT_OF[8]] == 8,
              "DIAL_SLOT_OF must be the inverse of DIAL_ORDER");

// The LEGACY index map (the pre-dotted 8-entry table): old index → the SAME musical
// value's index in the 12-entry table. Used by (a) the test harnesses — the marches'
// rng draws are sequence-pinned, so they draw the legacy index and map it, keeping
// every drawn VALUE identical — and (b) the patch remap: MonsoonPersistenceManager
// writes noteValueSchema=2 on save; its absence on load ⇒ a pre-dotted save ⇒
// NOTE_VALUE_PARAM translates through this map (released patches keep their dials).
inline int legacyNoteValueIndex(int oldIdx) {
    static constexpr int MAP[8] = {0, 2, 4, 6, 7, 9, 10, 12};  // 1/1 1/2 1/4 1/4T 1/8 1/8T 1/16 1/32
    if (oldIdx < 0) return 0;
    if (oldIdx > 7) return 12;
    return MAP[oldIdx];
}

// The "shorter note" authority (DOTTED BUILD): the table is NO LONGER index-monotone,
// so every "larger index = shorter" comparison must route through HERE — a direct
// FRACTION comparison, never a raw index compare. Identical results for legacy pairs.
inline int noteShorterOf(int a, int b) {
    return (NOTE_VALUES[a].fraction <= NOTE_VALUES[b].fraction) ? a : b;
}

// GATE-MODE NULLIFICATION (DOTTED BUILD step 1, Rodney's diagnosis): Mode B's note
// duration is Gate 1's WIDTH — the internal note-length countdown must not govern.
// The engine passes this sentinel index there purely as "a 1-step inert countdown"
// (SequencerEngine ~:1304 and ~:1431). It is a NAMED CONSTANT, not a musical choice,
// so it must never be written as a bare index literal again: when the table order
// changes, THIS line is the one place to update (pinned by the static_assert below).
constexpr int NV_GATE_NULLIFY = 10;  // 1/16 = exactly 1 step — the inert countdown
static_assert(NV_GATE_NULLIFY >= 0 && NV_GATE_NULLIFY < NUM_NOTE_VALUES,
              "NV_GATE_NULLIFY must be a valid table index");
static_assert(NOTE_VALUES[NV_GATE_NULLIFY].fraction * 16.f == 1.f,
              "NV_GATE_NULLIFY must address a 1-step value (the inert countdown)");
// TODO (out of scope for dotted): Mode B nullifies note length — routing it through
// the note-length path at all is vestigial; a future change should bypass it entirely.

// Convenience: a note-value index → length in 1/16 steps (no whole-step floor;
// sub-step lengths such as 1/32 = 0.5 and triplets render via the gate-seconds
// countdown in GateState). The MUSICAL 1/16-unit view — unchanged, the note
// table's display/record authority.
inline float noteValueSteps(int idx) {
    if (idx < 0 || idx >= NUM_NOTE_VALUES) return 1.0f;
    return NOTE_VALUES[idx].fraction * 16.0f;
}

// ── THE PULSE TABLE (CC_PROMPT_PPQN_48 stage 2) ────────────────────────────────
// Each division expressed IN GRID PULSES at the active PPQN: a whole note is
// 4·PPQN pulses, so value pulses = fraction · 4 · PPQN. At 24 every value is an
// exact integer (1/16 = 6, 1/32 = 3, 1/16T = 4, 1/8T = 8, 1/4T = 16, 1/16. = 9,
// 1/8. = 18 …); at 48 likewise (1/16 = 12, 1/32 = 6, 1/16T = 8, 1/8T = 16,
// 1/4T = 32, 1/16. = 18, 1/8. = 36 …). The ONE duration authority the gate
// widths, the step-unit holds and the legato-lead rule read.
inline int noteValuePulses(int idx, int ppqn) {
    if (idx < 0 || idx >= NUM_NOTE_VALUES) return PULSES_PER_STEP;
    const int p = (ppqn > 0) ? ppqn : 24;
    return (int)std::lround((double)NOTE_VALUES[idx].fraction * 4.0 * (double)p);
}

// The note's length in SEQUENCER STEPS (a step = PULSES_PER_STEP pulses, fixed —
// decision 1). At 24 PPQN this is bit-identical to noteValueSteps: the table's
// pulse counts are exact small integers there, and dividing by 6 rounds once to
// the same float the fraction·16 form produced (scaling by 16 = 2⁴ commutes with
// round-to-nearest). At 48 the units are the doubled step rate (a 1/16 = 2 steps).
inline float noteValueStepUnits(int idx, int ppqn) {
    if (idx < 0 || idx >= NUM_NOTE_VALUES) return 1.0f;
    return (float)noteValuePulses(idx, ppqn) / (float)PULSES_PER_STEP;
}

// Leading-edge legato: a note can LEAD a legato (hold its gate forward into the next
// note) only if it ends exactly on a STEP boundary — its pulse count is a multiple of
// PULSES_PER_STEP (CC_PROMPT_PPQN_48 stage 2; a step is 6 pulses at ANY PPQN).
// At 24 PPQN this is exactly the old integer-in-1/16-steps rule (pulses = steps·6,
// so multiple-of-6 ⟺ integer steps) — the 24 lead set is unchanged: 1/32 (3 pulses)
// is NOT a step boundary and stays non-lead, as today. At 48 the rule reads the
// doubled grid: a 1/32 (6 pulses) ends on a step and CAN lead; the triplets
// (1/16T = 8, 1/8T = 16, 1/4T = 32) never land on a multiple of 6 and still cannot.
// See LEGATO_TIE_REDESIGN.md.
inline bool noteCanLeadLegato(int idx, int ppqn = 24) {
    return (noteValuePulses(idx, ppqn) % PULSES_PER_STEP) == 0;
}
