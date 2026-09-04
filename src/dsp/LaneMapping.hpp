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

// ─────────────────────────────────────────────────────────────────────────────
// q-mix lane (PLANNED — inert tables; the live tables above are unchanged) ─────
//
// q-mix inserts ONE editor lane at index 2 (after MEL/OCT):
//     0 MEL  1 OCT  2 QMIX  3 REST  4 ACCENT  5 VARIATION  6 LEGATO   (7 editor lanes)
// so every editor index at/after 2 shifts +1. q-mix is NOT an L/O/R strand — it is driven
// by its OWN Philox stream (STREAM_SOURCE_SELECT = 3, see dsp/PhiloxRng.hpp), a per-voice
// generated-vs-external source pick. So its editor lane maps to a STREAM, not a strand.
//
// These *_QMIX tables are the drop-in replacements for the live tables above; swap the
// consumers over to them ATOMICALLY with the editor drawing 7 slots (SandsVisualEditorV4)
// and the generators' order arrays. Until then they are inert (nothing references them),
// so the current build is unaffected.
constexpr int STRAND_NONE       = -1;   // sentinel: editor lane has no engine strand (q-mix)
constexpr int QMIX_EDITOR_LANE  = 2;    // q-mix's editor lane index
constexpr int NUM_EDITOR_LANES_QMIX = 7;

// Existing editor lane (0..5, no q-mix) → new editor slot (skip slot 2). This is the
// generators' ESLOT=[0,1,3,4,5,6] expressed as a function; the ONE place this shift lives.
constexpr int laneSlot(int editorLaneNoQmix) {
    return editorLaneNoQmix < QMIX_EDITOR_LANE ? editorLaneNoQmix : editorLaneNoQmix + 1;
}

// Editor lane (7, incl. QMIX) → engine strand. QMIX = STRAND_NONE (its data is the q-mix
// stream, handled separately). Consumers that index MONO_LANE_TO_STRAND[l] must guard the
// sentinel (skip strand data for the q-mix lane) — see the worklist in the branch notes.
constexpr int MONO_LANE_TO_STRAND_QMIX[7] = {
    STRAND_MELODY,   // 0 MEL
    STRAND_OCTAVE,   // 1 OCT
    STRAND_NONE,     // 2 QMIX  ← q-mix stream, not a strand
    STRAND_RHYTHM,   // 3 REST
    STRAND_ACCENT,   // 4 ACCENT
    STRAND_VARIATION,// 5 VARIATION
    STRAND_LEGATO,   // 6 LEGATO
};

// Poly engine lane (0 REST 1 MEL 2 OCT 3 ACC) → editor lane, WITH q-mix at 2 (all +1 past slot 2).
//   REST→3  MEL→0  OCT→1  ACC→4        (was {2,0,1,3})
constexpr int ENGINE_LANE_TO_EDITOR_QMIX[4] = { 3, 0, 1, 4 };
// Inverse over 7 editor lanes; QMIX + VAR/LEG have no poly engine lane (STRAND_NONE).
constexpr int EDITOR_TO_ENGINE_LANE_QMIX[7] = { 1, 2, STRAND_NONE, 0, 3, STRAND_NONE, STRAND_NONE };

// q-mix's RNG stream key (== redDot::seed::STREAM_SOURCE_SELECT; kept as a literal here to
// avoid pulling PhiloxRng.hpp into this ordering header).
constexpr uint64_t QMIX_STREAM_KEY = 3;

// internal consistency (compile-time): the shift + round-trips hold.
static_assert(laneSlot(1) == 1 && laneSlot(2) == 3 && laneSlot(5) == 6, "qmix slot shift");
static_assert(ENGINE_LANE_TO_EDITOR_QMIX[0] == 3 && EDITOR_TO_ENGINE_LANE_QMIX[3] == 0, "qmix REST round-trip");
static_assert(MONO_LANE_TO_STRAND_QMIX[QMIX_EDITOR_LANE] == STRAND_NONE, "qmix lane has no strand");

// ─── NOTE: ALIGN THE ORDERS WHERE POSSIBLE ───────────────────────────────────
// Of the orderings in the header block, three are already collapsed to identity (engine
// strand renumbered to editor order; both MONO_PARAM tables identity). The ONE remaining
// misalignment is the POLY ENGINE lane order (0 REST 1 MEL 2 OCT 3 ACC) vs editor order
// (0 MEL 1 OCT 2 REST 3 ACC) — the sole reason ENGINE_LANE_TO_EDITOR/EDITOR_TO_ENGINE_LANE
// are non-identity and the source of the "MEL↔OCT↔REST circular permutation" bugs noted in
// StraitsEastSandsVisual.cpp. Renumbering the poly engine lanes to editor order would make
// both tables the identity and delete a whole class of remap bugs — but it touches the poly
// DATA model (engine.polyLen[v][lane], macroBase[lane], VoiceResolver lane arg, PROB_OUT_*
// indexing), so it's a deliberate refactor, best done on its own, not bundled with q-mix.
// The RNG STREAM order (RHYTHM0/MELODY1/CA2/SOURCE_SELECT3) is a separate axis and needn't
// align with lane order. Recommendation: align the poly engine order in a dedicated pass;
// until then keep the *_QMIX tables above as the single q-mix-aware source and mirror them
// in the generators rather than hand-rolling a third copy.

} // namespace dotModular
