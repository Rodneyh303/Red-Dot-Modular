#pragma once
#include <cstdint>   // uint64_t (QMIX_STREAM_KEY) — keep this header self-contained
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
    // by the renumber; editor lane == strand index (identity).
    STRAND_MELODY    = 0,
    STRAND_OCTAVE    = 1,
    STRAND_QMIX      = 2,   // Q-mix (quantizer mode: blend generated vs input melody)
    STRAND_RHYTHM    = 3,   // REST (was 2)
    STRAND_ACCENT    = 4,   // (was 3)
    STRAND_VARIATION = 5,   // (was 4)
    STRAND_LEGATO    = 6,   // (was 5)
    NUM_STRANDS      = 7,   // (was 6)
};

// ERADICATION Phase 3b: MONO_LANE_TO_STRAND, MONO_PARAM_TO_EDITOR,
// EDITOR_TO_MONO_PARAM — ALL DELETED. They were identity ({0,1,2,3,4,5,6}).
// Editor lane == engine strand == param lane (lane-index unification).
// Call sites now use the lane index directly.

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
// Strand enum — QMIX inserted at 2, editor-aligned (editor lane == strand index, identity):
//     STRAND_MELODY 0, OCTAVE 1, QMIX 2, RHYTHM 3, ACCENT 4, VARIATION 5, LEGATO 6; NUM 7
// STRAND_QMIX generates off STREAM_SOURCE_SELECT; every other strand off rhythm/melody.
constexpr int  QMIX_EDITOR_LANE = 2;    // q-mix's editor lane (and, editor-aligned, its strand)
constexpr int  POLY_NONE        = -1;   // (kept for legacy callers; no mono-only poly lane remains post-consolidation)
constexpr uint64_t QMIX_STREAM_KEY = 3; // == redDot::seed::STREAM_SOURCE_SELECT

// ERADICATION Phase 3b: ENGINE_LANE_TO_EDITOR_QMIX, EDITOR_TO_ENGINE_LANE_QMIX —
// DELETED. Identity since the lane-index unification (editor == engine == strand).
// Call sites now use the lane index directly.

// ─── LOR store-bank: the ONE canonical editor-lane → lorBase[] bank mapping ───
// The lorBase store (Monsoon.hpp editor.lorBase) is banked in EDITOR order — IDENTITY
// since the lane-index unification (editor == engine == strand):
//   MEL0 OCT1 QMIX2 REST3 ACC4 VAR5 LEG6.
// Historically VAR/LEG were appended past the poly lanes with hand-rolled literals
// (INVISIBLE to this header), which drifted when QMIX shifted the bank layout (VAR
// read QMIX's bank, LEG read VAR's) → the "editing QMIX also edits LEGATO / VAR-LEG
// edits don't take" bug class.
//
// ALL LOR-bank call sites (East lorBank(), Mono readStrand bLor, MonsoonExpanderManager
// poly VAR/LEG, save/load) MUST route through lorStoreBank() so a future lane-count
// change updates exactly one place and VAR/LEG can never drift again.
constexpr int POLY_LANE_COUNT = 7;   // MEL/OCT/QMIX/REST/ACC/VAR/LEG (== SandsGrid::POLY_LANES; SANDS CONSOLIDATION Step 1)
constexpr int EDITOR_LANE_COUNT = 7; // == SandsGrid::MONO_LANES / EAST_LANES (all 7 are poly now)

// editorLane (0..6) → lorBase[] bank. Poly lanes map through the QMIX table; VAR/LEG map
// to themselves (banks 5/6), derived from POLY_LANE_COUNT so they track the poly count.
constexpr int lorStoreBank(int editorLane) {
    return editorLane;   // identity (lane-index unification: editor == engine == strand)
}
// varleg index (0=VAR,1=LEG) → lorBase[] bank. SANDS CONSOLIDATION Step 1: VAR/LEG are now
// regular poly lanes (editor 5/6), so route through lorStoreBank (the canonical table) rather
// than the old "POLY_LANE_COUNT + vl" append formula (which gave 7/8, not 5/6, post-widen).
constexpr int varlegStoreBank(int vl) { return lorStoreBank(5 + vl); }

// Compile-time guards nailing the exact banks so any future renumber that forgets a call
// site trips here instead of in the field (the VAR/LEG banks that silently drifted for QMIX).
static_assert(lorStoreBank(0) == 0 && lorStoreBank(2) == 2 && lorStoreBank(3) == 3,
              "lorStoreBank is identity (lane-index unification: editor==engine)");
static_assert(lorStoreBank(5) == 5 && lorStoreBank(6) == 6, "VAR/LEG lorBase banks are 5/6");
static_assert(varlegStoreBank(0) == 5 && varlegStoreBank(1) == 6, "varleg banks: VAR5 LEG6");

// (laneSlot() REMOVED — it was the generators' ESLOT=[0,1,3,4...] preview-gap helper. q-mix is now
//  a plain lane at index 2 with full data + jacks, so there is no gap and no slot remap. Generators
//  index editor lanes 0..N directly.)

// ERADICATION Phase 3b: table static_asserts DELETED (tables deleted — identity).
// The identity invariant is now carried by lorStoreBank + toEngine/toEditor below.
static_assert((int)STRAND_QMIX == QMIX_EDITOR_LANE, "qmix is strand 2 (editor-aligned)");

// ─────────────────────────────────────────────────────────────────────────────
// STRONG LANE TYPES — make "editor lane vs engine lane" a COMPILE-TIME distinction.
//
// The recurring Sands bug ("the MELODY row modulates/labels REST") was ALWAYS the same
// shape: a lane index in EDITOR order (panel rows MEL,OCT,QMIX,REST,ACC) fed straight
// into an ENGINE-order store (REST,MEL,OCT,ACC,QMIX). Both were bare int, so the
// compiler couldn't see it and it only surfaced by hovering a knob in Rack.
//
// EditorLane / EngineLane are one-int strong typedefs:
//   • construct EXPLICITLY only (no int → EditorLane silently),
//   • do NOT convert to each other or to int implicitly (mixing them won't compile),
//   • expose .v to reach the raw index for the still-int-keyed store/port APIs,
//   • bridge ONLY through toEngine()/toEditor(), the single named conversion
//     (IDENTITY since the lane-index unification — the tables are deleted).
// Adoption is OPT-IN: existing int call sites keep compiling; UI code takes an
// EditorLane row and calls toEngine(row).v for the store. See test/test_lane_types.cpp.
struct EngineLane { int v; explicit constexpr EngineLane(int lane) : v(lane) {} };
struct EditorLane { int v; explicit constexpr EditorLane(int lane) : v(lane) {} };

// Editor row → engine lane. IDENTITY since the lane-index unification (editor ==
// engine == strand): all seven lanes map to themselves; out-of-range → POLY_NONE.
constexpr EngineLane toEngine(EditorLane el) {
    return EngineLane((el.v >= 0 && el.v < POLY_LANE_COUNT) ? el.v : POLY_NONE);
}
// Engine lane → editor row. IDENTITY (see toEngine).
constexpr EditorLane toEditor(EngineLane eng) {
    return EditorLane((eng.v >= 0 && eng.v < POLY_LANE_COUNT) ? eng.v : POLY_NONE);
}

// Lane-index unification: toEngine/toEditor are IDENTITY
static_assert(toEngine(EditorLane(0)).v == 0, "editor MELODY -> engine 0 (identity)");
static_assert(toEngine(EditorLane(2)).v == 2, "editor QMIX -> engine 2 (identity)");
static_assert(toEngine(EditorLane(3)).v == 3, "editor REST -> engine 3 (identity)");
static_assert(toEditor(toEngine(EditorLane(2))).v == 2, "editor->engine->editor round-trips (identity)");
static_assert(toEngine(EditorLane(5)).v == 5, "VAR identity");
static_assert(toEditor(toEngine(EditorLane(5))).v == 5, "VAR round-trips (identity)");
static_assert(toEditor(toEngine(EditorLane(6))).v == 6, "LEG round-trips (identity)");

// ─── NOTE: the poly engine order IS aligned to the editor order — DONE ───────
// The historical misalignment (poly engine 0 REST 1 MEL 2 OCT 3 ACC vs editor
// 0 MEL 1 OCT 2 QMIX 3 REST 4 ACC) was the sole reason the lane tables were
// non-identity and the source of the "MEL↔OCT↔REST circular permutation" bug
// class. The lane-index unification renumbered the PL_ enum to the editor/strand
// order, making EVERY lane mapping identity; the conversion tables were then
// deleted outright (Phase 3b) so a second generation can never coexist with the
// first. The RNG STREAM order (RHYTHM/MELODY/CA/SOURCE_SELECT) is a separate
// axis and does not follow lane order.

} // namespace dotModular
