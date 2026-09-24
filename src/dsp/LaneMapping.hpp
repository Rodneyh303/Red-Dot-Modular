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
    // Renumbered to EDITOR order (MEL/OCT/QMIX/REST/ACC/VAR/LEG) with q-mix at index 2.
    // The switch(strand) accessors are keyed by NAME, so their behaviour is unchanged
    // by the renumber; this just makes MONO_LANE_TO_STRAND the identity (editor lane == strand index).
    STRAND_MELODY    = 0,
    STRAND_OCTAVE    = 1,
    STRAND_QMIX      = 2,   // Q-mix (quantizer mode: blend generated vs input melody)
    STRAND_RHYTHM    = 3,   // REST (was 2)
    STRAND_ACCENT    = 4,   // (was 3)
    STRAND_VARIATION = 5,   // (was 4)
    STRAND_LEGATO    = 6,   // (was 5)
    NUM_STRANDS      = 7,   // (was 6)
};

// Editor lane index → engine strand index.
//   editor 0 MELODY    -> melody
//   editor 1 OCTAVE    -> octave
//   editor 2 QMIX      -> qmix
//   editor 3 REST      -> rhythm
//   editor 4 ACCENT    -> accent
//   editor 5 VARIATION -> variation
//   editor 6 LEGATO    -> legato
constexpr int MONO_LANE_TO_STRAND[7] = {
    STRAND_MELODY,      // 0 MELODY
    STRAND_OCTAVE,      // 1 OCTAVE
    STRAND_QMIX,        // 2 QMIX
    STRAND_RHYTHM,      // 3 REST
    STRAND_ACCENT,      // 4 ACCENT
    STRAND_VARIATION,   // 5 VARIATION
    STRAND_LEGATO,      // 6 LEGATO
};

// Mono LOR param bank → editor lane.  COLLAPSED TO IDENTITY: the Mono ParamId
// LOR bank (lenId/offId/rotId) was renumbered to EDITOR order (MEL,OCT,QMIX,REST,ACC,
// VAR,LEG), so the param index now IS the editor lane. These tables have no live
// callers any more; kept as identity (and documented) so any stragglers are safe.
// Removable once confirmed nothing references them.
constexpr int MONO_PARAM_TO_EDITOR[7] = { 0, 1, 2, 3, 4, 5, 6 };
constexpr int EDITOR_TO_MONO_PARAM[7] = { 0, 1, 2, 3, 4, 5, 6 };

// ── OLD 4-wide poly bridge tables DELETED (q-mix regression, SANDS_LANE_INDEX_AUDIT) ──────────
// The pre-q-mix ENGINE_LANE_TO_EDITOR[4] = {2,0,1,3}, EDITOR_TO_ENGINE_LANE[4] = {1,2,0,3} and the
// SPREAD_LANE_TO_EDITOR alias to the former have been REMOVED. They had NO index for editor lane 2
// (q-mix), so any straggler using them made q-mix the one unselectable lane on East/Macro. Every live
// consumer now routes through the q-mix-aware *_QMIX tables below (verified: zero dotModular::-qualified
// reads of the bare names remain). Deleting them — not just leaving them unused — is the fix: the audit
// records this exact "two generations of the table coexist" class biting repeatedly. There is now ONE
// poly bridge (the _QMIX pair) and it is length-guarded by the static_asserts after it.

// ─────────────────────────────────────────────────────────────────────────────
// q-mix lane (ACTIVE — q-mix strand is now live) ───────────────────────────────
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
// counts DO grow (SandsGrid MONO/EAST 6→7, POLY 4→5) — atomically with the engine
// q-mix strand + its param/per-voice arrays.
//
// Strand enum — QMIX inserted at 2, editor-aligned, so MONO_LANE_TO_STRAND stays the
// IDENTITY (editor lane == strand index):
//     STRAND_MELODY 0, OCTAVE 1, QMIX 2, RHYTHM 3, ACCENT 4, VARIATION 5, LEGATO 6; NUM 7
// STRAND_QMIX generates off STREAM_SOURCE_SELECT; every other strand off rhythm/melody.
constexpr int  QMIX_EDITOR_LANE = 2;    // q-mix's editor lane (and, editor-aligned, its strand)
constexpr int  POLY_NONE        = -1;   // mono-only editor lane has no poly engine lane (VAR/LEG)
constexpr uint64_t QMIX_STREAM_KEY = 3; // == redDot::seed::STREAM_SOURCE_SELECT

// Poly engine lane → editor lane, WITH q-mix as a poly lane (appended at poly index 4, editor 2).
//   REST→3  MEL→0  OCT→1  ACC→4  QMIX→2          (was {2,0,1,3})
constexpr int ENGINE_LANE_TO_EDITOR_QMIX[5] = { 3, 0, 1, 4, 2 };
// Inverse over 7 editor lanes; VAR/LEG are mono-only (POLY_NONE).
constexpr int EDITOR_TO_ENGINE_LANE_QMIX[7] = { 1, 2, 4, 0, 3, POLY_NONE, POLY_NONE };

// ─── LOR store-bank: the ONE canonical editor-lane → lorBase[] bank mapping ───
// The lorBase store (Monsoon.hpp editor.lorBase) is banked in the order
//   MEL0 OCT1 REST2 ACC3 QMIX4 VAR5 LEG6   (== editor order; strand-aligned).
// Poly lanes (editor 0..4) reach it via EDITOR_TO_ENGINE_LANE_QMIX; VAR/LEG (editor
// 5/6) are POLY_NONE — they have NO poly engine lane, so callers historically hand-
// rolled `vl + <literal>`, INVISIBLE to this header. When QMIX shifted the bank layout
// those literals silently drifted (VAR read QMIX's bank, LEG read VAR's) → the
// "editing QMIX also edits LEGATO / VAR-LEG edits don't take" bug class.
//
// ALL LOR-bank call sites (East lorBank(), Mono readStrand bLor, MonsoonExpanderManager
// poly VAR/LEG, save/load) MUST route through lorStoreBank() so a future lane-count
// change updates exactly one place and VAR/LEG can never drift again.
constexpr int POLY_LANE_COUNT = 5;   // MEL/OCT/QMIX/REST/ACC (== SandsGrid::POLY_LANES; kept
                                     // here so this header stays self-contained / include-light)
constexpr int EDITOR_LANE_COUNT = 7; // + VAR/LEG (== SandsGrid::MONO_LANES / EAST_LANES)

// editorLane (0..6) → lorBase[] bank. Poly lanes map through the QMIX table; VAR/LEG map
// to themselves (banks 5/6), derived from POLY_LANE_COUNT so they track the poly count.
constexpr int lorStoreBank(int editorLane) {
    return (editorLane >= 0 && editorLane < POLY_LANE_COUNT)
               ? EDITOR_TO_ENGINE_LANE_QMIX[editorLane]           // MEL0 OCT1 QMIX4 REST0? -> table
               : editorLane;                                      // VAR(5)/LEG(6): self
}
// varleg index (0=VAR,1=LEG) → lorBase[] bank. The safe replacement for the old `vl + 4`
// literal: VAR→5, LEG→6, derived from POLY_LANE_COUNT.
constexpr int varlegStoreBank(int vl) { return POLY_LANE_COUNT + vl; }

// Compile-time guards nailing the exact banks so any future renumber that forgets a call
// site trips here instead of in the field (the VAR/LEG banks that silently drifted for QMIX).
static_assert(lorStoreBank(0) == 1 && lorStoreBank(2) == 4 && lorStoreBank(3) == 0,
              "lorStoreBank poly lanes route through EDITOR_TO_ENGINE_LANE_QMIX");
static_assert(lorStoreBank(5) == 5 && lorStoreBank(6) == 6, "VAR/LEG lorBase banks are 5/6");
static_assert(varlegStoreBank(0) == 5 && varlegStoreBank(1) == 6, "varleg banks: VAR5 LEG6");

// (laneSlot() REMOVED — it was the generators' ESLOT=[0,1,3,4...] preview-gap helper. q-mix is now
//  a plain lane at index 2 with full data + jacks, so there is no gap and no slot remap. Generators
//  index editor lanes 0..N directly.)

static_assert(MONO_LANE_TO_STRAND[QMIX_EDITOR_LANE] == STRAND_QMIX, "qmix is strand 2 (editor-aligned)");
static_assert(ENGINE_LANE_TO_EDITOR_QMIX[4] == 2 && EDITOR_TO_ENGINE_LANE_QMIX[2] == 4, "qmix poly<->editor round-trip");

// LENGTH GUARDS (the point of deleting the old tables): tie each bridge table's width to the lane
// count so the NEXT lane-count change fails to COMPILE instead of silently dropping a lane — the exact
// recurrence the audit records. ENGINE_LANE_TO_EDITOR_QMIX is one entry per POLY lane;
// EDITOR_TO_ENGINE_LANE_QMIX + MONO_LANE_TO_STRAND are one per EDITOR lane.
static_assert(sizeof(ENGINE_LANE_TO_EDITOR_QMIX) / sizeof(int) == POLY_LANE_COUNT,
              "ENGINE_LANE_TO_EDITOR_QMIX must have one entry per poly lane (POLY_LANE_COUNT)");
static_assert(sizeof(EDITOR_TO_ENGINE_LANE_QMIX) / sizeof(int) == EDITOR_LANE_COUNT,
              "EDITOR_TO_ENGINE_LANE_QMIX must have one entry per editor lane (EDITOR_LANE_COUNT)");
static_assert(sizeof(MONO_LANE_TO_STRAND) / sizeof(int) == EDITOR_LANE_COUNT,
              "MONO_LANE_TO_STRAND must have one entry per editor lane (EDITOR_LANE_COUNT)");
// Every editor lane 0..POLY_LANE_COUNT-1 has a REAL poly engine lane; VAR/LEG are exactly POLY_NONE.
static_assert(EDITOR_TO_ENGINE_LANE_QMIX[POLY_LANE_COUNT - 1] != POLY_NONE
              && EDITOR_TO_ENGINE_LANE_QMIX[POLY_LANE_COUNT] == POLY_NONE,
              "poly editor lanes map to a real engine lane; the first mono-only lane is POLY_NONE");

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
