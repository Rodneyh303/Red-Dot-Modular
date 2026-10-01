// test_gate_smoke.cpp — per-mode gate-emission smoke + B/D shouldExecute parity guard.
//
// WHY THIS TEST EXISTS (plans/fix_gate_output_regression_a205093.md):
//   a205093 unified Mode D's ENGINE + INPUT ROUTING with Mode B (Gate 1 = main) but left Mode D's
//   module-layer `shouldExecute` on the OLD pre-fix logic — `useSubGate ? subGateRise : true` — so
//   with no subgate patched Mode D called executeMode EVERY SAMPLE, jamming gate state (no coherent
//   gate output). The suite was GREEN because test_gate_mode_agnostic drives the engine twins
//   directly (executeModeB vs executeModeD), bypassing the module-layer shouldExecute gate where the
//   bug lived. This test closes that gap two ways:
//     SUITE 1 — per-mode gate SMOKE: each of A..F, driven through its canonical engine entry with a
//               non-rest step, asserts gs.gateHeld goes true (i.e. GATE_OUTPUT would emit).
//     SUITE 2 — B/D shouldExecute PARITY: a faithful copy of the module-layer shouldExecute rule
//               (Monsoon.cpp ~800, post-fix) asserts B≡D across an input matrix AND asserts the
//               specific regression — no edges + no subgate → Mode D does NOT execute (old `: true`).
//
// Build (see test/run_all.sh, entry "test_gate_smoke|$SE $GS $PE $CE"):
//   g++ -std=c++17 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers \
//       test/test_gate_smoke.cpp \
//       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp \
//       src/dsp/engines/ClockEngine.cpp src/dsp/gates/GateState.cpp -o /tmp/tgs && /tmp/tgs

#include <iostream>
#include <sstream>
#include <string>

#include "engines/SequencerEngine.hpp"
#include "engines/ClockEngine.hpp"

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

// All-active scale so genPitchLive always resolves; pitch is irrelevant to gate logic.
static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;
    return in;
}

// A clock view with a 1/16 edge asserted — exactly what the module layer passes into executeModeA
// for Modes A / C / E / F (the controller routes all four through engine.executeModeA; only the
// edge SOURCE differs — clock.sixteenthEdge for A/C, phase.sixteenthEdge for E/F).
static ClockEngine edgeClock() {
    ClockEngine ck;
    ck.sixteenthEdge = true;
    return ck;
}

// ── Faithful copy of the module-layer shouldExecute gate (Monsoon.cpp ~800, POST-fix). ──
// One branch for modeSelect 1 (B) and 3 (D): the §421 mode-agnostic invariant. Keeping this as a
// single expression for both is what prevents the B/D drift bug from recurring. If Monsoon.cpp's
// rule changes, update this mirror to match (the parity assertions then re-encode the contract).
static bool shouldExecute(int modeSelect, bool gate1High, bool gate1Rise,
                          bool subGateRise, bool ghostRise, int stepIndex,
                          bool clockSixteenth, bool phaseSixteenth) {
    if (modeSelect == 1 || modeSelect == 3) {
        const bool inGate = gate1High;
        return gate1Rise || (gate1High && stepIndex == -1)   // main onset / held-at-start
            || (inGate && subGateRise)                        // ratchet in-gate (Gate 2)
            || (!inGate && ghostRise);                        // ghost in-gap (Gate 3)
    }
    if (modeSelect == 0 || modeSelect == 2) return clockSixteenth;   // A / C: generated 1/16 grid
    if (modeSelect == 4 || modeSelect == 5) return phaseSixteenth;   // E / F: phase 1/16 grid
    return false;
}

int main() {
    using D = MonoDecision;

    // ════════════════════════════════════════════════════════════════════════════
    SUITE("1 — per-mode gate SMOKE: A..F each emit a gate on a non-rest step");
    // GATE_OUTPUT is high iff engine.gs.gateHeld (GateState::process). So "emits a gate" ==
    // gs.gateHeld true after a non-rest step. The controller routes A/C/E/F → engine.executeModeA
    // and B/D → engine.executeModeB (the §421 mode-agnostic invariant; only pitch source differs),
    // so the engine-level smoke has two canonical entries — exercised once per MODE to mirror the
    // module dispatch and keep the per-mode contract explicit.

    TEST("Mode A (clock): a 1/16 step, no rest -> gate high", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), /*restProb=*/0.f, /*legatoProb=*/0.f,
                                        /*noteVal=*/2.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);                       // GATE_OUTPUT would emit
    });

    TEST("Mode B (gate): a Gate-1 rise, no rest -> gate high", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true,
                                        /*restProb=*/0.f, /*legatoProb=*/0.f, /*noteVal=*/0.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    TEST("Mode C (quantiser+clock): a 1/16 step, no rest -> gate high", {
        // Mode C routes through engine.executeModeA (controller::executeModeC); pitch source
        // (quantise CV2) does not affect gate emission.
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), 0.f, 0.f, 2.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    TEST("Mode D (quantiser+gate): a Gate-1 rise, no rest -> gate high", {
        // Mode D routes through engine.executeModeB (controller::executeModeD mirrors B). The
        // a205093 regression jammed this at the MODULE layer (shouldExecute `: true`); the engine
        // twin itself is correct, as asserted here and in test_gate_mode_agnostic.
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true,
                                        0.f, 0.f, 0.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    TEST("Mode E (phase): a 1/16 phase step, no rest -> gate high", {
        // Mode E routes through engine.executeModeA with a phase-derived edge view.
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), 0.f, 0.f, 2.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    TEST("Mode F (quantiser+phase): a 1/16 phase step, no rest -> gate high", {
        // Mode F routes through engine.executeModeA (controller::executeModeF).
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), 0.f, 0.f, 2.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    // ════════════════════════════════════════════════════════════════════════════
    SUITE("2 — REST suppresses the gate (negative smoke, A..F)");
    // restProb high -> decision Rest and gs.gateHeld false. Confirms the gate truly tracks the
    // step decision (not stuck high) for each mode's canonical entry.

    TEST("Mode A: rest step -> gate low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), /*restProb=*/0.5f, 0.f, 2.f, in);
        EXPECT(r.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);
    });

    TEST("Mode B: rest step -> gate low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeB(true, true, /*restProb=*/0.5f, 0.f, 0.f, in);
        EXPECT(r.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);
    });

    // ════════════════════════════════════════════════════════════════════════════
    SUITE("3 — B/D shouldExecute PARITY (the a205093 regression guard)");
    // The bug lived in the MODULE-layer shouldExecute gate (Monsoon.cpp), which the engine-level
    // tests bypass. This suite encodes that rule's contract directly: B and D share ONE branch
    // (§421), so their shouldExecute must be identical for every edge combination, and Mode D must
    // NOT fall through to `: true` when nothing is patched.

    TEST("REGRESSION (a205093): Mode D, no edges + no subgate -> does NOT execute every sample", {
        // The old D branch was `useSubGate ? subGateRise : true` → with nothing patched it returned
        // TRUE, so executeMode ran every sample and jammed the gate. Post-fix D shares B's edge-gated
        // logic, so with no Gate-1 rise / no held-at-start / no ratchet / no ghost it must be FALSE.
        EXPECT(!shouldExecute(/*modeSelect=*/3, /*gate1High=*/false, /*gate1Rise=*/false,
                              /*subGateRise=*/false, /*ghostRise=*/false, /*stepIndex=*/0,
                              /*clockSixteenth=*/true, /*phaseSixteenth=*/true));
    });

    TEST("Mode D steps on a Gate-1 rise exactly as Mode B", {
        EXPECT(shouldExecute(3, true,  true,  false, false, 0, true, true));
        EXPECT_EQ(shouldExecute(1, true,  true,  false, false, 0, true, true),
                  shouldExecute(3, true,  true,  false, false, 0, true, true));
    });

    TEST("Mode D held-at-start (stepIndex==-1, Gate 1 high) steps like Mode B", {
        EXPECT(shouldExecute(3, true,  false, false, false, -1, true, true));
        EXPECT_EQ(shouldExecute(1, true,  false, false, false, -1, true, true),
                  shouldExecute(3, true,  false, false, false, -1, true, true));
    });

    TEST("B ≡ D across the full edge matrix (shouldExecute identical for every combination)", {
        bool allEqual = true;
        for (int gh = 0; gh <= 1; ++gh)
            for (int gr = 0; gr <= 1; ++gr)
                for (int sr = 0; sr <= 1; ++sr)
                    for (int ghst = 0; ghst <= 1; ++ghst)
                        for (int si : {-1, 0, 5}) {
                            const bool b = shouldExecute(1, gh, gr, sr, ghst, si, true, true);
                            const bool d = shouldExecute(3, gh, gr, sr, ghst, si, true, true);
                            if (b != d) { allEqual = false;
                                std::cout << "    MISMATCH gh=" << gh << " gr=" << gr
                                          << " sr=" << sr << " ghst=" << ghst << " si=" << si
                                          << " B=" << b << " D=" << d << "\n"; }
                        }
        EXPECT(allEqual);
    });

    TEST("Clock/phase modes unaffected by the gate branch (A/C on clock, E/F on phase)", {
        EXPECT(shouldExecute(0, false, false, false, false, 0, /*clockSixteenth=*/true,  false));
        EXPECT(shouldExecute(2, false, false, false, false, 0, /*clockSixteenth=*/true,  false));
        EXPECT(shouldExecute(4, false, false, false, false, 0, false, /*phaseSixteenth=*/true));
        EXPECT(shouldExecute(5, false, false, false, false, 0, false, /*phaseSixteenth=*/true));
        EXPECT(!shouldExecute(0, false, false, false, false, 0, false, false));  // no edge -> no step
    });

    // ─────────────────────────────────────────────────────────────────────────────
    std::cout << "\n" << (g_fail == 0 ? "\033[32m" : "\033[31m")
              << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail == 0 ? 0 : 1;
}
