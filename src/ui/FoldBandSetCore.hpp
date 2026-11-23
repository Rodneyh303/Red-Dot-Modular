// FoldBandSetCore — Rack-free "the bands decide the width + the order" core for the
// seamless-fold module (docs/design/SEAMLESS_FOLD_PLAN.md, the 2026-11-22 rulings:
// WIDTH IS DERIVED FROM THE POPPED-OUT BANDS; subpanels re-orderable; JACK PANELS
// pinned to the EDGES; block/warn on removing a CABLED jack panel).
//
// This is the inverse of FoldBandLayoutCore (the seamless-draw core, which lays a
// fixed set of bands out WITHIN a given total width for drawing). Here the ENABLED
// BAND SET is the input and the module WIDTH + the left->right ORDER are the outputs.
// `layoutOrderedWidthPx` is exactly the `totalPx` FoldBandLayoutCore::layoutBands
// consumes, and `layoutOrder` is the order its specs should be given in.
//
// No rack:: includes — every number comes from FoldPanelWidthCore (kGridPx, kStepHp,
// clampHp) so the law is unit-tested headless (test_fold_band_set).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "FoldPanelWidthCore.hpp"

namespace redDot { namespace fold {

// A jack panel sits at an edge; a knob band is interior (re-orderable between the edges).
enum class BandEdge { Left = 0, Interior = 1, Right = 2 };

// One enabled band. `extentPx` is its own content width (the band's floor, as mm->px).
// `rank` orders bands WITHIN their group (left panels among themselves, interior among
// themselves, right among themselves); ties break by `id` so the order is total/stable.
// `isJackPanel` drives the removal guard (a cabled jack panel must not silently drop a
// cable). A jack panel is expected to carry edge Left or Right, never Interior.
struct FoldBand {
    int      id         = -1;
    float    extentPx   = 0.f;
    BandEdge edge       = BandEdge::Interior;
    int      rank       = 0;
    bool     isJackPanel = false;
};

// ── WIDTH ─────────────────────────────────────────────────────────────────────
// The module's total content width in px = the base panel + the sum of every enabled
// band's extent. ORDER-INDEPENDENT by construction (a sum). Negative/non-finite
// extents are treated as 0 (a band never shrinks the module).
inline float layoutOrderedWidthPx(float basePx, const std::vector<FoldBand>& bands) {
    float total = (std::isfinite(basePx) && basePx > 0.f) ? basePx : 0.f;
    for (const FoldBand& b : bands)
        if (std::isfinite(b.extentPx) && b.extentPx > 0.f) total += b.extentPx;
    return total;
}

// The module's width in HP: the smallest ALLOWED width (multiple of kStepHp in
// [kMinHp, kMaxHp]) whose px cover the content. Content wider than kMaxHp is capped
// at kMaxHp (the content then does not fit — a real constraint; the widget must refuse
// a pop-out that would exceed the cap, which is why this is exposed, below).
inline int widthHpForBands(float basePx, const std::vector<FoldBand>& bands) {
    const float px = layoutOrderedWidthPx(basePx, bands);
    int hp = (int)std::ceil(px / kGridPx);
    // Anchor the 2-HP step at kMinHp (the absolute rack grid), NOT at the content
    // floor: derived width must round UP to the next even HP >= content so every band
    // set lands on the same parity. clampHp(hp, hp) would make hp trivially "on grid".
    return clampHp(hp, kMinHp);   // floor kMinHp, snap up to the even grid, cap kMaxHp
}

// Would enabling one more band of this extent exceed the HP cap? (The widget asks
// before popping a band OUT.) True == the pop-out must be refused/warned.
inline bool bandWouldExceedCap(float basePx, const std::vector<FoldBand>& current, float addExtentPx) {
    float px = layoutOrderedWidthPx(basePx, current)
             + ((std::isfinite(addExtentPx) && addExtentPx > 0.f) ? addExtentPx : 0.f);
    return (int)std::ceil(px / kGridPx) > kMaxHp;
}

// ── ORDER ─────────────────────────────────────────────────────────────────────
// Left->right band order: every Left-edge (jack) band first, then the Interior
// (re-orderable knob) bands, then every Right-edge band; within each group by rank
// then id. Returns ids in final order. This is the ONE placement law — jack panels
// can never land between knob bands.
inline std::vector<int> layoutOrder(const std::vector<FoldBand>& bands) {
    std::vector<FoldBand> v = bands;
    std::stable_sort(v.begin(), v.end(), [](const FoldBand& a, const FoldBand& b) {
        if (a.edge != b.edge) return (int)a.edge < (int)b.edge;
        if (a.rank != b.rank) return a.rank < b.rank;
        return a.id < b.id;
    });
    std::vector<int> out;
    out.reserve(v.size());
    for (const FoldBand& b : v) out.push_back(b.id);
    return out;
}

// ── REMOVAL GUARD ───────────────────────────────────────────────────────────────
// Popping a band IN hides its port widgets; a port with no widget cannot hold a
// cable, so removing a CABLED jack panel would drop the cable. The core states the
// condition; the widget chooses block vs warn (the ruling allows either).
inline bool removalNeedsGuard(bool isJackPanel, bool hasCabledPort) {
    return isJackPanel && hasCabledPort;
}

}} // namespace redDot::fold
