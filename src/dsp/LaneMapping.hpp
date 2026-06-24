#pragma once
//
// LaneMapping.hpp — SINGLE SOURCE OF TRUTH for the visual-editor lane order
// vs. the engine strand order.
//
// There are two intentionally-different orderings in play:
//
//   • EDITOR lane order (SandsVisualEditorV4::Lane):
//        0 REST  1 MELODY  2 OCTAVE  3 LEGATO  4 ACCENT  5 VARIATION
//     Chosen so the FIRST THREE mono lanes (REST/MELODY/OCTAVE) line up with the
//     three POLY lanes, so the poly visual and the top half of the mono visual
//     show the same lanes in the same on-screen positions.
//
//   • ENGINE strand order (SequencerEngine members / DNA param grouping):
//        0 rhythm  1 variation  2 legato  3 accent  4 melody  5 octave
//
// These differ by a permutation. It was previously encoded by hand in at least
// two places (MonsoonSandsManager::readStrand and the visual display feed), and
// a mismatch scrambled which lane showed which strand (MELODY showed variation,
// OCTAVE showed legato, …). This header states the permutation ONCE so every
// consumer maps editor-lane → engine-strand the same way.
//
// To change the mapping, edit MONO_LANE_TO_STRAND here only.

namespace dotModular {

// Engine strand index (matches the readStrand() order in MonsoonSandsManager and
// the slot each engine.<strand>Len/Off/Rot occupies).
enum EngineStrand {
    STRAND_RHYTHM    = 0,
    STRAND_VARIATION = 1,
    STRAND_LEGATO    = 2,
    STRAND_ACCENT    = 3,
    STRAND_MELODY    = 4,
    STRAND_OCTAVE    = 5,
    NUM_STRANDS      = 6,
};

// Editor lane index → engine strand index.
//   editor 0 REST      -> rhythm
//   editor 1 MELODY    -> variation
//   editor 2 OCTAVE    -> legato
//   editor 3 LEGATO    -> accent
//   editor 4 ACCENT    -> melody
//   editor 5 VARIATION -> octave
constexpr int MONO_LANE_TO_STRAND[6] = {
    STRAND_RHYTHM,      // 0 REST
    STRAND_VARIATION,   // 1 MELODY
    STRAND_LEGATO,      // 2 OCTAVE
    STRAND_ACCENT,      // 3 LEGATO
    STRAND_MELODY,      // 4 ACCENT
    STRAND_OCTAVE,      // 5 VARIATION
};

} // namespace dotModular
