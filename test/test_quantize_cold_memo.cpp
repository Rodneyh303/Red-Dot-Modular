// test_quantize_cold_memo.cpp — regression for the quantize() COLD-PATH memo bug (found by CC
// during CC_PROMPT_POLY_TIE_EMERGENT, fixed 2026-11-22). In the empty-scale state
// (activeSemiCount == 0 — a fresh/reset engine before any scale is set) quantize() returned
// clamp(vIn) but did NOT update lastQuantOut, so the top-of-function memo (keyed on lastQuantIn,
// which WAS updated) served the stale reset value (0.0) to a REPEATED same input.
// FAILS before the fix (second call returns 0.0), PASSES after (returns the clamp).
#include <cstdio>
#include <cmath>
#include "engines/SequencerEngine.hpp"

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

int main() {
    std::printf("test_quantize_cold_memo\n");

    // Fresh engine, no scale set -> activeSemiCount == 0 (the cold path). lastQuantIn defaults
    // to -100 so the first non-(-100) input misses the memo and reaches the cold path.
    SequencerEngine eng; eng.reset();

    const float v = 2.5f;                       // inside [0,5], not the reset 0.0
    float first  = eng.quantize(v);
    float second = eng.quantize(v);             // same input -> hits the memo

    CHECK(std::fabs(first  - v) < 1e-6f, "cold path returns the clamped input");
    CHECK(std::fabs(second - v) < 1e-6f, "REPEATED cold input returns the clamp, not stale 0.0");
    CHECK(first == second, "memo is consistent across repeated cold inputs");

    // A second distinct value, repeated, must also be stable (not the previous memo nor 0.0).
    const float v2 = 3.75f;
    float a = eng.quantize(v2);
    float b = eng.quantize(v2);
    CHECK(std::fabs(a - v2) < 1e-6f && a == b, "second cold value stable and clamped");

    // Clamp still applies on the cold path (out-of-range input).
    float hi = eng.quantize(9.0f);
    CHECK(std::fabs(hi - 5.0f) < 1e-6f, "cold path clamps above 5V");
    CHECK(eng.quantize(9.0f) == hi, "clamped cold value also memo-stable");

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
