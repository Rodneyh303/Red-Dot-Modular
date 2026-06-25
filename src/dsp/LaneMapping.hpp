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
//        0 REST  1 MELODY  2 OCTAVE  3 LEGATO  4 ACCENT  5 VARIATION
//
//   • POLY ENGINE lane order (East/Macro lorId / engine.polyLen[v][lane] /
//     macroBase[lane] / VoiceResolver lane arg):
//        0 REST  1 MELODY  2 OCTAVE  3 ACCENT
//
// To change the editor order, edit the tables here only — every consumer
// (East, Macro, Mono visual expander) routes through these, so there are no
// hand-rolled per-file lane arrays to keep in sync.

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

// Mono param bank index (SandsMonoVisualIds::lenId(l)) → editor lane index.
// Param bank order: 0=REST 1=MELODY 2=OCTAVE 3=LEGATO 4=ACCENT 5=VARIATION.
// (East/Macro mono mirror only touch indices 0..2 — REST/MEL/OCT — but the
//  full 6 are defined for the Mono visual expander's own LOR sync.)
constexpr int MONO_PARAM_TO_EDITOR[6] = {
    2,   // param 0 (REST)      → editor lane 2
    0,   // param 1 (MELODY)    → editor lane 0
    1,   // param 2 (OCTAVE)    → editor lane 1
    5,   // param 3 (LEGATO)    → editor lane 5
    3,   // param 4 (ACCENT)    → editor lane 3
    4,   // param 5 (VARIATION) → editor lane 4
};

// Poly engine lane index (0=REST 1=MELODY 2=OCTAVE 3=ACCENT — the order used
// by East/Macro lorId, engine.polyLen[v][lane], macroBase[lane], and the
// VoiceResolver lane argument) → editor lane index.
//   engine 0 REST   → editor 2
//   engine 1 MELODY → editor 0
//   engine 2 OCTAVE → editor 1
//   engine 3 ACCENT → editor 3
constexpr int ENGINE_LANE_TO_EDITOR[4] = { 2, 0, 1, 3 };

// Spread lanes (REST/MEL/OCT/ACCENT) share the poly engine→editor mapping.
// Alias kept for call-site readability where "spread lane" is the natural term.
constexpr const int* SPREAD_LANE_TO_EDITOR = ENGINE_LANE_TO_EDITOR;

} // namespace dotModular
