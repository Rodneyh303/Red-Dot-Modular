// test_step_advance.cpp — coverage for StepAdvance::decide (StepAdvance.hpp), the gate-mode
// step-advance decision (Variation repurposed to p(advance)). Pure function, zero prior test
// inclusion (coverage-gap fill 2026-11-22). INDEPENDENT oracle recomputes the spec table.
#include <cstdio>
#include <cmath>
#include "dsp/StepAdvance.hpp"

using dotModular::StepAdvance;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// Independent oracle — the spec's concrete table (StepAdvance.hpp header), written fresh:
//   p <  0 : BACKWARD iff u < -p, else REPEAT
//   p >= 0 : FORWARD  iff u <  p, else REPEAT   (p==0 -> always REPEAT since u>=0)
static int oracle(float p, float u) {
    if (p < 0.f) return (u < -p) ? -1 : 0;
    return (u < p) ? 1 : 0;
}

int main() {
    std::printf("test_step_advance\n");

    // 1. Exhaustive sweep p x u against the oracle.
    for (float p = -1.0f; p <= 1.0f + 1e-6f; p += 1.0f / 32.0f) {
        for (float u = 0.0f; u < 1.0f; u += 1.0f / 64.0f) {
            int got = (int)StepAdvance::decide(p, u);
            CHECK(got == oracle(p, u), "decide matches the spec oracle");
            CHECK(got == -1 || got == 0 || got == 1, "decision is one of {-1,0,1}");
        }
    }

    // 2. Named identities from the header.
    for (float u = 0.0f; u < 1.0f; u += 1.0f / 128.0f) {
        CHECK(StepAdvance::decide(1.0f, u)  == StepAdvance::FORWARD,  "p=+1 always FORWARD");
        CHECK(StepAdvance::decide(-1.0f, u) == StepAdvance::BACKWARD, "p=-1 always BACKWARD");
        CHECK(StepAdvance::decide(0.0f, u)  == StepAdvance::REPEAT,   "p=0 always REPEAT (stutter)");
    }

    // 3. The boundary is strict (u == p -> REPEAT, not advance): u<p, not u<=p.
    CHECK(StepAdvance::decide(0.5f, 0.5f)  == StepAdvance::REPEAT,   "u==p -> REPEAT (forward side)");
    CHECK(StepAdvance::decide(0.5f, 0.4999f) == StepAdvance::FORWARD, "u just below p -> FORWARD");
    CHECK(StepAdvance::decide(-0.5f, 0.5f) == StepAdvance::REPEAT,   "u==-p -> REPEAT (backward side)");
    CHECK(StepAdvance::decide(-0.5f, 0.4999f) == StepAdvance::BACKWARD, "u just below -p -> BACKWARD");

    // 4. Sign symmetry: for the same |p|, forward at +p mirrors backward at -p (same u).
    for (float ap = 0.1f; ap <= 0.9f; ap += 0.1f) {
        for (float u = 0.0f; u < 1.0f; u += 0.05f) {
            int fwd = (int)StepAdvance::decide( ap, u);
            int bwd = (int)StepAdvance::decide(-ap, u);
            // forward decision magnitude equals backward decision magnitude, opposite sign
            CHECK(std::abs(fwd) == std::abs(bwd), "±p mirror: same advance magnitude");
            if (fwd == 1) CHECK(bwd == -1, "±p mirror: +p forward <-> -p backward");
            if (fwd == 0) CHECK(bwd == 0,  "±p mirror: both repeat together");
        }
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
