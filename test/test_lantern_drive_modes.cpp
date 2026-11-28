// test_lantern_drive_modes.cpp — Lantern's record path under PHASE and GATE drive (incl. sub
// gates). The existing Lantern suites (test_lantern_bars / test_lantern_mask) drive Mode A (clock
// forward, and a reverse lap-arrival block); they never exercise phase drive or the gate-mode
// executors. Lantern's beginSample() is drive-mode-AGNOSTIC — it reads eng.playedColumn() +
// eng.lastStepResult each step edge, whatever executor produced them — so this pins that the
// write-path routing and the follow-disc display laws hold under every executor, including a
// ratchet/ghost sub-gate stream (several gate edges feeding the recorder).
//
// THE LAWS (identical to marchModeA in test_lantern_bars, re-asserted per drive mode):
//   • the write slot IS the absolute played-step split: writeBar = forStep/16, writeCol = forStep%16
//   • the follow-disc display: displayBar = playedColumn/16, displayCol = playedColumn%16
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "SequencerEngine.hpp"
#include "RateManager.hpp"
#include "VoiceResolver.hpp"
#include "ui/LanternRecordCore.hpp"
namespace dm = dotModular;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

static void author(SequencerEngine& eng, PatternInput& in) {
    eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
    in.restProb = 0.f;             // never rest — every step sounds, so every step is observable
    for (int d = 0; d < 12; ++d) in.semiWeights[d] = ((d % 3) == 0) ? 0.8f : 0.3f;
    eng.numPolyVoices = 2;
}

// After any step executor updates the engine, record and assert the two laws. Returns edges seen.
static int recordAndCheck(SequencerEngine& eng, LanternRecordCore& rec, PatternInput& in, const char* tag) {
    const LanternRecordCore::StepEdge e = rec.beginSample(eng);
    if (!e.valid) return 0;
    rec.recordMonsoonSource(eng, e);
    char msg[200];
    const int forStep = eng.lastStepResult.forStep;
    if (forStep >= 0 && forStep < dm::kMaskRing) {
        std::snprintf(msg, sizeof(msg), "%s: write slot is the abs split (bar=step/16,col=step%%16) at forStep=%d", tag, forStep);
        CHECK(e.writeBar == forStep / LanternRecordCore::BAR_STEPS &&
              e.writeCol == forStep % LanternRecordCore::BAR_STEPS, msg);
    }
    const int pc = eng.playedColumn();
    if (pc >= 0 && pc < dm::kMaskRing) {
        std::snprintf(msg, sizeof(msg), "%s: follow-disc display == playedColumn split at col=%d", tag, pc);
        CHECK(rec.displayBar == pc / LanternRecordCore::BAR_STEPS &&
              rec.displayCol == pc % LanternRecordCore::BAR_STEPS, msg);
    }
    return 1;
}

int main() {
    std::printf("== test_lantern_drive_modes ==\n");

    // gate 1 — PHASE drive (snapToPhaseStep): position comes from the phase grid, not a clock edge.
    {
        std::printf("gate 1: PHASE drive (snapToPhaseStep) — record path follows the phase position\n");
        SequencerEngine eng; eng.reset(); PatternInput in; author(eng, in);
        LanternRecordCore rec; redDot::RateManager rm;
        int edges = 0;
        for (int i = 0; i < 40; ++i) {   // > 32 so BOTH bars (the 2-bar 0..31 ring) are swept
            eng.snapToPhaseStep((long)i * 6, 6);
            rm.advance(redDot::Rate::Step, true);
            auto r = eng.executeModeA(rm, in.noteValue, in, +1);
            if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
            edges += recordAndCheck(eng, rec, in, "PHASE");
        }
        CHECK(edges >= 16, "gate 1: phase drive produced recordable step edges");
    }

    // gate 2 — GATE mode, main gate (Mode B): a main gate rise advances the playhead one step,
    // then executeModeB executes it. Lantern records each with the correct split.
    {
        std::printf("gate 2: GATE mode main gate (executeModeB) — record path follows gate-edge steps\n");
        SequencerEngine eng; eng.reset(); PatternInput in; author(eng, in);
        LanternRecordCore rec;
        int edges = 0;
        for (int i = 0; i < 20; ++i) {
            eng.advancePlayhead(+1);                                 // the main gate rise advances one step
            eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true, in.noteValue, in);
            if (eng.numPolyVoices > 0) eng.executePolyVoices(in);
            edges += recordAndCheck(eng, rec, in, "GATE-main");
        }
        CHECK(edges >= 16, "gate 2: gate-mode main gate produced recordable step edges");
    }

    // gate 3 — GATE mode with SUB GATES (ratchet + ghost): the main rise advances the step; the
    // ratchet/ghost sub-events fire WITHIN the step (no new column). THE PROPERTY: Lantern records
    // once per STEP, not once per sub-gate — a ratchet/ghost sub-event must NOT produce a spurious
    // Lantern cell. So the recordable-edge count equals the number of MAIN steps, not the (larger)
    // total sub-gate event count.
    {
        std::printf("gate 3: GATE mode + SUB GATES (ratchet/ghost) — one Lantern cell per STEP, not per sub-gate\n");
        SequencerEngine eng; eng.reset(); PatternInput in; author(eng, in);
        LanternRecordCore rec;
        int mainSteps = 0, subEvents = 0, edges = 0;
        // Per main step: the main rise (advance + record), then 2 ratchet rises + 1 ghost rise
        // inside the step (no advance — must NOT record a new cell).
        for (int step = 0; step < 20; ++step) {
            eng.advancePlayhead(+1);
            eng.executeModeBSubdivided(/*mainRise=*/true, /*mainHigh=*/true, /*ratchetRise=*/false,
                                       in.noteValue, in, /*ghostRise=*/false, /*ghostHigh=*/false);
            if (eng.numPolyVoices > 0) eng.executePolyVoices(in);
            ++mainSteps;
            edges += recordAndCheck(eng, rec, in, "GATE-sub main");
            // sub-gate events inside the SAME step — no advancePlayhead:
            for (int r = 0; r < 2; ++r) {
                eng.executeModeBSubdivided(false, true, /*ratchetRise=*/true, in.noteValue, in, false, false);
                ++subEvents;
                CHECK(!rec.beginSample(eng).valid, "gate 3: a ratchet sub-gate does NOT open a new Lantern cell");
            }
            eng.executeModeBSubdivided(false, false, false, in.noteValue, in, /*ghostRise=*/true, /*ghostHigh=*/true);
            ++subEvents;
            CHECK(!rec.beginSample(eng).valid, "gate 3: a ghost sub-gate does NOT open a new Lantern cell");
        }
        CHECK(edges == mainSteps, "gate 3: exactly one Lantern cell per main step");
        CHECK(subEvents > mainSteps, "gate 3: the run really exercised more sub-gate events than steps");
    }

    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
