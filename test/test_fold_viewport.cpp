// test_fold_viewport.cpp — the Straits group viewport (FoldViewportCore.hpp): show N
// subpanels, scroll across the rest. INDEPENDENT oracles — expected values recomputed
// from the spec, never from the function under test. Compile: see run_all.sh.
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "ui/FoldViewportCore.hpp"
#include "ui/FoldPanelWidthCore.hpp"

using namespace redDot::fold;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// Oracles (independent).
static int oMaxScroll(int total, int vis) {
    int t = std::max(0, total);
    int v = (t == 0) ? 1 : std::min(std::max(1, vis), t);
    return std::max(0, t - v);
}

int main() {
    std::printf("test_fold_viewport\n");

    // Sweep total panels and requested visible counts, cross-check every accessor.
    for (int total = 0; total <= 12; ++total) {
        for (int vis : {1, 2, 4, 6, 8}) {
            int ms = maxScroll(total, vis);
            CHECK(ms == oMaxScroll(total, vis), "maxScroll matches oracle");
            CHECK(ms >= 0, "maxScroll non-negative");

            // clampScroll lands in [0, maxScroll] for any raw offset incl. out of range.
            for (int raw : {-5, 0, 1, 3, 7, 100}) {
                int c = clampScroll(raw, total, vis);
                CHECK(c >= 0 && c <= ms, "clampScroll in range");
                if (raw >= 0 && raw <= ms) CHECK(c == raw, "clampScroll identity in range");
            }

            // visibleRange: window inside [0,total), count = min(visible',remaining),
            // and it exactly covers the tail when scrolled to the end.
            for (int scroll = -2; scroll <= total + 2; ++scroll) {
                VisibleRange r = visibleRange(scroll, vis, total);
                if (total == 0) { CHECK(r.first == 0 && r.count == 0, "empty viewport"); continue; }
                int v = std::min(std::max(1, vis), total);
                int first = std::min(std::max(scroll, 0), std::max(0, total - v));
                int count = std::min(v, total - first);
                CHECK(r.first == first && r.count == count, "visibleRange matches oracle");
                CHECK(r.first >= 0 && r.first + r.count <= total, "window inside panel list");
                CHECK(r.count >= 1, "at least one panel visible when panels exist");
            }

            // Arrow enablement is consistent with the clamped position.
            CHECK(canScrollLeft(0, total, vis) == false, "no left scroll at home");
            CHECK(canScrollLeft(ms, total, vis) == (ms > 0), "left enabled only when scrolled");
            CHECK(canScrollRight(ms, total, vis) == false, "no right scroll at end");
            CHECK(canScrollRight(0, total, vis) == (ms > 0), "right enabled when more remain");
            // everything fits -> no scrolling either way
            if (total <= std::min(std::max(1,vis), std::max(total,1)) && total <= vis) {
                CHECK(ms == 0, "fits -> no scroll");
                CHECK(!canScrollLeft(0,total,vis) && !canScrollRight(0,total,vis), "fits -> arrows off");
            }
        }
    }

    // Width: viewport content = visible panels + gaps; HP snapped to the even grid.
    {
        const float panel = 165.f, gap = 2.f, base = 90.f;   // Straits lane-panel ~165 wide
        for (int vis : {1, 2, 4, 6}) {
            float content = (float)vis * panel + (float)(vis - 1) * gap;
            CHECK(std::fabs(viewportContentPx(vis, panel, gap) - content) < 1e-3f, "viewport content px");
            int hp = widthHpForViewport(base, vis, panel, gap);
            int want = (int)std::ceil((base + content) / 15.f);
            if (want < 12) want = 12;
            if (want <= 120 && (want - 12) % 2 != 0) want += 1;
            if (want > 120) want = 120;
            CHECK(hp == want, "viewport HP matches oracle");
            CHECK(hp >= 12 && hp <= 120 && (hp == 120 || (hp - 12) % 2 == 0), "viewport HP allowed");
            // monotone: more visible panels -> wider (or capped)
            if (vis > 1) CHECK(widthHpForViewport(base, vis, panel, gap) >= widthHpForViewport(base, 1, panel, gap), "wider for more panels");
        }
    }

    // Changing visible re-clamps scroll, keeping the left edge where possible.
    {
        int total = 10;
        // at the end with visible=2 (scroll 8), switch to visible=6 -> maxScroll 4 -> clamps to 4.
        CHECK(reclampScrollOnVisibleChange(8, 6, total) == 4, "shrinking window clamps to new end");
        // at scroll 2 with visible=2, switch to visible=4 -> maxScroll 6 -> stays 2 (left stable).
        CHECK(reclampScrollOnVisibleChange(2, 4, total) == 2, "left edge stable when it still fits");
        // everything now fits -> scroll 0
        CHECK(reclampScrollOnVisibleChange(5, 10, total) == 0, "all fit -> home");
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
