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
// STAGE 2 pins (sections 7..14 — the subphase tier, CC_PROMPT_PHASE_TIER.md Stage 2):
//   7. UNPATCHED = STAGE 1 — processTiers(..., false, ...) IS process() (the delegate:
//      identical pulsePos/edges/jump outputs across a sweep incl. a jump; the tier dirs
//      neutral, the tier state dropped so a later cable reseeds).
//   8. QUOTIENT/REMAINDER at regionSize = 1 BAR (patternLength 32): the seed lands
//      floor((floor(t/rF)·rF + s·rF) · PPB) — hand-computed probes incl. the wrapped
//      transport input (17 V reads as 0.7) and the exact-boundary coarse (t = 0.5 ->
//      region 1); the snap lands the bar-2 musical step (134 -> cell 22 = bar 2, 6.4).
//   9. QUOTIENT/REMAINDER at regionSize = 1 QUARTER: rF = 0.125, 8 regions across the
//      two bars (the doc's own numbers); probes land bar-2 quarter-1 mid (108 -> 18),
//      the last quarter's 3/4 (186 -> 31), quarter-1 sub 0.25 (30 -> 5).
//  10. THE TRANSPORT'S FINE MOTION IS QUANTISED AWAY — within a region, moving the
//      transport does NOT move the position (the coarse term frozen, dirT +1, dirS 0,
//      no edges, no jump).
//  11. THE SUBPHASE POSITIONS WITHIN THE REGION — a small continuous s move advances
//      the composed position continuously (no jump), dirS +1, dirT neutral.
//  12. THE SIGN-PRODUCT MATRIX — fwd·fwd = forward, fwd·rev = backward, rev·rev =
//      FORWARD (the both-reverse pin — the design doc's open check, adjudicated
//      INTENDED), flat sub = the transport's sign alone, flat transport = the
//      subphase's sign alone.
//  13. THE REGION LANDING — a transport crossing into the next region fires the EXISTING
//      jump contract (jumped, jumpSixteenths = regionSixteenths, forward), lands at the
//      same sub-position in the new region (pulse 134 -> cell 22), and the snap + drift
//      rule accept it (no strobe).
//  14. NO-CROSS + THE SUBPHASE WRAP — a full subphase sweep through its ramp wrap never
//      leaves the region's pulses ([0, 95] STRICT, the floor mapping); the wrap itself is
//      a backward jump that stays inside the region ("its jumps stay inside one bar").
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

    // ── 7. UNPATCHED = STAGE 1 — the delegate is the plain path ──────────────────────
    {
        PhaseEngine pa, pb;
        pa.patternLength = pb.patternLength = 32;
        bool ok = true;
        // A sweep with one continuous climb, one JUMP (a >1/16 scrub) and one reversal —
        // every output field must match sample for sample.
        for (int k = 0; k < 40; ++k) {
            float v = (k < 20) ? k * 10.0f / 32.f                      // continuous climb
                    : (k < 30) ? 19.0f                                  // the jump hold
                               : (40 - k) * 10.0f / 32.f;               // the reversal
            pa.process     (v,      true, ST, 24);
            pb.processTiers(v, 0.f, false, 16.0, ST, 24);
            ok = ok && pa.pulsePos == pb.pulsePos && pa.jumped == pb.jumped
                 && pa.jumpSixteenths == pb.jumpSixteenths && pa.reverse == pb.reverse
                 && pa.sixteenthEdge == pb.sixteenthEdge && pa.quarterEdge == pb.quarterEdge;
            ok = ok && pb.dirT == 0 && pb.dirS == 0;                   // neutral, always
        }
        check(ok, "UNPATCHED: processTiers(false) IS process() (pulsePos/edges/jump identical, dirs neutral)");
        // The tier state is dropped: a fresh cable reseeds from the CURRENT voltages.
        pb.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);
        check(pb.pulsePos == 38, "UNPATCHED->PATCHED: the reseed lands the composition of the current pair (38)");
    }

    // ── 8. QUOTIENT/REMAINDER at regionSize = 1 BAR (the spec's pin) ─────────────────
    {
        // patternLength 32, regionSixteenths 16 -> rF = 0.5, PPB = 192. The SEED call IS
        // the composition probe: pulsePos = floor((floor(t/rF)·rF + s·rF) · PPB).
        struct P { float tV, sV; long want; int step; const char* what; };
        const P probes[] = {
            {  3.0f,  4.0f,   38,  6, "t=0.3 s=0.4: region 0, sub 0.4 -> pulse 38, cell 6" },
            {  7.0f,  4.0f,  134, 22, "t=0.7 s=0.4: region 1, sub 0.4 -> pulse 134, cell 22 (bar 2)" },
            {  9.9f,  9.8f,  190, 31, "t=0.99 s=0.98: region 1, sub 0.98 -> pulse 190, cell 31" },
            { 17.0f,  4.0f,  134, 22, "t=1.7 (input wraps to 0.7) s=0.4 -> pulse 134, cell 22" },
            {  5.0f,  5.0f,  144, 24, "t=0.5 EXACT (coarse = region 1) s=0.5 -> pulse 144, cell 24" },
        };
        bool ok = true;
        for (const P& p : probes) {
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(p.tV, p.sV, true, 16.0, ST, 24);
            E e; e.reset(); e.setWindow(32, 0);
            e.snapToPhaseStep(ph.pulsePos, P16);
            ok = ok && (ph.pulsePos == p.want) && (e.stepIndex == p.step);
            if (ph.pulsePos != p.want || e.stepIndex != p.step)
                std::printf("    %s: pulsePos=%ld stepIndex=%d\n", p.what, ph.pulsePos, e.stepIndex);
        }
        check(ok, "Q/R@1bar: the seed lands the quotient/remainder decomposition exactly (5 probes + snap)");
    }

    // ── 9. QUOTIENT/REMAINDER at regionSize = 1 QUARTER (the spec's pin) ─────────────
    {
        // rF = 4/32 = 0.125 — 8 regions across the two bars (the design doc's numbers).
        struct P { float tV, sV; long want; int step; };
        const P probes[] = {
            { 5.0f, 5.0f,  108, 18 },   // t=0.5 (region 4 = bar 2 quarter 1) s=0.5 -> pulse 108, cell 18
            { 9.0f, 7.5f,  186, 31 },   // t=0.9 (region 7 = the last quarter) s=0.75 -> pulse 186, cell 31
            { 2.0f, 2.5f,   30,  5 },   // t=0.2 (region 1) s=0.25 -> pulse 30, cell 5
        };
        bool ok = true;
        for (const P& p : probes) {
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(p.tV, p.sV, true, 4.0, ST, 24);
            E e; e.reset(); e.setWindow(32, 0);
            e.snapToPhaseStep(ph.pulsePos, P16);
            ok = ok && (ph.pulsePos == p.want) && (e.stepIndex == p.step);
            if (ph.pulsePos != p.want || e.stepIndex != p.step)
                std::printf("    pulsePos=%ld want=%ld stepIndex=%d\n", ph.pulsePos, p.want, e.stepIndex);
        }
        // The 8 regions: sweeping the transport covers every coarse address 0..7.
        std::set<int> regions;
        for (int k = 1; k <= 32; ++k) {
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(k * 10.0f / 32.f, 0.f, true, 4.0, ST, 24);
            double t = (double)(k * 10.0f / 32.f) / 10.0; t -= std::floor(t);
            regions.insert((int)std::floor(t / 0.125));
        }
        bool eight = (regions.size() == 8);
        for (int r = 0; r < 8; ++r) eight = eight && (regions.count(r) == 1);
        check(ok && eight, "Q/R@1quarter: the probes land exactly and the sweep covers ALL 8 regions");
    }

    // ── 10. THE TRANSPORT'S FINE MOTION IS QUANTISED AWAY ───────────────────────────
    {
        ph.reset(); ph.patternLength = 32;
        ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);      // seed: region 0, sub 0.4 -> 38
        ph.processTiers(4.0f, 4.0f, true, 16.0, ST, 24);      // t +0.1, still region 0, s flat
        check(ph.pulsePos == 38 && !ph.jumped && !ph.pulseEdge && !ph.sixteenthEdge,
              "QUANTISED: the transport's within-region motion does NOT move the position (no edges, no jump)");
        check(ph.dirT == 1 && ph.dirS == 0,
              "QUANTISED: dirT reads the transport motion (+1), dirS neutral (flat)");
    }

    // ── 11. THE SUBPHASE POSITIONS WITHIN THE REGION (continuous) ────────────────────
    {
        ph.reset(); ph.patternLength = 32;
        ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);      // seed -> 38
        ph.processTiers(3.0f, 4.2f, true, 16.0, ST, 24);      // s +0.02, t flat
        check(ph.pulsePos == 40 && !ph.jumped,
              "SUBPOS: a small continuous subphase move advances the position (38 -> 40, no jump)");
        check(ph.dirS == 1 && ph.dirT == 0, "SUBPOS: dirS +1, dirT neutral");
    }

    // ── 12. THE SIGN-PRODUCT MATRIX (the semantic direction) ────────────────────────
    {
        // effReverse = (tDir · sDir) < 0 with neutral = +1 — the exact expression Monsoon
        // applies. Each scenario drives a REAL ramp pair and asserts a literal answer.
        auto effOf = [](const PhaseEngine& p) {
            int t = (p.dirT == 0) ? 1 : p.dirT;
            int s = (p.dirS == 0) ? 1 : p.dirS;
            return (t * s) < 0;
        };
        bool fwdFwd = true, fwdRev = true, bothRev = true, subFlat = true, tFlat = true;
        {   // forward · forward -> FORWARD
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);
            ph.processTiers(3.5f, 4.5f, true, 16.0, ST, 24);
            fwdFwd = (ph.dirT == 1 && ph.dirS == 1 && !effOf(ph));
        }
        {   // forward · reverse -> BACKWARD ("a backward step within the region")
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(3.0f, 4.5f, true, 16.0, ST, 24);
            ph.processTiers(3.5f, 4.0f, true, 16.0, ST, 24);
            fwdRev = (ph.dirT == 1 && ph.dirS == -1 && effOf(ph));
        }
        {   // BOTH reverse -> FORWARD — the design doc's open check, adjudicated INTENDED
            // (the same composition the voice/lane directions produce everywhere else).
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(3.5f, 4.5f, true, 16.0, ST, 24);
            ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);
            bothRev = (ph.dirT == -1 && ph.dirS == -1 && !effOf(ph));
        }
        {   // flat subphase: the transport's sign alone ("flat input carries no direction")
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(3.5f, 4.0f, true, 16.0, ST, 24);
            ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);   // t reverses, s flat
            subFlat = (ph.dirS == 0 && effOf(ph));             // backward, the transport's own
        }
        {   // flat transport: the subphase's sign alone
            ph.reset(); ph.patternLength = 32;
            ph.processTiers(3.0f, 4.5f, true, 16.0, ST, 24);
            ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);   // t flat, s reverses
            tFlat = (ph.dirT == 0 && effOf(ph));
        }
        check(fwdFwd, "SIGN-PRODUCT: forward x forward = forward");
        check(fwdRev, "SIGN-PRODUCT: forward x reverse = backward (the backward step within the region)");
        check(bothRev, "SIGN-PRODUCT: BOTH reverse = FORWARD (the open check, adjudicated intended)");
        check(subFlat, "SIGN-PRODUCT: a flat subphase carries no direction (the transport's sign governs)");
        check(tFlat,  "SIGN-PRODUCT: a flat transport carries no direction (the subphase's sign governs)");
    }

    // ── 13. THE REGION LANDING — the existing jump contract at a crossing ───────────
    {
        ph.reset(); ph.patternLength = 32;
        ph.processTiers(4.0f, 4.0f, true, 16.0, ST, 24);      // t=0.4 s=0.4: region 0 -> 38
        ph.processTiers(5.2f, 4.0f, true, 16.0, ST, 24);      // t crosses 0.5 -> region 1, s held
        check(ph.jumped && ph.jumpSixteenths == 16 && !ph.reverse,
              "REGION LANDING: the crossing fires the jump contract (forward, jumpSixteenths = 16 = regionSixteenths)");
        check(ph.pulsePos == 134,
              "REGION LANDING: lands at the SAME sub-position in the new region (pulse 134 = cell 22.4)");
        E e; e.reset(); e.setWindow(32, 0);
        e.snapToPhaseStep(ph.pulsePos, P16);
        check(e.stepIndex == 22 && e.phaseStepFor(ph.pulsePos / P16) == e.stepIndex,
              "REGION LANDING: the snap lands cell 22 and satisfies the drift rule (no strobe)");
    }

    // ── 14. NO-CROSS + THE SUBPHASE WRAP — inside the region, always ─────────────────
    {
        ph.reset(); ph.patternLength = 32;
        ph.processTiers(3.0f, 4.0f, true, 16.0, ST, 24);      // seed: region 0 -> 38
        bool inRegion = true, wrapJump = false, landedInRegion = false;
        for (int j = 1; j <= 35; ++j) {                        // s climbs 0.4 -> 1.10 (wraps at 1.0)
            ph.processTiers(3.0f, 4.0f + 2.0f * (float)j, true, 16.0, ST, 24);
            inRegion = inRegion && (ph.pulsePos >= 0) && (ph.pulsePos <= 95);   // STRICT (floor)
            if (ph.jumped && ph.jumpSixteenths < 0) {
                wrapJump = true;
                landedInRegion = (ph.pulsePos >= 0 && ph.pulsePos <= 95);
            }
        }
        check(inRegion, "NO-CROSS: a full subphase sweep (through its ramp wrap) never leaves the region's pulses [0,95]");
        check(wrapJump && landedInRegion,
              "NO-CROSS: the subphase ramp wrap is a BACKWARD jump that stays inside the region");
    }

    std::printf("test_phase_tier: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
