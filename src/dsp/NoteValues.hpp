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
// Index → musical value (DOTTED BUILD step 2: the 4 dotted values interleaved at
// their duration positions — Rodney's OPTION B order, each dotted right after its
// straight base; dotted-1/32 EXCLUDED, needs 48 PPQN, unsafe — PPQN_48_96_NOT_SAFE.md):
//    0=1/1   1=1/2   2=1/2.  3=1/4   4=1/4.  5=1/4T
//    6=1/8   7=1/8.  8=1/8T  9=1/16 10=1/16. 11=1/32
// (durations in 1/16 steps: 16, 8, 12, 4, 6, 2.667, 2, 3, 1.333, 1, 1.5, 0.5 —
//  NOT index-monotone: the "shorter note" comparisons route through noteShorterOf,
//  which compares FRACTIONS, never raw indices.)
//
// allowedPPQN is a bitmask of the clock resolutions at which the value is
// reachable: 1 = PPQN 1, 2 = PPQN 4, 4 = PPQN 24.

#include <cmath>   // std::floor (noteCanLeadLegato)

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

constexpr int NUM_NOTE_VALUES = 12;

// The one and only table. Rodney's OPTION B order (dotted after its straight base).
constexpr NoteValue NOTE_VALUES[NUM_NOTE_VALUES] = {
    {1.0f,        1|2|4, "1/1",  -1},
    {0.5f,        1|2|4, "1/2",  -1},
    {0.75f,       1|2|4, "1/2.",  3},   //  2 — dotted half,      12 steps (integer)
    {0.25f,       1|2|4, "1/4",  -1},
    {0.375f,      1|2|4, "1/4.",  4},   //  4 — dotted quarter,    6 steps (integer)
    {1.0f/6.0f,   4,     "1/4T",  0},
    {0.125f,      2|4,   "1/8",  -1},
    {0.1875f,     2|4,   "1/8.",  5},   //  7 — dotted eighth,     3 steps (integer)
    {1.0f/12.0f,  4,     "1/8T",  1},
    {0.0625f,     2|4,   "1/16", -1},
    {0.09375f,    4,     "1/16.", 6},   // 10 — dotted sixteenth,  1.5 steps (FRACTIONAL, PPQN24 only)
    {0.03125f,    4,     "1/32",  2},
};

// The LEGACY index map (the pre-dotted 8-entry table): old index → the SAME musical
// value's index in the 12-entry table. Used by (a) the test harnesses — the marches'
// rng draws are sequence-pinned, so they draw the legacy index and map it, keeping
// every drawn VALUE identical — and (b) the patch remap: MonsoonPersistenceManager
// writes noteValueSchema=2 on save; its absence on load ⇒ a pre-dotted save ⇒
// NOTE_VALUE_PARAM translates through this map (released patches keep their dials).
inline int legacyNoteValueIndex(int oldIdx) {
    static constexpr int MAP[8] = {0, 1, 3, 5, 6, 8, 9, 11};   // 1/1 1/2 1/4 1/4T 1/8 1/8T 1/16 1/32
    if (oldIdx < 0) return 0;
    if (oldIdx > 7) return 11;
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
constexpr int NV_GATE_NULLIFY = 9;   // 1/16 = exactly 1 step — the inert countdown
static_assert(NV_GATE_NULLIFY >= 0 && NV_GATE_NULLIFY < NUM_NOTE_VALUES,
              "NV_GATE_NULLIFY must be a valid table index");
static_assert(NOTE_VALUES[NV_GATE_NULLIFY].fraction * 16.f == 1.f,
              "NV_GATE_NULLIFY must address a 1-step value (the inert countdown)");
// TODO (out of scope for dotted): Mode B nullifies note length — routing it through
// the note-length path at all is vestigial; a future change should bypass it entirely.

// Convenience: a note-value index → length in 1/16 steps (no whole-step floor;
// sub-step lengths such as 1/32 = 0.5 and triplets render via the gate-seconds
// countdown in GateState).
inline float noteValueSteps(int idx) {
    if (idx < 0 || idx >= NUM_NOTE_VALUES) return 1.0f;
    return NOTE_VALUES[idx].fraction * 16.0f;
}

// Leading-edge legato: a note can LEAD a legato (hold its gate forward into the next
// note) only if it ends exactly on a 1/16 grid edge — i.e. its length in 1/16 steps is
// an integer. Fractional lengths (1/4T=2.667, 1/8T=1.333, 1/32=0.5) end off-grid and
// cannot cleanly reach the next (grid-aligned) onset, so they can never lead a legato.
// See LEGATO_TIE_REDESIGN.md. (Integer-step lengths: 1/1,1/2,1/4,1/8,1/16 = 16,8,4,2,1.)
inline bool noteCanLeadLegato(int idx) {
    float ns = noteValueSteps(idx);
    return ns == std::floor(ns);
}
