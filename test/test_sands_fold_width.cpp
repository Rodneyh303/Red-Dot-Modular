// test_sands_fold_width.cpp — the fold-resize spike's WIDGET-HALF tests
// (CC_PROMPT_FOLD_RESIZE_SPIKE.md): the core EXTENSIONS the widget needs — the
// mm→px factor + the min-content rule (the band's floor) — plus the saved-width
// round trip and the garbage set. INDEPENDENT ORACLES ONLY: never isAllowedHp
// (the doc's circularity note — the first draft's "allowed" check was circular,
// so the oracle here recomputes the SPEC: an allowed width is a multiple of 2
// inside [12, 120] from the prompt's own constants).
//
// Compile: see test/run_all.sh ("test_sands_fold_width").
#include <cstdio>
#include <cmath>
#include "ui/FoldPanelWidthCore.hpp"

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// The INDEPENDENT oracle — the prompt's spec, not the core's code:
// kMinHp=12, kMaxHp=120, kStepHp=2 (120 always allowed).
static bool specAllowed(int v) { return v >= 12 && v <= 120 && (v % 2 == 0 || v == 120); }

int main() {
    std::printf("test_sands_fold_width: the fold core extensions (mmToPx, contentMinHp) + the saved-width round trip — independent oracles\n");
    char msg[256];

    // ── 1. mmToPx — Rack's mm→px factor (96 dpi): 25.4mm == 96px, independently ──
    {
        const float got = redDot::fold::mmToPx(25.4f);
        std::snprintf(msg, sizeof msg, "mmToPx(25.4) = %f, want 96 (96 dpi)", (double)got);
        CHECK(std::fabs(got - 96.f) < 1e-3f, msg);
        std::snprintf(msg, sizeof msg, "mmToPx(1) = %f, want 96/25.4 = %f", (double)redDot::fold::mmToPx(1.f), 96.0 / 25.4);
        CHECK(std::fabs(redDot::fold::mmToPx(1.f) - 96.f / 25.4f) < 1e-4f, msg);
    }

    // ── 2. contentMinHp — the smallest width that FITS the content, independently:
    //    hp * 15px >= rightMm * (96/25.4) px, and taking 2HP less would NOT fit ──
    for (const float rightMm : { 10.f, 50.f, 100.f, 141.f, 300.f, 600.f }) {
        const int hp = redDot::fold::contentMinHp(rightMm);
        const float contentPx = rightMm * (96.f / 25.4f);
        std::snprintf(msg, sizeof msg, "contentMinHp(%fmm) = %d: %fpx < the content %fpx", (double)rightMm, hp, (double)(hp * 15.f), (double)contentPx);
        CHECK((float)hp * 15.f >= contentPx - 1e-3f || hp == 120, msg);   // 120 = the cap: oversized content clamps (the never-trusted law)
        std::snprintf(msg, sizeof msg, "contentMinHp(%fmm) = %d is not spec-allowed", (double)rightMm, hp);
        CHECK(specAllowed(hp), msg);
        std::snprintf(msg, sizeof msg, "contentMinHp(%fmm) = %d: two HP less (%d) would still fit — not minimal", (double)rightMm, hp, hp - 2);
        CHECK(((float)(hp - 2)) * 15.f < contentPx - 1e-3f || hp == 12, msg);
    }

    // ── 3. the round trip: a saved allowed width decodes to itself, every step ──
    for (int hp = 12; hp <= 120; hp += 2) {
        std::snprintf(msg, sizeof msg, "round trip %d HP", hp);
        CHECK(redDot::fold::decodeSavedHp(true, (double)hp, 40, 12) == hp, msg);
    }

    // ── 4. the garbage set (the patch-JSON reality: absent / NaN / inf / huge /
    //    off-grid / below-min / over-cap) — every result SPEC-allowed, never a crash ──
    {
        CHECK(redDot::fold::decodeSavedHp(false, 0.0, 40, 12) == 40, "absent key -> the default");
        CHECK(redDot::fold::decodeSavedHp(true, NAN, 40, 12) == 40, "NaN -> the default");
        CHECK(redDot::fold::decodeSavedHp(true, INFINITY, 40, 12) == 40, "inf -> the default");
        CHECK(redDot::fold::decodeSavedHp(true, -1e9, 40, 12) == 12, "huge negative -> the floor");
        CHECK(redDot::fold::decodeSavedHp(true, 1e9, 40, 12) == 120, "huge positive -> the cap");
        CHECK(redDot::fold::decodeSavedHp(true, 13.0, 40, 12) == 14, "off-grid 13 -> 14 (snap UP, content fits)");
        CHECK(redDot::fold::decodeSavedHp(true, 5.0, 40, 12) == 12, "below-min 5 -> 12");
        CHECK(redDot::fold::decodeSavedHp(true, 200.0, 40, 12) == 120, "over-cap 200 -> 120");
        const double garbage[] = { 0.0, -0.0, 3.7, 11.9, 13.0, 17.5, 121.0, 1e6, -1e6, 1e300, -1e300 };
        for (double g : garbage) {
            const int v = redDot::fold::decodeSavedHp(true, g, 40, 12);
            std::snprintf(msg, sizeof msg, "garbage %f -> %d, not spec-allowed", g, v);
            CHECK(specAllowed(v), msg);
        }
    }

    // ── 5. the min-content floor is honoured through decode: with a band floor of 30,
    //    no decode ever lands below it (the content can never be squeezed) ──
    {
        const double garbage[] = { 0.0, 5.0, 13.0, 20.0, NAN, -1e9 };
        for (double g : garbage) {
            const int v = redDot::fold::decodeSavedHp(true, g, 40, 30);
            std::snprintf(msg, sizeof msg, "decode(%f) with minContent 30 -> %d, below the band floor", g, v);
            CHECK(v >= 30, msg);
        }
        CHECK(redDot::fold::decodeSavedHp(false, 0.0, 40, 30) == 40, "absent with a floor -> the default (above the floor)");
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
