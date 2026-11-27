// test_phase_initial_sync.cpp — the STARTUP DOWNBEAT: phase drive gates beat 1.
//
// THE GOTCHA (reproduce-first): PhaseEngine's first processed sample after a reset
// is a delta-less SEED — contractually EDGE-LESS (the test_phase_tier oracle pins
// the edge stream to the reference per-sample implementation, and pulseChanged to
// real movement, so the downbeat canNOT be synthesized inside PhaseEngine; a first
// attempt there broke both pins and was reverted). So a phase Monsoon started with
// the ramp at phase 0 seats on step 0 SILENTLY (the drift snap seats the column
// without gating) and first GATES at step 1 — while a clock Monsoon enters and
// plays step 0 on its first edge (the netEntered latch). Two Monsoons promoted as
// sync-able miss each other by exactly beat 1.
//
// THE FIX lives at the TRANSPORT layer, copying GATE mode's held-at-start term
// (gate1High && !engine.started()): PhaseEngine::startupDownbeat(engineStarted) is
// a pure predicate — true when the seeded ramp sits on a step boundary and the
// transport is still parked — OR'd into the step-edge dispatch (Monsoon.cpp). It is
// one-shot by transport state (the enter flips started() == playedColumn()>=0), a
// mid-run repatch never fires it (already started; havePrev guards unpatched), and
// it is DIRECTIONLESS: the resolver's entry latch consumes the first motion without
// counting, so direction resolves at the next PPQN pulse — we cannot know it sooner.
//
// GATES
//  A1 the predicate fires at a phase-0 seed AND the seed stays edge-less (the
//     PhaseEngine contract this fix must not touch).
//  A2 engine already started -> false (mid-run repatch safe).
//  A3 no seed (reset only / disconnected) -> false.
//  A4 mid-step seed -> false (seated off the step grid: nothing to gate).
//  A5 direction resolves on the first REAL pulse (fwd stays fwd; a backward delta
//     flips reverse) — never at the seed.
//  B  the dispatch simulation (faithful Monsoon order: process -> step-edge lambda
//     -> execute -> drift snap): OLD dispatch (stepEdge alone) never gates step 0
//     (first executed column is 1) — the bug, pinned as documentation; NEW dispatch
//     (stepEdge || startupDownbeat) gates columns 0,1,2,... in LOCKSTEP with a
//     clock-driven engine at the same tempo, one-shot at the seed.
//
// Rack-free. Compile: see test/run_all.sh ("test_phase_initial_sync").

#include <cstdio>
#include <cstdint>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "engines/PhaseEngine.hpp"
#include "PhiloxRng.hpp"
using redDot::PhiloxRng;
using redDot::PhaseEngine;   // PULSES_PER_STEP is the global in dsp/NoteValues.hpp

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static float volt(double phase) { return (float)(phase * 10.0); }

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.6f;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

static void seedEngine(SequencerEngine& eng) {
    PhiloxRng rng; rng.seed64(20261127u);
    eng.reset();
    eng.numPolyVoices = 0;
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]=rng.nextFloat(); eng.pe.octaveRandom[i]=rng.nextFloat();
        eng.pe.qmixRandom[i]=rng.nextFloat();   eng.pe.rhythmRandom[i]=rng.nextFloat();
        eng.pe.accentRandom[i]=rng.nextFloat(); eng.pe.variationRandom[i]=rng.nextFloat();
        eng.pe.legatoRandom[i]=rng.nextFloat();
    }
    eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
}

// ── the dispatch simulation ────────────────────────────────────────────────────
// One sample of the Monsoon phase-drive loop, in the real order: phase.process ->
// step-edge dispatch -> rateManager Step tier -> executeModeA(+1) -> drift snap
// (mirrors Monsoon.cpp: absStep = pulsePos / PULSES_PER_STEP; snap when the
// phase-step rule disagrees with the played column). Records each executed column.
struct PhaseRig {
    SequencerEngine   eng;
    PhaseEngine       pe;
    redDot::RateManager rm;
    PatternInput      in = makeInput();
    std::vector<int>  cols;      // playedColumn at each stepped execute
    std::vector<int>  samples;   // the sample index each execute landed on
    int               sampleIdx = 0;

    PhaseRig() { seedEngine(eng); pe.reset(); }

    void sample(double phase, bool useDownbeat, bool connected = true) {
        pe.process(volt(phase), connected, 1.f/44100.f, 24);
        bool stepEdge = pe.stepEdge
                     || (useDownbeat && pe.startupDownbeat(eng.started()));
        rm.advance(redDot::Rate::PPQN, pe.pulseEdge);
        rm.advance(redDot::Rate::Step, stepEdge);
        if (rm.ticked(redDot::Rate::Step)) {
            StepResult r = eng.executeModeA(rm, 6.f, in, +1);
            if (r.stepped) { cols.push_back(eng.playedColumn()); samples.push_back(sampleIdx); }
        }
        if (pe.pulseChanged) {                           // the drift snap (Monsoon order: after execute)
            long absStep = pe.pulsePos / PULSES_PER_STEP;
            if (eng.phaseStepFor(absStep) != eng.playedColumn())
                eng.snapToPhaseStep(pe.pulsePos, PULSES_PER_STEP);
        }
        ++sampleIdx;
    }
};

int main() {
    const double PPB = (double)PULSES_PER_STEP * 16.0;   // 96 pulses per span
    const double dP  = 1.0 / PPB;                        // one pulse per sample
    const float  st  = 1.f / 44100.f;
    const int    ppqn = 24;

    // ---- A1 predicate fires at a phase-0 seed; the seed stays EDGE-LESS --------
    {
        PhaseEngine pe; pe.reset();
        pe.process(volt(0.0), true, st, ppqn);           // the seed sample
        CHECK(pe.pulsePos == 0,                 "A1: seated on pulse 0 (step 0)");
        CHECK(pe.startupDownbeat(false),        "A1: startupDownbeat fires (parked + seeded on the grid)");
        CHECK(!pe.stepEdge && !pe.pulseEdge && !pe.sixteenthEdge && !pe.quarterEdge,
                                                "A1: the seed sample stays edge-less (PhaseEngine contract untouched)");
        CHECK(!pe.pulseChanged,                 "A1: the seed is not a position CHANGE");
        CHECK(!pe.reverse,                      "A1: no direction asserted at the seed");
    }

    // ---- A2 already started -> never fires (mid-run repatch safe) ---------------
    {
        PhaseEngine pe; pe.reset();
        pe.process(volt(0.0), true, st, ppqn);
        CHECK(!pe.startupDownbeat(true), "A2: engine started -> no downbeat (repatch mid-run stays silent)");
    }

    // ---- A3 no seed -> never fires ----------------------------------------------
    {
        PhaseEngine pe; pe.reset();                      // havePrev false: nothing processed yet
        CHECK(!pe.startupDownbeat(false), "A3: no seeded ramp (reset only / unpatched) -> no downbeat");
        pe.process(volt(0.25), false, st, ppqn);         // disconnected sample: still no seed
        CHECK(!pe.startupDownbeat(false), "A3: disconnected input -> no downbeat");
    }

    // ---- A4 mid-step seed -> no downbeat ----------------------------------------
    {
        PhaseEngine pe; pe.reset();
        pe.process(volt(0.03), true, st, ppqn);          // pulse 2: inside step 0, off the grid
        CHECK(pe.pulsePos == 2,            "A4: seated mid-step at pulse 2");
        CHECK(!pe.startupDownbeat(false),  "A4: off the step grid -> nothing to gate at the origin");
    }

    // ---- A5 direction resolves on the first REAL pulse, never at the seed -------
    {
        PhaseEngine fwd; fwd.reset();
        fwd.process(volt(0.0), true, st, ppqn);
        fwd.process(volt(dP),  true, st, ppqn);
        CHECK(fwd.pulseEdge && !fwd.stepEdge, "A5: first real pulse is pulse 1 (no step edge)");
        CHECK(!fwd.reverse,                   "A5: forward delta -> forward");

        PhaseEngine rev; rev.reset();
        rev.process(volt(0.0),  true, st, ppqn);
        rev.process(volt(0.99), true, st, ppqn);         // shortest path: -0.01 -> backward
        CHECK(rev.reverse, "A5: backward delta after the seed -> reverse resolved on pulse 1");
    }

    // ---- B the dispatch simulation: old misses beat 1; new locks to clock -------
    {
        const int SAMPLES = 6 * 8;                       // 8 steps' worth of pulses

        // the clock reference: a step edge every PULSES_PER_STEP samples, first at
        // sample 0 (the first clock pulse of the run) — enters and plays step 0.
        SequencerEngine clk; seedEngine(clk);
        redDot::RateManager crm; PatternInput cin = makeInput();
        std::vector<int> clkCols, clkSamples;
        for (int s = 0; s < SAMPLES; ++s) {
            bool edge = (s % PULSES_PER_STEP) == 0;
            crm.advance(redDot::Rate::PPQN, true);
            crm.advance(redDot::Rate::Step, edge);
            if (crm.ticked(redDot::Rate::Step)) {
                StepResult r = clk.executeModeA(crm, 6.f, cin, +1);
                if (r.stepped) { clkCols.push_back(clk.playedColumn()); clkSamples.push_back(s); }
            }
        }
        CHECK(!clkCols.empty() && clkCols[0] == 0,
              "B: clock reference enters and plays step 0 on its first edge");

        // OLD dispatch (stepEdge alone): the bug, pinned. Step 0 is seated by the
        // drift snap but never gated; the first executed column is 1.
        PhaseRig oldRig;
        for (int s = 0; s < SAMPLES; ++s) oldRig.sample(s * dP, /*useDownbeat=*/false);
        CHECK(!oldRig.cols.empty() && oldRig.cols[0] == 1,
              "B(old): raw edge stream alone MISSES beat 1 (first gated column is 1) - the gotcha");
        for (int c : oldRig.cols) CHECK(c != 0, "B(old): step 0 is never gated in the whole run");

        // NEW dispatch (|| startupDownbeat): beat 1 gates at the seed, one-shot,
        // and the executed columns land in LOCKSTEP with the clock reference.
        PhaseRig newRig;
        for (int s = 0; s < SAMPLES; ++s) newRig.sample(s * dP, /*useDownbeat=*/true);
        CHECK(!newRig.cols.empty() && newRig.cols[0] == 0 && newRig.samples[0] == 0,
              "B(new): the downbeat gates step 0 on the seed sample");
        CHECK(newRig.cols == clkCols && newRig.samples == clkSamples,
              "B(new): phase and clock Monsoons start STEP-SYNCED (same columns, same samples)");
        int zeros = 0; for (size_t i = 0; i < newRig.cols.size(); ++i)
            if (newRig.samples[i] < PULSES_PER_STEP) ++zeros;
        CHECK(zeros == 1, "B(new): the downbeat is a ONE-SHOT (exactly one execute before pulse 6)");
    }

    if (g_fail == 0) std::printf("test_phase_initial_sync: ALL PASS\n");
    else             std::printf("test_phase_initial_sync: %d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
