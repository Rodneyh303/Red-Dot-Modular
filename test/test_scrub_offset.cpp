// test_scrub_offset.cpp — the pure +/-32 scrub math (ScrubOffsetCore.hpp): floor-safe
// negative split, exact k/32 representability, snap, bridge-key indices. INDEPENDENT
// oracles. Compile: see run_all.sh.
#include <cstdio>
#include <cmath>
#include <cstdint>
#include "dsp/ScrubOffsetCore.hpp"

using namespace redDot::scrub;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

int main() {
    std::printf("test_scrub_offset\n");

    // 1. FLOOR split is negative-safe: frac always in [0,1), f = floor(s).
    //    The spec example: s = -3.25 -> f = -4, frac = 0.75.
    {
        Split a = splitScrub(-3.25f);
        CHECK(a.f == -4 && std::fabs(a.frac - 0.75f) < 1e-6f, "negative split floors (-3.25 -> -4,0.75)");
        Split b = splitScrub(3.25f);
        CHECK(b.f == 3 && std::fabs(b.frac - 0.25f) < 1e-6f, "positive split (3.25 -> 3,0.25)");
        // sweep: f == floor(s), frac == s - floor(s) in [0,1), reconstruct s
        for (float s = -32.f; s <= 32.f; s += 0.125f) {
            Split sp = splitScrub(s);
            int wantF = (int)std::floor(s);
            CHECK(sp.f == wantF, "f == floor(s)");
            CHECK(sp.frac >= 0.f && sp.frac < 1.f, "frac in [0,1)");
            CHECK(std::fabs(((float)sp.f + sp.frac) - s) < 1e-4f, "f+frac reconstructs s");
        }
        // exact integers give frac == 0 (not 0.9999 or negative)
        for (int k = -32; k <= 32; ++k) {
            Split sp = splitScrub((float)k);
            CHECK(sp.f == k && sp.frac == 0.f, "integer s -> frac exactly 0");
        }
        // clamp + non-finite
        CHECK(splitScrub(99.f).f == 32, "clamp high");
        CHECK(splitScrub(-99.f).f == -32, "clamp low");
        CHECK(splitScrub(std::nanf("")).f == 0 && splitScrub(INFINITY).f == 0, "non-finite -> neutral");
    }

    // 2. EXACT k/32 representability and round trip.
    for (int k = -32; k <= 32; ++k) {
        float p = positionToParam(k);
        CHECK(std::fabs(p - (float)k / 32.f) < 1e-9f, "position -> k/32 exact");
        CHECK(paramToPositionInt(p) == k, "param -> position round trips");
        // exactness: k/32 * 32 is exactly k (power-of-two denominator)
        CHECK((int)std::lround(p * 32.f) == k, "k/32 * 32 == k exactly");
    }
    CHECK(positionToParam(0) == 0.f, "neutral is zero");
    CHECK(paramToPositionInt(99.f) == 32 && paramToPositionInt(-99.f) == -32, "param clamps");

    // 3. SNAP: nearest integer position, exact k/32, idempotent, ties away from zero.
    for (int k = -32; k <= 31; ++k) {
        float base = (float)k / 32.f;
        float justAbove = base + 0.3f / 32.f;    // within the k cell, closer to k
        CHECK(std::fabs(snapToIntegerPosition(justAbove) - base) < 1e-9f, "snaps to nearest k");
        CHECK(snapToIntegerPosition(snapToIntegerPosition(justAbove)) == snapToIntegerPosition(justAbove), "snap idempotent");
    }
    // the snapped output is always exactly some k/32
    for (float v = -1.f; v <= 1.f; v += 0.017f) {
        float s = snapToIntegerPosition(v);
        int k = (int)std::lround(s * 32.f);
        CHECK(std::fabs(s - (float)k / 32.f) < 1e-9f, "snap output exact k/32");
        CHECK(k >= -32 && k <= 32, "snap output in range");
    }

    // 4. BRIDGE-KEY indices: hi and lo differ by exactly 1 in BOTH directions; f<0 reads ahead.
    for (int64_t N : {0LL, 5LL, 100LL, 1000LL}) {
        for (int f = -32; f <= 32; ++f) {
            int64_t hi = drawIndexHi(N, f), lo = drawIndexLo(N, f);
            CHECK(hi - lo == 1, "hi - lo == 1 (bridge key consistent)");
            CHECK(hi == N - f, "hi == N - f");
            if (f < 0) CHECK(hi == N + (-f), "negative f reads AHEAD (N + |f|)");
            if (f > 0) CHECK(hi == N - f && hi < N, "positive f reads BEHIND");
            if (f == 0) CHECK(hi == N, "neutral reads the current draw");
        }
    }

    // 5. mix CV -> position clamps to the span.
    CHECK(std::fabs(scrubPositionFromMix(1.f) - 32.f) < 1e-6f, "full +CV = +32");
    CHECK(std::fabs(scrubPositionFromMix(-1.f) + 32.f) < 1e-6f, "full -CV = -32");
    CHECK(std::fabs(scrubPositionFromMix(0.f)) < 1e-6f, "zero CV = neutral");
    CHECK(std::fabs(scrubPositionFromMix(5.f) - 32.f) < 1e-6f, "over-range CV clamps");

    // 6. cache half-span covers the span + the copula window.
    CHECK(cacheHalfSpan(16) == 64 + 32 + 16, "half span = K + SCRUB_K + margin");
    CHECK(cacheHalfSpan() >= SCRUB_K + COPULA_K, "half span covers offset + window");

    // 7. THE CACHE BUDGET: moving-average depth trades against jog range (Rodney 2026-11-23).
    //    The allocation is fixed; K + scrub + margin <= kCacheHalfSpan.
    {
        CHECK(kCacheHalfSpan == 112, "the fixed cache allocation is 112 entries each side");
        // Rodney's worked table (independent arithmetic: 112 - K - 16)
        CHECK(maxScrubForK(64) == 32, "K=64 -> jog +/-32 (today's default)");
        CHECK(maxScrubForK(32) == 64, "K=32 -> jog +/-64 (halving the average doubles the shuttle)");
        CHECK(maxScrubForK(16) == 80, "K=16 -> jog +/-80");
        // the invariant holds exactly wherever the range is positive
        for (int K = 0; K <= 96; ++K) {
            int sc = maxScrubForK(K);
            CHECK(sc >= 0, "jog range never negative");
            if (sc > 0) CHECK(K + sc + 16 == kCacheHalfSpan, "K + scrub + margin == allocation");
            CHECK(budgetFits(K, sc), "the advertised pair fits the budget");
            if (sc > 0) CHECK(!budgetFits(K, sc + 1), "one more than advertised does NOT fit");
        }
        // inverse consistency
        for (int K : {16, 32, 48, 64}) CHECK(maxKForScrub(maxScrubForK(K)) == K, "maxK inverts maxScrub");
        // monotone: a longer average always buys a shorter reach
        for (int K = 0; K < 96; ++K) CHECK(maxScrubForK(K) >= maxScrubForK(K + 1), "range monotone in K");
        // changing K re-clamps a live offset
        CHECK(clampOffsetForK(50, 32) == 50, "offset 50 is legal at K=32 (limit 64)");
        CHECK(clampOffsetForK(50, 64) == 32, "the same offset clamps to 32 when K rises to 64");
        CHECK(clampOffsetForK(-50, 64) == -32, "clamps symmetrically on the negative side");
        // guards
        CHECK(maxScrubForK(999) == 0,  "an absurd K leaves no jog range (clamped, not negative)");
        CHECK(maxScrubForK(-5) == maxScrubForK(0), "negative K treated as 0");
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
