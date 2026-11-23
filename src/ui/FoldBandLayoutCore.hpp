// FoldBandLayoutCore — Rack-free band tiling for the seamless-fold draw (step 2 of
// SEAMLESS_FOLD_PLAN.md; CC_PROMPT_FOLD_SEAMLESS_DRAW.md §1). Given the module's total
// width and an ordered list of band specs, it lays the bands out so they TILE the box
// exactly — no gap, no overlap, integer interior dividers (half-pixel dividers are the
// seam). `SandsFold.cpp` does the drawing; every number comes from here (the
// FoldPanelWidthCore discipline).
//
// Pairs with FoldBandSetCore: that core decides WHICH bands are enabled, their order
// (jack panels edge-pinned), and the module's total width; feed its ordered specs and
// its width here. This core only tiles a known total across a known ordered list.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "FoldPanelWidthCore.hpp"

namespace redDot { namespace fold {

// ── band model ──
// titleId: an opaque id the draw maps to a subpanel name ("SENDS", ...).
// minWidthPx: the band's floor (its content extent). flexible: the grow target — the
// band that absorbs the leftover width. Exactly one flexible band is the norm; 0 or >1
// are handled deterministically (see layoutBands).
struct BandSpec { int titleId = -1; float minWidthPx = 0.f; bool flexible = false; };
struct BandRect { int titleId = -1; float x0Px = 0.f; float x1Px = 0.f;
    float widthPx() const { return x1Px - x0Px; } };

// Header/footer strip geometry (were hardcoded in SandsFoldWidget::draw).
constexpr float kHeaderMm = 24.f;
constexpr float kFooterMm = 8.f;
inline float headerPx() { return mmToPx(kHeaderMm); }
inline float footerPx() { return mmToPx(kFooterMm); }

// Title fit: a conservative fixed per-character advance (fraction of the font px). Real
// glyph metrics are narrower, so "fits" here never lies the other way (no overlap).
constexpr float kCharAdvancePerFontPx = 0.62f;
constexpr float kTitleInsetPx = 3.f;   // left inset of a band title from its x0

// The flexible band's index in `specs`: the first flagged flexible, else the LAST band
// (0-flexible fallback: the last band absorbs the remainder so the tiling stays exact).
// >1 flexible: only the first absorbs; the rest take their min.
inline int flexibleIndex(const std::vector<BandSpec>& specs) {
    for (size_t i = 0; i < specs.size(); ++i) if (specs[i].flexible) return (int)i;
    return specs.empty() ? -1 : (int)specs.size() - 1;
}

// Lay the ordered specs across [0, totalPx]. Non-flexible bands take their minWidthPx;
// the flexible band takes the remainder, clamped to its own min (never negative).
// When totalPx >= sum of mins the rects tile [0, totalPx] EXACTLY (first x0==0, last
// x1==totalPx, every x1==next.x0). When totalPx < sum of mins (should not happen in the
// derived-width model, where total >= content >= sum of mins) the flexible band clamps
// to its min and the tiling overflows past totalPx — defined, never negative.
inline std::vector<BandRect> layoutBands(float totalPx, const std::vector<BandSpec>& specs) {
    std::vector<BandRect> out;
    if (specs.empty()) return out;
    const float total = (std::isfinite(totalPx) && totalPx > 0.f) ? totalPx : 0.f;
    const int flex = flexibleIndex(specs);

    float sumOtherMins = 0.f;
    for (int i = 0; i < (int)specs.size(); ++i)
        if (i != flex) sumOtherMins += std::max(0.f, specs[i].minWidthPx);

    const float flexMin   = (flex >= 0) ? std::max(0.f, specs[flex].minWidthPx) : 0.f;
    const float flexWidth = std::max(flexMin, total - sumOtherMins);

    float x = 0.f;
    out.reserve(specs.size());
    for (int i = 0; i < (int)specs.size(); ++i) {
        const float w = (i == flex) ? flexWidth : std::max(0.f, specs[i].minWidthPx);
        BandRect r; r.titleId = specs[i].titleId; r.x0Px = x; r.x1Px = x + w;
        out.push_back(r);
        x += w;
    }
    return out;
}

// The interior divider positions (between adjacent bands), PIXEL-SNAPPED to integers.
// Returns one x per interior boundary (size = bands-1). Half-pixel dividers are the
// seam, so these are rounded; with grid-aligned widths they are already integer.
inline std::vector<int> dividerX(const std::vector<BandRect>& rects) {
    std::vector<int> xs;
    if (rects.size() < 2) return xs;
    xs.reserve(rects.size() - 1);
    for (size_t i = 0; i + 1 < rects.size(); ++i)
        xs.push_back((int)std::lround(rects[i].x1Px));
    return xs;
}

// A band title's left anchor (draw station). Inside the band by construction.
inline float titleAnchorPx(const BandRect& r) { return r.x0Px + kTitleInsetPx; }

// Does a `charCount`-long title at `fontPx` fit inside the band (with the inset on both
// sides)? Conservative — a false here means "abbreviate/omit", decided here not by
// overdrawing the neighbour.
inline bool titleFits(const BandRect& r, int charCount, float fontPx) {
    if (charCount <= 0) return true;
    const float need = (float)charCount * kCharAdvancePerFontPx * fontPx;
    return need <= r.widthPx() - 2.f * kTitleInsetPx;
}

}} // namespace redDot::fold
