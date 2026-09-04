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
// q-mix is a FULL Sands lane: same LEN/OFF/ROT + spread + per-step editing + CV/attens +
// prob-out as MEL/OCT/REST/ACC. That parity is the ENTIRE POINT of adding it to the Sands
// editors. The ONE thing special about it: its values are drawn from q-mix's OWN Philox
// stream (STREAM_SOURCE_SELECT = 3, see dsp/PhiloxRng.hpp), not the shared rhythm/melody
// streams — so "which notes" (q-mix) decorrelates from "where they interleave" (melody).
//
// It inserts as editor lane 2 (after MEL/OCT), so every editor index at/after 2 shifts +1:
//     0 MEL  1 OCT  2 QMIX  3 REST  4 ACCENT  5 VARIATION  6 LEGATO
// q-mix is per-voice → it's a POLY lane too. Because it's a REAL lane with data, the DATA
// counts DO grow (SandsGrid MONO/EAST 6→7, POLY 4→5) — but ONLY together with the engine
// q-mix strand + its param/per-voice arrays. One atomic feature: strand + arrays + counts +
// editor + these mappings. (Correction: an earlier note here modelled q-mix as a dataless
// slot with STRAND_NONE — wrong. It has a full strand; the counts do go up.)
//
// Planned strand enum — QMIX inserted at 2, editor-aligned, so MONO_LANE_TO_STRAND stays the
// IDENTITY (editor lane == strand index):
//     STRAND_MELODY 0, OCTAVE 1, QMIX 2, RHYTHM 3, ACCENT 4, VARIATION 5, LEGATO 6; NUM 7
// STRAND_QMIX generates off STREAM_SOURCE_SELECT; every other strand off rhythm/melody.
constexpr int  QMIX_EDITOR_LANE = 2;    // q-mix's editor lane (and, editor-aligned, its strand)
constexpr int  NUM_STRANDS_QMIX = 7;
constexpr int  POLY_NONE        = -1;   // mono-only editor lane has no poly engine lane (VAR/LEG)
constexpr uint64_t QMIX_STREAM_KEY = 3; // == redDot::seed::STREAM_SOURCE_SELECT

// Editor lane (7) → engine strand: IDENTITY under the editor-aligned enum above.
constexpr int MONO_LANE_TO_STRAND_QMIX[7] = { 0, 1, 2, 3, 4, 5, 6 };

// Poly engine lane → editor lane, WITH q-mix as a poly lane (appended at poly index 4, editor 2).
//   REST→3  MEL→0  OCT→1  ACC→4  QMIX→2          (was {2,0,1,3})
constexpr int ENGINE_LANE_TO_EDITOR_QMIX[5] = { 3, 0, 1, 4, 2 };
// Inverse over 7 editor lanes; VAR/LEG are mono-only (POLY_NONE).
constexpr int EDITOR_TO_ENGINE_LANE_QMIX[7] = { 1, 2, 4, 0, 3, POLY_NONE, POLY_NONE };

// laneSlot() — the generators' ESLOT=[0,1,3,4,5,6] as a function. INTERIM ONLY: it exists so
// the geometry-preview generators can leave editor slot 2 EMPTY until the q-mix strand lands.
// In the finished feature q-mix is just lane 2 with full data — no gap, no laneSlot needed.
constexpr int laneSlot(int editorLaneNoQmix) {
    return editorLaneNoQmix < QMIX_EDITOR_LANE ? editorLaneNoQmix : editorLaneNoQmix + 1;
}

static_assert(MONO_LANE_TO_STRAND_QMIX[QMIX_EDITOR_LANE] == 2, "qmix is strand 2 (editor-aligned)");
static_assert(ENGINE_LANE_TO_EDITOR_QMIX[4] == 2 && EDITOR_TO_ENGINE_LANE_QMIX[2] == 4, "qmix poly<->editor round-trip");
static_assert(laneSlot(1) == 1 && laneSlot(2) == 3, "interim slot shift");

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
// Because q-mix is itself a NEW poly lane, landing it already touches the poly data model
// (polyLen/macroBase/VoiceResolver/PROB_OUT) — so folding the poly-order alignment INTO the
// q-mix pass (making all orderings identity with q-mix at index 2 everywhere) is worth weighing
// against doing it separately; either way, do it deliberately, not by accident.

} // namespace dotModular
