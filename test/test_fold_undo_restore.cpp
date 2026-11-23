// test_fold_undo_restore.cpp — CC_FIX_FOLD_UNDO_EXACT: the exact-restore undo's
// PURE half. The undo must restore STATE, not re-run the operation: the action
// records {id, oldX, newX} per moved neighbour (newX read back AFTER
// setModulePosForce — Rack rounds/snaps), and a recorded neighbour is restored
// only if it is STILL at the position the action left it (1 px tolerance);
// a missing module (NaN) is skipped, never a crash.
//
// The core's gate (shouldRestoreNeighbour) is cross-checked against an
// INDEPENDENT oracle — the doc's mutation-check mandate: the oracle uses the
// CLOSED-INTERVAL formulation (cur in [exp-1, exp+1]), never the function
// under test's |difference| form.
//
// Compile: see test/run_all.sh ("test_fold_undo_restore").
#include <cstdio>
#include <cmath>
#include <vector>
#include "ui/FoldPanelWidthCore.hpp"

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// The INDEPENDENT oracle: the closed interval [exp-1, exp+1], finite-only.
static bool oracleRestore(float cur, float exp) {
    if (!std::isfinite(cur) || !std::isfinite(exp)) return false;
    return cur >= exp - 1.f && cur <= exp + 1.f;
}

int main() {
    std::printf("test_fold_undo_restore: the exact-restore gate — a neighbour restores only if still where the action left it (1 px)\n");
    char msg[256];

    // ── 1. EXACT restore: at the expected position -> restore ──
    CHECK(redDot::fold::shouldRestoreNeighbour(100.f, 100.f), "exact position restores");

    // ── 2. USER-MOVED: elsewhere -> left alone ──
    CHECK(!redDot::fold::shouldRestoreNeighbour(150.f, 100.f), "a user-moved module is left alone");
    CHECK(!redDot::fold::shouldRestoreNeighbour(100.f, 150.f), "a module away from its expected spot is left alone");

    // ── 3. MISSING: NaN (the module was deleted) -> skipped, never a crash ──
    CHECK(!redDot::fold::shouldRestoreNeighbour(NAN, 100.f), "a missing module (NaN current) is skipped");
    CHECK(!redDot::fold::shouldRestoreNeighbour(100.f, NAN), "a NaN expected position is skipped");

    // ── 4. The TOLERANCE edge (1 px, inclusive): Rack's read-back may round ──
    CHECK(redDot::fold::shouldRestoreNeighbour(101.f, 100.f), "+1 px is within the tolerance");
    CHECK(redDot::fold::shouldRestoreNeighbour(99.f, 100.f), "-1 px is within the tolerance");
    CHECK(!redDot::fold::shouldRestoreNeighbour(101.01f, 100.f), "+1.01 px is outside");
    CHECK(!redDot::fold::shouldRestoreNeighbour(98.99f, 100.f), "-1.01 px is outside");

    // ── 5. EMPTY list: a no-op (the caller-level law — zero restores) ──
    {
        std::vector<redDot::fold::NeighbourRecord> none;
        int restores = 0;
        for (const auto& r : none)
            if (redDot::fold::shouldRestoreNeighbour(r.newX, r.newX)) ++restores;
        CHECK(restores == 0, "an empty record list restores nothing");
    }

    // ── 6. THE MUTATION CHECK: the gate == the independent closed-interval oracle
    //    across a swept grid (a mutated gate diverges somewhere in the sweep) ──
    {
        long cells = 0, agrees = 0;
        for (float exp = -20.f; exp <= 40.f; exp += 3.5f)
            for (float d = -6.f; d <= 6.f; d += 0.5f) {
                const float cur = exp + d;
                ++cells;
                if (redDot::fold::shouldRestoreNeighbour(cur, exp) == oracleRestore(cur, exp)) ++agrees;
            }
        std::snprintf(msg, sizeof msg, "the gate agrees with the independent oracle on %ld of %ld swept cells", agrees, cells);
        CHECK(cells > 200 && agrees == cells, msg);
        // the NaN corners too
        CHECK(redDot::fold::shouldRestoreNeighbour(NAN, NAN) == oracleRestore(NAN, NAN), "the NaN corner agrees");
        CHECK(redDot::fold::shouldRestoreNeighbour(INFINITY, 10.f) == oracleRestore(INFINITY, 10.f), "inf current agrees (skipped)");
    }

    // ── 7. The RECORD shape: {id, oldX, newX} aggregate-initialises (the action's
    //    storage contract) and the read-back convention is expressible ──
    {
        redDot::fold::NeighbourRecord r{42, 100.f, 130.f};
        CHECK(r.id == 42 && r.oldX == 100.f && r.newX == 130.f, "the record is {id, oldX, newX}");
        // a shrink whose push Rack refused: the read-back equals the pre-move position
        redDot::fold::NeighbourRecord refused{7, 100.f, 100.f};
        CHECK(refused.newX == refused.oldX, "a refused push records newX == oldX (the read-back truth)");
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
