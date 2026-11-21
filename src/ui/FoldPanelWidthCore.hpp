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

}} // namespace redDot::fold
