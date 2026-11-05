// PHASE TIER — transport phase span (Stage 1) + subphase (Stage 2) pins.
//
// Design authority: docs/design/PHASE_TIER_SYSTEM_IDEA.md (decided). Build spec:
// docs/design/CC_PROMPT_PHASE_TIER.md. Implementation map: docs/design/PHASE_TIER_PLAN.md.
//
// STAGE 1 pins (this file's sections 1..6):
//   1. SPAN PROBES at 16 — pulsePos == floor(normPhase(v) * p16 * 16): the OLD hardcoded
//      formula, pinned from the engine's own outputs (bit-identical at the default).
//   2. SPAN PROBES at 32 — the same probes against p16 * 32 (the full-span mapping, the
//      D9 answer: option A).
//   3. THE 32-STEP SWEEP — a continuous one-1/16-per-sample drive at patternLength 32,
//      window 32/0: pulsePos == 6k exactly (binary-exact deltas), the landing sequence
//      covers ALL 32 steps, two consecutive sweeps digest EQUAL (period 32), the span
//      wrap reads CONTINUOUS FORWARD (shortest-path, no false jump).
//   4. THE 16-SPAN CYCLE — at patternLength 16, three consecutive spans all land the
//      MUSICAL cycle (stepIndex == k % 16 every step — the drift-check consistency
//      condition; digest per span equal).
//   5. THE BAR-2 JUMP LANDING — the steps-32 regression repair: a bar-2 jump at a
//      16-window lands on the musical step (cycle k -> cell k), NOT park-at-startStep,
//      and the landing satisfies the drift rule (no strobe).
//   6. THE PHASE-STEP RULE — phaseStepFor is in-window for EVERY (length, offset) window
//      and equals (startStep + absStep % length) % ring everywhere; at the default window
//      it equals absStep % 16 exactly (the old drift expression, bit-identical).
//
// Build (run_all.sh): test_phase_tier|$SE $GS $PE
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <set>
#include <algorithm>

#include "SequencerEngine.hpp"
#include "PhaseEngine.hpp"

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

using E = SequencerEngine;
using redDot::PhaseEngine;
namespace dm = dotModular;

static constexpr int P16 = 6;          // pulsesPer16th at 24 PPQN
static constexpr float ST = 1e-3f;     // sampleTime (bpm plumbing only; positions never read it)

// FNV-1a over a byte range — the house digest.
static uint64_t fnv1a(const std::vector<uint8_t>& bytes) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (uint8_t b : bytes) { h ^= b; h *= 0x100000001b3ULL; }
    return h;
}
static std::vector<uint8_t> toBytes(const std::vector<int>& v) {
    std::vector<uint8_t> out; out.reserve(v.size());
    for (int x : v) out.push_back((uint8_t)x);
    return out;
}

// Feed the phase ramp so each process() call advances EXACTLY one 1/16 (P16 pulses):
// phase = k/spanLen (k 1/16ths of the span) -> voltage k * 10 / spanLen. The per-sample
// delta is 10/spanLen — 0.3125 at span 32, 0.625 at 16, both binary-exact, so
// prevContPhase accumulates with zero rounding drift and llround lands on exact pulse
// multiples (6k). A P16-pulse move is NOT a jump (JUMP_PULSES is > P16) — continuous
// path throughout.
static void driveStep(PhaseEngine& ph, int k, int spanLen) {
    ph.process(k * 10.0f / (float)spanLen, true, ST, 24);
}

// A STATIC probe: reset first (a fresh havePrev=false seed), then two calls — the first
// seeds pulsePos = floor(phase*PPB) and returns, the second settles onto the continuous
// path's resting position llround(phase*PPB) (the seed floors, the continuous path
// llrounds — pre-existing rounding pair, at most one pulse apart; the resting value is
// the engine's stable position and the one the span formula pins).
static long probeResting(PhaseEngine& ph, float v, int patternLen) {
    ph.reset(); ph.patternLength = patternLen;
    ph.process(v, true, ST, 24);                        // the floor seed
    ph.process(v, true, ST, 24);                        // settle onto llround
    return ph.pulsePos;
}

int main() {
    PhaseEngine ph;

    // ── 1. SPAN PROBES at the default (16) — the old formula, from the engine ────────
    {
        const float probes[] = { 0.f, 0.5f, 1.0f, 3.335f, 5.f, 7.5f, 9.99f, 10.f, 11.25f };
        bool ok = true;
        for (float v : probes) {
            long got = probeResting(ph, v, 16);
            double n = v / 10.0; n -= std::floor(n);                    // normPhase
            long want = (long)std::llround(n * (double)(P16 * 16));     // the OLD span (p16*16), continuous-path rounding
            ok = ok && (got == want);
            if (got != want) std::printf("    v=%.3f pulsePos=%ld want=%ld\n", v, got, want);
        }
        check(ok, "SPAN@16: resting pulsePos == llround(normPhase(v) * p16 * 16) — the old hardcoded span");
    }

    // ── 2. SPAN PROBES at 32 — the full-span mapping ─────────────────────────────────
    {
        const float probes[] = { 0.f, 0.5f, 1.0f, 2.5f, 5.f, 7.5f, 9.99f, 10.f, 11.25f };
        bool ok = true;
        for (float v : probes) {
            long got = probeResting(ph, v, 32);
            double n = v / 10.0; n -= std::floor(n);
            long want = (long)std::llround(n * (double)(P16 * 32));
            ok = ok && (got == want);
            if (got != want) std::printf("    v=%.3f pulsePos=%ld want=%ld\n", v, got, want);
        }
        // The same PHYSICAL voltage lands twice as deep at 32 as at 16 — the span doubled.
        check(ok, "SPAN@32: resting pulsePos == llround(normPhase(v) * p16 * 32) — the full-span mapping");
    }

    // ── 3. THE 32-STEP SWEEP — covers all 32, period 32, wrap reads forward ──────────
    {
        ph.reset(); ph.patternLength = 32;
        E e; e.reset(); e.setWindow(32, 0);
        std::vector<int> sweep1, sweep2;
        bool pulsesExact = true, wrapForward = true;
        long prevPulse = -1;
        for (int k = 0; k < 64; ++k) {                       // two full sweeps
            driveStep(ph, k, 32);
            if (ph.pulsePos != (long)k * P16)
                std::printf("    k=%d pulsePos=%ld want=%ld\n", k, ph.pulsePos, (long)k * P16);
            pulsesExact = pulsesExact && (ph.pulsePos == (long)k * P16);
            wrapForward = wrapForward && (ph.pulsePos >= prevPulse);   // never walks back
            prevPulse = ph.pulsePos;
            e.snapToPhaseStep(ph.pulsePos, P16);
            if (k < 32) sweep1.push_back(e.stepIndex); else sweep2.push_back(e.stepIndex);
        }
        check(pulsesExact, "SWEEP@32: pulsePos == 6k exactly (binary-exact drive, no rounding drift)");
        check(wrapForward, "SWEEP@32: the span wrap reads CONTINUOUS FORWARD (pulsePos never walks back)");
        std::set<int> uniq(sweep1.begin(), sweep1.end());
        check((int)uniq.size() == 32, "SWEEP@32: the landing sequence covers ALL 32 steps");
        bool period = (sweep1 == sweep2);
        check(period, "SWEEP@32: two consecutive sweeps digest EQUAL (period 32)");
        check(fnv1a(toBytes(sweep1)) == fnv1a(toBytes(sweep2)), "SWEEP@32: FNV-1a digests equal across sweeps");
    }

    // ── 4. THE 16-SPAN CYCLE — every span lands the musical cycle ────────────────────
    {
        ph.reset(); ph.patternLength = 16;
        E e; e.reset(); e.setWindow(16, 0);
        std::vector<int> spanA, spanB, spanC;
        for (int k = 0; k < 48; ++k) {                       // three spans
            driveStep(ph, k, 16);
            e.snapToPhaseStep(ph.pulsePos, P16);
            if (k < 16) spanA.push_back(e.stepIndex); else if (k < 32) spanB.push_back(e.stepIndex); else spanC.push_back(e.stepIndex);
        }
        bool cyc = true;
        for (int k = 0; k < 48; ++k) {
            int landed = (k < 16) ? spanA[k] : (k < 32 ? spanB[k-16] : spanC[k-32]);
            cyc = cyc && (landed == k % 16);                 // the musical cycle every span
        }
        check(cyc, "CYCLE@16: three consecutive spans all land k %% 16 (drift-rule consistency, no strobe)");
        check(spanA == spanB && spanB == spanC, "CYCLE@16: span digests equal (period 16 at the cycle rule)");
    }

    // ── 5. THE BAR-2 JUMP LANDING — the steps-32 regression repair ───────────────────
    {
        // absStep 20 at a 16-window: the pre-Stage-1 snap reduced mod 32 (20), found it
        // out-of-window and PARKED at startStep 0; the drift check (mod 16) expected 4
        // and re-snapped every sample. The cycle rule lands the MUSICAL step.
        {
            E e; e.reset(); e.setWindow(16, 0);
            e.snapToPhaseStep((long)20 * P16, P16);
            check(e.stepIndex == 4, "BAR2-JUMP@16: absStep 20 lands the musical step 4 (was park-at-0 + strobe)");
            check(e.phaseStepFor(20) == e.stepIndex, "BAR2-JUMP@16: the landing satisfies the drift rule (no re-snap strobe)");
        }
        // The anchor pin coexists: the position-holders battery's probe (absStep 47 at
        // p16 24 -> tick 48) keeps its landing (47 %% 16 == 15 == 47 %% 32 here).
        {
            E e; e.reset(); e.setWindow(16, 0);
            e.snapToPhaseStep(47 * 24, 24);
            check(e.stepIndex == 15, "BAR2-JUMP@16: the position-holders anchor probe (absStep 47 -> cell 15) unchanged");
        }
        // At the 32-window the jump lands the FULL-SPAN address directly.
        {
            E e; e.reset(); e.setWindow(32, 0);
            e.snapToPhaseStep((long)20 * P16, P16);
            check(e.stepIndex == 20, "BAR2-JUMP@32: absStep 20 lands cell 20 (the full-span address)");
        }
        // Backward jumps (negative absStep) stay correct.
        {
            E e; e.reset(); e.setWindow(16, 0);
            e.snapToPhaseStep((long)-6 * P16, P16);          // absStep -6 -> cycle 10
            check(e.stepIndex == 10, "BAR2-JUMP@16: a negative absStep (-6) lands cycle 10");
        }
    }

    // ── 6. THE PHASE-STEP RULE — in-window everywhere; == old expression at default ──
    {
        E e; e.reset();
        bool okAll = true, okDefault = true;
        const int offs[] = { 0, 1, 5, 16, 31 };
        for (int len = 1; len <= 32; ++len) {
            for (int off : offs) {
                e.setWindow(len, off);
                for (long abs = -70; abs <= 70; ++abs) {
                    int s = e.phaseStepFor(abs);
                    int cycle = (int)(((abs % len) + len) % len);
                    int want = (off + cycle) & (E::kMaskRing - 1);
                    okAll = okAll && (s == want) && e.isStepInWindow(s);
                }
            }
        }
        e.setWindow(16, 0);
        for (long abs = -70; abs <= 70; ++abs)
            okDefault = okDefault && (e.phaseStepFor(abs) == (int)((((abs % 16) + 16) % 16)));
        check(okAll, "RULE: phaseStepFor == (startStep + absStep %% length) %% ring, ALWAYS in-window (1..32 x 5 offsets, absStep -70..70)");
        check(okDefault, "RULE: at the default window phaseStepFor == absStep %% 16 exactly (the old drift expression, bit-identical)");
    }

    std::printf("test_phase_tier: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
