// test_boundary_engine_edge.cpp — the LockManager phrase boundary now rides the ENGINE's own wrap
// flag, so it is DIRECTION-AGNOSTIC: one boundary per phrase in clock-fwd, clock-rev AND phase.
//
// THE FIX: LockManager::tick(playedColumn, atPhraseBoundary) sets boundaryNow_ = atPhraseBoundary
// (the engine's eng.lastStepResult.wrapped) instead of the old column heuristic
// `playedColumn < prevStep_`, which assumed forward travel and INVERTED in reverse (fired on every
// non-boundary step, silent at the true low->high wrap). That boundary feeds BOTH CA/Cantonment
// (queueFires) and rateManager (Rate::Phrase), so this one change fixes the reverse commit timing
// for everything phrase-rated — the engine-edge fix flagged in #371/#372 (their gate 2, "EXPECTED
// TO MOVE", flips to correct once this lands). Intertropical already rode this flag and was correct
// in reverse (test_intertropical_scene); this brings CA/Cantonment onto the same sound signal.
//
// REPRODUCE-FIRST: the reverse gate FAILS on the old column heuristic (fires lenG-1 times per
// phrase) and PASSES on the engine flag (once). Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "SequencerEngine.hpp"
#include "RateManager.hpp"
#include "MonsoonLockManager.hpp"
using dotModular::LockManager;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

// March the engine `phrases` phrases in `dir` (or phase when phase=true), feeding the LockManager
// exactly as Monsoon does (tick(playedColumn, lastStepResult.wrapped)); count boundaryNow fires.
static int boundaryFires(int lenG, int phrases, int dir, bool phase) {
    SequencerEngine eng; eng.reset();
    eng.updateWindow((float)lenG, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    PatternInput in; in.restProb = 0.f;
    bool locked = false; LockManager lock{locked};
    redDot::RateManager rm;
    int fires = 0;
    for (int i = 0; i < lenG * phrases + 1; ++i) {
        if (phase) eng.snapToPhaseStep((long)i * 6, 6);
        rm.advance(redDot::Rate::Step, true);
        eng.executeModeA(rm, in.noteValue, in, dir);
        lock.tick(eng.playedColumn(), eng.lastStepResult.wrapped);
        if (lock.boundaryNow()) ++fires;
    }
    return fires;
}

int main() {
    std::printf("== test_boundary_engine_edge ==\n");
    const int lenG = 8, phrases = 4;

    // FORWARD — one boundary per phrase (was already correct; the regression guard).
    CHECK(boundaryFires(lenG, phrases, +1, false) == phrases,
          "clock FORWARD: boundaryNow fires once per phrase");

    // REVERSE — the FIX: one boundary per phrase (the old column heuristic fired every non-boundary
    // step here; this reproduce-first gate fails on it and passes on the engine flag).
    CHECK(boundaryFires(lenG, phrases, -1, false) == phrases,
          "clock REVERSE: boundaryNow fires once per phrase (NOT inverted) — the engine-edge fix");

    // PHASE — one boundary per phrase.
    CHECK(boundaryFires(lenG, phrases, +1, true) == phrases,
          "PHASE drive: boundaryNow fires once per phrase");

    // non-dividing window, both directions.
    CHECK(boundaryFires(7, 4, +1, false) == 4, "fwd lenG=7: one per phrase");
    CHECK(boundaryFires(7, 4, -1, false) == 4, "rev lenG=7: one per phrase (the fix)");

    // the UNLOCK edge is unchanged by the fix (boundary source changed, unlock logic did not).
    {
        bool locked = false; LockManager lock{locked};
        lock.tick(0, false); lock.tick(1, false);
        locked = true;  lock.tick(2, false);              // now locked
        locked = false; const bool uf = (lock.tick(2, false), lock.unlockNow());
        CHECK(uf, "the unlock edge still fires (locked -> unlocked), independent of the boundary source");
    }

    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
