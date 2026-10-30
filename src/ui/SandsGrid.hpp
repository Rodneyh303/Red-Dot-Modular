#pragma once
#include "../dsp/LaneMapping.hpp"   // POLY_LANE_COUNT / EDITOR_LANE_COUNT for the cross-header guard below
// ─────────────────────────────────────────────────────────────────────────────
// SandsGrid — the ONE lane grid shared by all three Sands visual expanders.
//
// Before this header there were THREE independent ladders:
//   Mono  : ROW_TOP=14, ROW_BOT=108, 6 lanes  → laneH 15.667, lane tops 14 / 29.67 / 45.33 / 61 …
//   East  : ED_Y=23 (=18+TAB_OFFSET), ED_H=48, 4 lanes → laneH 12, lane tops 23 / 35 / 47 / 59
//   Macro : identical to East
// East/Macro's 4 lanes are the SAME lanes as Mono's first 4 (MEL/OCT/REST/ACCENT), yet nothing
// lined up when the modules sat side by side.
//
// A common LANE_H = 13 mm (Option B) puts them on identical tops and clears the Marina Bay Sands
// art at the bottom. Q-mix is a PLAIN lane at index 2 — no special height, no gap:
//     lane tops 0..4 = 14, 27, 40, 53, 66  (all three)
//     Mono/East lanes 0..6 → 14 … 105       (7 × 13; ROW_BOT 105)
//     Macro      lanes 0..4 → 14 … 79        (5 × 13; ED_H 65)
//
// East/Macro's voice tabs (V1..V16, two rows) move ABOVE the grid, into 3..13 mm, so lane 0
// (MELODY) can start at 14 mm like Mono's. Their module logo therefore moves to the panel FOOTER.
//
// Mirror any change here in panel_src/gen_east_clean.py and panel_src/gen_macro_mono.py
// (both carry a RANGE_MM that must equal RANGE_LANE_H — the painted editor recess,
// lane dividers and every rowY()-anchored control shift down by it in lockstep).
// ─────────────────────────────────────────────────────────────────────────────
namespace dotModular {
namespace SandsGrid {

    static constexpr float LANE_TOP   = 14.f;   // top of lane 0 — identical on all three
    static constexpr float LANE_H     = 13.f;   // one lane height everywhere (Option B: 14→13)
    // q-mix is a full lane at index 2. Engine supports 7 strands (Phase 1 complete).
    static constexpr int   MONO_LANES = 7;      // MEL, OCT, QMIX, REST, ACCENT, VARIATION, LEGATO
    // SANDS CONSOLIDATION Step 1: VARIATION + LEGATO are now FULL poly lanes (no longer mono-only).
    // All three families (Mono/East/Macro) carry the full uniform 7-lane set.
    static constexpr int   POLY_LANES = 7;      // MEL, OCT, QMIX, REST, ACCENT, VARIATION, LEGATO
    static constexpr int   EAST_LANES = 7;

    // Voice-tab band, above the grid (East/Macro only). Two rows of 5mm: 3..13.
    static constexpr float TAB_TOP   = 3.f;
    static constexpr float TAB_ROW_H = 5.f;
    static constexpr float TAB_ROWS  = 2.f;
    static_assert(TAB_TOP + TAB_ROWS * TAB_ROW_H <= LANE_TOP,
                  "voice-tab band must finish above lane 0");

    // Editor column. CC_PROMPT_04 STEP 1: ED_X 88→97 — the w-level knob column takes the
    // old 88 slot LEFT of the grid on East+Macro (bars keep width; everything downstream
    // of the grid shifts +9 with it in the generators + widget headers).
    static constexpr float ED_X = 97.f;
    static constexpr float ED_W = 111.f;

    // UNIFIED ADDRESSING Stage 3 (CC_PROMPT_UNIFIED_ADDRESSING_SPEC.md): the thin
    // VOICE-RANGE strip — a constant-block lane reserved at the TOP of the Sands grid
    // (the frame the probability lanes nest within), on the East + Macro editors only.
    // It shows the selected voice's effective read window (flat lit = in-range, dim =
    // out-of-range), animates with the engine's per-voice read cell, and is the drag-
    // authoring surface for the voice-LOR rows. The probability lanes keep their full
    // 13 mm — the editor BOX grows DOWN by the strip height instead (the gen scripts'
    // ED_H +RANGE_MM, rowY() +RANGE_MM in lockstep), so lane 0 starts at LANE_TOP +
    // RANGE_LANE_H and the strip occupies [LANE_TOP, LANE_TOP+RANGE_LANE_H).
    static constexpr float RANGE_LANE_H = 5.f;

    static constexpr float monoBottom() { return LANE_TOP + MONO_LANES * LANE_H; }  // 105 (7×13)
    static constexpr float polyBottom() { return LANE_TOP + POLY_LANES * LANE_H; }  // 79  (5×13)
    static constexpr float monoHeight() { return MONO_LANES * LANE_H; }             // 91  (7×13)
    static constexpr float polyHeight() { return POLY_LANES * LANE_H; }             // 65  (5×13)
    // The EDITOR BOX heights (strip + lanes). East/Macro size the editor widget and the
    // painted recess with these; the lane area itself is still mono/polyHeight().
    static constexpr float monoBoxHeight() { return RANGE_LANE_H + monoHeight(); }  // 96 (5+7×13)
    static constexpr float polyBoxHeight() { return RANGE_LANE_H + polyHeight(); }  // 96 (5+7×13)

    // Lane centre for either family — the single formula both used separately before.
    static constexpr float laneCentre(int lane) { return LANE_TOP + (lane + 0.5f) * LANE_H; }

    // NOTE: q-mix is a PLAIN lane at index 2 — it uses LANE_H like every other lane. There is
    // deliberately NO q-mix-specific height/helpers. Lane ORDER + the q-mix strand live in
    // dsp/LaneMapping.hpp (single source of truth); this header owns only GEOMETRY.

} // namespace SandsGrid

// Cross-header guard: dsp/LaneMapping.hpp keeps its own POLY_LANE_COUNT/EDITOR_LANE_COUNT
// (so it stays include-light and can host lorStoreBank/varlegStoreBank). They MUST equal the
// SandsGrid geometry counts, or the LOR-bank helpers would silently mis-map VAR/LEG again.
// Assert equality here (this header pulls in both) so a future lane-count change to one side
// that forgets the other trips at compile time.
static_assert(SandsGrid::POLY_LANES == POLY_LANE_COUNT,
              "SandsGrid::POLY_LANES must match dsp/LaneMapping.hpp POLY_LANE_COUNT (lorStoreBank)");
static_assert(SandsGrid::MONO_LANES == EDITOR_LANE_COUNT,
              "SandsGrid::MONO_LANES must match dsp/LaneMapping.hpp EDITOR_LANE_COUNT");
static_assert(SandsGrid::EAST_LANES == EDITOR_LANE_COUNT,
              "SandsGrid::EAST_LANES must match dsp/LaneMapping.hpp EDITOR_LANE_COUNT");

} // namespace dotModular
