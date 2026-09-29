// test_mode_b_gap.cpp — LEGATO_GATE_GAP_BUG.md executable spec test.
//
// Mode B legato must key off GATE ADJACENCY (overlap / <1ms low), not note duration. The fix
// (executeModeB): at a rise, if the gate was NOT adjacent (a >=1ms low — gate1Adjacent=false),
// clear gs.gateHeld/holdRemain/slurForward BEFORE wasHeldMono is captured, so wasHeld reads false
// -> fresh NewNote. If adjacent (gate1Adjacent=true), leave it -> wasHeld stays true -> legato
// candidate (still subject to legatoProb/prevSlur).
//
// The pair the failed fix (d0b8ea4) could NOT satisfy together:
//   (1) overlapping / tight (<1ms) gates -> TIE candidate;
//   (2) clearly-separated (>=1ms low) gates -> FRESH note.
// Any fix that gets one but not the other is wrong.
//
// This is an ENGINE test: it drives executeModeB directly with the gate1Adjacent bool. The 1ms
// low-duration ACCUMULATOR that produces gate1Adjacent lives in the MODULE layer (Monsoon::process,
// edge-timed: reset while HIGH, accumulate while LOW) and is therefore direction-agnostic by
// construction — reverse-safety is asserted there, not here (Mode B's executeModeB is forward-only;
// the accumulator keys off the physical gate, not the play index, so forward and reverse see the
// identical adjacency for the same gate waveform).
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

// Drive one Gate 1 RISE through the real engine with an explicit adjacency flag. gate1High=true
// models the gate high at the edge (width is a module-layer concern, not here).
static StepResult rise(SequencerEngine& eng, bool adjacent, float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    return eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true, /*gate1Adjacent=*/adjacent,
                            restProb, legatoProb, noteVal, in);
}

int main() {
    // ── (1) Overlap / tight (<1ms): gates ADJACENT -> TIE candidate ──────────────────────────
    SUITE("adjacent gates tie (overlap / <1ms)");
    TEST("note A (legato=1, adjacent) commits slurForward; note B adjacent -> Tie/Legato", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        StepResult a = rise(eng, /*adjacent=*/true,  /*rest=*/0.f, /*legato=*/1.0f, 4.f);
        (void)a;   // A is the lead (LegatoMax at legato=1.0); it commits gs.slurForward
        StepResult b = rise(eng, /*adjacent=*/true,  /*rest=*/0.f, /*legato=*/0.5f, 4.f);
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);   // adjacent -> connects
    });

    // ── (2) Clearly-separated (>=1ms low): NOT adjacent -> FRESH note ───────────────────────
    SUITE("separated gates retrigger (>=1ms low)");
    TEST("note B after a >=1ms gap (adjacent=false) -> NewNote, even with a committed slur", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        StepResult a = rise(eng, /*adjacent=*/true,  /*rest=*/0.f, /*legato=*/1.0f, 4.f);  // commits slurForward
        (void)a;
        StepResult b = rise(eng, /*adjacent=*/false, /*rest=*/0.f, /*legato=*/0.5f, 4.f);  // long gap
        EXPECT(b.decision == D::NewNote);   // gap cleared wasHeld -> fresh, NO tie across the gap
    });

    TEST("a long gap breaks the chain: B fresh, and a following adjacent C still ties to B (not A)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, /*adjacent=*/true,  0.f, 1.0f, 4.f);   // A
        StepResult b = rise(eng, /*adjacent=*/false, 0.f, 1.0f, 4.f);   // gap -> B fresh (NewNote)
        EXPECT(b.decision == D::NewNote || b.decision == D::LegatoMax);
        // C adjacent to B: connects to B (B committed slurForward at legato=1.0), not stale-A.
        StepResult c = rise(eng, /*adjacent=*/true,  0.f, 0.5f, 4.f);
        EXPECT(c.decision == D::Tie || c.decision == D::Legato);
    });

    // ── (3) Normal adjacent phrasing unchanged (byte-identical to pre-fix adjacent path) ─────
    SUITE("adjacent phrasing unchanged");
    TEST("a run of adjacent gates at legato=1.0 all connect (LegatoMax), as before the fix", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        StepResult a = rise(eng, /*adjacent=*/true, 0.f, 1.0f, 4.f);
        EXPECT(a.decision == D::LegatoMax || a.decision == D::NewNote);  // lead
        StepResult b = rise(eng, /*adjacent=*/true, 0.f, 1.0f, 4.f);
        EXPECT(b.decision == D::LegatoMax);   // legato=1.0 forces connect (unchanged)
        StepResult c = rise(eng, /*adjacent=*/true, 0.f, 1.0f, 4.f);
        EXPECT(c.decision == D::LegatoMax);
    });

    TEST("adjacent=true throughout never produces a spurious fresh note (no over-correction)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, /*adjacent=*/true, 0.f, 1.0f, 4.f);
        for (int i = 0; i < 4; ++i) {
            StepResult r = rise(eng, /*adjacent=*/true, 0.f, 1.0f, 4.f);
            EXPECT(r.decision != D::NewNote);   // adjacent + legato=1.0 -> always connects
        }
    });

    // ── (4) Reverse-safety: direction-agnostic by construction ───────────────────────────────
    // Mode B's executeModeB is forward-only at the engine; the gate1Adjacent bool is symmetric.
    // The DIRECTION-AGNOSTISM lives in the module-layer accumulator (edge-timed: reset on the
    // falling edge, accumulate while low — keys off the physical gate, not the play index), so the
    // SAME gate waveform yields the SAME adjacency in forward and reverse. This test asserts the
    // engine side of that contract: the gap decision depends ONLY on the adjacency bool, not on any
    // internal direction/counter state, so forward and reverse calls with the same bool agree.
    SUITE("reverse-safety (engine contract)");
    TEST("the gap decision is a pure function of gate1Adjacent (no hidden direction state)", {
        // Two engines, identical adjacency sequence -> identical decisions. (Reverse at the module
        // layer feeds the same bools; the engine must not introduce a direction asymmetry.)
        SequencerEngine f, r; f.numPolyVoices = 0; r.numPolyVoices = 0;
        StepResult fa = rise(f, true,  0.f, 1.0f, 4.f);
        StepResult ra = rise(r, true,  0.f, 1.0f, 4.f);
        EXPECT(fa.decision == ra.decision);
        StepResult fb = rise(f, false, 0.f, 0.5f, 4.f);   // gap
        StepResult rb = rise(r, false, 0.f, 0.5f, 4.f);   // same gap
        EXPECT(fb.decision == rb.decision);
        EXPECT(fb.decision == D::NewNote);                // gap -> fresh, both directions
        StepResult fc = rise(f, true,  0.f, 0.5f, 4.f);   // adjacent after the gap
        StepResult rc = rise(r, true,  0.f, 0.5f, 4.f);
        EXPECT(fc.decision == rc.decision);               // identical -> direction-agnostic
    });

    std::cout << "\n-----\nmode_b_gap: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
