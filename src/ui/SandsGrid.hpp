#pragma once
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
// A common LANE_H = 14 mm puts them on identical tops — Mono shrinks 15.667→14, East/Macro grow
// 12→14, which is exactly the trade Rodney asked for:
//     lane tops 0..3 = 14, 28, 42, 56   (both)
//     Mono  lanes 0..5 → 14 … 98        (ROW_BOT 108 → 98)
//     East/Macro 0..3 → 14 … 70         (ED_H 48 → 56)
//
// East/Macro's voice tabs (V1..V16, two rows) move ABOVE the grid, into 3..13 mm, so lane 0
// (MELODY) can start at 14 mm like Mono's. Their module logo therefore moves to the panel FOOTER.
//
// Mirror any change here in panel_src/gen_east_clean.py and panel_src/gen_macro_mono.py.
// ─────────────────────────────────────────────────────────────────────────────
namespace dotModular {
namespace SandsGrid {

    static constexpr float LANE_TOP   = 14.f;   // top of lane 0 — identical on all three
    static constexpr float LANE_H     = 14.f;   // one lane height everywhere
    static constexpr int   MONO_LANES = 6;      // MEL, OCT, REST, ACCENT, VARIATION, LEGATO
    static constexpr int   POLY_LANES = 4;      // MEL, OCT, REST, ACCENT (East/Macro)

    // Voice-tab band, above the grid (East/Macro only). Two rows of 5mm: 3..13.
    static constexpr float TAB_TOP   = 3.f;
    static constexpr float TAB_ROW_H = 5.f;
    static constexpr float TAB_ROWS  = 2.f;
    static_assert(TAB_TOP + TAB_ROWS * TAB_ROW_H <= LANE_TOP,
                  "voice-tab band must finish above lane 0");

    // Editor column (unchanged; already shared by all three).
    static constexpr float ED_X = 88.f;
    static constexpr float ED_W = 111.f;

    static constexpr float monoBottom() { return LANE_TOP + MONO_LANES * LANE_H; }  // 98
    static constexpr float polyBottom() { return LANE_TOP + POLY_LANES * LANE_H; }  // 70
    static constexpr float monoHeight() { return MONO_LANES * LANE_H; }             // 84
    static constexpr float polyHeight() { return POLY_LANES * LANE_H; }             // 56

    // Lane centre for either family — the single formula both used separately before.
    static constexpr float laneCentre(int lane) { return LANE_TOP + (lane + 0.5f) * LANE_H; }

} // namespace SandsGrid
} // namespace dotModular
