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
                       float restProb, float legatoProb, float noteVal, float variation = 0.5f,
                       bool subgatesActive = true) {
    PatternInput in = makeInput();
    in.variationAmount = variation;
    in.legato = legatoProb;
    StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, ratchetRise,
                                              restProb, noteVal, in,
                                              ghostRise, ghostHigh);
    const bool isRest = (r.decision == MonoDecision::Rest);
    // Ghost only sounds when a candidate actually fired (engine.ghostActive) AND the ghost gate is
    // high — Gate 3 high alone is NOT enough (variation=0 -> no ghost candidate).
    const bool ghostSounding = eng.ghostActive && ghostHigh;
    // NO slurForward bridge: the gate closes on the fall, so a pause between gates does NOT hold
    // legato — legato only when gate edges join (one gate high when the next arrives, e.g. a ghost
    // high when a main gate rises -> ghostSounding keeps gateHeld true -> the main legatos in).
    (void)subgatesActive;
    const bool gateOpen = !isRest && (mainHigh || ghostSounding);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = !isRest && (mainHigh || ghostSounding);
    return r;
}

// Poly variant: also runs executePolyVoices (the module layer's postExecute_ does this after every
// stepped executeModeB*).  At a mono NewNote onset, monoGateStart=true -> each poly voice
// independently rolls its own rest/legato/pitch (the correlated + reversible payoff).
static StepResult stepPoly(SequencerEngine& eng, bool mainRise, bool ratchetRise, bool ghostRise,
                            bool mainHigh, bool ghostHigh,
                            float restProb, float legatoProb, float noteVal, float variation = 0.5f) {
    PatternInput in = makeInput();
    in.variationAmount = variation;
    in.legato = legatoProb;
    StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, ratchetRise,
                                              restProb, noteVal, in,
                                              ghostRise, ghostHigh);
    if (r.stepped && eng.numPolyVoices > 0)
        eng.executePolyVoices(in);
    const bool isRest = (r.decision == MonoDecision::Rest);
    const bool ghostSounding = eng.ghostActive && ghostHigh;
    // No slurForward bridge (legato only when gate edges join — see step()).
    const bool gateOpen = !isRest && (mainHigh || ghostSounding);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = !isRest && (mainHigh || ghostSounding);
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
    TEST("a SILENT gap between main gates does NOT legato (legato only when edges join)", {
        // note 1 (onset + intra tie) -> silent gap (no edge, gate low) -> note 2 (fresh main rise).
        // No slurForward bridge: the gate closes on the fall, so note 2 is a fresh NewNote (legato
        // only when gate edges join — a silent gap is not a join).
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  true,  false, true,  false, 0.f, 0.5f, 4.f);  // note 1 onset
        step(eng, false, true,  false, true,  false, 0.f, 0.5f, 4.f);  // note 1 intra-gate tie
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap (gate low)
        StepResult r = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // note 2: fresh main rise
        EXPECT(r.decision == D::NewNote);  // no slur across a silent gap
    });

    TEST("PLAIN gate mode (no subgates): a pause between main gates does NOT hold legato", {
        // No subgates -> no slurForward bridge.  Main gate 1 falls (pause), main gate 2 rises: the
        // gate closes on the fall, so gate 2 is a fresh NewNote (legato only when edges join).
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f, 0.5f, /*subgates=*/false);  // main 1 onset
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f, 0.5f, /*subgates=*/false);  // pause (gap)
        StepResult r = step(eng, true, false, false, true, false, 0.f, 0.5f, 4.f, 0.5f, /*subgates=*/false);  // main 2 rise
        EXPECT(r.decision == D::NewNote);  // fresh note — no slur across the pause
    });

    TEST("intra-gate sub-cells tie (contiguous), but a silent gap to the next note does NOT legato", {
        // Sub-cells WITHIN a gate are contiguous (gate high) -> they tie.  But a silent gap to the
        // next main gate is NOT a join -> note 2 is a fresh NewNote (no slurForward bridge).
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  true,  false, true,  false, 0.f, 0.5f, 4.f);  // onset
        StepResult intra = step(eng, false, true,  false, true,  false, 0.f, 0.5f, 4.f);  // sub-cell 2 (in-gate)
        EXPECT(intra.decision == D::Tie || intra.decision == D::Legato);  // contiguous -> ties
        step(eng, false, true,  false, true,  false, 0.f, 0.5f, 4.f);  // sub-cell 3 (in-gate)
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap (gate low)
        StepResult inter = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // note 2
        EXPECT(inter.decision == D::NewNote);  // silent gap -> fresh note, no slur
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("4 — no edge -> no step (unpatched / silent gap)");
    TEST("no edge -> stepped=false", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        const PatternInput in = makeInput();
        StepResult r = eng.executeModeBSubdivided(false, false, false, 0.f, 2.f, in);
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

    TEST("variation=0 -> NO ghost candidate (silent gap, even with rest=0 and Gate 3 high)", {
        // The ghost candidate is variation-gated: r_vary >= variationAmount -> no ghost.  At
        // variation=0 every r_vary (0..1) >= 0 -> no ghost ever fires, so the gaps stay empty
        // even with Gate 3 patched high.  (Ratchet + main are NOT variation-gated — only ghost.)
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true, false, false, true, false, 0.f, 0.f, 4.f);    // main onset
        step(eng, false, false, false, false, false, 0.f, 0.f, 4.f);  // silent gap
        StepResult r = step(eng, false, false, true, false, true, 0.f, 0.f, 4.f, /*variation=*/0.f);
        EXPECT(r.stepped);
        EXPECT(!eng.ghostActive);        // no ghost candidate fired
        EXPECT(!eng.gs.gateHeld);       // gate stays low (Gate 3 high alone doesn't open it)
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

    TEST("main -> ghost across a SILENT gap does NOT legato (the gate closed on the main's fall)", {
        // The main note falls (silent gap), then the ghost rises.  The gate closed on the fall ->
        // wasHeld=false at the ghost -> fresh NewNote.  (main->ghost legato only if the ghost rises
        // while the main is still high — but the ghost is in-gap, so this is a gap -> no slur.)
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main note
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap (gate low)
        StepResult r = step(eng, false, false, true,  false, true,  0.f, 0.5f, 4.f);  // first ghost cell
        EXPECT(r.decision == D::NewNote);  // no slur across a silent gap
    });

    TEST("ghost->main legatos (ghost high at main rise); main->ghost across a silent gap does not", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main A
        step(eng, false, false, false, false, false, 0.f, 0.5f, 4.f);  // silent gap
        StepResult g1 = step(eng, false, false, true,  false, true,  0.f, 0.5f, 4.f);  // ghost (gap -> fresh)
        EXPECT(g1.decision == D::NewNote);                              // main->ghost across silent gap: no slur
        StepResult m2 = step(eng, true,  false, false, true,  false, 0.f, 0.5f, 4.f);  // main B (ghost high -> join)
        EXPECT(m2.decision == D::Tie || m2.decision == D::Legato);    // ghost->main: ghost high -> legatos
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
        eng.voices[1].restProb = 0.f; eng.voices[2].restProb = 1.f;
        stepPoly(eng, true, true, false, true, false, 0.f, 0.f, 4.f);  // mono onset
        EXPECT(eng.voices[1].gs.gateHeld);
        EXPECT(!eng.voices[2].gs.gateHeld);
    });

    TEST("a ratcheted sub-cell (mono NewNote, legato=0) re-articulates the playing voice", {
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[1].restProb = 0.f;
        stepPoly(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
        StepResult r = stepPoly(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
        EXPECT(r.decision == D::NewNote);
        EXPECT(eng.voices[1].gs.gateHeld);
    });

    TEST("a ghost onset in a gap drives per-voice rolls (poly ghost notes)", {
        SequencerEngine eng; eng.numPolyVoices = 2;
        eng.voices[1].restProb = 0.f; eng.voices[2].restProb = 1.f;
        stepPoly(eng, true, false, false, true, false, 0.f, 0.f, 4.f);   // main onset
        stepPoly(eng, false, false, false, false, false, 0.f, 0.f, 4.f); // silent gap
        stepPoly(eng, false, false, true,  false, true,  0.f, 0.f, 4.f); // ghost onset in gap
        EXPECT(eng.voices[1].gs.gateHeld);    // voice 0 plays the ghost
        EXPECT(!eng.voices[2].gs.gateHeld);   // voice 1 rests
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // POLY (P2): the un-fused STEP output re-triggers per cell (Straits/Changi emit at fine grid).
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("P2 — poly STEP output (un-fused) re-triggers per cell");
    TEST("at a mono onset, the playing voice's gsStep is high (re-struck)", {
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[1].restProb = 0.f;
        stepPoly(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
        EXPECT(eng.voices[1].gsStep.gateHeld);
    });

    TEST("a ghost onset re-strikes gsStep (ghost cells pulse the step output)", {
        SequencerEngine eng; eng.numPolyVoices = 1;
        eng.voices[1].restProb = 0.f;
        stepPoly(eng, true, false, false, true, false, 0.f, 0.f, 4.f);
        stepPoly(eng, false, false, false, false, false, 0.f, 0.f, 4.f);
        stepPoly(eng, false, false, true,  false, true,  0.f, 0.f, 4.f);  // ghost onset
        EXPECT(eng.voices[1].gsStep.gateHeld);  // re-struck at the ghost cell
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // G4 — ghost + tie-across-gap interaction (GATE_SUBDIVISION_STEP_GATE.md §458):
    // a sounding ghost in the gap CONSUMES the pending gap-slur — the slur ties INTO the ghost
    // (the nearer onset), not the main gate. The ghost then redraws its own legato (commit ->
    // continue; not -> end at the ghost). Emergent from the edge-driven model + the bridge.
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G4 — ghost consumes the pending gap-slur (tie-across-rests bridge)");
    // A bridge-aware variant of step(): models tieAcrossRests=ON (the gap bridge holds gateHeld =
    // slurForward across the gap) so a ghost onset in the gap sees wasHeld and ties IN.
    auto stepBridge = [](SequencerEngine& eng, bool mainRise, bool ghostRise, bool mainHigh,
                         bool ghostHigh, float restProb, float legatoProb, float noteVal,
                         float variation = 0.5f) {
        PatternInput in = makeInput(); in.variationAmount = variation;
        in.legato = legatoProb;
        StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, /*ratchetRise=*/false,
                                                 restProb, noteVal, in,
                                                 ghostRise, ghostHigh);
        const bool isRest = (r.decision == MonoDecision::Rest);
        const bool ghostSounding = eng.ghostActive && ghostHigh;
        // Bridge (tieAcrossRests=ON): the gate stays held across the gap iff slurForward committed.
        const bool gateOpen = !isRest && (mainHigh || ghostSounding || eng.gs.slurForward);
        eng.gs.gateHeld = gateOpen;
        if (!gateOpen) eng.gs.holdRemain = 0.f;
        return r;
    };
    TEST("a sounding ghost in the gap is the next onset -> the slur ties INTO the ghost", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        // A: main onset, commits slurForward (legato=1.0).
        stepBridge(eng, /*mainRise=*/true,  /*ghostRise=*/false, /*mainHigh=*/true,  /*ghostHigh=*/false, 0.f, 1.0f, 4.f);
        // Gap: main gate low; the bridge keeps gateHeld = slurForward (true) so wasHeld stays true.
        // A ghost fires in the gap (variation=1.0 -> candidate fires). It is the next onset.
        StepResult g = stepBridge(eng, /*mainRise=*/false, /*ghostRise=*/true,  /*mainHigh=*/false, /*ghostHigh=*/true,  0.f, 0.5f, 4.f, /*variation=*/1.0f);
        // The ghost tied IN from A's slur (prevSlur reached it) -> not a fresh NewNote.
        EXPECT(g.decision == D::Tie || g.decision == D::Legato || g.decision == D::LegatoMax);
    });
    TEST("a rested ghost (no candidate) is transparent — the slur passes through to the next onset", {
        SequencerEngine eng; eng.numPolyVoices = 0;
        stepBridge(eng, true, false, true, false, 0.f, 1.0f, 4.f);              // A commits
        // Gap ghost that RESTS (variation=0 -> no candidate). It is NOT an onset for this voice:
        // ghostActive stays false (no ghost sounded) and slurForward is untouched, so the slur
        // passes THROUGH it (the bridge keeps the gate held). The returned decision is the preserved
        // prior one (not a fresh Rest) — transparency is ghostActive==false + chain survival, not a
        // Rest decision.
        StepResult g = stepBridge(eng, false, true, false, true, 0.f, 0.5f, 4.f, /*variation=*/0.f);
        EXPECT(!eng.ghostActive);                                  // no ghost sounded -> transparent
        EXPECT(eng.gs.slurForward == true);                        // A's slur survived (not consumed/broken)
        // The bridge still holds; a following main gate ties in (A's slur survived the transparent ghost).
        StepResult m = stepBridge(eng, true, false, true, false, 0.f, 0.5f, 4.f);
        EXPECT(m.decision == D::Tie || m.decision == D::Legato || m.decision == D::LegatoMax);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    // G5 — per-voice ghost placement (GATE_SUBDIVISION_STEP_GATE.md §398/§227):
    // At a ghost cell (the mono ghosted), each poly voice rolls its OWN variation to decide if it
    // ghosts. A voice whose variation does not pass is a RESTED GHOST (transparent — silent, the
    // slur passes through it). Per the f9c4189 per-voice VAR draw model, a poly voice reads its
    // OWN per-voice draw (polyRandom(bank, PL_VARIATION)) — NOT mono's variationRandom. Mono still
    // reads variationRandom. So: delegated voice = mono's STEP + its own per-voice draw; Local East
    // voice = its own STEP + its own per-voice draw. The draws are seeded directly here (the unit
    // test does not run the Philox draw pipeline, so polyRandom persists as seeded).
    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G5 — per-voice ghost placement (variation gate per voice)");
    TEST("shared (delegated): mono ghosts -> all poly voices ghost (per-voice draws pass)", {
        SequencerEngine eng; eng.numPolyVoices = 2;
        eng.voices[1].restProb = 0.f; eng.voices[2].restProb = 0.f;
        // variationAmount=1.0 -> ghost iff r_vary < 1.0. Seed every per-voice VAR draw to 0.1 so
        // both delegated voices (mono step + own draw) ghost deterministically; mono reads its
        // variationRandom (default 0) and also ghosts -> ghostActive=true.
        for (int i = 0; i < 16; ++i) {
            eng.pe.polyRandom(1, SequencerEngine::PL_VARIATION)[i] = 0.1f;
            eng.pe.polyRandom(2, SequencerEngine::PL_VARIATION)[i] = 0.1f;
        }
        stepPoly(eng, true, false, false, true, false, 0.f, 0.f, 4.f, /*variation=*/1.0f);  // main onset
        stepPoly(eng, false, false, false, false, false, 0.f, 0.f, 4.f, 1.0f);              // gap (no edge)
        stepPoly(eng, false, false, true,  false, true,  0.f, 0.f, 4.f, 1.0f);              // ghost onset
        EXPECT(eng.ghostActive);                       // mono ghosted
        EXPECT(eng.voices[1].gs.gateHeld);             // voice 0 ghosts (own draw 0.1 < 1.0)
        EXPECT(eng.voices[2].gs.gateHeld);             // voice 1 ghosts (own draw 0.1 < 1.0)
    });
    TEST("Local East: voice 0's VAR LOR points to a high-variation step -> rested ghost (transparent); voice 1 (delegated) ghosts", {
        SequencerEngine eng; eng.numPolyVoices = 2;
        eng.voices[1].restProb = 0.f; eng.voices[2].restProb = 0.f;
        // Pin the LORs so the steps are deterministic (len=1 => step = off). Per-voice draws are
        // seeded in polyRandom(bank, PL_VARIATION) (the f9c4189 per-voice model); mono reads
        // variationRandom.
        //   mono VAR LOR: len=1, off=0 -> step 0 -> variationRandom[0]=0.1 (< 0.5) -> mono ghosts.
        //   voice 0: Local East VAR, len=1, off=1 -> step 1 -> polyRandom(1,PL_VAR)[1]=0.9 (>= 0.5) -> RESTED GHOST.
        //   voice 1: delegated -> mono step 0 -> polyRandom(2,PL_VAR)[0]=0.1 (< 0.5) -> ghosts.
        eng.pe.variationRandom[0] = 0.1f;                          // mono ghosts
        eng.pe.polyRandom(1, SequencerEngine::PL_VARIATION)[1] = 0.9f;   // voice 0 Local-East step -> rested ghost
        eng.pe.polyRandom(2, SequencerEngine::PL_VARIATION)[0] = 0.1f;   // voice 1 delegated, mono step -> ghosts
        eng.strandLenRef(dotModular::STRAND_VARIATION) = 1;
        eng.strandOffRef(dotModular::STRAND_VARIATION) = 0;
        eng.polyLORRef(0, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_LEN) = 1;
        eng.polyLORRef(0, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_OFF) = 1;
        eng.setVarlegLocalEast(0, 0, true);   // voice 0 reads its own VAR LOR (Local East)
        const float variation = 0.5f;
        stepPoly(eng, true, false, false, true, false, 0.f, 0.f, 4.f, variation);  // main onset
        stepPoly(eng, false, false, false, false, false, 0.f, 0.f, 4.f, variation); // gap
        stepPoly(eng, false, false, true,  false, true,  0.f, 0.f, 4.f, variation); // ghost onset (mono ghosts)
        EXPECT(eng.ghostActive);                       // mono ghosted (variationRandom[0]=0.1 < 0.5)
        EXPECT(!eng.voices[1].gs.gateHeld);            // voice 0: RESTED GHOST (transparent)
        EXPECT(!eng.voices[1].participating);          //   not part of the chain
        EXPECT(eng.voices[2].gs.gateHeld);             // voice 1: delegated -> ghosts
    });

    std::cout << "\n-----\nsubgate: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
