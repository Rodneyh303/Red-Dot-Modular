// test_subgate.cpp — GATE_SUBDIVISION_STEP_GATE.md executable spec test.
//
// subGate subdivision (GATE mode / Mode B).  Three edge streams advance the playhead and each runs
// executeStep (rest/legato/accent + all pitch lanes draw, Tie emergent from pitch equality):
//   - main gate rise   — a main-gate onset (note event begin); main always wins.
//   - ratchet (Gate 2) — a fine-grid edge INSIDE a main gate -> sub-cell (ratchet/tie/rest).
//   - ghost  (Gate 3)  — a fine-grid edge OUTSIDE main gates (in a gap) -> a GHOST note.
// Ghost + main are symmetric: both are external gates whose level drives note width (the IMPL 2b
// mirror reads mainHigh || ghostHigh).  Legato flows both ways across the ghost<->main boundary via
// the leading-edge slurForward model.  A ghost/ratchet onset is a CANDIDATE note — executeStep rolls
// the rest lane first, so restProb may still silence it.
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

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;
    return in;
}

// Drive one edge through the real engine, then mirror the module-layer IMPL 2b gate driver
// (runs every sample between edges): gateOpen = !isRest && (mainHigh || ghostHigh || slurForward).
// Ghost + main are symmetric — whichever external gate is high bounds the note width.
static StepResult step(SequencerEngine& eng, bool mainRise, bool ratchetRise, bool ghostRise,
                       bool mainHigh, bool ghostHigh,
                       float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, ratchetRise,
                                              restProb, legatoProb, noteVal, in,
                                              ghostRise, ghostHigh);
    const bool isRest = (r.decision == MonoDecision::Rest);
    const bool gateOpen = !isRest && (mainHigh || ghostHigh || eng.gs.slurForward);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = !isRest && (mainHigh || ghostHigh);
    return r;
}

// Poly variant: also runs executePolyVoices (the module layer's postExecute_ does this after every
// stepped executeModeB*).  At a mono NewNote onset, monoGateStart=true -> each poly voice
// independently rolls its own rest/legato/pitch (the correlated + reversible payoff).
static StepResult stepPoly(SequencerEngine& eng, bool mainRise, bool ratchetRise, bool ghostRise,
                            bool mainHigh, bool ghostHigh,
                            float restProb, float legatoProb, float noteVal) {
    const PatternInput in = makeInput();
    StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, ratchetRise,
                                              restProb, legatoProb, noteVal, in,
                                              ghostRise, ghostHigh);
    if (r.stepped && eng.numPolyVoices > 0)
        eng.executePolyVoices(in);
    const bool isRest = (r.decision == MonoDecision::Rest);
    const bool gateOpen = !isRest && (mainHigh || ghostHigh || eng.gs.slurForward);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = !isRest && (mainHigh || ghostHigh);
    return r;
}

int main() {
    using D = MonoDecision;

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("1 — ratchet (in-gate): N sub-cells = N decisions, playhead advances");
    TEST("a main gate spanning 3 ratchet cells produces 3 stepped decisions (ratchet, legato=0)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        StepResult r0 = step(eng, /*mainRise=*/true,  /*ratchet=*/true,  /*ghost=*/false, /*mainHigh=*/true,  /*ghostHigh=*/false, 0.f, 0.f, 2.f);
        StepResult r1 = step(eng, /*mainRise=*/false, /*ratchet=*/true,  /*ghost=*/false, /*mainHigh=*/true,  /*ghostHigh=*/false, 0.f, 0.f, 2.f);
        StepResult r2 = step(eng, /*mainRise=*/false, /*ratchet=*/true,  /*ghost=*/false, /*mainHigh=*/true,  /*ghostHigh=*/false, 0.f, 0.f, 2.f);
        EXPECT(r0.stepped && r1.stepped && r2.stepped);
        EXPECT(r0.decision != D::MidNote && r1.decision != D::MidNote && r2.decision != D::MidNote);
        EXPECT_EQ(eng.stepIndex, 2);
    });

    TEST("no edge -> no step (a silent gap with nothing patched advances nothing)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true, true, false, true, false, 0.f, 0.f, 2.f);   // onset, step 0
        StepResult r = step(eng, false, false, false, false, false, 0.f, 0.f, 2.f);  // no edge
        EXPECT(!r.stepped);
        EXPECT_EQ(eng.stepIndex, 0);   // playhead did not advance
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("2 — intra-gate legato: tie vs ratchet across sub-cells within one main gate");
    TEST("legato high -> second sub-cell ties/legatos (holds across the sub-cell boundary)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true, true, false, true, false, 0.f, 0.5f, 4.f);                 // onset commits slurForward
        StepResult r = step(eng, false, true, false, true, false, 0.f, 0.5f, 4.f); // intra-gate boundary
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);
    });

    TEST("legato low -> every sub-cell re-articulates (a ratchet: NewNote each onset)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
        StepResult r = step(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
        EXPECT(r.decision == D::NewNote);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("3 — inter-gate slur: a slur across a main-gate boundary holds (the TRAP)");
    TEST("slur across a main-gate boundary holds (Tie/Legato, not NewNote)", {
        // note 1 (onset + intra tie) -> silent gap (no edge) -> note 2 (fresh main rise).
        // The IMPL 2b bridges the silent gap with slurForward; note 2 connects.
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  true,  false, true,  false, 0.f, 0.5f, 4.f);  // note 1 onset, commits slurForward
        step(eng, false, true,  false, true,  false, 0.f, 0.5f, 4.f);  // note 1 intra-gate tie
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap (no edge) — slurForward persists via IMPL 2b
        StepResult r = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // note 2: fresh main rise
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);
        EXPECT(r.decision != D::NewNote);
    });

    TEST("a multi-sub-cell slur chain (legato high throughout) bridges the gap to the next note", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  true,  false, true,  false, 0.f, 0.5f, 4.f);  // onset commits slurForward
        step(eng, false, true,  false, true,  false, 0.f, 0.5f, 4.f);  // sub-cell 2: ties, re-commits
        step(eng, false, true,  false, true,  false, 0.f, 0.5f, 4.f);  // sub-cell 3: ties, re-commits
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap (slurForward persists)
        StepResult inter = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // note 2
        EXPECT(inter.decision == D::Tie || inter.decision == D::Legato);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("4 — no edge -> no step (unpatched / silent gap)");
    TEST("no edge -> stepped=false", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeBSubdivided(false, false, false, 0.f, 0.f, 2.f, in);
        EXPECT(!r.stepped);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // GHOST (G3): ghost notes fill the gaps.  Ghost + main are symmetric (level drives width);
    // legato flows both ways; a ghost onset is a candidate that rest may silence.
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G1 — ghost onset in a gap shapes a note (rest/legato/pitch), playhead advances");
    TEST("a ghost rise in a gap (rest=0) plays a note; gate follows the ghost gate width", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true, false, false, true, false, 0.f, 0.f, 4.f);    // main onset
        step(eng, false, false, false, false, false, 0.f, 0.f, 4.f);  // silent gap (no edge)
        StepResult r = step(eng, false, false, true, false, true, 0.f, 0.f, 4.f);  // ghost onset in gap
        EXPECT(r.stepped);
        EXPECT(r.decision != D::MidNote);
        EXPECT(eng.gs.gateHeld);   // ghost note sounding (ghostHigh bounds the width)
    });

    TEST("a ghost onset is a CANDIDATE — rest high silences it (the gap stays empty for that cell)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true, false, false, true, false, 0.f, 0.f, 4.f);    // main onset
        step(eng, false, false, false, false, false, 0.f, 0.f, 4.f);  // silent gap
        StepResult r = step(eng, false, false, true, false, true, 0.5f, 0.f, 4.f);  // ghost onset, rest high
        EXPECT(r.stepped);
        EXPECT(r.decision == D::Rest);   // candidate ghost note rested
        EXPECT(!eng.gs.gateHeld);        // silent
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G2 — legato flows BOTH ways across the ghost<->main boundary");
    TEST("ghost -> main: a ghost note high at a main-gate rise legatos into the main note", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main note 1, commits slurForward
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap
        step(eng, false, false, true,  false, true,  0.f, 0.5f, 4.f);  // ghost note (commits slurForward)
        StepResult r = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main note 2 arrives mid-ghost
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);  // ghost legatos into main
    });

    TEST("main -> ghost: a main note legatos into the first ghost cell of the following gap", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main note, commits slurForward
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap (slurForward persists via IMPL 2b)
        StepResult r = step(eng, false, false, true,  false, true,  0.f, 0.5f, 4.f);  // first ghost cell
        EXPECT(r.decision == D::Tie || r.decision == D::Legato);  // main legatos into ghost
    });

    TEST("TRAP: ghost->main AND main->ghost slurs in one pattern; no forced re-articulation", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main A, commits slurForward
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap
        StepResult g1 = step(eng, false, false, true,  false, true,  0.f, 0.5f, 4.f);  // ghost (main->ghost slur)
        EXPECT(g1.decision == D::Tie || g1.decision == D::Legato);
        StepResult m2 = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main B (ghost->main slur)
        EXPECT(m2.decision == D::Tie || m2.decision == D::Legato);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G3 — region select: ratchet drives in-gate, ghost drives in-gap");
    TEST("ratchet in-gate + ghost in-gap: each drives its own region (both patched)", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  true,  false, true,  false, 0.f, 0.f, 4.f);  // main onset + ratchet sub-cell
        StepResult rac = step(eng, false, true,  false, true,  false, 0.f, 0.f, 4.f);  // ratchet sub-cell (in-gate)
        EXPECT(rac.stepped && rac.decision == D::NewNote);  // ratchet re-articulates in-gate
        step(eng, false, false, false, false, false, 0.f, 0.f, 4.f);  // silent gap edge? no — no edge
        StepResult gh = step(eng, false, false, true,  false, true,  0.f, 0.f, 4.f);  // ghost in gap
        EXPECT(gh.stepped && gh.decision != D::MidNote);  // ghost drives the gap
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // POLY (P1): per-voice subdivision — the correlated + reversible payoff.
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("P1 — per-voice subdivision: poly voices independently rest/play at onsets");
    TEST("at a mono onset, voice 0 (restProb=0) plays, voice 1 (restProb=1) rests", {
        SequencerEngine eng; eng.numPolyVoices = 2;
        eng.voices[0].restProb = 0.f; eng.voices[1].restProb = 1.f;
        stepPoly(eng, true, true, false, true, false, 0.f, 0.f, 4.f);  // mono onset
        EXPECT(eng.voices[0].gs.gateHeld);
        EXPECT(!eng.voices[1].gs.gateHeld);
    });

    TEST("a ratcheted sub-cell (mono NewNote, legato=0) re-articulates the playing voice", {
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[0].restProb = 0.f;
        stepPoly(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
        StepResult r = stepPoly(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
        EXPECT(r.decision == D::NewNote);
        EXPECT(eng.voices[0].gs.gateHeld);
    });

    TEST("a ghost onset in a gap drives per-voice rolls (poly ghost notes)", {
        SequencerEngine eng; eng.numPolyVoices = 2;
        eng.voices[0].restProb = 0.f; eng.voices[1].restProb = 1.f;
        stepPoly(eng, true, false, false, true, false, 0.f, 0.f, 4.f);   // main onset
        stepPoly(eng, false, false, false, false, false, 0.f, 0.f, 4.f); // silent gap
        stepPoly(eng, false, false, true,  false, true,  0.f, 0.f, 4.f); // ghost onset in gap
        EXPECT(eng.voices[0].gs.gateHeld);    // voice 0 plays the ghost
        EXPECT(!eng.voices[1].gs.gateHeld);   // voice 1 rests
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // POLY (P2): the un-fused STEP output re-triggers per cell (Straits/Changi emit at fine grid).
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("P2 — poly STEP output (un-fused) re-triggers per cell");
    TEST("at a mono onset, the playing voice's gsStep is high (re-struck)", {
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[0].restProb = 0.f;
        stepPoly(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
        EXPECT(eng.voices[0].gsStep.gateHeld);
    });

    TEST("a ghost onset re-strikes gsStep (ghost cells pulse the step output)", {
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[0].restProb = 0.f;
        stepPoly(eng, true, false, false, true, false, 0.f, 0.f, 4.f);
        stepPoly(eng, false, false, false, false, false, 0.f, 0.f, 4.f);
        stepPoly(eng, false, false, true,  false, true,  0.f, 0.f, 4.f);  // ghost onset
        EXPECT(eng.voices[0].gsStep.gateHeld);  // re-struck at the ghost cell
    });

    std::cout << "\n-----\nsubgate: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
