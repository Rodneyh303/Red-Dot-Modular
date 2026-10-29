// test_sands_snap_geom.cpp — the Sands snap cluster's PURE near-miss decision
// (ui/SandsSnapGeom.hpp). The cluster's APP-hosted glue (the attract scan, the
// snapshot resnap, the guest-flag harvest) is rack-hosted and lives outside this
// rig — pinned structurally instead — but the DECISION that governs all of it,
// "is this family module in the snap window right of the run's seating edge, on
// the host's row?", is pure math and pinned here.
//
// The law it must mirror: Rack v2 RackWidget::updateExpanders() connects
// expanders ONLY when the ROUNDED grid boxes touch EXACTLY (p2Left.equals(pRight),
// X AND Y). The magnet's window is therefore the same-row gap in [0,
// SANDS_SNAP_SLOP] grid units: 0 heals a flush-but-unlinked stuck state, (0,2]
// is the "the user meant it docked" window Rack's exact-equality linker just
// declined (task 84: two HP — the subpanels are 26/52 HP wide and a browser
// drop aims the grab point, not the left edge), >2 is a deliberate park,
// negative is an overlap (Rack's own drop resolution's business, or a
// deliberate Ctrl-force), and a different row is a deliberate park (Rack
// rounds drag y to rows, so a row mismatch is chosen).
#include "ui/SandsSnapGeom.hpp"
#include <cstdio>

static int fails = 0;
static void expect(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++fails; }
}

int main() {
    using namespace redDot;

    // ── same row: the linker just missed it ──
    expect(sandsNearMiss(0.f, 10.f, 0.f, 10.f), "gap 0 — flush-but-unlinked heal");
    expect(sandsNearMiss(0.f, 10.25f, 0.f, 10.f), "gap 0.25 — sub-pixel release gap");
    expect(sandsNearMiss(0.f, 10.5f, 0.f, 10.f), "gap 0.5 — half an HP");
    expect(sandsNearMiss(0.f, 11.f, 0.f, 10.f), "gap 1.0 — one HP inside the window");
    expect(sandsNearMiss(0.f, 12.f, 0.f, 10.f), "gap 2.0 — the slop edge is INCLUSIVE");

    // ── same row: outside the window ──
    expect(!sandsNearMiss(0.f, 12.0001f, 0.f, 10.f), "gap beyond slop — deliberate park");
    expect(!sandsNearMiss(0.f, 25.f, 0.f, 10.f), "gap 15 — parked far right");
    expect(!sandsNearMiss(0.f, 9.5f, 0.f, 10.f), "overlap (negative gap) — not the magnet's business");
    expect(!sandsNearMiss(0.f, -3.f, 0.f, 10.f), "parked left of the run — right-attachment only");

    // ── rows: a row mismatch is a chosen park (Rack rounds drag y to rows) ──
    expect(!sandsNearMiss(1.f, 10.5f, 0.f, 10.f), "one row below — deliberate");
    expect(!sandsNearMiss(-1.f, 10.5f, 0.f, 10.f), "one row above — deliberate");

    // ── negative rows/columns: the math must not assume non-negative grids ──
    expect(sandsNearMiss(-3.f, 4.2f, -3.f, 4.f), "negative row + flush-ish gap");
    expect(!sandsNearMiss(-3.f, 4.2f, -2.f, 4.f), "negative rows still compare exactly");

    // ── custom slop: the window is a parameter (keeps the constant honest) ──
    expect(sandsNearMiss(0.f, 12.f, 0.f, 10.f, 2.f), "slop 2 admits gap 2");
    expect(!sandsNearMiss(0.f, 12.f, 0.f, 10.f, 1.f), "slop 1 rejects gap 2");
    expect(SANDS_SNAP_SLOP == 2.0f, "SANDS_SNAP_SLOP stays two grid units (2 HP)");

    if (fails) { std::printf("test_sands_snap_geom: %d FAILURES\n", fails); return 1; }
    std::printf("test_sands_snap_geom: all checks passed\n");
    return 0;
}
