#pragma once
// SandsSnapGeom — the PURE geometry decisions of the Sands snap cluster
// (ui/SandsSnap.hpp), split out so the rack-free unit rig can pin them: this
// header deliberately has NO rack dependency (plain floats), while SandsSnap.hpp
// carries the APP-hosted glue around it.
//
// THE LINKER LAW this decision mirrors — Rack v2 RackWidget::updateExpanders()
// (the ONLY thing that sets leftExpander/rightExpander for normal, browser-added
// modules): two modules connect if and only if their ROUNDED grid boxes touch
// EXACTLY — `p2Left.equals(pRight)` / `p2Right.equals(pLeft)` — which compares X
// AND Y, so "adjacent" means: same grid row, and the left module's right grid
// edge == the right module's left grid edge, as integers. There is no tolerance:
// a subpanel dropped with even one pixel of gap on its release position is never
// linked, and a chain-walking resnap then sees NOTHING, forever. (The Straits
// example never hits this because StraitsBase SPAWNS its lanes at exact flush
// positions — the Sands subpanels arrive from the browser.)
//
// sandsNearMiss is the magnet's window: the family module sits on the host's
// row and its left grid edge is in [0, SANDS_SNAP_SLOP] grid units right of the
// run's seating edge — close enough that the user clearly MEANT it docked, past
// what Rack's exact-equality linker would have caught on its own. Gap 0 is
// included on purpose: it heals a flush-but-unlinked stuck state (and is a
// no-op re-seat otherwise). Negative gap (overlap) is NOT the magnet's business —
// Rack's own drop resolution (setModulePosNearest/Squeeze) already seats drops
// without overlap, and a Ctrl-forced overlap is deliberate.
//
// Both coordinates are ROUNDED GRID UNITS (ModuleWidget::getGridBox), i.e. HP
// columns and 380px rows — the same quantisation the linker compares, so "snap
// to flush" and "linked by Rack" are the same predicate by construction.
namespace redDot {

// Snap window width in grid units. 1.0 = one HP (15 px): a drop within one HP
// right of the run's seating edge snaps flush; anything wider is a deliberate
// park and stays where the user put it.
inline constexpr float SANDS_SNAP_SLOP = 1.0f;

inline bool sandsNearMiss(float candTop, float candLeft,
                          float expectedTop, float expectedRight,
                          float slop = SANDS_SNAP_SLOP) {
    if (candTop != expectedTop) return false;   // a deliberate other-row park
    const float gap = candLeft - expectedRight; // + = gap, 0 = flush, - = overlap
    return gap >= 0.f && gap <= slop;
}

} // namespace redDot
