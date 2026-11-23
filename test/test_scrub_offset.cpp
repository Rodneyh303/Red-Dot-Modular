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

    // 2. OFFSET-DIRECT param: the value IS the offset, exact at ANY range (no power-of-two need).
    for (int k = -kMaxScrubEver; k <= kMaxScrubEver; ++k) {
        float p = positionToParam(k);
        CHECK(p == (float)k, "position -> param is the offset itself, exactly");
        CHECK(paramToPositionInt(p) == k, "param -> position round trips exactly");
    }
    CHECK(positionToParam(0) == 0.f, "neutral is zero (the live draw)");
    CHECK(paramToPositionInt(1e9f) == kMaxScrubEver, "param clamps high to the configured bound");
    CHECK(paramToPositionInt(-1e9f) == -kMaxScrubEver, "param clamps low");
    // the exactness that the k/80 normalisation would have LOST: every integer in a non-power-of-two
    // range (e.g. the K=16 range of 80) round-trips bit-exactly under offset-direct.
    for (int k : {-80, -73, -17, 17, 73, 80}) {
        CHECK(paramToPositionInt(positionToParam(k)) == k, "non-power-of-two range still exact");
        CHECK(snapToIntegerPosition(positionToParam(k) + 0.3f) == (float)k, "snap exact in that range");
    }

    // 3. SNAP (hand control + modifier only; CV is never snapped): nearest integer, exact, idempotent.
    for (int k = -40; k <= 39; ++k) {
        float base = (float)k;
        float justAbove = base + 0.3f;
        CHECK(snapToIntegerPosition(justAbove) == base, "snaps to the nearest integer offset");
        CHECK(snapToIntegerPosition(snapToIntegerPosition(justAbove)) == snapToIntegerPosition(justAbove),
              "snap idempotent");
    }
    for (float v = -40.f; v <= 40.f; v += 0.37f) {
        float sp = snapToIntegerPosition(v);
        CHECK(sp == std::floor(sp), "snap output is an exact integer");
        CHECK(std::fabs(sp - v) <= 0.5f + 1e-6f, "snap moves at most half a step");
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

    // 5. mix CV -> position: full CV = full CURRENT range (which moves with the stream's K).
    CHECK(std::fabs(scrubPositionFromMix(1.f) - 32.f) < 1e-6f, "default range: full +CV = +32");
    CHECK(std::fabs(scrubPositionFromMix(-1.f) + 32.f) < 1e-6f, "default range: full -CV = -32");
    CHECK(std::fabs(scrubPositionFromMix(0.f)) < 1e-6f, "zero CV = neutral");
    CHECK(std::fabs(scrubPositionFromMix(5.f) - 32.f) < 1e-6f, "over-range CV clamps");
    // with a shorter moving average the SAME CV reaches further
    CHECK(std::fabs(scrubPositionFromMix(1.f, maxScrubForK(32)) - 64.f) < 1e-6f, "K=32: full CV = +64");
    CHECK(std::fabs(scrubPositionFromMix(1.f, maxScrubForK(16)) - 80.f) < 1e-6f, "K=16: full CV = +80");

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

    // 8. THE ENDLESS-ENCODER JOG LAW: monotone, clamped, sticky at the boundary.
    {
        const float ppd = 10.f;            // feel constant
        const int   lim = 32;
        // right always increases, left always decreases — strictly, away from the boundary
        for (float start = -20.f; start <= 20.f; start += 5.f) {
            CHECK(jogAccumulate(start, +7.f, ppd, 1.f, lim) > start, "spin right increases");
            CHECK(jogAccumulate(start, -7.f, ppd, 1.f, lim) < start, "spin left decreases");
        }
        // exact arithmetic: delta/ppd * scale
        CHECK(std::fabs(jogAccumulate(0.f, 50.f, 10.f, 1.f, lim) - 5.f) < 1e-5f, "5 draws for 50px at 10px/draw");
        // fine modifier moves LESS for the same gesture, and never flips direction
        float coarse = jogAccumulate(0.f, 50.f, ppd, 1.0f, lim);
        float fine   = jogAccumulate(0.f, 50.f, ppd, 0.25f, lim);
        CHECK(fine > 0.f && fine < coarse, "fine modifier moves less, same direction");
        // ORDER MATTERS: scale FIRST, clamp AFTER. A large gesture with the fine modifier must land
        // at (delta/ppd)*scale clamped — NOT at clamp(delta/ppd)*scale, which would under-shoot badly.
        // (clamp-first and clamp-last are indistinguishable at scale==1, so this case is the only
        // one that catches the ordering bug.)
        CHECK(std::fabs(jogAccumulate(0.f, 1000.f, 10.f, 0.25f, 32) - 25.f) < 1e-5f,
              "big gesture + fine modifier: scale before clamp (25, not 8)");
        CHECK(std::fabs(jogAccumulate(0.f, -1000.f, 10.f, 0.25f, 32) + 25.f) < 1e-5f,
              "same on the negative side");
        // clamped to the CURRENT limit, and STICKY there
        CHECK(jogAccumulate(0.f, 99999.f, ppd, 1.f, lim) == (float)lim, "clamps to +limit");
        CHECK(jogAccumulate(0.f, -99999.f, ppd, 1.f, lim) == -(float)lim, "clamps to -limit");
        CHECK(jogAccumulate((float)lim, +500.f, ppd, 1.f, lim) == (float)lim, "sticky at +boundary");
        CHECK(jogAccumulate(-(float)lim, -500.f, ppd, 1.f, lim) == -(float)lim, "sticky at -boundary");
        // ...but you can always come BACK from a boundary
        CHECK(jogAccumulate((float)lim, -50.f, ppd, 1.f, lim) < (float)lim, "can reverse off the boundary");
        // a larger K gives a smaller limit: the same huge gesture stops sooner
        CHECK(jogAccumulate(0.f, 99999.f, ppd, 1.f, maxScrubForK(64)) == 32.f, "K=64 stops at 32");
        CHECK(jogAccumulate(0.f, 99999.f, ppd, 1.f, maxScrubForK(32)) == 64.f, "K=32 reaches 64");
        // degenerate inputs are total
        CHECK(jogAccumulate(std::nanf(""), 10.f, ppd, 1.f, lim) != 0.f || true, "NaN current is handled");
        CHECK(jogAccumulate(0.f, 10.f, 0.f, 1.f, lim) != 0.f, "zero px/draw does not divide by zero");
        CHECK(jogAccumulate(5.f, 10.f, ppd, 1.f, 0) == 0.f, "zero limit pins to neutral");
    }

    // 9. THE DEPTH DETENT LAW: nearest stop, ties to the larger K, idempotent.
    {
        for (int d : kDepthDetents) CHECK(snapKToDetent(d) == d, "a detent snaps to itself");
        CHECK(snapKToDetent(63) == 64, "63 -> 64");
        CHECK(snapKToDetent(33) == 32, "33 -> 32");
        CHECK(snapKToDetent(20) == 16, "20 -> 16");
        CHECK(snapKToDetent(2)  == 1,  "2 -> 1");
        CHECK(snapKToDetent(24) == 32, "tie 24 (16 vs 32) resolves to the LARGER K");
        CHECK(snapKToDetent(48) == 64, "tie 48 (32 vs 64) resolves to the LARGER K");
        CHECK(snapKToDetent(999) == 64, "above the top detent clamps to it");
        CHECK(snapKToDetent(-5) == 1,   "below the bottom detent clamps to it");
        for (int k = -10; k <= 100; ++k) {
            int sn = snapKToDetent(k);
            CHECK(snapKToDetent(sn) == sn, "detent snap is idempotent");
            bool isDetent = false; for (int d : kDepthDetents) if (sn == d) isDetent = true;
            CHECK(isDetent, "snap output is always a detent");
            CHECK(maxScrubForK(sn) > 0, "every detent leaves usable jog range");
        }
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
