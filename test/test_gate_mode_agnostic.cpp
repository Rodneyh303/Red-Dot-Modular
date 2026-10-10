// test_gate_mode_agnostic.cpp — GATE_SUBDIVISION_STEP_GATE.md §421 INVARIANT test.
//
// INVARIANT: gate/subgate/ghost/legato/rest/accent behaviour is ONE mode-agnostic path — q-mix
// (the pitch-origin axis) is the ONLY thing that differs between generator and quantiser; it
// switches WHERE PITCH IS READ FROM, nothing else. No "if quantiser mode" special cases in the
// gate logic. This test locks the invariant so it cannot silently rot.
//
// Two comparisons, same gate sequence (rises + a rest + a legato-commit):
//  (A) quantiserPitchSource=false                       -> generated pitch (generator)
//  (B) quantiserPitchSource=true,  qmixLevel=0.0        -> forceGenerated -> generated pitch
//  (C) quantiserPitchSource=true,  qmixLevel=1.0        -> quantised external CV
//  POLARITY (MODE_COLLAPSE_6_TO_3 §29): 0 = generated, 1 = quantised (the inverse of the
//  pre-collapse polarity, where high q-mix meant generated). The §421 INVARIANT itself is
//  polarity-agnostic — only which qmixLevel value means "force generated" flips.
//
//  A vs B: the quantiser FLAG alone must change NOTHING — fully bit-identical (decision, gate
//          envelope, accent, pitch, Tie/Legato). Proves the flag doesn't branch the gate logic.
//  B vs C: switching the PITCH SOURCE (generated vs quantised) must leave the gate behaviour
//          identical — gate envelope (gs.gateHeld), Rest/NewNote/Connected category, and accent
//          bit-identical; only the pitch (currentPitchV) and the pitch-derived Tie-vs-Legato
//          LABEL may differ (Tie vs Legato keys off semitone equality, so a different pitch can
//          flip the label — but the gate ENVELOPE is identical for Tie and Legato: both hold, no
//          retrigger). Group Legato/Tie/LegatoMax as "connected" for the comparison.
//
// Build (see test/run_all.sh, entry "test_gate_mode_agnostic|$SE $GS $PE"):
//   g++ -std=c++17 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers \
//       test/test_gate_mode_agnostic.cpp \
//       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp \
//       src/dsp/gates/GateState.cpp -o /tmp/tgma && /tmp/tgma

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "engines/SequencerEngine.hpp"

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

using D = MonoDecision;

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;
    return in;
}

// "connected" = the gate holds through (no fresh attack): Legato / LegatoMax / Tie. NewNote is a
// fresh attack; Rest is silent. This category is pitch-INDEPENDENT (NewNote-vs-connected keys off
// the slur handshake, not pitch); only the Tie-vs-Legato LABEL within "connected" is pitch-derived.
static const char* category(MonoDecision d) {
    switch (d) {
        case D::Rest: case D::MidNote: return "silent/hold";   // gate-low or mid-hold (not a fresh attack)
        case D::NewNote: return "fresh";
        case D::Legato: case D::LegatoMax: case D::Tie: return "connected";
    }
    return "?";
}

// A captured step: the gate-relevant outputs (decision category, gate envelope, accent) + pitch.
struct Snap {
    const char* cat;
    bool gateHeld;
    bool accented;
    float pitch;
    MonoDecision dec;
};

// Drive a fixed gate sequence through executeModeB and capture per-step gate behaviour + pitch.
// restProb/legatoProb are fixed so the decision sequence is deterministic for a given pitch source.
static std::vector<Snap> runSeq(bool quantiserSrc, float qmixLevel, float quantiserCV) {
    SequencerEngine eng; eng.numPolyVoices = 0;
    eng.quantiserPitchSource = quantiserSrc;
    eng.quantiserCV[0] = quantiserCV;
    PatternInput in = makeInput();
    // Phase A: qmixLevel and accentProb now on voices[0], not PatternInput
    const float v0qmix = qmixLevel;
    const float v0accent = 0.0f;   // accent off (deterministic) — gate behaviour we compare
    std::vector<Snap> out;
    auto step = [&](float restProb, float legatoProb) {
        eng.voices[0].restProb   = restProb;
        eng.voices[0].legatoProb = legatoProb;
        eng.voices[0].accentProb    = v0accent;
        eng.voices[0].qmixLevel     = v0qmix;
        eng.voices[0].variationProb = in.variationAmount;
        StepResult r = eng.executeModeB(/*gate1Rise=*/true, /*gate1High=*/true, 4.f, in);
        out.push_back({ category(r.decision), eng.gs.gateHeld, r.accented, eng.gs.currentPitchV, r.decision });
    };
    // Sequence: fresh note, legato-commit (connect), connect, rest, fresh, connect.
    step(0.f, 1.0f);   // 0: NewNote (lead, commits slurForward)
    step(0.f, 1.0f);   // 1: connected (LegatoMax)
    step(0.f, 0.5f);   // 2: connected (prevSlur from 1's commit)
    step(1.0f, 0.5f);  // 3: Rest (generated rest — breaks the chain)
    step(0.f, 1.0f);   // 4: fresh (after a rest, no held predecessor)
    step(0.f, 1.0f);   // 5: connected
    return out;
}

int main() {
    SUITE("§421 INVARIANT — gate behaviour is mode-agnostic (q-mix switches pitch only)");

    // (A) generator (flag off)  vs  (B) quantiser flag ON + force-generated (qmixLevel=0.0).
    // Must be FULLY bit-identical: the quantiser flag alone changes nothing. (0 = generated, §29)
    TEST("A (generator) vs B (quantiser flag on, force-generated): bit-identical gate+pitch", {
        auto a = runSeq(/*quantiserSrc=*/false, /*qmixLevel=*/0.f,   /*cv=*/1.0f);
        auto b = runSeq(/*quantiserSrc=*/true,  /*qmixLevel=*/0.0f,  /*cv=*/1.0f);  // 0 = generated (§29)
        EXPECT(a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i) {
            EXPECT(std::string(a[i].cat) == std::string(b[i].cat));
            EXPECT(a[i].gateHeld == b[i].gateHeld);
            EXPECT(a[i].accented == b[i].accented);
            EXPECT(a[i].dec == b[i].dec);             // full decision incl. Tie/Legato — pitch is the same
            EXPECT(std::fabs(a[i].pitch - b[i].pitch) < 1e-6f);  // same pitch (both generated)
        }
    });

    // (B) generated pitch  vs  (C) quantised external CV — same quantiser flag, different pitch source.
    // Gate behaviour identical; only pitch (and the pitch-derived Tie/Legato label) may differ.
    TEST("B (generated) vs C (quantised CV): gate envelope + category + accent identical; pitch differs", {
        auto b = runSeq(/*quantiserSrc=*/true, /*qmixLevel=*/0.0f,  /*cv=*/1.0f);  // 0 = generated
        auto c = runSeq(/*quantiserSrc=*/true, /*qmixLevel=*/1.0f,  /*cv=*/1.0f);  // 1 = quantised (§29)
        EXPECT(b.size() == c.size());
        bool pitchDiffered = false;
        for (size_t i = 0; i < b.size(); ++i) {
            EXPECT(std::string(b[i].cat) == std::string(c[i].cat));   // Rest/fresh/connected identical
            EXPECT(b[i].gateHeld == c[i].gateHeld);                   // gate envelope identical
            EXPECT(b[i].accented == c[i].accented);                   // accent identical
            // The "connected" category's Tie-vs-Legato LABEL may differ (pitch-derived) — allowed.
            // But silent/fresh categories must be the EXACT decision (no pitch dependence there).
            if (std::string(b[i].cat) != "connected")
                EXPECT(b[i].dec == c[i].dec);
            if (std::fabs(b[i].pitch - c[i].pitch) > 1e-6f) pitchDiffered = true;
        }
        EXPECT(pitchDiffered);   // the pitch source actually switched (else the test is vacuous)
    });

    // Sweep q-mix level across its range — the gate envelope must be invariant at every setting.
    TEST("gate envelope invariant across the q-mix range (0.0, 0.25, 0.5, 0.75, 1.0)", {
        auto base = runSeq(true, 0.0f, 1.0f);   // force-generated reference (0 = generated, deterministic)
        for (float q : { 0.25f, 0.5f, 0.75f, 1.0f }) {
            auto s = runSeq(true, q, 1.0f);     // mix of generated + quantised per step
            EXPECT(s.size() == base.size());
            for (size_t i = 0; i < s.size(); ++i) {
                EXPECT(std::string(s[i].cat) == std::string(base[i].cat));   // category identical
                EXPECT(s[i].gateHeld == base[i].gateHeld);                   // envelope identical
                EXPECT(s[i].accented == base[i].accented);                   // accent identical
            }
        }
    });

    std::cout << "\n-----\ngate_mode_agnostic: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
