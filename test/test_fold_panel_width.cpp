// test_fold_panel_width — the seamless-fold resize spike's Rack-free core (docs/design/SEAMLESS_FOLD_PLAN.md step 1).
// Pins: HP clamp + snap, HP<->px, the min-content rule, the saved-width decode (missing / garbage / out of range /
// NaN never crash and always land on an allowed width), right-anchored child placement, and the neighbour shift.
// Build (run_all.sh): test_fold_panel_width|
#include <cstdio>
#include <cmath>
#include <limits>
#include "ui/FoldPanelWidthCore.hpp"
namespace fw = redDot::fold;
static int g_fail = 0;
// INDEPENDENT oracle for "allowed" (does not call the code under test): inside [lo, kMaxHp] and on the step grid from lo (kMaxHp always ok).
static bool allowedOracle(int hp, int minContentHp) {
    const int lo = std::min(std::max(minContentHp, fw::kMinHp), fw::kMaxHp);
    if (hp < lo || hp > fw::kMaxHp) return false;
    return hp == fw::kMaxHp || ((hp - lo) % fw::kStepHp) == 0;
}
#define CHECK(c, m) do { if (!(c)) { if (g_fail < 30) std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

int main() {
    // 1. clamp + snap: result is ALWAYS an allowed width (multiple of kStepHp, within [minHp, kMaxHp]).
    for (int minHp = fw::kMinHp; minHp <= fw::kMaxHp; ++minHp)
        for (int hp = -50; hp <= 300; ++hp) {
            const int r = fw::clampHp(hp, minHp);
            CHECK(r >= minHp && r <= fw::kMaxHp, "clamp stays in [minContent, kMaxHp]");
            CHECK(allowedOracle(r, minHp), "clamp result is an allowed width");
        }
    CHECK(fw::clampHp(fw::kMaxHp + 500, fw::kMinHp) == fw::kMaxHp, "above max clamps to max");
    CHECK(fw::clampHp(-3, fw::kMinHp) >= fw::kMinHp, "below min clamps up");
    // idempotent
    for (int hp = 0; hp <= 200; ++hp) {
        const int a = fw::clampHp(hp, 20);
        CHECK(fw::clampHp(a, 20) == a, "clamp is idempotent");
    }
    // 2. min-content rule: the content can never be squeezed below what it needs.
    CHECK(fw::clampHp(10, 40) >= 40, "min-content wins over a narrower request");
    CHECK(fw::clampHp(fw::kMaxHp, fw::kMaxHp + 20) == fw::kMaxHp, "min-content above the cap still yields the cap");
    // 3. HP <-> px round trip.
    for (int hp = fw::kMinHp; hp <= fw::kMaxHp; ++hp) {
        CHECK(std::fabs(fw::hpToPx(hp) - hp * fw::kGridPx) < 1e-4f, "hpToPx = hp * grid");
        CHECK(fw::pxToHp(fw::hpToPx(hp)) == hp, "px->hp round trip");
    }
    CHECK(fw::pxToHp(fw::hpToPx(31) + 3.f) == 31, "pxToHp rounds to the nearest HP");
    // 4. saved-width decode: never crash, always allowed.
    const int def = 40, minHp = 20;
    CHECK(fw::decodeSavedHp(false, 0.0, def, minHp) == fw::clampHp(def, minHp), "missing key -> default");
    CHECK(fw::decodeSavedHp(true, std::numeric_limits<double>::quiet_NaN(), def, minHp) == fw::clampHp(def, minHp), "NaN -> default");
    CHECK(fw::decodeSavedHp(true, std::numeric_limits<double>::infinity(), def, minHp) == fw::clampHp(def, minHp), "inf -> default");
    CHECK(fw::decodeSavedHp(true, -1e12, def, minHp) == fw::clampHp(minHp, minHp), "huge negative -> min");
    CHECK(fw::decodeSavedHp(true, 1e12, def, minHp) == fw::kMaxHp, "huge positive -> max");
    CHECK(fw::decodeSavedHp(true, 33.4, def, minHp) == fw::clampHp(33, minHp), "fractional rounds then clamps");
    for (double v : {-5.0, 0.0, 0.5, 19.0, 20.0, 21.0, 63.7, 1000.0}) {
        const int r = fw::decodeSavedHp(true, v, def, minHp);
        CHECK(allowedOracle(r, minHp), "decode always returns an allowed width");
    }
    // round trip: a saved allowed width reloads as itself.
    for (int hp = minHp; hp <= fw::kMaxHp; hp += fw::kStepHp)
        CHECK(fw::decodeSavedHp(true, (double)hp, def, minHp) == hp, "saved allowed width reloads exactly");
    // 5. right-anchored children keep their inset when the width changes.
    CHECK(std::fabs(fw::rightAnchoredX(fw::hpToPx(40), 30.f) - (fw::hpToPx(40) - 30.f)) < 1e-4f, "right anchor = width - inset");
    CHECK(fw::rightAnchoredX(fw::hpToPx(60), 30.f) - fw::rightAnchoredX(fw::hpToPx(40), 30.f) > 0.f, "anchor moves right as width grows");
    // 6. neighbour shift: modules to the right move by exactly the width delta.
    CHECK(std::fabs(fw::neighbourShiftPx(40, 52) - 12 * fw::kGridPx) < 1e-4f, "grow 12HP -> +12*grid");
    CHECK(std::fabs(fw::neighbourShiftPx(52, 40) + 12 * fw::kGridPx) < 1e-4f, "shrink 12HP -> -12*grid");
    CHECK(fw::neighbourShiftPx(40, 40) == 0.f, "no change -> no shift");
    // 7. step set is non-empty and ordered.
    CHECK(fw::kMinHp < fw::kMaxHp && fw::kStepHp >= 1, "sane constants");
    if (g_fail) { std::printf("test_fold_panel_width: %d FAIL\n", g_fail); return 1; }
    std::printf("test_fold_panel_width: PASS\n");
    return 0;
}
