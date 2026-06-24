#pragma once
//
// LaneMapping.hpp — SINGLE SOURCE OF TRUTH for the visual-editor lane order
// vs. the engine strand order.
//
// There are two intentionally-different orderings in play:
//
//   • EDITOR lane order (SandsVisualEditorV4::Lane):
//        0 MELODY  1 OCTAVE  2 REST  3 ACCENT  4 VARIATION  5 LEGATO
//     Chosen so the FIRST FOUR mono lanes (MEL/OCT/REST/ACCENT) align with the
//     four POLY lanes on East/Macro, keeping melody-type and rhythm-type lanes
//     grouped, and accent adjacent to rest.
//
//   • ENGINE strand order (SequencerEngine members / DNA param grouping):
//        0 rhythm  1 variation  2 legato  3 accent  4 melody  5 octave
//
//   • MONO PARAM bank order (SandsMonoVisualIds::lenId(l)):
//        0 REST  1 MELODY  2 OCTAVE  (first 3 only; LEG/ACC/VAR handled separately)
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
//   editor 0 MELODY    -> melody
//   editor 1 OCTAVE    -> octave
//   editor 2 REST      -> rhythm
//   editor 3 ACCENT    -> accent
//   editor 4 VARIATION -> variation
//   editor 5 LEGATO    -> legato
constexpr int MONO_LANE_TO_STRAND[6] = {
    STRAND_MELODY,      // 0 MELODY
    STRAND_OCTAVE,      // 1 OCTAVE
    STRAND_RHYTHM,      // 2 REST
    STRAND_ACCENT,      // 3 ACCENT
    STRAND_VARIATION,   // 4 VARIATION
    STRAND_LEGATO,      // 5 LEGATO
};

// Mono param bank index (lenId(l)) → editor lane index.
// lenId bank: 0=REST 1=MEL 2=OCT (matches old editor order; now needs remapping).
constexpr int MONO_PARAM_TO_EDITOR[3] = {
    2,   // param 0 (REST)    → editor lane 2
    0,   // param 1 (MELODY)  → editor lane 0
    1,   // param 2 (OCTAVE)  → editor lane 1
};

} // namespace dotModular
