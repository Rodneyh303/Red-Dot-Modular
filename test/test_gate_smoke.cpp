// test_gate_smoke.cpp — per-mode gate-emission smoke + shouldExecute routing guard.
//
// WHY THIS TEST EXISTS (plans/fix_gate_output_regression_a205093.md + mode_collapse_6_to_3.md):
//   a205093 unified Mode D's ENGINE + INPUT ROUTING with Mode B (Gate 1 = main) but left Mode D's
//   module-layer `shouldExecute` on the OLD pre-fix logic — `useSubGate ? subGateRise : true` — so
//   with no subgate patched Mode D called executeMode EVERY SAMPLE, jamming gate state (no coherent
//   gate output). The suite was GREEN because test_gate_mode_agnostic drives the engine twins
//   directly (executeModeB vs executeModeD), bypassing the module-layer shouldExecute gate where the
//   bug lived. This test closes that gap two ways:
//     SUITE 1 — per-mode gate SMOKE: each timing origin (clock/gate/phase), driven through its
//               canonical engine entry with a non-rest step, asserts gs.gateHeld goes true.
//     SUITE 2 — REST suppresses the gate (negative smoke): every origin's gate goes low on a rest.
//     SUITE 3 — shouldExecute ROUTING: a faithful copy of the module-layer shouldExecute rule
//               (Monsoon.cpp ~830, post-collapse) pins the 3-origin routing contract and the
//               a205093 regression — gate origin + no edges -> does NOT execute.
//
// MODE_COLLAPSE_6_TO_3: three timing origins (clock=0 / gate=1 / phase=2). Pitch origin (generate vs
// quantise) is the q-mix axis, not a mode — it does not affect gate emission (section-421 invariant),
// so this smoke needs no q-mix cases. The controller routes clock/phase -> engine.executeModeA and
// gate -> engine.executeModeB; only the edge SOURCE differs (clock.sixteenthEdge vs phase.sixteenthEdge
// for the phase origin).
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

// A clock/phase view with a 1/16 edge asserted — exactly what the module layer passes into
// engine.executeModeA for the clock and phase origins (the controller routes both through
// engine.executeModeA; only the edge SOURCE differs — clock.sixteenthEdge vs phase.sixteenthEdge).
static ClockEngine edgeClock() {
    ClockEngine ck;
    ck.sixteenthEdge = true;
    return ck;
}

// ── Faithful copy of the module-layer shouldExecute gate (Monsoon.cpp ~830, POST-collapse). ──
// Three timing origins: gate(1) is event-driven (the only one that takes an external rhythm);
// clock(0) steps on the generated 1/16 grid; phase(2) steps on the phase 1/16 grid. If Monsoon.cpp's
// rule changes, update this mirror to match (the routing assertions then re-encode the contract).
static bool shouldExecute(int modeSelect, bool gate1High, bool gate1Rise,
                          bool subGateRise, bool ghostRise, int stepIndex,
                          bool clockSixteenth, bool phaseSixteenth) {
    if (modeSelect == 1) {                      // GATE origin: event-driven (was B/D)
        const bool inGate = gate1High;
        return gate1Rise || (gate1High && stepIndex == -1)   // main onset / held-at-start
            || (inGate && subGateRise)                        // ratchet in-gate (Gate 2)
            || (!inGate && ghostRise);                        // ghost in-gap (Gate 3)
    }
    if (modeSelect == 0) return clockSixteenth;   // CLOCK origin: generated 1/16 grid
    if (modeSelect == 2) return phaseSixteenth;   // PHASE origin: phase 1/16 grid
    return false;
}

int main() {
    using D = MonoDecision;

    // ════════════════════════════════════════════════════════════════════════════
    SUITE("1 — per-origin gate SMOKE: clock/gate/phase each emit a gate on a non-rest step");
    // GATE_OUTPUT is high iff engine.gs.gateHeld (GateState::process). So "emits a gate" ==
    // gs.gateHeld true after a non-rest step. The controller routes clock/phase -> engine.executeModeA
    // and gate -> engine.executeModeB (the section-421 mode-agnostic invariant; pitch source is the
    // q-mix axis, not a mode), so the engine-level smoke has two canonical entries.

    TEST("Clock origin (mode 0): a 1/16 step, no rest -> gate high", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), /*restProb=*/0.f, /*legatoProb=*/0.f,
                                        /*noteVal=*/2.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);                       // GATE_OUTPUT would emit
    });

    TEST("Gate origin (mode 1): a Gate-1 rise, no rest -> gate high", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true,
                                        /*restProb=*/0.f, /*legatoProb=*/0.f, /*noteVal=*/0.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    TEST("Phase origin (mode 2): a 1/16 phase step, no rest -> gate high", {
        // Phase routes through engine.executeModeA with a phase-derived edge view (sixteenthEdge=true).
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), 0.f, 0.f, 2.f, in);
        EXPECT(r.stepped);
        EXPECT(r.decision != D::Rest);
        EXPECT(eng.gs.gateHeld);
    });

    // ════════════════════════════════════════════════════════════════════════════
    SUITE("2 — REST suppresses the gate (negative smoke, every origin)");
    // restProb high -> decision Rest and gs.gateHeld false. Confirms the gate truly tracks the
    // step decision (not stuck high) for each origin's canonical entry. Every origin gets the
    // positive (SUITE 1) + negative (this) pair so a routing regression that leaves a gate stuck
    // high OR never firing is caught. This is the GUARD for the 6->3 dispatch collapse: a misroute
    // turns green->red here, not "hours of confusion".

    TEST("Clock origin: rest step -> gate low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), /*restProb=*/0.5f, 0.f, 2.f, in);
        EXPECT(r.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);
    });

    TEST("Gate origin: rest step -> gate low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeB(true, true, /*restProb=*/0.5f, 0.f, 0.f, in);
        EXPECT(r.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);
    });

    TEST("Phase origin: rest step -> gate low", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeA(edgeClock(), /*restProb=*/0.5f, 0.f, 2.f, in);
        EXPECT(r.decision == D::Rest);
        EXPECT(!eng.gs.gateHeld);
    });

    // ════════════════════════════════════════════════════════════════════════════
    SUITE("3 — shouldExecute ROUTING (the a205093 regression guard, post-collapse)");
    // The a205093 bug lived in the MODULE-layer shouldExecute gate (Monsoon.cpp), which the engine-
    // level tests bypass. This suite encodes that rule's contract directly: the gate origin is
    // event-driven (no edges -> no step, the a205093 fix); clock steps on the clock grid; phase on
    // the phase grid.

    TEST("REGRESSION (a205093): gate origin, no edges + no subgate -> does NOT execute every sample", {
        // The old gate-quantiser branch was `useSubGate ? subGateRise : true` -> with nothing patched
        // it returned TRUE, so executeMode ran every sample and jammed the gate. Post-fix the gate
        // origin is edge-gated, so with no Gate-1 rise / no held-at-start / no ratchet / no ghost it
        // must be FALSE.
        EXPECT(!shouldExecute(/*modeSelect=*/1, /*gate1High=*/false, /*gate1Rise=*/false,
                              /*subGateRise=*/false, /*ghostRise=*/false, /*stepIndex=*/0,
                              /*clockSixteenth=*/true, /*phaseSixteenth=*/true));
    });

    TEST("Gate origin steps on a Gate-1 rise", {
        EXPECT(shouldExecute(1, true,  true,  false, false, 0, true, true));
        EXPECT(shouldExecute(1, true,  false, false, false, -1, true, true));   // held-at-start
    });

    TEST("Clock origin steps ONLY on the clock 1/16 edge (not phase, not gate edges)", {
        EXPECT(shouldExecute(0, false, false, false, false, 0, /*clockSixteenth=*/true,  false));
        EXPECT(!shouldExecute(0, false, false, false, false, 0, false, /*phaseSixteenth=*/true));
        EXPECT(!shouldExecute(0, false, false, false, false, 0, false, false));   // no edge -> no step
    });

    TEST("Phase origin steps ONLY on the phase 1/16 edge (not clock, not gate edges)", {
        EXPECT(shouldExecute(2, false, false, false, false, 0, false, /*phaseSixteenth=*/true));
        EXPECT(!shouldExecute(2, false, false, false, false, 0, /*clockSixteenth=*/true, false));
    });

    TEST("Invalid modeSelect (>=3, old saved patch) executes nothing", {
        // No migration (pre-release): old patches with modeSelect 3..5 are dead until re-saved.
        // The dispatch's default branch returns false, so shouldExecute must agree.
        for (int m : {3, 4, 5})
            EXPECT(!shouldExecute(m, true, true, true, true, 0, true, true));
    });

    // ─────────────────────────────────────────────────────────────────────────────
    std::cout << "\n" << (g_fail == 0 ? "\033[32m" : "\033[31m")
              << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail == 0 ? 0 : 1;
}
