// test_ring_view.cpp — CA5's lane-addressing sub-step (CC_PROMPT_REVERSE_COMPLETIONS_THEN_CA5
// Part 2): the RingView — the wrapping-window STORAGE read, operator[](i) =
// data[(O+i) & (ring-1)] with ring == the storage size. The doc's mandated cases
// (wrap, non-wrap, O+L==ring, L==1) + the two pins that make the routing safe:
// (a) the masked row read it replaces — `row[step & (kPatternLengthCap - 1)]` —
// equals RingView(row, 0, kStepStorage)[step] for EVERY step incl. negative
// (two's-complement & == the double-mod), the byte-identity the PatternEngine
// site rides; (b) the view's ring is the STORAGE size — the engine's
// `& (kMaskRing - 1)` 16-folds are domain reductions, NOT ring wraps, and are
// deliberately unrouted (masking 31 where they mask 15 would change behaviour).
//
// Compile: see test/run_all.sh ("test_ring_view").
#include <cstdio>
#include <array>
#include "engines/RingView.hpp"
#include "engines/PatternEngine.hpp"   // kStepStorage / kPatternLengthCap

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

int main() {
    std::printf("test_ring_view: the wrapping-window storage read — data[(O+i) & (ring-1)]\n");
    char msg[256];

    // ── 1. NON-WRAP: O=0, L=4 on the 8-ring ──
    {
        std::array<float, 8> a = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
        redDot::RingView<const float> v{a, 0, 4};
        CHECK(v.size() == 4, "non-wrap size");
        for (int i = 0; i < 4; ++i) {
            std::snprintf(msg, sizeof msg, "non-wrap [%d] = %f, want %d", i, v[(size_t)i], i);
            CHECK(v[(size_t)i] == (float)i, msg);
        }
    }

    // ── 2. WRAP: O=6, L=4 on the 8-ring — the window crosses the seam ──
    {
        std::array<float, 8> a = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
        redDot::RingView<const float> v{a, 6, 4};
        const float want[4] = {6.f, 7.f, 0.f, 1.f};
        for (int i = 0; i < 4; ++i) {
            std::snprintf(msg, sizeof msg, "wrap [%d] = %f, want %f", i, v[(size_t)i], want[i]);
            CHECK(v[(size_t)i] == want[i], msg);
        }
    }

    // ── 3. The doc's own example: O=30, L=4 on the 32-ring ──
    {
        std::array<int, 32> a = {};
        for (int i = 0; i < 32; ++i) a[(size_t)i] = i;
        redDot::RingView<const int> v{a, 30, 4};
        const int want[4] = {30, 31, 0, 1};
        for (int i = 0; i < 4; ++i) {
            std::snprintf(msg, sizeof msg, "doc example [%d] = %d, want %d", i, v[(size_t)i], want[i]);
            CHECK(v[(size_t)i] == want[i], msg);
        }
    }

    // ── 4. O+L == ring: the window ends exactly at the seam (no wrap) ──
    {
        std::array<float, 8> a = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
        redDot::RingView<const float> v{a, 4, 4};
        for (int i = 0; i < 4; ++i) {
            std::snprintf(msg, sizeof msg, "O+L==ring [%d] = %f, want %d", i, v[(size_t)i], 4 + i);
            CHECK(v[(size_t)i] == (float)(4 + i), msg);
        }
    }

    // ── 5. L==1: the single-cell window, at the seam and off it ──
    {
        std::array<float, 8> a = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
        redDot::RingView<const float> mid{a, 5, 1};
        redDot::RingView<const float> seam{a, 7, 1};
        CHECK(mid.size() == 1 && mid[0] == 5.f, "L==1 mid");
        CHECK(seam.size() == 1 && seam[0] == 7.f, "L==1 at the seam");
        redDot::RingView<const float> none{a, 3, 0};
        CHECK(none.empty(), "L==0 empty");
    }

    // ── 6. THE ROUTING PIN (byte-identity, the PatternEngine site's law): the
    //    masked row read `row[step & (kPatternLengthCap - 1)]` equals
    //    RingView(row, 0, kStepStorage)[step] for EVERY step — including
    //    negative and over-ring steps (two's-complement & == the double-mod) ──
    {
        std::array<float, kStepStorage> row = {};
        for (int i = 0; i < kStepStorage; ++i) row[(size_t)i] = (float)i;
        redDot::RingView<const float> v{row, 0, kStepStorage};
        CHECK(kStepStorage == kPatternLengthCap, "the cap-mask equals the storage ring (Stage 2b's flip — the routing's precondition, static_asserted in PatternEngine.hpp)");
        for (int step = -3; step <= kStepStorage + 3; ++step) {
            const float viaMask = row[(size_t)(step & (kPatternLengthCap - 1))];
            const float viaView = v[(size_t)step];
            std::snprintf(msg, sizeof msg, "routing pin at step %d: mask %f vs view %f", step, viaMask, viaView);
            CHECK(viaMask == viaView, msg);
        }
    }

    // ── 7. The MUTABLE view: writes land in the storage ──
    {
        std::array<float, 8> a = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
        redDot::RingView<float> v{a, 6, 3};
        v[0] = 60.f; v[1] = 61.f; v[2] = 62.f;
        CHECK(a[6] == 60.f && a[7] == 61.f && a[0] == 62.f, "mutable writes wrap into the storage");
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
