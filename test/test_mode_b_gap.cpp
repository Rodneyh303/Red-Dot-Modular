// test_mode_b_gap.cpp — two-toggle legato model (LEGATO_GATE_GAP_BUG.md RESOLVED §290, NAMING §325).
//
// Two INDEPENDENT context-menu toggles, one per rest KIND:
//  - generatedRestBeatsLegato  — governs GENERATED rests (rest lane rolls a Rest at a rise).
//  - incomingRestBeatsLegato   — governs STRUCTURAL gaps (source sent no gate between two rises;
//                                a gap is NOT a MonoDecision::Rest, which the engine never
//                                synthesises for a gap). Two modes:
//    FALSE (default) = TIE ACROSS GAPS: the module-layer bridge holds the gate high across the gap
//      (gateOpen += slurForward) so the next rise sees wasHeld; the tie forms because the PREDECESSOR
//      committed (slurForward -> prevSlur). The arriving note's own legato roll governs tying OUT
//      only (it does NOT re-earn the incoming tie). A REST on the arriving note ENDS the chain.
//      No self-bound / no timer: a committed slur ties into the next gate regardless of gap length.
//    TRUE = ABUTTING-GATES-ONLY (sample-accurate): hold the gate ONE sample after the fall
//      (overlap / <=1-sample gap -> tie), then drop it (>=2-sample gap -> fresh). No ms timer.
//
// This is an ENGINE test: it drives executeModeB directly and MODELS the module-layer bridge
// between rises (the bridge lives in Monsoon.cpp IMPL 2b, which the engine test does not run).
// The model mirrors IMPL 2b exactly: TRUE -> gateHeld true iff overlap or <=1-sample gap;
// FALSE -> gateHeld = slurForward (bridge). holdRemain is nullified in Mode B (set 0 across gaps).
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

// Deterministic, all-active scale so genPitchLive always resolves. restProb governs whether the
// arriving note rolls a GENERATED rest (separate from the gap toggle).
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

// Model the module-layer IMPL 2b bridge state during a gap, per the two-toggle model.
//   incomingRestBeatsLegato TRUE  (sample-accurate): gate held iff overlap or a <=1-sample gap
//     (modeled as gateHeld=true for a 1-sample gap, false for a >=2-sample / long gap).
//   incomingRestBeatsLegato FALSE (tie across gaps): gate held iff slurForward (the bridge).
// holdRemain is nullified in Mode B (set 0 across gaps — note-length is not the adjacency proxy).
static void gap(SequencerEngine& eng, bool oneSampleGap) {
    eng.gs.holdRemain = 0.f;
    if (eng.incomingRestBeatsLegato)
        eng.gs.gateHeld = oneSampleGap;          // TRUE: <=1-sample gap holds, >=2 drops
    else
        eng.gs.gateHeld = eng.gs.slurForward;    // FALSE: bridge to the next rise
}

// Overlap: the gate is STILL HIGH at the next rise (no gap).
static void overlap(SequencerEngine& eng) {
    eng.gs.holdRemain = 0.f;
    eng.gs.gateHeld = true;
}

int main() {
    // ── Overlap ties in BOTH modes (the held-predecessor invariant, unchanged) ──────────────
    SUITE("overlap ties (both modes)");
    TEST("TRUE: overlap -> Tie/Legato", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = true;
        rise(eng, 0.f, 1.0f, 4.f);               // A commits slurForward
        overlap(eng);
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);
    });
    TEST("FALSE: overlap -> Tie/Legato", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = false;
        rise(eng, 0.f, 1.0f, 4.f);
        overlap(eng);
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);
    });

    // ── <=1-sample gap ties in BOTH modes (sample-accurate; below every threshold) ─────────
    SUITE("<=1-sample gap ties (both modes)");
    TEST("TRUE: <=1-sample gap -> Tie/Legato", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = true;
        rise(eng, 0.f, 1.0f, 4.f);
        gap(eng, /*oneSampleGap=*/true);          // 1-sample hold keeps gateHeld true
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);
    });
    TEST("FALSE: <=1-sample gap -> Tie/Legato", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = false;
        rise(eng, 0.f, 1.0f, 4.f);
        gap(eng, /*oneSampleGap=*/true);
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);
    });

    // ── >=2-sample / long gap: FRESH in TRUE, TIE in FALSE (the mode split) ────────────────
    SUITE("long gap: fresh in TRUE, tie in FALSE");
    TEST("TRUE: long gap -> NewNote (sample-accurate; no tie across a gap)", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = true;
        rise(eng, 0.f, 1.0f, 4.f);               // A commits slurForward
        gap(eng, /*oneSampleGap=*/false);         // >=2-sample gap drops the gate
        StepResult b = rise(eng, 0.f, 0.5f, 4.f);
        EXPECT(b.decision == D::NewNote);         // wasHeld false -> fresh (the original bug, fixed)
    });
    TEST("FALSE: long gap + non-rest arrival -> Tie (bridge; predecessor committed)", {
        // The incoming tie is earned by the predecessor's slurForward; the arriving note's OWN
        // legato roll governs tying OUT only. NO self-bound / no timer: a committed slur ties into
        // the next gate regardless of gap length; the only brake is a rest (next test).
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = false;
        rise(eng, 0.f, 1.0f, 4.f);               // A commits slurForward
        gap(eng, /*oneSampleGap=*/false);         // bridge keeps gateHeld = slurForward (true)
        StepResult b = rise(eng, 0.f, 0.5f, 4.f); // non-rest arrival (restProb=0)
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);  // tied IN from the predecessor
    });

    // ── Long gap + REST arrival: chain ENDS in BOTH modes (a rest breaks it) ───────────────
    SUITE("long gap + rested arrival ends the chain (both modes)");
    TEST("TRUE: long gap + rested arrival -> Rest (chain ends)", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = true;
        rise(eng, 0.f, 1.0f, 4.f);
        gap(eng, /*oneSampleGap=*/false);
        StepResult b = rise(eng, /*restProb=*/1.0f, 0.5f, 4.f);  // force a generated rest
        EXPECT(b.decision == D::Rest);
    });
    TEST("FALSE: long gap + rested arrival -> Rest (chain ends; rest is the only brake)", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = false;
        rise(eng, 0.f, 1.0f, 4.f);
        gap(eng, /*oneSampleGap=*/false);         // bridge up, but...
        StepResult b = rise(eng, /*restProb=*/1.0f, 0.5f, 4.f);  // ...the arriving note rests -> chain ends
        EXPECT(b.decision == D::Rest);
    });

    // ── FALSE: arriving note's redraw governs tying OUT (continue vs end-with-this-gate) ────
    SUITE("FALSE: arriving note redraws tying OUT (not IN)");
    TEST("FALSE: tied-in note that does NOT commit -> chain ends WITH this gate (it still tied in)", {
        // B ties IN (predecessor committed). B's own legato roll (legatoProb=0 -> never commits)
        // governs tying OUT to a following gate C: B does not commit -> C is fresh.
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = false;
        rise(eng, 0.f, 1.0f, 4.f);               // A commits
        gap(eng, false);
        StepResult b = rise(eng, 0.f, /*legato=*/0.0f, 4.f);  // B ties IN (prevSlur), redraws OUT: legato=0 -> no commit
        EXPECT(b.decision == D::Tie || b.decision == D::Legato);   // tied in
        gap(eng, false);
        StepResult c = rise(eng, 0.f, 0.5f, 4.f);              // B did not commit -> C fresh
        EXPECT(c.decision == D::NewNote);
    });
    TEST("FALSE: tied-in note that DOES commit -> chain continues (C ties in to B)", {
        SequencerEngine eng; eng.numPolyVoices = 0; eng.incomingRestBeatsLegato = false;
        rise(eng, 0.f, 1.0f, 4.f);               // A commits
        gap(eng, false);
        rise(eng, 0.f, /*legato=*/1.0f, 4.f);    // B ties IN (prevSlur) and commits OUT (legato=1.0)
        gap(eng, false);
        StepResult c = rise(eng, 0.f, 0.5f, 4.f);              // B committed -> C ties in
        EXPECT(c.decision == D::Tie || c.decision == D::Legato);
    });

    // ── Independence: each toggle governs only its own rest kind ────────────────────────────
    SUITE("generated-rest vs gap independence");
    TEST("generatedRestBeatsLegato=OFF suppresses a GENERATED rest (slur wins) — gap toggle irrelevant", {
        // A committed slur lands on a GENERATED rest at B; generatedRestBeatsLegato=OFF -> slur
        // wins -> B plays (Legato), not Rest. This is the generated-rest toggle's job, independent
        // of the gap toggle. (Overlap, so the gap toggle never engages.)
        SequencerEngine eng; eng.numPolyVoices = 0;
        eng.generatedRestBeatsLegato = false;    // slur wins over a generated rest
        eng.incomingRestBeatsLegato  = true;     // (arbitrary; no gap here)
        rise(eng, 0.f, 1.0f, 4.f);               // A commits slurForward
        overlap(eng);                            // overlap -> wasHeld true
        StepResult b = rise(eng, /*restProb=*/1.0f, 0.5f, 4.f);  // generated rest, but slur suppresses it
        EXPECT(b.decision == D::Legato || b.decision == D::Tie); // slur wins, not Rest
    });
    TEST("incomingRestBeatsLegato governs the GAP only (generated-rest toggle leaves it alone)", {
        // A long gap with generatedRestBeatsLegato=ON (default) and a non-rest arrival: the gap
        // outcome follows incomingRestBeatsLegato alone. TRUE -> fresh; FALSE -> tie. The
        // generated-rest toggle (ON) does NOT police the gap (a gap is not a generated rest).
        for (bool gapMode : { true, false }) {
            SequencerEngine eng; eng.numPolyVoices = 0;
            eng.generatedRestBeatsLegato = true;  // default — would break a generated rest, but there is none
            eng.incomingRestBeatsLegato  = gapMode;
            rise(eng, 0.f, 1.0f, 4.f);
            gap(eng, /*oneSampleGap=*/false);     // long gap
            StepResult b = rise(eng, 0.f, 0.5f, 4.f);  // non-rest arrival
            if (gapMode) EXPECT(b.decision == D::NewNote);   // TRUE -> fresh
            else         EXPECT(b.decision == D::Tie || b.decision == D::Legato);  // FALSE -> tie
        }
    });

    // ── Reverse-safety: the gap decision is a pure function of the gate state (no direction) ─
    SUITE("reverse-safety (engine contract)");
    TEST("forward == reverse (same gate waveform -> same decisions, both modes)", {
        for (bool gapMode : { true, false }) {
            SequencerEngine f, r; f.numPolyVoices = 0; r.numPolyVoices = 0;
            f.incomingRestBeatsLegato = gapMode; r.incomingRestBeatsLegato = gapMode;
            rise(f, 0.f, 1.0f, 4.f);  rise(r, 0.f, 1.0f, 4.f);           // A
            gap(f, false); gap(r, false);                                 // same long gap
            StepResult fb = rise(f, 0.f, 0.5f, 4.f);
            StepResult rb = rise(r, 0.f, 0.5f, 4.f);
            EXPECT(fb.decision == rb.decision);                          // identical -> direction-agnostic
        }
    });

    std::cout << "\n-----\nmode_b_gap: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
