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
// Index → musical value (current order interleaves triplets with their straight
// counterparts, slowest → fastest):
//   0=1/1  1=1/2  2=1/4  3=1/4T  4=1/8  5=1/8T  6=1/16  7=1/32
//
// allowedPPQN is a bitmask of the clock resolutions at which the value is
// reachable: 1 = PPQN 1, 2 = PPQN 4, 4 = PPQN 24.

struct NoteValue {
    float       fraction;     // fraction of a whole note (×16 ⇒ length in 1/16 steps)
    int         allowedPPQN;  // bitmask: 1=PPQN1, 2=PPQN4, 4=PPQN24
    const char* label;        // dial / menu label
};

constexpr int NUM_NOTE_VALUES = 8;

// The one and only table. Keep ordered slowest → fastest.
constexpr NoteValue NOTE_VALUES[NUM_NOTE_VALUES] = {
    {1.0f,        1|2|4, "1/1" },
    {0.5f,        1|2|4, "1/2" },
    {0.25f,       1|2|4, "1/4" },
    {1.0f/6.0f,   4,     "1/4T"},
    {0.125f,      2|4,   "1/8" },
    {1.0f/12.0f,  4,     "1/8T"},
    {0.0625f,     2|4,   "1/16"},
    {0.03125f,    4,     "1/32"},
};

// Convenience: a note-value index → length in 1/16 steps (no whole-step floor;
// sub-step lengths such as 1/32 = 0.5 and triplets render via the gate-seconds
// countdown in GateState).
inline float noteValueSteps(int idx) {
    if (idx < 0 || idx >= NUM_NOTE_VALUES) return 1.0f;
    return NOTE_VALUES[idx].fraction * 16.0f;
}
