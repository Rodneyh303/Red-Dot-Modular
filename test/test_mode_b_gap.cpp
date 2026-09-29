// test_mode_b_gap.cpp — LEGATO_GATE_GAP_BUG.md regression test (held-predecessor invariant).
//
// Mode B legato keys off the HELD-PREDECESSOR invariant: a tie/legato requires the previous gate
// STILL HIGH at this rise (overlap / hold) — wasHeld = gs.gateHeld (note-length is nullified in
// Mode B, so holdRemain is not the adjacency proxy).  There is NO slurForward gate bridge and NO
// gap timer: a gap drops the gate => wasHeld false => fresh NewNote; overlap keeps it high => tie.
// (The earlier slurForward bridge + 1ms gate1Adjacent timer were removed — the experiment in
// plans/legato_gate_gap_fix.md showed the bridge was the cause of the long-gap bug and the timer
// was a redundant/harmful band-aid.)
//
// This is an ENGINE test: it drives executeModeB directly and models the module-layer gate state
// between rises (gs.gateHeld = gate1High during the gap; the module layer sets exactly this).
//
// Build (see test/run_all.sh, entry "test_mode_b_gap|$SE $GS $PE"):
//   g++ -std=c++17 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers \
//       test/test_mode_b_gap.cpp \
//       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp \
//       src/dsp/gates/GateState.cpp -o /tmp/tmg && /tmp/tmg

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "engines/SequencerEngine.hpp"

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

using D = MonoDecision;

// Deterministic, all-active scale so genPitchLive always resolves. restProb=0 (no rests) so the
// rest branch never interferes with the adjacency-under-test.
static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;
    return in;
}

// Drive one Gate 1 RISE through the real engine. gate1High=true models the gate high at the edge.
static StepResult rise(SequencerEngine& eng, float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    return eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true, restProb, legatoProb, noteVal, in);
}

// Model the module-layer gate state during a gap (gate1High=false): the gate is LOW, note-length
// decayed.  This is what IMPL 2b sets per-sample while Gate 1 is low (no bridge).
static void gapLow(SequencerEngine& eng) {
    eng.gs.gateHeld = false;
    eng.gs.holdRemain = 0.f;
}

// Model the module-layer gate state during OVERLAP (gate1High still true at the next rise).
static void overlapHigh(SequencerEngine& eng) {
    eng.gs.gateHeld = true;
    eng.gs.holdRemain = 0.f;
}

int main() {
    // ── (1) Long gap -> FRESH (the bug, fixed by the held-predecessor invariant) ─────────────
    SUITE("long gap retriggers (>=1ms low)");
    TEST("note B after a long gap (gate low) -> NewNote, even with a committed slur", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, /*rest=*/0.f, /*legato=*/1.0f, 4.f);   // A commits slurForward (legato=1.0)
        gapLow(eng);                                     // long gap: gate low, note-length decayed
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::NewNote);                // wasHeld false -> fresh, NO tie across the gap
    });

    TEST("a long gap breaks the chain: B fresh, then an overlapping C ties to B (not stale A)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, 0.f, 1.0f, 4.f);                       // A
        StepResult b = rise(eng, 0.f, 1.0f, 4.f);        // (no gap sim -> gate still high) B connects
        EXPECT(b.decision == D::LegatoMax);              // legato=1.0 forces connect
        gapLow(eng);                                     // gap before C
        StepResult c = rise(eng, 0.f, 0.5f, 4.f);        // gap -> C fresh
        EXPECT(c.decision == D::NewNote);
    });

    // ── (2) Overlap / held-across -> TIE (the held-predecessor invariant works) ─────────────
    SUITE("overlap / held-across ties");
    TEST("note B with the gate STILL HIGH (overlap) -> Tie/Legato", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, 0.f, 1.0f, 4.f);                       // A commits slurForward
        overlapHigh(eng);                                // overlap: gate still high at B's rise
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);   // wasHeld true -> connects
    });

    // ── (3) Normal adjacent phrasing unchanged ───────────────────────────────────────────────
    SUITE("adjacent phrasing unchanged");
    TEST("a run of gates at legato=1.0 all connect (LegatoMax), as before", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        StepResult a = rise(eng, 0.f, 1.0f, 4.f);
        EXPECT(a.decision == D::LegatoMax || a.decision == D::NewNote);   // lead
        StepResult b = rise(eng, 0.f, 1.0f, 4.f);
        EXPECT(b.decision == D::LegatoMax);              // legato=1.0 forces connect (unchanged)
        StepResult c = rise(eng, 0.f, 1.0f, 4.f);
        EXPECT(c.decision == D::LegatoMax);
    });

    // ── (4) Reverse-safety: the gap decision is a pure function of the gate state ───────────
    // Mode B's executeModeB is forward-only at the engine; the decision depends only on gs.gateHeld
    // (set by the edge-timed module layer), not on any internal direction/counter state, so forward
    // and reverse calls with the same gate waveform agree.  (The module-layer accumulator that would
    // feed a timer is gone; there is no direction state to diverge.)
    SUITE("reverse-safety (engine contract)");
    TEST("the gap decision is a pure function of gs.gateHeld (no hidden direction state)", {
        SequencerEngine f, r; f.numPolyVoices = 0; r.numPolyVoices = 0;
        rise(f, 0.f, 1.0f, 4.f);  rise(r, 0.f, 1.0f, 4.f);           // A, both
        gapLow(f); gapLow(r);                                         // same gap
        StepResult fb = rise(f, 0.f, 0.5f, 4.f);
        StepResult rb = rise(r, 0.f, 0.5f, 4.f);
        EXPECT(fb.decision == rb.decision);
        EXPECT(fb.decision == D::NewNote);                            // gap -> fresh, both directions
        overlapHigh(f); overlapHigh(r);                               // overlap after the gap
        StepResult fc = rise(f, 0.f, 0.5f, 4.f);
        StepResult rc = rise(r, 0.f, 0.5f, 4.f);
        EXPECT(fc.decision == rc.decision);                           // identical -> direction-agnostic
    });

    std::cout << "\n-----\nmode_b_gap: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
