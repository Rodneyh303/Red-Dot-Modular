// test_mode_b_gate.cpp — MODE_B_SPEC.md §6 executable spec test.
//
// Mode B = external Gate 1 drives the sequencer: one Gate 1 RISE = one step; the note
// DURATION is Gate 1's width, NOT the internal note-length; REST and LEGATO still apply
// exactly as Mode A. Two layers implement this (see plans/mode_b_spec_impl.md):
//   IMPL 2a (ENGINE, executeModeB): per-rise clear of gs/gsStep holdRemain+gatePulseRemain
//     so the internal note-length no longer arms a multi-step hold. Without it, the
//     executeStep MidNote early-return (holdRemain>=1 || gatePulseRemain>0) SWALLOWS every
//     gate rise that arrives faster than the note length -> "long held notes; rest/legato
//     have no effect" regression.
//   IMPL 2b (MODULE, Monsoon::process): the gate STATE (gs.gateHeld) is driven from Gate 1
//     every sample (+ MODEL 1 slurForward bridge across the gap). Because GATE_OUTPUT, STEP,
//     CV, and the Lantern ALL read that one gs state, they agree by construction (spec §5).
//
// SUITEs 1-3 exercise the REAL SequencerEngine (IMPL 2a). SUITE 4 mirrors the IMPL 2b gate
// driver expression and asserts the §6 CRITICAL invariant: the Lantern-read state == the
// output-path state (they are the same gs.gateHeld, so they cannot diverge).
//
// Build (needs the engine TUs — see test/run_all.sh, entry "test_mode_b_gate|$SE $GS $PE"):
//   g++ -std=c++17 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers \
//       test/test_mode_b_gate.cpp \
//       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp \
//       src/dsp/gates/GateState.cpp -o /tmp/tmb && /tmp/tmb

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
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << "EXPECT_EQ(" #a "," #b ") : " << (long long)(a) << " != " << (long long)(b); \
    throw std::runtime_error(_s.str()); } } while(0)

static int g_pass = 0, g_fail = 0;

// A note-shaping input with a deterministic, all-active scale so genPitchLive always resolves.
static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;   // keep pitch in one octave (irrelevant to gate logic)
    return in;
}

// Drive one external Gate 1 RISE through the real engine and return the decision.
// gate1High=true models the gate being high at the rising edge (its width is applied by the
// module layer, not here). restProb/legatoProb/noteVal are the Mode-A-identical shaping inputs.
static StepResult rise(SequencerEngine& eng, float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    return eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true, restProb, legatoProb, noteVal, in);
}

int main() {
    using D = MonoDecision;

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("Mode B §3 — internal note-length NULLIFIED (the swallow is defeated)");
    // The core regression: with a multi-step noteVal, fast consecutive rises must STILL each
    // produce a fresh stepped decision — never MidNote. Pre-fix, rise 2+ returned MidNote
    // because holdRemain from the 1/4-note arm was still >=1 at the next edge.
    for (int nv = 0; nv <= 7; ++nv) {
        TEST(std::string("noteVal idx ") + std::to_string(nv) + ": every fast rise is a fresh step (never MidNote)", {
            SequencerEngine eng;
            eng.numPolyVoices = 0;
            // restProb=0 (r_rest defaults to 0, not < 0) -> never rest; legatoProb=0 -> plain notes.
            for (int i = 0; i < 8; ++i) {
                StepResult r = rise(eng, /*restProb=*/0.f, /*legatoProb=*/0.f, /*noteVal=*/(float)nv);
                EXPECT(r.stepped);                       // one step advance per rise
                EXPECT(r.decision != D::MidNote);        // the swallow must be gone
                EXPECT(r.decision == D::NewNote);        // plain notes, no legato
            }
        });
    }

    TEST("holdRemain never carries a multi-step hold into the next rise (regardless of noteVal)", {
        // Longest note value (idx 0 = 1/1 = 16 steps) is the worst case for the old swallow.
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, 0.f, 0.f, /*noteVal=*/0.f);            // arm a 1/1 note internally
        // The per-rise clear runs BEFORE executeStep's guard on the NEXT rise, so the guard
        // sees a cleared countdown and does not early-return: the next rise is a fresh step.
        StepResult r2 = rise(eng, 0.f, 0.f, /*noteVal=*/0.f);
        EXPECT(r2.stepped);
        EXPECT(r2.decision == D::NewNote);               // not MidNote despite the 16-step arm
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("Mode B §2 — REST still punches holes (Mode-A-identical)");
    TEST("restProb high -> decision Rest and gate state low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        // r_rest defaults to 0; restProb=0.5 -> 0 < 0.5 -> rest fires deterministically.
        StepResult r = rise(eng, /*restProb=*/0.5f, /*legatoProb=*/0.f, /*noteVal=*/2.f);
        EXPECT(r.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);                        // rest closes the fused gate
        EXPECT(!r.accented);                             // no accent on a rest
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("Mode B §4 — LEGATO decision reused from Mode A; REST wins over a pending slur");
    TEST("a held predecessor + legato roll connects (Tie/Legato, not a fresh NewNote)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, 0.f, /*legatoProb=*/0.5f, 4.f);        // note 1: NewNote, commits slurForward
        StepResult r2 = rise(eng, 0.f, /*legatoProb=*/0.5f, 4.f);   // note 2: connects back
        EXPECT(r2.decision == D::Tie || r2.decision == D::Legato);
    });

    TEST("rest wins over a pending slur (§4b): slur into a rest step -> Rest, gate low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, 0.f, /*legatoProb=*/0.5f, 4.f);        // note 1 commits slurForward
        StepResult r2 = rise(eng, /*restProb=*/0.5f, /*legatoProb=*/0.5f, 4.f);  // next step rests
        EXPECT(r2.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("Mode B §5 — gate STATE follows Gate 1 (+ MODEL 1 bridge); state == output invariant");
    // Faithful mirror of the IMPL 2b driver in Monsoon::process:
    //   isRest    -> gateHeld = false        (rest wins, gate low)
    //   gate1High -> gateHeld = true          (note sounds for the gate's width)
    //   gap (low) -> gateHeld = slurForward   (MODEL 1 bridge iff this note slurs forward)
    // We RUN this rule on a real engine's gs, then assert the SAME gs.gateHeld is what any
    // read-path (output OR Lantern) would observe — they cannot diverge (spec §6 CRITICAL).
    // Mirror of the IMPL 2b driver INCLUDING the holdRemain-zero-on-close (MODE_B_SPEC.md
    // "REMAINING BUG" fix), so the state==output invariant is tested end to end.
    auto driveGateB = [](SequencerEngine& eng, bool gate1High, bool isRest) {
        const bool gateOpen = !isRest && (gate1High || eng.gs.slurForward);
        eng.gs.gateHeld = gateOpen;
        if (!gateOpen) eng.gs.holdRemain = 0.f;   // keeps Lantern's sounding test == the output
    };

    TEST("REST forces gate low regardless of Gate 1 level", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        eng.gs.slurForward = true;                       // even a pending slur must yield to rest
        driveGateB(eng, /*gate1High=*/true, /*isRest=*/true);
        EXPECT(!eng.gs.gateHeld);
    });

    TEST("note + Gate 1 HIGH -> gate high (width follows the external gate)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        eng.gs.slurForward = false;
        driveGateB(eng, /*gate1High=*/true, /*isRest=*/false);
        EXPECT(eng.gs.gateHeld);
    });

    TEST("non-legato note in the GAP (Gate 1 low) -> gate low = clean re-articulation", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        eng.gs.slurForward = false;
        driveGateB(eng, /*gate1High=*/false, /*isRest=*/false);
        EXPECT(!eng.gs.gateHeld);
    });

    TEST("legato note in the GAP -> gate BRIDGES high to the next rise (MODEL 1)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        eng.gs.slurForward = true;                       // committed to slur forward at onset
        driveGateB(eng, /*gate1High=*/false, /*isRest=*/false);
        EXPECT(eng.gs.gateHeld);                         // bridged across the gap
    });

    TEST("CRITICAL (§6): Lantern-read state == output-path state (single gs, no divergence)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        eng.gs.slurForward = true;
        driveGateB(eng, /*gate1High=*/false, /*isRest=*/false);
        // The output path reads gs.gateHeld; the Lantern's SOUNDING test reads the SAME field
        // (gs.gateHeld || gs.holdRemain>0). With one source of truth they are identical.
        const bool outputSees  = eng.gs.gateHeld;
        const bool lanternSees = eng.gs.gateHeld || (eng.gs.holdRemain > 0.0001f);
        EXPECT_EQ((int)outputSees, (int)lanternSees);
    });

    TEST("REMAINING BUG regression: real rise arms holdRemain, then REST -> Lantern NOT sounding", {
        // The exact shipped bug: a real gate rise runs executeStep -> triggerNote, which re-arms
        // gs.holdRemain > 0. If the next step is a REST, the OUTPUT gate is low (gateHeld=false)
        // but the Lantern's sounding = gateHeld || holdRemain>0 would STILL read true unless the
        // driver zeroes holdRemain. Drive it end to end and assert output == Lantern == not sounding.
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, /*restProb=*/0.f, /*legatoProb=*/0.f, /*noteVal=*/2.f);   // real note: arms holdRemain
        EXPECT(eng.gs.holdRemain > 0.0001f);                                // precondition: armed
        driveGateB(eng, /*gate1High=*/true, /*isRest=*/true);              // this step is a REST
        const bool outputSees  = eng.gs.gateHeld;                           // output path
        const bool lanternSees = eng.gs.gateHeld || (eng.gs.holdRemain > 0.0001f);  // Lantern:347
        EXPECT(!outputSees);                                                // output correctly low
        EXPECT(!lanternSees);                                               // Lantern also NOT sounding
        EXPECT_EQ((int)outputSees, (int)lanternSees);                       // no divergence
    });

    TEST("REMAINING BUG regression: real rise then non-legato GAP -> Lantern NOT sounding", {
        // Same class, the GAP case: note played (holdRemain armed), Gate 1 falls, no slur -> the
        // gate must read low in BOTH the output and the Lantern (holdRemain must not keep it alive).
        SequencerEngine eng; eng.numPolyVoices = 0;
        rise(eng, 0.f, /*legatoProb=*/0.f, 2.f);           // note played; slurForward stays false
        EXPECT(eng.gs.holdRemain > 0.0001f);
        driveGateB(eng, /*gate1High=*/false, /*isRest=*/false);   // Gate 1 low, non-legato gap
        EXPECT(!eng.gs.gateHeld);
        EXPECT(!(eng.gs.gateHeld || (eng.gs.holdRemain > 0.0001f)));   // Lantern agrees: silent
    });

    // ─────────────────────────────────────────────────────────────────────────────
    std::cout << "\n" << (g_fail == 0 ? "\033[32m" : "\033[31m")
              << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail == 0 ? 0 : 1;
}
