// test_cross_mode_retrace.cpp — the DRIVER-MODE-AGNOSTIC retrace guarantee.
//
// GROUND-TRUTH TODAY, SPEC FOR THE ONE-COUNTER VERSION TOMORROW. This pins the
// property the phase-netsteps handover rests on: the READ folds to the played
// column REGARDLESS of which driver advanced the counter, and regardless of a
// driver-mode change mid-journey. Phase mode accumulates its position through
// absStep (the unwrapped prevContPhase running sum); clock/gate advance the
// counter by +/-1. The two conventions differ in RAW magnitude (the off-by-one
// is raw-netSteps-only, an origin choice: phasor-zero vs musical-entry) but
// agree once folded: fold(readCoord(), len) == playedColumn() in EVERY cell.
//
// This is the precondition the setWindow in-window re-seed is waiting on
// (PositionResolver.hpp: the re-seed "stays until the edges are moved onto the
// same coordinate"). If this holds without the re-seed propping it up, the three
// faces can move onto the one counter. So the test must KEEP HOLDING as that
// retirement lands — a regression here is the re-seed silently coming back.
//
// FOUR gates, all asserting (no prints-only):
//  G1 PHASE MULTI-CYCLE (laps): a forward phasor sweep abs 0..19 crosses the lap
//     at 16 — the counter accumulates past len_, the read folds back. Then a
//     clock-reverse handover retraces DOWN through the same cells. (Task #52.)
//  G2 PHASE FWD then PHASE REV then CLOCK REV: in-mode flip, then a cross-mode
//     handover that continues reverse past the 0-wrap. The column never jumps.
//  G3 PHASE REV across 0-wrap then CLOCK FWD handover: the reverse sweep below 0
//     hands over to a forward clock that retraces UP. Direction survives the
//     handover (the SLICE2B stale-direction regression's cross-mode cousin).
//  G4 the SOLE raw-origin difference is the ENTRY LATCH: clock's first advance
//     from fresh ENTERS (col 0, counter unmoved) and only later advances count,
//     so it needs N+1 ticks to reach phase's step N (phase assigns directly, no
//     latch). The divergence is exactly one step, not a lap or a sign — and
//     within each driver fold(readCoord)==playedColumn throughout. The one-
//     counter unification must decide who owns the latch (phase owns none today).
//
// Rack-free. Compile: see test/run_all.sh ("test_cross_mode_retrace").

#include "test_stubs.hpp"
#include "engines/SequencerEngine.hpp"
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static int fold(long long rc, int len) { return (int)(((rc % len) + len) % len); }

// advance phase to step s (PPQN-quantised), assert the read folds to the column.
static void phaseAt(SequencerEngine& e, int s, int len, const char* tag) {
    e.snapToPhaseStep((long)s * 6, 6);   // PULSES_PER_STEP == 6
    long long rc = e.pos_.readCoord();
    int col = e.pos_.playedColumn();
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s phase s=%d: fold(rc=%lld)=%d != col=%d", tag, s, rc, fold(rc, len), col);
    CHECK(fold(rc, len) == col, buf);
}

// advance clock/gate by motion, assert the read folds to the column.
static void clockStep(SequencerEngine& e, int motion, int len, const char* tag) {
    e.advancePlayhead(motion);
    long long rc = e.pos_.readCoord();
    int col = e.pos_.playedColumn();
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s clock(%+d): fold(rc=%lld)=%d != col=%d", tag, motion, rc, fold(rc, len), col);
    CHECK(fold(rc, len) == col, buf);
}

int main() {
    const int L = 16;

    // ---- G1: phase multi-cycle (laps) + clock-reverse handover -------------
    {
        SequencerEngine e; e.reset(); e.numPolyVoices = 0;
        e.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        for (int s = 0; s <= 19; ++s) phaseAt(e, s, L, "G1");   // crosses lap at 16
        for (int k = 0; k < 8; ++k) clockStep(e, -1, L, "G1");  // handover reverse
    }

    // ---- G2: phase FWD, phase REV (in-mode flip), clock REV (handover) -----
    {
        SequencerEngine e; e.reset(); e.numPolyVoices = 0;
        e.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        for (int s = 0; s <= 10; ++s) phaseAt(e, s, L, "G2");
        for (int s = 9; s >= 5; --s) phaseAt(e, s, L, "G2");    // reverse the phase knob
        for (int k = 0; k < 7; ++k) clockStep(e, -1, L, "G2");  // continue reverse past 0-wrap
    }

    // ---- G3: phase REV across 0-wrap, clock FWD handover (direction survives)
    {
        SequencerEngine e; e.reset(); e.numPolyVoices = 0;
        e.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        for (int s = 5; s >= 0; --s) phaseAt(e, s, L, "G3");    // reverse sweep 5..0
        phaseAt(e, -1, L, "G3");                                // one below 0
        for (int k = 0; k < 4; ++k) clockStep(e, +1, L, "G3");  // handover FORWARD, retrace up
    }

    // ---- G4: the SOLE raw-origin difference is the entry latch (measured) ---
    // Phase assigns the phrase position directly (snapToPhaseStep: step N -> col N,
    // no latch). Clock has the netEntered_ latch: the FIRST advance from a fresh
    // reset ENTERS (col 0, counter unmoved) and only subsequent advances count. So
    // at the same nominal step index the two drivers sit ONE step apart, and the
    // clock needs N+1 ticks to reach phase's step-N column. This one-step latch is
    // the whole of the raw divergence — not a lap, not a sign flip. The one-counter
    // unification has to decide who owns it (phase today owns none).
    {
        // phase: direct assignment, no entry latch.
        SequencerEngine ep; ep.reset(); ep.numPolyVoices = 0;
        ep.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        ep.snapToPhaseStep((long)5 * 6, 6);
        CHECK(ep.pos_.playedColumn() == 5 && ep.pos_.netSteps() == 5,
              "G4: phase step 5 -> col 5, counter 5 (no entry latch)");
        CHECK(fold(ep.pos_.readCoord(), L) == ep.pos_.playedColumn(),
              "G4: phase fold == column");

        // clock: the first advance enters without counting.
        SequencerEngine ec; ec.reset(); ec.numPolyVoices = 0;
        ec.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        ec.advancePlayhead(+1);
        CHECK(ec.pos_.playedColumn() == 0 && ec.pos_.netSteps() == 0,
              "G4: clock 1st advance ENTERS (col 0, counter 0) - the entry latch");
        ec.advancePlayhead(+1);
        CHECK(ec.pos_.playedColumn() == 1 && ec.pos_.netSteps() == 1,
              "G4: clock 2nd advance counts (col 1)");

        // the latch is exactly one step: clock needs N+1 ticks for phase's step N.
        SequencerEngine ec2; ec2.reset(); ec2.numPolyVoices = 0;
        ec2.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        for (int k = 0; k < 6; ++k) ec2.advancePlayhead(+1);    // 1 entry + 5 counts
        CHECK(ec2.pos_.playedColumn() == 5,
              "G4: clock after 6 ticks == phase step 5 (the latch is exactly 1)");
        CHECK(fold(ec2.pos_.readCoord(), L) == ec2.pos_.playedColumn(),
              "G4: clock fold == column");
    }

    if (g_fail == 0) std::printf("test_cross_mode_retrace: ALL PASS\n");
    else             std::printf("test_cross_mode_retrace: %d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
