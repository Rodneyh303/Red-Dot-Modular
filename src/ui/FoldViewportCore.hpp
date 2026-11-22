// FoldViewportCore — Rack-free horizontal viewport for the Straits group panel
// (SEAMLESS_FOLD_PLAN / GROUP_PANEL_INVENTORY: the Straits subpanels exceed the HP cap,
// so instead of unbounded width the player picks an HP that shows N subpanels (1/2/4/6)
// and SCROLLS back and forth across the rest).
//
// The model: an ordered run of `totalPanels` fixed-width subpanels; the module shows a
// contiguous window of `visible` of them; `scroll` is the index of the leftmost visible
// panel. All pure — the widget draws the window and the scroll arrows from these numbers.
// Composes with FoldBandSetCore: the viewport is ONE interior band whose width comes from
// widthHpForViewport; the host jacks / Causeway stay edge-pinned around it.
#pragma once
#include <algorithm>
#include <cmath>
#include "FoldPanelWidthCore.hpp"

namespace redDot { namespace fold {

// The viewport sizes Rodney offers (how many subpanels are shown at once). The core
// accepts ANY count; this is just the menu the widget presents.
constexpr int kViewportChoices[4] = {1, 2, 4, 6};

// visible is at least 1 and never more than the panel count (showing more slots than
// panels is just the panel count — no empty trailing slots in the width).
inline int clampVisible(int visible, int totalPanels) {
    int t = std::max(0, totalPanels);
    int v = std::max(1, visible);
    return (t == 0) ? 1 : std::min(v, t);
}

// How far the window can scroll: 0 when everything fits, else total - visible.
inline int maxScroll(int totalPanels, int visible) {
    return std::max(0, std::max(0, totalPanels) - clampVisible(visible, totalPanels));
}

// A scroll offset is always in [0, maxScroll].
inline int clampScroll(int offset, int totalPanels, int visible) {
    const int hi = maxScroll(totalPanels, visible);
    return std::min(std::max(offset, 0), hi);
}

// The visible window [first, first+count): first is the clamped scroll, count is the
// panels actually shown (visible, trimmed by what remains, floored at 0 for 0 panels).
struct VisibleRange { int first = 0; int count = 0; };
inline VisibleRange visibleRange(int scroll, int visible, int totalPanels) {
    VisibleRange r;
    if (totalPanels <= 0) return r;             // {0,0}
    const int v = clampVisible(visible, totalPanels);
    r.first = clampScroll(scroll, totalPanels, v);
    r.count = std::min(v, totalPanels - r.first);
    return r;
}

// Arrow enablement (back and forth, clamped — not circular).
inline bool canScrollLeft (int scroll, int totalPanels, int visible) {
    return clampScroll(scroll, totalPanels, visible) > 0;
}
inline bool canScrollRight(int scroll, int totalPanels, int visible) {
    return clampScroll(scroll, totalPanels, visible) < maxScroll(totalPanels, visible);
}

// The viewport's content width in px: `visible` panels plus the gaps between them.
inline float viewportContentPx(int visible, float panelWidthPx, float gapPx) {
    const int v = std::max(1, visible);
    const float w = (std::isfinite(panelWidthPx) && panelWidthPx > 0.f) ? panelWidthPx : 0.f;
    const float g = (std::isfinite(gapPx) && gapPx > 0.f) ? gapPx : 0.f;
    return (float)v * w + (float)(v - 1) * g;
}

// The module HP for a chosen viewport: base panel + the viewport content, snapped UP to
// the even HP grid (kMinHp anchor — the FoldBandSetCore convention), capped at kMaxHp.
// Because the viewport bounds how many panels show, the chosen sizes (1/2/4/6) stay well
// under the cap even when there are far more subpanels than fit.
inline int widthHpForViewport(float basePx, int visible, float panelWidthPx, float gapPx) {
    const float px = ((std::isfinite(basePx) && basePx > 0.f) ? basePx : 0.f)
                   + viewportContentPx(visible, panelWidthPx, gapPx);
    int hp = (int)std::ceil(px / kGridPx);
    return clampHp(hp, kMinHp);
}

// When `visible` changes (the player picks a different HP), keep the left edge where it
// is but re-clamp so the window never runs past the end.
inline int reclampScrollOnVisibleChange(int oldScroll, int newVisible, int totalPanels) {
    return clampScroll(oldScroll, totalPanels, newVisible);
}

}} // namespace redDot::fold
