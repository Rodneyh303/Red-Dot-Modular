// test_phrase_boundary_absolute — the forward phrase edge is a DETERMINISTIC absolute
// transport position, and SNAPS BACK when the global offset reverts.
//
// THE DECISION (CC_PROMPT_POSITION_RESOLVER_C3_UNIT_OF_ABSOLUTENESS.md — finishing slice 5):
// the signed transportTick_ is the unit of absoluteness; the forward phrase edge is the
// window-start crossing on that ONE clock, phaseClock(transportTick_) == 0 — NOT the played-
// column accumulator phrasePos()==0, which re-parks in setWindow (PositionResolver.hpp :111)
// on a mid-phrase window re-author and so drifts against the clock.
//
// THE BUG (Rodney's Rack feel-test): the dice/slew redraw fired at the drifted column
// (phaseClock stuck at ~8/14/30) instead of at the window start — "not when transport is at
// zero, more like 8" — and it affected redice too, because dice and slew share the one edge
// still on col_. The reads already fold phaseClock (SequencerEngine.cpp :844/:1255/:1529); the
// edge was the last consumer on the column.
//
// THE CONTRACT (forward): once started, wrapped == (wasStarted && phaseClock(transportTick_)==0).
// In the aligned case col_ == startStep + phaseClock, so phrasePos()==0 <=> phaseClock==0 and the
// contract holds pre-fix too (T1 control). It only diverges under the setWindow re-park (T2), so:
//   T1 (control, no window change) PASSES before AND after — pins the contract + harness.
//   T2 (offset perturbed mid-phrase, then reverted) FAILS before (drift), PASSES after.
// Directionless HOLD (advancePlayhead(0)): the tick freezes, the last direction is retained, so the
// held step reads the SAME edge — covered by the same per-step invariant (interleaved in T2).
// Compile: see test/run_all.sh.

#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

static void fillRandoms(SequencerEngine& eng) {
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
        eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    const auto v = VoiceResolver::slot(1);
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(v, LaneId{kLanes[l]})[i]=.5f;
}

// One forward step (dir): returns the engine's `wrapped` (the phrase edge), and the wasStarted
// it was computed against. Drives the SAME path the engine uses (advancePlayhead).
struct StepObs { bool wrapped; bool wasStarted; int phaseClock; int phrasePos; int col; long long tick; };
static StepObs step(SequencerEngine& eng, int dir) {
    StepObs o;
    o.wasStarted = eng.started();
    o.wrapped    = eng.advancePlayhead(dir, nullptr);
    o.tick       = (long long)eng.transportTick_;
    o.phaseClock = eng.pos_.phaseClock(eng.transportTick_);
    o.phrasePos  = eng.pos_.phrasePos();
    o.col        = eng.pos_.playedColumn();
    return o;
}

// The forward contract, asserted on one observed step. Returns true if it HELD.
static bool contractHolds(const StepObs& o) {
    return o.wrapped == (o.wasStarted && o.phaseClock == 0);
}

int main(){
    std::printf("test_phrase_boundary_absolute: the forward phrase edge is a deterministic absolute transport position\n");
    char msg[220];
    const int LEN = 16;

    // ── T1: control — a forward run with NO window change. The contract holds at every started
    //    step (pre-fix and post-fix): with no re-park, col_ == startStep + phaseClock. ──
    {
        SequencerEngine eng; eng.reset(); eng.numPolyVoices = 1; fillRandoms(eng);
        eng.updateWindow((float)LEN, 0.f, false, 0.f, 0.f, false);   // len 16, off 0
        int bad = 0;
        for (int s = 0; s < 3*LEN; ++s) {                            // ~3 phrases
            StepObs o = step(eng, +1);
            if (!contractHolds(o)) ++bad;
        }
        std::snprintf(msg, sizeof msg, "T1 control (no window change): %d started steps violate wrapped==(phaseClock==0)", bad);
        CHECK(bad == 0, msg);
    }

    // ── T2: reproduce — perturb the global offset mid-phrase, then REVERT to 0, and keep driving.
    //    The played column re-parks (PositionResolver :111) and drifts against the clock, so the
    //    column-based edge no longer lands at phaseClock==0 and does NOT snap back on revert.
    //    A directionless HOLD is interleaved to pin that a frozen tick reads the same edge. ──
    {
        SequencerEngine eng; eng.reset(); eng.numPolyVoices = 1; fillRandoms(eng);
        eng.updateWindow((float)LEN, 0.f, false, 0.f, 0.f, false);
        // warm exactly 20 steps at off 0 (lands col at (20-1)%16 = 3)
        for (int s = 0; s < 20; ++s) step(eng, +1);
        // perturb: off 8 (re-parks col 3 -> outside [8,24) -> park), then 3, then 11 — each a
        // mid-phrase window re-author that decouples col_ from the running tick.
        const int perturb[] = { 8, 3, 11 };
        const int hold[]    = { 5, 7, 5 };
        for (int p = 0; p < 3; ++p) {
            eng.updateWindow((float)LEN, 0.f, false, (float)perturb[p], 0.f, false);
            for (int s = 0; s < hold[p]; ++s) step(eng, +1);
        }
        // REVERT to off 0 — the forward boundary must snap back to phaseClock==0.
        eng.updateWindow((float)LEN, 0.f, false, 0.f, 0.f, false);

        int bad = 0, boundaries = 0, holdsBad = 0;
        for (int s = 0; s < 3*LEN; ++s) {
            StepObs o = step(eng, +1);
            if (!contractHolds(o)) ++bad;
            if (o.wrapped) ++boundaries;
            // a directionless HOLD every 8th step: tick frozen, edge must be unchanged vs the
            // step just taken (same phaseClock => same contract).
            if (s % 8 == 3) {
                long long tBefore = (long long)eng.transportTick_;
                StepObs h = step(eng, 0);
                if (!contractHolds(h)) ++holdsBad;
                if ((long long)eng.transportTick_ != tBefore) ++holdsBad;   // the tick must freeze on a hold
            }
        }
        std::snprintf(msg, sizeof msg, "T2 after offset revert: %d started steps violate wrapped==(phaseClock==0)", bad);
        CHECK(bad == 0, msg);
        std::snprintf(msg, sizeof msg, "T2 hold-at-boundary: %d held steps misfire or move the tick", holdsBad);
        CHECK(holdsBad == 0, msg);
        // determinism: over 3 reverted phrases the edge fires exactly once per phrase (3), at the
        // window start — not drifted, not doubled.
        std::snprintf(msg, sizeof msg, "T2 snap-back: %d phrase boundaries over 3 phrases (expected 3)", boundaries);
        CHECK(boundaries == 3, msg);
    }

    std::printf(g_fail ? "\nFAILED (%d)\n" : "\nOK\n", g_fail);
    return g_fail ? 1 : 0;
}
