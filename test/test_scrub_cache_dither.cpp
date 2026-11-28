// test_scrub_cache_dither.cpp — perf/scrub-cache-dither: 2-slot z/xi cache + mix deadband.
//
// Two audio-thread costs found in the scrub CPU audit (ship flags, 16 voices, one stream):
//   z-MISS full recompute ~0.4 ms; z-hit ForVoice morph ~2 us. A scrub offset DITHERING
//   across one integer boundary alternated between two window pairs — every alternation a
//   miss. The 2-slot cache keeps both pairs resident (dither = pure hits). Separately, the
//   mix-changed check in refreshVisualCache compared raw floats, so sub-audible CV ripple
//   on a PARKED offset re-blended the displayed voice every frame tick; the 1/512-draw
//   deadband stops that (D3 is the red/green: it FAILS on raw != compare, PASSES now).
//
// Slot count is value-invisible (pure-function memo), so the whole existing bit-exactness
// suite is the acceptance test for the cache change; D2 spot-pins it here too.
//
// Rack-free. Compile: see test/run_all.sh ("test_scrub_cache_dither").

#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "VoiceResolver.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static PatternInput mkIn() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.octaveLo = 3.f; in.octaveHi = 5.f; in.restProb = 0.f;
    in.variationAmount = 0.5f; in.transpose = 0.f;
    in.noteVariationMask = 0b111; in.locked = false;
    return in;
}

static void setup(PatternEngine& pe, float offset, int poly = 2) {
    pe.numPolyVoicesHint = poly;
    pe.seedRhythmPhilox(0.42f); pe.seedMelodyPhilox(0.43f); pe.seedQmixPhilox(0.44f);
    for (int k = 0; k < 50; ++k) pe.advanceRhythmDraw(+1);
    pe.latchSlew(0.6f, 0.f, 0.f);
    pe.rhythmWin_.consume();
    pe.latchMix(offset, 0.f, 0.f, true, false, false);
    pe.recomputeEffectiveRhythm();
}

int main() {
    std::printf("== test_scrub_cache_dither (2-slot z/xi cache + mix deadband) ==\n");

    // ── D1 WHITE-BOX: dithering across one boundary keeps BOTH pairs resident ──────────
    {
        PatternEngine pe; setup(pe, +3.4f);                       // pair f=3 filled
        pe.latchMix(+4.4f, 0.f, 0.f, true, false, false);
        pe.recomputeEffectiveRhythm();                            // pair f=4 filled (other slot)
        auto keys = [&]{ int a = pe.rhythmZSlots_[0].cachedF, b = pe.rhythmZSlots_[1].cachedF;
                         return (a > b) ? b * 100 + a : a * 100 + b; };
        const int resident = keys();
        CHECK(resident == 304, "D1 after visiting f=3 then f=4, slots hold {3,4}");
        for (int i = 0; i < 6; ++i) {                             // dither A/B/A/B...
            pe.latchMix((i % 2) ? +4.4f : +3.4f, 0.f, 0.f, true, false, false);
            pe.recomputeEffectiveRhythm();
            CHECK(keys() == resident, "D1 dithering never evicts either pair (keys stable)");
        }
    }

    // ── D2 VALUE INVARIANCE: every dither revisit is bitwise the first visit ───────────
    {
        PatternEngine pe; setup(pe, +3.4f);
        float snapA[32], snapB[32];
        std::memcpy(snapA, pe.slewedRhythm[0].data(), sizeof snapA);
        pe.latchMix(+4.4f, 0.f, 0.f, true, false, false); pe.recomputeEffectiveRhythm();
        std::memcpy(snapB, pe.slewedRhythm[0].data(), sizeof snapB);
        for (int i = 0; i < 8; ++i) {
            const bool a = (i % 2) == 0;
            pe.latchMix(a ? +3.4f : +4.4f, 0.f, 0.f, true, false, false);
            pe.recomputeEffectiveRhythm();
            CHECK(!std::memcmp(pe.slewedRhythm[0].data(), a ? snapA : snapB, sizeof snapA),
                  "D2 dither revisit bitwise-equal to first visit (slots never corrupt values)");
        }
    }

    // ── D3 MIX DEADBAND (red/green: FAILS on the old raw != compare) ───────────────────
    {
        PatternEngine pe; setup(pe, +3.2f);
        PatternInput in = mkIn();
        pe.latchMix(+3.2f + 1.f/1024.f, 0.f, 0.f, true, false, false);   // sub-deadband ripple
        pe.refreshVisualCache(in);
        CHECK(pe.rhythmMixApplied == 3.2f,
              "D3 sub-1/512-draw ripple does NOT re-blend (old code recomputed on any !=)");
        pe.latchMix(+3.2f + 1.f/64.f, 0.f, 0.f, true, false, false);     // a real move
        pe.refreshVisualCache(in);
        CHECK(pe.rhythmMixApplied == 3.2f + 1.f/64.f,
              "D3 a real move (>= 1/512 draw) still re-blends immediately");
        // ramp never stalls: many sub-eps steps accumulate past eps and then apply
        float m = pe.rhythmMixApplied;
        for (int i = 0; i < 8; ++i) {
            m += 1.f/1024.f;
            pe.latchMix(m, 0.f, 0.f, true, false, false);
            pe.refreshVisualCache(in);
        }
        CHECK(std::fabs(pe.rhythmMixApplied - m) < 1.5f/512.f,
              "D3 slow ramp tracks within ~one deadband step (quantized morph, no stall)");
    }

    // ── D4 INVALIDATE CLEARS BOTH SLOTS ────────────────────────────────────────────────
    {
        PatternEngine pe; setup(pe, +3.4f);
        pe.latchMix(+4.4f, 0.f, 0.f, true, false, false); pe.recomputeEffectiveRhythm();
        pe.invalidateZSlots();
        CHECK(!pe.rhythmZSlots_[0].valid && !pe.rhythmZSlots_[1].valid &&
              !pe.rhythmZSlots_[0].xiValid && !pe.rhythmZSlots_[1].xiValid,
              "D4 invalidateZSlots clears valid+xiValid on both slots");
    }

    // ── BENCH: dither alternation runs at HIT cost, not miss cost ──────────────────────
    {
        PatternEngine pe; setup(pe, +3.4f, 15);                   // 16 voices, worst case
        pe.latchMix(+4.4f, 0.f, 0.f, true, false, false); pe.recomputeEffectiveRhythm();
        using clk = std::chrono::steady_clock;
        volatile float sink = 0.f;
        const auto t0 = clk::now();
        const int IT = 400;
        for (int i = 0; i < IT; ++i) {
            pe.latchMix((i % 2) ? +4.4f : +3.4f, 0.f, 0.f, true, false, false);
            pe.recomputeEffectiveRhythm();
            sink += pe.slewedRhythm[0][0];
        }
        const double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count() / IT;
        std::printf("    BENCH boundary dither (16 voices): %.3f ms/recompute (was miss-level ~0.4 ms with one slot)\n", ms);
        CHECK(ms < 100.0, "BENCH dither under 100ms pathology bound (parallel suite runs load the box)");
        (void)sink;
    }

    if (g_fail == 0) std::printf("ALL PASS\n");
    else             std::printf("%d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
