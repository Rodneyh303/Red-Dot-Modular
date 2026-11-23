// test_fold_band_layout.cpp — CC_PROMPT_FOLD_SEAMLESS_DRAW §2. The band tiling core
// (FoldBandLayoutCore.hpp): exact tiling, integer dividers, mins honoured, titles that
// fit. INDEPENDENT oracle — the expected tiling is recomputed by brute force (accumulate
// mins, remainder to the flexible band), never by calling layoutBands. Compile: run_all.sh.
#include <cstdio>
#include <cmath>
#include <vector>
#include "ui/FoldBandLayoutCore.hpp"
#include "ui/FoldPanelWidthCore.hpp"

using namespace redDot::fold;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// Independent oracle: the flexible index = first flagged, else last.
static int oracleFlex(const std::vector<BandSpec>& s) {
    for (size_t i = 0; i < s.size(); ++i) if (s[i].flexible) return (int)i;
    return s.empty() ? -1 : (int)s.size() - 1;
}
// Independent oracle: the band widths, computed from scratch.
static std::vector<float> oracleWidths(float total, const std::vector<BandSpec>& s) {
    int flex = oracleFlex(s);
    float others = 0.f;
    for (int i = 0; i < (int)s.size(); ++i) if (i != flex) others += std::max(0.f, s[i].minWidthPx);
    float flexMin = (flex >= 0) ? std::max(0.f, s[flex].minWidthPx) : 0.f;
    float flexW = std::max(flexMin, (std::isfinite(total) && total > 0.f ? total : 0.f) - others);
    std::vector<float> w;
    for (int i = 0; i < (int)s.size(); ++i) w.push_back(i == flex ? flexW : std::max(0.f, s[i].minWidthPx));
    return w;
}

static void checkTiling(float total, const std::vector<BandSpec>& specs, const char* tag) {
    auto rects = layoutBands(total, specs);
    auto wo = oracleWidths(total, specs);
    CHECK(rects.size() == specs.size(), tag);
    // widths match oracle; x0==0; contiguous; titleIds preserved in order
    float x = 0.f;
    float sumMins = 0.f; for (auto& s : specs) sumMins += std::max(0.f, s.minWidthPx);
    for (size_t i = 0; i < rects.size(); ++i) {
        CHECK(std::fabs(rects[i].widthPx() - wo[i]) < 1e-3f, tag);
        CHECK(std::fabs(rects[i].x0Px - x) < 1e-3f, tag);          // contiguous, first x0==0
        CHECK(rects[i].widthPx() >= -1e-6f, tag);                  // never negative
        CHECK(rects[i].titleId == specs[i].titleId, tag);
        if (i + 1 < rects.size())
            CHECK(std::fabs(rects[i].x1Px - rects[i+1].x0Px) < 1e-6f, tag);  // x1==next.x0, no gap/overlap
        x = rects[i].x1Px;
    }
    // last x1 == max(total, sumMins): exact when total allows, overflow-to-mins otherwise
    float expectEnd = std::max((std::isfinite(total) && total > 0.f ? total : 0.f), sumMins);
    CHECK(std::fabs(rects.back().x1Px - expectEnd) < 1e-3f, tag);
}

int main() {
    std::printf("test_fold_band_layout\n");

    // Spec lists: 1 band; 2 flexible-left; 2 flexible-right; 3 bands.
    std::vector<std::vector<BandSpec>> specLists = {
        { {100, 90.f, true} },
        { {100, 120.f, true}, {101, 90.f, false} },               // flexible left
        { {100, 90.f, false}, {101, 120.f, true} },               // flexible right
        { {100, 60.f, false}, {101, 120.f, true}, {102, 60.f, false} },
    };

    // 1. For every allowed HP from each list's min up to the cap, exact tiling + monotone width.
    for (auto& specs : specLists) {
        float sumMins = 0.f; for (auto& s : specs) sumMins += s.minWidthPx;
        int minHp = contentMinHp(sumMins / mmToPx(1.f));   // sumMins back to mm then min HP
        if (minHp < kMinHp) minHp = kMinHp;
        float prevFlexW = -1.f; int prevHp = -1;
        for (int hp = minHp; hp <= kMaxHp; hp += kStepHp) {
            if (!isAllowedHp(hp, kMinHp)) continue;
            float total = hpToPx(hp);
            checkTiling(total, specs, "exact tiling across HP");
            // dividers are integer-valued
            auto rects = layoutBands(total, specs);
            auto divs = dividerX(rects);
            CHECK(divs.size() == (specs.size() > 1 ? specs.size() - 1 : 0), "divider count");
            for (size_t i = 0; i < divs.size(); ++i)
                CHECK(std::fabs((float)divs[i] - rects[i].x1Px) < 0.5f, "divider snaps to the boundary");
            // the flexible band width is monotone non-decreasing in HP
            int fi = oracleFlex(specs);
            if (fi >= 0) {
                float fw = rects[fi].widthPx();
                if (prevHp >= 0) CHECK(fw >= prevFlexW - 1e-3f, "flexible width monotone in HP");
                prevFlexW = fw; prevHp = hp;
            }
        }
    }

    // 2. Title fit: at the MINIMUM width a long title does not fit; a short one does;
    //    a fitting title never extends past its band.
    {
        std::vector<BandSpec> specs = { {100, 45.f, true} };       // narrow band (~3 HP)
        auto rects = layoutBands(hpToPx(kMinHp), specs);
        CHECK(titleFits(rects[0], 2, 10.f) == true,  "short title fits");
        CHECK(titleFits(rects[0], 40, 10.f) == false, "long title does not fit at min HP");
        // a fitting title's drawn extent stays inside the band
        if (titleFits(rects[0], 5, 10.f)) {
            float end = titleAnchorPx(rects[0]) + 5 * kCharAdvancePerFontPx * 10.f;
            CHECK(end <= rects[0].x1Px + 1e-3f, "fitting title stays in band");
        }
        CHECK(titleAnchorPx(rects[0]) >= rects[0].x0Px, "title anchor inside band");
    }

    // 3. Two titles never overlap at the minimum HP (each title clipped to its own band).
    {
        std::vector<BandSpec> specs = { {100, 90.f, false}, {101, 120.f, true} };
        float sumMins = 90.f + 120.f;
        auto rects = layoutBands(std::max(hpToPx(kMinHp), sumMins), specs);
        // the first title, if it fits, ends before the second band starts
        if (titleFits(rects[0], 6, 9.f)) {
            float end0 = titleAnchorPx(rects[0]) + 6 * kCharAdvancePerFontPx * 9.f;
            CHECK(end0 <= rects[1].x0Px + 1e-3f, "title0 ends before band1");
        }
    }

    // 4. Degenerate inputs: zero/negative total, no flexible, two flexible — no crash, defined.
    {
        std::vector<BandSpec> specs = { {100, 90.f, false}, {101, 60.f, false} };   // 0 flexible -> last absorbs
        checkTiling(0.f, specs, "zero total, 0 flexible");
        checkTiling(-50.f, specs, "negative total");
        auto r = layoutBands(400.f, specs);
        CHECK(r.back().widthPx() >= 60.f, "0-flexible: last band absorbs remainder");
        CHECK(std::fabs(r.back().x1Px - 400.f) < 1e-3f, "0-flexible still tiles to total");

        std::vector<BandSpec> two = { {100, 50.f, true}, {101, 50.f, true}, {102, 50.f, false} };
        checkTiling(300.f, two, "two flexible: first absorbs");
        auto r2 = layoutBands(300.f, two);
        CHECK(std::fabs(r2[1].widthPx() - 50.f) < 1e-3f, "second flexible stays at min");
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
