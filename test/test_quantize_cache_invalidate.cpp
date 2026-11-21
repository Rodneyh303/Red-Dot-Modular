// test_quantize_cache_invalidate — the quantiser's one-entry memo must not outlive the active-degree set.
// Regression (Rodney, Rack, 2026-11-21): with q-mix at 100% a fader whose weight was set to 0 could still
// light green (a quantised note landing on a muted degree). quantize() memoised (CV in -> degree out) and
// only the CV was the key, so a HELD cv (a chord/Scaler note) kept returning the old degree after the fader
// was zeroed — until the CV happened to change. rebuildScaleCache now drops the memo.
#include <cstdio>
#include <cmath>
#include "engines/SequencerEngine.hpp"
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)
static int degOf(float v) { return (int)std::lround((v - std::floor(v)) * 12.f) % 12; }
int main() {
    SequencerEngine eng; eng.reset(); eng.pe.tuning.resetToEqual12TET();
    float w[12] = {};
    w[0] = w[4] = w[7] = 1.f;
    eng.rebuildScaleCache(w, 12);
    CHECK(degOf(eng.quantize(2.5f)) == 7, "{0,4,7}: 2.5 V snaps to degree 7");
    w[7] = 0.f;                                   // the user pulls fader 7 to zero
    eng.rebuildScaleCache(w, 12);
    const int d = degOf(eng.quantize(2.5f));      // SAME held CV
    CHECK(d != 7, "after zeroing fader 7 the SAME held CV must not still quantise to degree 7");
    CHECK(d == 4 || d == 0, "it snaps to an ACTIVE degree (4 or 0)");
    w[7] = 1.f;                                   // and back
    eng.rebuildScaleCache(w, 12);
    CHECK(degOf(eng.quantize(2.5f)) == 7, "restoring fader 7 restores the snap with the CV still held");
    if (g_fail) { std::printf("test_quantize_cache_invalidate: %d FAILED\n", g_fail); return 1; }
    std::printf("test_quantize_cache_invalidate: OK\n");
    return 0;
}
