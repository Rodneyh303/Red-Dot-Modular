#pragma once
#include <cstddef>
#include "SandsGrid.hpp"        // dotModular::SandsGrid::MONO_LANES / POLY_LANES / EAST_LANES
// ─────────────────────────────────────────────────────────────────────────────
// SandsLaneNames — the ONE canonical set of lane-NAME tables for the Sands visual
// expanders (Mono / East / Macro), in the two orderings that actually appear at
// config()/label sites.
//
// WHY THIS EXISTS (SANDS_LANE_INDEX_AUDIT.md): q-mix insertion kept mislabelling
// because every header carried its OWN local {"MEL","OCT",…} literal, and those
// literals silently kept the pre-q-mix count/order while the lane CONSTANTS moved.
// Centralising the tables + a length static_assert turns the next lane-count change
// from a silent "melody row labelled OCTAVE" into a COMPILE ERROR.
//
// There are exactly two orderings a label array is ever written in (the audit's #1/#2):
//   • EDITOR order — what the user sees top-to-bottom, what row/label/dir arrays use:
//         MEL OCT QMIX REST ACC VAR LEG        (q-mix at index 2)
//   • ENGINE/SPREAD order — PROB_OUT ids on East, sprPid[], spreadEffective[]:
//         REST MEL OCT ACC Q-MIX               (q-mix appended at index 4)
//
// PICK BY THE ARRAY'S CONVENTION, NOT BY EYE. Indexing the spread table with an
// editor lane (or vice-versa) is exactly the bug this file is meant to end — every
// consumer already owns the right bridge (EDITOR_TO_ENGINE_LANE_QMIX) for the id, and
// must feed THAT table's index the matching-convention lane.
//
// `inline constexpr` so the arrays are usable across translation units (ODR-safe).
// ─────────────────────────────────────────────────────────────────────────────
namespace dotModular {
namespace SandsLaneNames {

    // EDITOR order (index == editor lane). Length == MONO_LANES / EAST_LANES (both 7).
    // First 5 (0..4) are the POLY lanes; 5/6 are the mono-only VAR/LEG.
    inline constexpr const char* EDITOR[SandsGrid::MONO_LANES] =
        { "MEL", "OCT", "QMIX", "REST", "ACC", "VAR", "LEG" };
    static_assert(sizeof(EDITOR) / sizeof(EDITOR[0]) == SandsGrid::MONO_LANES,
                  "EDITOR lane-name table length must equal SandsGrid::MONO_LANES");
    static_assert(SandsGrid::MONO_LANES == SandsGrid::EAST_LANES,
                  "EDITOR table serves both Mono and East (equal lane counts)");

    // ENGINE/SPREAD order (index == poly engine/spread lane). Length == POLY_LANES (5).
    // Used where the id is engine-ordered: East PROB_OUT_*, sprPid[], spreadEffective[].
    inline constexpr const char* SPREAD[SandsGrid::POLY_LANES] =
        { "REST", "MEL", "OCT", "ACC", "Q-MIX" };
    static_assert(sizeof(SPREAD) / sizeof(SPREAD[0]) == SandsGrid::POLY_LANES,
                  "SPREAD lane-name table length must equal SandsGrid::POLY_LANES");

    // EDITOR order restricted to the POLY lanes (0..4) — for label sites that iterate
    // the poly lanes in EDITOR order (e.g. Mono delegation targets, East owner cells).
    // It is EDITOR[0..POLY_LANES); named so call sites can't accidentally grab the
    // spread ordering when they mean editor.
    inline constexpr const char* EDITOR_POLY[SandsGrid::POLY_LANES] =
        { "MEL", "OCT", "QMIX", "REST", "ACC" };
    static_assert(sizeof(EDITOR_POLY) / sizeof(EDITOR_POLY[0]) == SandsGrid::POLY_LANES,
                  "EDITOR_POLY lane-name table length must equal SandsGrid::POLY_LANES");

} // namespace SandsLaneNames
} // namespace dotModular
