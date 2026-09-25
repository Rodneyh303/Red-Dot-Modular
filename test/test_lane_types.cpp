// test_lane_types.cpp — the EditorLane/EngineLane strong types + toEngine/toEditor bridge.
//
// These types exist to make "editor lane vs engine lane" a COMPILE-TIME distinction, so the
// recurring "MELODY row modulates REST" bug (an editor index fed into an engine-order store)
// can't be written. This test pins the runtime conversion; the compile-time safety is proven
// by the static_asserts in dsp/LaneMapping.hpp and the (commented) negative cases below.
#include "dsp/LaneMapping.hpp"
#include <cstdio>

using namespace dotModular;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } } while (0)

int main() {
    // Editor order (panel rows): 0 MEL, 1 OCT, 2 QMIX, 3 REST, 4 ACC, 5 VAR, 6 LEG.
    // Engine order (store):      0 REST, 1 MEL, 2 OCT, 3 ACC, 4 QMIX.
    CHECK(toEngine(EditorLane(0)).v == 1, "editor MELODY -> engine 1");
    CHECK(toEngine(EditorLane(1)).v == 2, "editor OCTAVE -> engine 2");
    CHECK(toEngine(EditorLane(2)).v == 4, "editor QMIX -> engine 4");
    CHECK(toEngine(EditorLane(3)).v == 0, "editor REST -> engine 0");
    CHECK(toEngine(EditorLane(4)).v == 3, "editor ACCENT -> engine 3");

    // VAR/LEG are mono-only: no engine lane.
    CHECK(toEngine(EditorLane(5)).v == POLY_NONE, "editor VAR -> POLY_NONE");
    CHECK(toEngine(EditorLane(6)).v == POLY_NONE, "editor LEG -> POLY_NONE");

    // Inverse: engine -> editor for the five poly lanes.
    CHECK(toEditor(EngineLane(0)).v == 3, "engine REST -> editor 3");
    CHECK(toEditor(EngineLane(1)).v == 0, "engine MEL  -> editor 0");
    CHECK(toEditor(EngineLane(2)).v == 1, "engine OCT  -> editor 1");
    CHECK(toEditor(EngineLane(3)).v == 4, "engine ACC  -> editor 4");
    CHECK(toEditor(EngineLane(4)).v == 2, "engine QMIX -> editor 2");

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
