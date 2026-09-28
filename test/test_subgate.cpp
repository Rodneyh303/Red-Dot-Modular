// test_subgate.cpp — GATE_SUBDIVISION_STEP_GATE.md §"Test" executable spec test.
//
// subGate (Gate 3) is a fine-grid clock; the main gate (Gate 1 in Mode B, Gate 2 in Mode D) is
// the note-event stream.  At each subGate onset the playhead advances; where the main gate is
// HIGH, executeStep shapes the sub-cell (rest/legato/accent + all pitch lanes draw, Tie emergent
// from pitch equality).  Where the main gate is LOW (gap), the playhead still advances but no
// note shapes (forced Rest).  Two tie scopes both preserved: intra-gate (tie across subGate cells
// within one main gate) and inter-gate (slur across a main-gate boundary).
//
// SUITES:
//   1 — subGate advances the playhead; a main gate spanning N cells = N sub-cell decisions.
//   2 — intra-gate legato: legato high -> Tie/Legato across sub-cells within a gate (the
//       subdivision hold).  legato low -> NewNote each sub-cell (a ratchet).
//   3 — gap (main gate low) -> Rest; playhead still advances.
//   4 — THE TRAP: inter-gate legato HOLDS across a main-gate boundary AND intra-gate
//       re-articulates at a subGate boundary, IN THE SAME PATTERN.  This catches subGate
//       stomping inter-gate legato (the spec's headline regression).
//   5 — subGateRise=false -> no step (the caller uses executeModeB when subGate is unpatched).
//
// Build (needs the engine TUs — see test/run_all.sh, entry "test_subgate|$SE $GS $PE"):
//   g++ -std=c++17 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers \
//       test/test_subgate.cpp \
//       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp \
//       src/dsp/gates/GateState.cpp -o /tmp/tsg && /tmp/tsg

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

// Drive one subGate onset through the real engine, then simulate the module-layer IMPL 2b gate
// driver (runs every sample between subGate onsets): gateOpen = !isRest && (mainGateHigh ||
// slurForward).  This is the faithful mirror of Monsoon.cpp's Mode-B gate-state driver, so the
// engine sees the same gs.gateHeld it would in Rack.
static StepResult subStep(SequencerEngine& eng, bool mainGateRise, bool mainGateHigh,
                          float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    StepResult r = eng.executeModeBSubdivided(mainGateRise, mainGateHigh, /*subGateRise=*/true,
                                              restProb, legatoProb, noteVal, in);
    // IMPL 2b gate-state driver (mirror of Monsoon.cpp:868-884):
    const bool isRest = (r.decision == MonoDecision::Rest);
    const bool gateOpen = !isRest && (mainGateHigh || eng.gs.slurForward);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = !isRest && mainGateHigh;
    return r;
}

// Poly variant: also runs executePolyVoices (the module layer's postExecute_ does this after
// every stepped executeModeB*).  This is what makes per-voice subdivision flow at subGate onsets:
// at a mono NewNote sub-cell, monoGateStart=true -> each poly voice independently rolls its own
// rest/legato/pitch (the correlated + reversible payoff — WHICH voices split is seeded per-voice).
static StepResult subStepPoly(SequencerEngine& eng, bool mainGateRise, bool mainGateHigh,
                               float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    StepResult r = eng.executeModeBSubdivided(mainGateRise, mainGateHigh, /*subGateRise=*/true,
                                              restProb, legatoProb, noteVal, in);
    if (r.stepped && eng.numPolyVoices > 0)
        eng.executePolyVoices(in);   // mirror postExecute_
    // IMPL 2b mono gate driver (mirror).
    const bool isRest = (r.decision == MonoDecision::Rest);
    const bool gateOpen = !isRest && (mainGateHigh || eng.gs.slurForward);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = !isRest && mainGateHigh;
    return r;
}

int main() {
    using D = MonoDecision;

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("1 — subGate advances the playhead; N cells = N decisions");
    TEST("a main gate spanning 3 subGate cells produces 3 stepped decisions (ratchet, legato=0)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        StepResult r0 = subStep(eng, /*rise=*/true,  /*high=*/true,  0.f, 0.f, 2.f);  // onset
        StepResult r1 = subStep(eng, /*rise=*/false, /*high=*/true,  0.f, 0.f, 2.f);  // sub-cell
        StepResult r2 = subStep(eng, /*rise=*/false, /*high=*/true,  0.f, 0.f, 2.f);  // sub-cell
        EXPECT(r0.stepped && r1.stepped && r2.stepped);
        EXPECT(r0.decision != D::MidNote);              // each sub-cell is a fresh decision
        EXPECT(r1.decision != D::MidNote);
        EXPECT(r2.decision != D::MidNote);
        EXPECT_EQ(eng.stepIndex, 2);                    // playhead advanced 3 steps (0,1,2)
    });

    TEST("subGateRise with main gate LOW still advances the playhead (the clock runs in a gap)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true,  true,  0.f, 0.f, 2.f);   // step 0 (onset)
        StepResult r = subStep(eng, false, false, 0.f, 0.f, 2.f);   // gap sub-cell
        EXPECT(r.stepped);
        EXPECT_EQ(eng.stepIndex, 1);                   // playhead still advanced (subGate is the clock)
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("2 — intra-gate legato: tie across sub-cells within one main gate");
    TEST("legato high -> second sub-cell ties/legatos (holds across the sub-cell boundary)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true, true, 0.f, 0.5f, 4.f);                 // onset commits slurForward
        StepResult r = subStep(eng, false, true, 0.f, 0.5f, 4.f); // intra-gate boundary
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);  // holds, not a fresh NewNote
    });

    TEST("legato low -> every sub-cell re-articulates (a ratchet: NewNote each onset)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true, true, 0.f, 0.f, 4.f);                  // onset
        StepResult r = subStep(eng, false, true, 0.f, 0.f, 4.f);  // intra-gate boundary
        EXPECT(r.decision == D::NewNote);                         // re-articulated, not tied
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("3 — gap (main gate low): playhead advances, gate silent unless a slur bridges");
    TEST("a gap with NO committed slur -> gate silent (legato=0, no bridge)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true, true, 0.f, 0.f, 4.f);    // onset (legato=0 -> no slurForward)
        StepResult r = subStep(eng, false, false, 0.f, 0.f, 4.f);  // gap
        EXPECT(r.stepped);
        EXPECT_EQ(eng.stepIndex, 1);                 // playhead advanced
        EXPECT(!eng.gs.gateHeld);                    // silent — no slur bridge
    });
    TEST("a gap with a committed slur -> gate bridges (slurForward persists across the gap)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true, true, 0.f, 0.5f, 4.f);   // onset commits slurForward
        StepResult r = subStep(eng, false, false, 0.f, 0.5f, 4.f);  // gap
        EXPECT(r.stepped);
        EXPECT(eng.gs.gateHeld);                     // slurForward bridges the gap
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("4 — THE TRAP: inter-gate legato HOLDS across a main-gate boundary (subGate must not stomp it)");
    // The headline regression test (spec §"TWO tie scopes" TRAP).  In ONE pattern:
    //   note 1 (onset + an intra-gate tie) -> gap -> note 2 (a fresh main-gate rise).
    // Note 2's onset must connect (Tie/Legato) via the slurForward bridge — subGate must NOT
    // force a re-articulation at the main-gate edge just because it is also a subGate boundary.
    TEST("slur across a main-gate boundary holds under subGate (Tie/Legato, not NewNote)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true,  true,  0.f, 0.5f, 4.f);  // note 1 onset, commits slurForward
        subStep(eng, false, true,  0.f, 0.5f, 4.f);  // note 1 intra-gate tie
        subStep(eng, false, false, 0.f, 0.5f, 4.f);  // gap (slurForward persists)
        StepResult r = subStep(eng, true,  true,  0.f, 0.5f, 4.f);  // note 2: fresh main-gate rise
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);    // slur held across the boundary
        EXPECT(r.decision != D::NewNote);                           // NOT forced to re-articulate
    });

    TEST("a multi-sub-cell slur chain (legato high throughout) bridges the gap to the next note", {
        // The leading-edge model re-commits slurForward at EACH step from that step's own legato
        // roll (line ~617: gs.slurForward = leadsSlur).  So a slur chain requires every sub-cell
        // to roll legato (re-commit) — a single legato=0 sub-cell clears slurForward and breaks
        // the chain.  This test: onset + 2 tied sub-cells (all legato=0.5) + gap + note 2 — the
        // chain holds across the gap because every step re-committed.
        SequencerEngine eng; eng.numPolyVoices = 0;
        subStep(eng, true,  true,  0.f, 0.5f, 4.f);  // onset commits slurForward
        subStep(eng, false, true,  0.f, 0.5f, 4.f);  // sub-cell 2: ties, re-commits
        subStep(eng, false, true,  0.f, 0.5f, 4.f);  // sub-cell 3: ties, re-commits
        subStep(eng, false, false, 0.f, 0.5f, 4.f);  // gap (slurForward persists)
        StepResult inter = subStep(eng, true,  true,  0.f, 0.5f, 4.f);  // note 2: inter-gate boundary
        EXPECT(inter.decision == D::Tie || inter.decision == D::Legato);  // slur held across boundary
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("5 — subGateRise=false -> no step (unpatched path uses executeModeB)");
    TEST("no subGate edge -> stepped=false (the caller falls back to executeModeB)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeBSubdivided(true, true, /*subGateRise=*/false,
                                                  0.f, 0.f, 2.f, in);
        EXPECT(!r.stepped);                          // no subGate edge -> no step
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // POLY (P1): per-voice subdivision — the correlated + reversible payoff.
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("P1 — per-voice subdivision: poly voices independently rest/play at subGate onsets");
    TEST("at a mono onset, voice 0 (restProb=0) plays, voice 1 (restProb=1) rests", {
        SequencerEngine eng; eng.numPolyVoices = 2;
        eng.voices[0].restProb = 0.f;   // never rests -> always plays
        eng.voices[1].restProb = 1.f;  // always rests -> silent
        subStepPoly(eng, /*rise=*/true, /*high=*/true, 0.f, 0.f, 4.f);  // mono onset (NewNote)
        EXPECT(eng.voices[0].gs.gateHeld);    // voice 0 plays
        EXPECT(!eng.voices[1].gs.gateHeld);   // voice 1 rests
    });

    TEST("a ratcheted sub-cell (mono NewNote, legato=0) re-articulates the playing voice", {
        // legato=0 -> mono re-articulates every sub-cell (ratchet).  Each NewNote sub-cell fires
        // monoGateStart -> the playing voice re-rolls + re-triggers (a ratchet hit per sub-cell).
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[0].restProb = 0.f;   // always plays
        subStepPoly(eng, true,  true,  0.f, 0.f, 4.f);  // onset
        StepResult r = subStepPoly(eng, false, true, 0.f, 0.f, 4.f);  // ratchet sub-cell
        EXPECT(r.decision == D::NewNote);              // mono re-articulated
        EXPECT(eng.voices[0].gs.gateHeld);             // voice still sounding (re-triggered)
    });

    TEST("a tied sub-cell (mono Tie, legato high) extends the playing voice's hold", {
        // legato=0.5 -> mono ties across sub-cells.  monoGateStart=false at a Tie -> the playing
        // voice extends its hold (no re-trigger) — the subdivision sustain.
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[0].restProb = 0.f;
        subStepPoly(eng, true,  true,  0.f, 0.5f, 4.f);  // onset (commits slurForward)
        StepResult r = subStepPoly(eng, false, true, 0.f, 0.5f, 4.f);  // tied sub-cell
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);  // mono sustained
        EXPECT(eng.voices[0].gs.gateHeld);             // voice still sounding (held, not re-triggered)
    });

    std::cout << "\n-----\nsubgate: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
