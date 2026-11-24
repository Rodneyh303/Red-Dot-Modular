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
    // RANGE EDGE (Rodney, Rack): 6 poly channels in, voices 7..16 read 0 V; with C muted they still flashed C green.
    // The candidate B of octave -1 (-1/12 V) won, then the [0,5] V clamp turned it into 0 V = degree C.
    {
        float m[12]; for (int i = 0; i < 12; ++i) m[i] = 1.f;
        m[0] = 0.f;                                   // C muted
        eng.rebuildScaleCache(m, 12);
        CHECK(degOf(eng.quantize(0.f)) != 0, "0 V with C muted must NOT quantise to C");
        CHECK(degOf(eng.quantize(5.0f)) != 0 || eng.quantize(5.0f) < 5.f, "5 V with C muted must NOT quantise to C");
        for (float cv = 0.f; cv <= 5.f; cv += 0.013f)
            CHECK(degOf(eng.quantize(cv)) != 0, "no CV in [0,5] V may quantise to a muted C");
    }
    // WEIGHT IS USED (Rodney, Rack): C at ~0.005 with the others near 1.0 lit far too often.
    // ── BEHAVIOURAL CHANGE (QUANTISER_WEIGHTED_PARTITION, 2026-11-24; FEEL-TEST ITEM) ──
    // The quantiser is now a WEIGHTED PARTITION: each w>0 degree owns a slice of the axis
    // PROPORTIONAL to its fader, centered on the degree ("a tiny value claims a sliver" —
    // Rodney). So a faint C owns a ~0.0005-wide sliver around its own center: a real melody
    // lands on C ~0.05% of a sweep (the fix), and a CV sitting EXACTLY on C lands in that
    // sliver -> C. This SUPERSEDES the old radius-model behaviour, where a dense (1-semitone)
    // neighbour "covered" the faint degree's exact center and an exact C hit snapped away.
    // The old DENSE/SPARSE split is gone — a faint degree now owns its center sliver either
    // way (the sparse case below already expected C). The guarantee that matters — C's SHARE
    // over a sweep is ~its weight, not ~half — is pinned explicitly.
    {
        float m[12]; for (int i = 0; i < 12; ++i) m[i] = 1.f;
        m[0] = 0.005f;
        eng.rebuildScaleCache(m, 12);
        // exact C now lands in C's sliver -> C (the partition is consistent dense or sparse).
        for (int o = 0; o <= 4; ++o)
            CHECK(degOf(eng.quantize((float)o)) == 0, "exact C at weight 0.005 lands in C's own sliver");
        // the SHARE is what the fix is about: over a [0,5] sweep C sounds ~its weight (<<1%),
        // nowhere near the old ~half. (Distinct CVs to dodge the one-entry memo.)
        {
            int cHits = 0, total = 0;
            for (int k = 0; k < 20000; ++k) {
                float cv = 5.f * ((float)k + 0.137f) / 20000.f;
                if (degOf(eng.quantize(cv)) == 0) ++cHits;
                ++total;
            }
            CHECK(cHits * 100 < total, "faint C's share over a full sweep is < 1% (the partition fix)");
        }
        CHECK(degOf(eng.quantize(2.0f + 4.f / 12.f)) == 4, "a full-weight degree keeps its own exact hit (E)");
        float u[12]; for (int i = 0; i < 12; ++i) u[i] = 1.f;
        eng.rebuildScaleCache(u, 12);
        CHECK(degOf(eng.quantize(2.0f + 5.f / 12.f)) == 5, "uniform weights: exact hit stays (F)");
        CHECK(degOf(eng.quantize(2.0f + 5.2f / 12.f)) == 5, "uniform weights: nearest degree wins (5.2 -> F)");
        // a sparse scale: the faint C still owns its sliver (unchanged — matched the old fallback).
        float sp[12] = {}; sp[0] = 0.005f; sp[7] = 1.f;
        eng.rebuildScaleCache(sp, 12);
        CHECK(degOf(eng.quantize(2.0f)) == 0, "sparse scale: the faint C owns its exact-center sliver");
    }
    if (g_fail) { std::printf("test_quantize_cache_invalidate: %d FAILED\n", g_fail); return 1; }
    std::printf("test_quantize_cache_invalidate: OK\n");
    return 0;
}
