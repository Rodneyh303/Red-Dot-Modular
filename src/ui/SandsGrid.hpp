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
// A common LANE_H = 13 mm (Option B) puts them on identical tops and clears the Marina Bay Sands
// art at the bottom. Q-mix is a PLAIN lane at index 2 — no special height, no gap:
//     lane tops 0..4 = 14, 27, 40, 53, 66  (all three)
//     Mono/East lanes 0..6 → 14 … 105       (7 × 13; ROW_BOT 105)
//     Macro      lanes 0..4 → 14 … 79        (5 × 13; ED_H 65)
//
// East/Macro's voice tabs (V1..V16, two rows) move ABOVE the grid, into 3..13 mm, so lane 0
// (MELODY) can start at 14 mm like Mono's. Their module logo therefore moves to the panel FOOTER.
//
// Mirror any change here in panel_src/gen_east_clean.py and panel_src/gen_macro_mono.py.
// ─────────────────────────────────────────────────────────────────────────────
namespace dotModular {
namespace SandsGrid {

    static constexpr float LANE_TOP   = 14.f;   // top of lane 0 — identical on all three
    static constexpr float LANE_H     = 13.f;   // one lane height everywhere (Option B: 14→13)
    // q-mix is a full lane at index 2. Engine supports 7 strands (Phase 1 complete).
    static constexpr int   MONO_LANES = 7;      // MEL, OCT, QMIX, REST, ACCENT, VARIATION, LEGATO
    static constexpr int   POLY_LANES = 5;      // MEL, OCT, QMIX, REST, ACCENT (Macro; East's spread rows)
    // East displays all seven lanes (adds VARIATION, LEGATO).
    // Lanes 5/6 (VAR/LEG) are display-only until the per-voice LOR feature lands (EAST_EXTRA_LANES.md).
    static constexpr int   EAST_LANES = 7;

    // Voice-tab band, above the grid (East/Macro only). Two rows of 5mm: 3..13.
    static constexpr float TAB_TOP   = 3.f;
    static constexpr float TAB_ROW_H = 5.f;
    static constexpr float TAB_ROWS  = 2.f;
    static_assert(TAB_TOP + TAB_ROWS * TAB_ROW_H <= LANE_TOP,
                  "voice-tab band must finish above lane 0");

    // Editor column (unchanged; already shared by all three).
    static constexpr float ED_X = 88.f;
    static constexpr float ED_W = 111.f;

    static constexpr float monoBottom() { return LANE_TOP + MONO_LANES * LANE_H; }  // 105 (7×13)
    static constexpr float polyBottom() { return LANE_TOP + POLY_LANES * LANE_H; }  // 79  (5×13)
    static constexpr float monoHeight() { return MONO_LANES * LANE_H; }             // 91  (7×13)
    static constexpr float polyHeight() { return POLY_LANES * LANE_H; }             // 65  (5×13)

    // Lane centre for either family — the single formula both used separately before.
    static constexpr float laneCentre(int lane) { return LANE_TOP + (lane + 0.5f) * LANE_H; }

    // NOTE: q-mix is a PLAIN lane at index 2 — it uses LANE_H like every other lane. There is
    // deliberately NO q-mix-specific height/helpers. Lane ORDER + the q-mix strand live in
    // dsp/LaneMapping.hpp (single source of truth); this header owns only GEOMETRY.

} // namespace SandsGrid
} // namespace dotModular
