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
    // Renumbered to EDITOR order (MEL/OCT/REST/ACC/VAR/LEG) as part of the
    // lane-order collapse. The switch(strand) accessors are keyed by NAME, so
    // their behaviour is unchanged by the renumber; this just makes
    // MONO_LANE_TO_STRAND the identity (editor lane == strand index).
    STRAND_MELODY    = 0,
    STRAND_OCTAVE    = 1,
    STRAND_RHYTHM    = 2,   // REST
    STRAND_ACCENT    = 3,
    STRAND_VARIATION = 4,
    STRAND_LEGATO    = 5,
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

// Mono LOR param bank → editor lane.  COLLAPSED TO IDENTITY: the Mono ParamId
// LOR bank (lenId/offId/rotId) was renumbered to EDITOR order (MEL,OCT,REST,ACC,
// VAR,LEG), so the param index now IS the editor lane. These tables have no live
// callers any more; kept as identity (and documented) so any stragglers are safe.
// Removable once confirmed nothing references them.
constexpr int MONO_PARAM_TO_EDITOR[6] = { 0, 1, 2, 3, 4, 5 };
constexpr int EDITOR_TO_MONO_PARAM[6] = { 0, 1, 2, 3, 4, 5 };

// Poly engine lane index (0=REST 1=MELODY 2=OCTAVE 3=ACCENT — the order used
// by East/Macro lorId, engine.polyLen[v][lane], macroBase[lane], and the
// VoiceResolver lane argument) → editor lane index.
//   engine 0 REST   → editor 2
//   engine 1 MELODY → editor 0
//   engine 2 OCTAVE → editor 1
//   engine 3 ACCENT → editor 3
constexpr int ENGINE_LANE_TO_EDITOR[4] = { 2, 0, 1, 3 };

// Inverse: EDITOR poly lane → poly ENGINE lane (for macroBase[lane] indexing).
//   editor 0 MELODY → engine 1
//   editor 1 OCTAVE → engine 2
//   editor 2 REST   → engine 0
//   editor 3 ACCENT → engine 3
constexpr int EDITOR_TO_ENGINE_LANE[4] = { 1, 2, 0, 3 };

// Spread lanes (REST/MEL/OCT/ACCENT) share the poly engine→editor mapping.
// Alias kept for call-site readability where "spread lane" is the natural term.
constexpr const int* SPREAD_LANE_TO_EDITOR = ENGINE_LANE_TO_EDITOR;

} // namespace dotModular
