// test_lane_types.cpp — the EditorLane/EngineLane strong types + toEngine/toEditor bridge.
//
// Lane-index unification: editor == engine == strand (the PL_* enum was reordered
// to match the visual/editor/strand order). toEngine/toEditor are now IDENTITY.
// These types are kept for call-site readability but no longer permute.
#include "dsp/LaneMapping.hpp"
#include <cstdio>

using namespace dotModular;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } } while (0)

int main() {
    // Lane-index unification: editor == engine (identity).
    //   0 MEL  1 OCT  2 QMIX  3 REST  4 ACC  5 VAR  6 LEG
    CHECK(toEngine(EditorLane(0)).v == 0, "editor MELODY -> engine 0 (identity)");
    CHECK(toEngine(EditorLane(1)).v == 1, "editor OCTAVE -> engine 1 (identity)");
    CHECK(toEngine(EditorLane(2)).v == 2, "editor QMIX -> engine 2 (identity)");
    CHECK(toEngine(EditorLane(3)).v == 3, "editor REST -> engine 3 (identity)");
    CHECK(toEngine(EditorLane(4)).v == 4, "editor ACCENT -> engine 4 (identity)");

    // VAR/LEG are full poly lanes (identity too).
    CHECK(toEngine(EditorLane(5)).v == 5, "editor VAR -> engine 5 (identity)");
    CHECK(toEngine(EditorLane(6)).v == 6, "editor LEG -> engine 6 (identity)");
    CHECK(toEditor(toEngine(EditorLane(5))).v == 5, "editor VAR round-trips (identity)");
    CHECK(toEditor(toEngine(EditorLane(6))).v == 6, "editor LEG round-trips (identity)");

    // Inverse: engine -> editor (identity).
    CHECK(toEditor(EngineLane(0)).v == 0, "engine 0 -> editor 0 (identity)");
    CHECK(toEditor(EngineLane(1)).v == 1, "engine 1 -> editor 1 (identity)");
    CHECK(toEditor(EngineLane(2)).v == 2, "engine 2 -> editor 2 (identity)");
    CHECK(toEditor(EngineLane(3)).v == 3, "engine 3 -> editor 3 (identity)");
    CHECK(toEditor(EngineLane(4)).v == 4, "engine 4 -> editor 4 (identity)");

    // Round-trip over the poly lanes.
    for (int el = 0; el < POLY_LANE_COUNT; ++el)
        CHECK(toEditor(toEngine(EditorLane(el))).v == el, "editor->engine->editor round-trips");

    // ── Compile-time safety (documented; uncommenting must FAIL to compile) ──────
    // EngineLane bad = EditorLane(0);        // no EditorLane -> EngineLane conversion
    // int raw = EditorLane(0);               // no EditorLane -> int conversion
    // EditorLane x = 0;                      // construction is explicit only
    // if (EditorLane(0) == EngineLane(0)) {} // no cross-type comparison

    if (failures == 0) { std::printf("13 passed, 0 failed\n"); return 0; }
    std::printf("%d failed\n", failures);
    return 1;
}
