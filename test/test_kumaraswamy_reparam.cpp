// test_kumaraswamy_reparam.cpp — Kumaraswamy (mode, concentration) <-> (a,b) authoring map
// Compile: see test/run_all.sh (header-only, no companion sources).
//
// Sub-task A of CC_PROMPT_P1 (TODO_distribution_palette_and_editor.md #1): native (a,b) are a
// poor CONTROL basis — both knobs pull mode AND spread. The authoring basis is
// (mode, concentration) with conc := a (the LEFT shape exponent) and b solved CLOSED-FORM to
// hold the mode: exact inverse both directions, no root-finding. Internal evaluation and the
// .dmcurve schema STAY (a,b); this tests the MAPPING only (UI wiring pairs with the warp UI
// prompt). A parametric never faces the editor's eps-monotonicity gate — the requirement is
// just finite a,b>0 with a strictly increasing invCDF, which the clamps guarantee.
//
// Pinned clamps (verified LOAD-BEARING, CC_PROMPT_P1 — do not widen):
//   conc ∈ [1, CONC_MAX=5]   conc := a drives b ~ mode^(-a): conc=5 → b up to ~2.5e8 (invCDF
//                            still strictly increasing across a 1000-pt grid in double);
//                            conc=8 → b ~1e13 and the invCDF goes FLAT (ties = the numerical
//                            atom); conc=100 → b ~1e169, totally flat. 5 reaches peak height
//                            ~3.7 — musically ample; a needle spike IS a numerical atom.
//   mode ∈ [MODE_EPS=0.02, 0.98]   keeps m^a off the 0/denormal boundary (NaN guard).
//
// Sections:
//   1. Round-trip: (mode,conc) -> (a,b) -> (mode,conc) over the interior grid (~1e-9), and
//      (a,b) -> (mode,conc) -> (a,b) for in-image pairs (b spans ~1..1e8, so RELATIVE tol).
//   2. Mode correct: the analytic mode of the mapped (a,b) == the requested mode (the
//      orthogonality guarantee — the conc knob never moves the mode).
//   3. Finite + valid: over the FULL clamped box, a,b finite and > 0 and the invCDF strictly
//      increasing over a 1000-pt u grid (the check that caught CONC_MAX=100's flat curves).
//   4. Edge/degenerate: conc=1 -> uniform (1,1); modeConcFromAb(1,1) -> the (0.5, 1) sentinel;
//      authored a<=1 / b<=1 shapes -> documented boundary mode, no NaN/inf; out-of-box inputs
//      clamp into the box.
//
// jansson: resolved to the test/jansson.h shim via -Itest (the test/rack.hpp precedent).

#include <cmath>
#include <cstdio>

#include "Distribution.hpp"

using dotModular::KumaraswamyDistribution;

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static void checkNear(double got, double want, double tol, const char* what) {
    if (std::fabs(got - want) <= tol) { ++g_pass; return; }
    std::printf("  FAIL: %s (got %.12g, want %.12g, tol %g)\n", what, got, want, tol);
}

// The analytic Kumaraswamy mode for a>1, b>1 — the definition the mapping inverts.
static double analyticMode(double a, double b) {
    return std::pow((a - 1.0) / (a * b - 1.0), 1.0 / a);
}

// Strict increase over an n-pt uniform u grid. !(x > prev) also catches ties (the
// float-flat/numerical-atom case the CONC_MAX clamp exists to prevent) and NaN.
static bool invCdfStrictlyIncreasing(const KumaraswamyDistribution& d, int n = 1000) {
    double prev = d.invCDF(0.0);
    for (int i = 1; i <= n; ++i) {
        const double x = d.invCDF((double)i / (double)n);
        if (!(x > prev)) return false;
        prev = x;
    }
    return true;
}

// ── 1. round-trip ──────────────────────────────────────────────────────────────────────────────
static void testRoundTrip() {
    std::printf("  round-trip (mode,conc) <-> (a,b)\n");
    const double concs[] = {1.5, 2.0, 3.0, 4.5, 5.0};
    for (int i = 0; i <= 18; ++i) {   // mode 0.05 .. 0.95 step 0.05 (interior of the box;
        const double m = 0.05 + 0.05 * (double)i;   // the 0.02/0.98/1.0 boundaries are the
        for (double c : concs) {                    // clamped/sentinel cases, tested in §4)
            double a, b, m2, c2;
            KumaraswamyDistribution::abFromModeConc(m, c, a, b);
            KumaraswamyDistribution::modeConcFromAb(a, b, m2, c2);
            checkNear(m2, m, 1e-9, "round-trip mode");
            checkNear(c2, c, 1e-9, "round-trip conc");
            // Other direction: (a,b) -> (mode,conc) -> (a,b). These pairs lie in the forward
            // image so no clamp fires; b spans ~1..1e8, so compare RELATIVELY.
            double a2, b2;
            KumaraswamyDistribution::abFromModeConc(m2, c2, a2, b2);
            checkNear(a2, a, 1e-12, "round-trip a");
            checkNear(b2, b, 1e-9 * (b > 1.0 ? b : 1.0), "round-trip b");
        }
    }
}

// ── 2. mode correct (the orthogonality guarantee) ──────────────────────────────────────────────
static void testModeCorrect() {
    std::printf("  mode correct: analytic mode of mapped (a,b) == requested\n");
    const double concs[] = {1.5, 2.0, 3.5, 5.0};
    for (int i = 0; i <= 18; ++i) {
        const double m = 0.05 + 0.05 * (double)i;
        for (double c : concs) {
            double a, b;
            KumaraswamyDistribution::abFromModeConc(m, c, a, b);
            check(a > 1.0 && b > 1.0, "mapped (a,b) in the a>1,b>1 unimodal region");
            checkNear(analyticMode(a, b), m, 1e-9, "analytic mode == requested mode");
        }
    }
}

// ── 3. finite + valid over the FULL clamped box ────────────────────────────────────────────────
static void testFiniteValid() {
    std::printf("  finite + strictly increasing over the full clamped box\n");
    check(KumaraswamyDistribution::CONC_MAX == 5.0, "CONC_MAX pinned at 5 (load-bearing)");
    check(KumaraswamyDistribution::MODE_EPS == 0.02, "MODE_EPS pinned at 0.02");
    int cells = 0;
    for (int i = 0; i <= 48; ++i) {   // mode 0.02 .. 0.98 step 0.02 (edges included)
        const double m = 0.02 + 0.02 * (double)i;
        for (int j = 0; j <= 16; ++j) {   // conc 1.0 .. 5.0 step 0.25 (edges included)
            const double c = 1.0 + 0.25 * (double)j;
            double a, b;
            KumaraswamyDistribution::abFromModeConc(m, c, a, b);
            check(std::isfinite(a) && std::isfinite(b) && a > 0.0 && b > 0.0,
                  "mapped (a,b) finite and positive");
            const KumaraswamyDistribution d(a, b);
            check(invCdfStrictlyIncreasing(d), "invCDF strictly increasing (1000-pt grid)");
            ++cells;
        }
    }
    check(cells == 49 * 17, "box grid coverage (833 cells)");
}

// ── 4. edge / degenerate ───────────────────────────────────────────────────────────────────────
static void testEdge() {
    std::printf("  edge + degenerate\n");
    // conc == 1: uniform regardless of mode (mode is irrelevant — documented)
    double a, b;
    KumaraswamyDistribution::abFromModeConc(0.7, 1.0, a, b);
    check(a == 1.0 && b == 1.0, "conc=1 -> (a,b)=(1,1) uniform");
    KumaraswamyDistribution::abFromModeConc(0.02, 1.0, a, b);
    check(a == 1.0 && b == 1.0, "conc=1 uniform at the mode floor too");
    // the uniform's inverse: the mode formula is 0/0 -> the documented sentinel
    double m, c;
    KumaraswamyDistribution::modeConcFromAb(1.0, 1.0, m, c);
    check(m == 0.5 && c == 1.0, "modeConcFromAb(1,1) -> sentinel (mode 0.5, conc 1)");
    // authored monotone/J shapes (never produced by the forward map): boundary mode, no NaN
    KumaraswamyDistribution::modeConcFromAb(0.5, 3.0, m, c);   // density decreasing -> mass at 0
    check(std::isfinite(m) && std::isfinite(c) && m == 0.0 && c == 1.0,
          "a<=1 (decreasing) -> mode 0, conc clamped to 1");
    KumaraswamyDistribution::modeConcFromAb(3.0, 0.5, m, c);   // density increasing -> mass at 1
    check(std::isfinite(m) && std::isfinite(c) && m == 1.0 && c == 3.0,
          "b<=1 (increasing) -> mode 1, conc = a");
    KumaraswamyDistribution::modeConcFromAb(1.0, 2.0, m, c);   // a=1 ramp: decreasing -> 0
    check(std::isfinite(m) && std::isfinite(c) && m == 0.0 && c == 1.0,
          "a=1 ramp -> mode 0, conc 1");
    KumaraswamyDistribution::modeConcFromAb(0.5, 0.5, m, c);   // U-shape: heavier side, finite
    check(std::isfinite(m) && std::isfinite(c) && (m == 0.0 || m == 1.0),
          "U-shape input -> finite boundary mode");
    // out-of-box inputs clamp into the box (authoring safety)
    double aIn, bIn, aCl, bCl;
    KumaraswamyDistribution::abFromModeConc(1.5, 2.0, aIn, bIn);   // mode above the ceiling
    KumaraswamyDistribution::abFromModeConc(1.0 - KumaraswamyDistribution::MODE_EPS, 2.0, aCl, bCl);
    check(aIn == aCl && bIn == bCl, "mode>0.98 clamps to 1-MODE_EPS");
    KumaraswamyDistribution::abFromModeConc(0.5, 50.0, aIn, bIn);  // conc above the cap
    KumaraswamyDistribution::abFromModeConc(0.5, KumaraswamyDistribution::CONC_MAX, aCl, bCl);
    check(aIn == aCl && bIn == bCl, "conc>CONC_MAX clamps to 5");
    KumaraswamyDistribution::abFromModeConc(-1.0, 0.0, aIn, bIn);  // everything low
    check(aIn == 1.0 && bIn == 1.0, "mode<eps and conc<1 -> uniform");
    KumaraswamyDistribution::abFromModeConc(std::nan(""), std::nan(""), aIn, bIn);
    check(std::isfinite(aIn) && std::isfinite(bIn) && aIn > 0.0 && bIn > 0.0,
          "NaN inputs -> finite (clamped) output");
}

int main() {
    std::printf("== Kumaraswamy (mode, concentration) reparam ==\n");
    testRoundTrip();
    testModeCorrect();
    testFiniteValid();
    testEdge();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
