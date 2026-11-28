// FoldPanelWidthCore — Rack-free sizing core for the seamless-fold resizable module
// (docs/design/SEAMLESS_FOLD_PLAN.md step 1). No rack:: includes: the widget does the
// Rack work (box.size.x, setModulePosForce, undo); the numbers all come from here so
// they can be unit-tested (test_fold_panel_width).
//
// Law: every width the module ever has is an ALLOWED width — a multiple of kStepHp inside
// [minContentHp, kMaxHp]. Anything else (saved garbage, a drag, a stale patch) is snapped,
// never trusted. The content can never be squeezed below what it needs (the min-content rule).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace redDot { namespace fold {

constexpr float kGridPx = 15.f;   // == rack::RACK_GRID_WIDTH (one HP); kept literal so this header stays Rack-free
constexpr int   kMinHp  = 12;     // absolute floor for any fold module
constexpr int   kMaxHp  = 120;    // absolute cap
constexpr int   kStepHp = 2;      // widths move in 2 HP steps

// a width is allowed iff it is inside [minContentHp, kMaxHp] and on the step grid
// (kMaxHp is always allowed, even if the grid would skip it).
inline int clampHp(int hp, int minContentHp) {
    const int lo = std::min(std::max(minContentHp, kMinHp), kMaxHp);
    int v = std::min(std::max(hp, lo), kMaxHp);
    if (v != kMaxHp) {                                  // snap to the step grid, rounding up from lo
        const int off = (v - lo) % kStepHp;
        v = (off == 0) ? v : std::min(v + (kStepHp - off), kMaxHp);
    }
    return v;
}
inline bool isAllowedHp(int hp, int minContentHp) { return clampHp(hp, minContentHp) == hp; }

inline float hpToPx(int hp) { return hp * kGridPx; }
inline int   pxToHp(float px) { return (int)std::lround(px / kGridPx); }

// The saved width comes from the patch JSON (a double, may be absent or garbage). Always lands on an allowed width.
inline int decodeSavedHp(bool present, double value, int defaultHp, int minContentHp) {
    if (!present || !std::isfinite(value)) return clampHp(defaultHp, minContentHp);
    if (value <= -1e9) return clampHp(kMinHp, minContentHp);
    if (value >= 1e9)  return kMaxHp;
    return clampHp((int)std::lround(value), minContentHp);
}

// Rack's mm→px factor (96 dpi) — kept literal like kGridPx so the header stays Rack-free.
constexpr float kMmPx = 96.f / 25.4f;
inline float mmToPx(float mm) { return mm * kMmPx; }

// The min-content rule (the widget-half spike): the smallest ALLOWED width whose
// px still fit the content's right edge in mm. Rounds UP (ceil) then clamps/snaps
// up — the content can never be squeezed; two HP less never fits.
inline int contentMinHp(float contentRightMm) {
    int hp = (int)std::ceil(mmToPx(contentRightMm) / kGridPx);
    return clampHp(hp, kMinHp);
}

// x for a child that must keep a fixed inset from the RIGHT edge (screws, edge decor) after a width change.
inline float rightAnchoredX(float widthPx, float insetPx) { return widthPx - insetPx; }

// how far modules to the RIGHT of this one must move when the width changes (setModulePosForce).
inline float neighbourShiftPx(int oldHp, int newHp) { return (float)(newHp - oldHp) * kGridPx; }

// ── the exact-restore undo records (CC_FIX_FOLD_UNDO_EXACT) ─────────────────────
// UNDO RESTORES STATE, never the operation: the action stores one record per moved
// neighbour and restores it only if it is STILL where the action left it.
struct NeighbourRecord {
    int64_t id = -1;
    float oldX = 0.f;   // the position BEFORE the push
    float newX = 0.f;   // the position AFTER setModulePosForce — READ BACK (Rack rounds/snaps; a push it refuses records newX == oldX)
};
// The pure restore gate: restore only if still at the expected x (1 px tolerance).
// A NaN / non-finite current = the module is gone (deleted since) -> skip, never a crash.
inline bool shouldRestoreNeighbour(float currentX, float expectedX, float tolPx = 1.f) {
    if (!std::isfinite(currentX) || !std::isfinite(expectedX)) return false;
    return std::fabs(currentX - expectedX) <= tolPx;
}

// ── RESIZE ORDER + TIMING (the adjacency-preserving plan) ───────────────────────
// setModulePosForce places a module AT the target and SHOVES whatever the target
// overlaps. So the ONLY way a per-neighbour push is a clean rigid translation is to
// visit the neighbours in an order where each target lands on EMPTY space (no
// overlap at call time), and to resize ourself at the moment that keeps it empty:
//
//   GROW  (shift>0): resize self LAST. Translate neighbours FARTHEST-FIRST
//                    (descending x): the far module moves +shift into open space,
//                    then each nearer module moves +shift into the gap the farther
//                    one just vacated — never overlapping it. Finally our box grows
//                    into the shift-wide gap left at our right edge. Flush.
//   SHRINK(shift<0): resize self FIRST (frees a shift-wide gap at our right edge).
//                    Translate neighbours NEAREST-FIRST (ascending x): the nearest
//                    moves -|shift| flush against our new edge, then each farther
//                    module follows into the gap the nearer one vacated. Flush.
//
// The OLD spike always pushed-then-resized in getModuleIds() order: on grow that
// re-shifted already-shoved modules (gaps mid-chain → the chain broke); on shrink
// the push hit our still-wide box (neighbours stopped short → the accepted gap).
//
// resizeSelfFirst: true iff we must resize before touching the neighbours (shrink).
inline bool resizeSelfFirst(float shiftPx) { return shiftPx < 0.f; }

// Strict-weak order for the neighbour walk given the signed pixel shift: grow =>
// descending x (farthest first); shrink => ascending x (nearest first).
inline bool resizeOrderBefore(float ax, float bx, float shiftPx) {
    return (shiftPx > 0.f) ? (ax > bx) : (ax < bx);
}

// Processing order (indices into xs) for a resize by signed pixel `shift`.
// Pure and testable: the widget sorts its collected neighbours by this, the
// simulation test drives the same planner to prove flush/no-overlap layouts.
inline std::vector<int> resizeNeighbourOrder(const std::vector<float>& xs, float shiftPx) {
    std::vector<int> idx(xs.size());
    for (std::size_t i = 0; i < xs.size(); ++i) idx[i] = (int)i;
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b) { return resizeOrderBefore(xs[a], xs[b], shiftPx); });
    return idx;
}

}} // namespace redDot::fold
