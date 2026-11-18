// diag_global_dip_advoffset.cpp — REPRODUCE-FIRST for the global<voice misalignment.
//
// SAME-SEED DIFFERENTIAL, gate mode, WANDERING voices. Two engines fed an identical gate
// stream AND an identical LIVE variation-lane LOR modulation; engine A holds global=16,
// engine B dips 16->4->16. Absent the dip they are byte-identical, so any divergence in
// advanceOffset_ is the dip alone.
//
// KEY (Rodney, 2026-11-18): the escape wander is driven by the probability the variation
// lane READS, so range/LOR modulation of that read is what the dip can perturb. The first
// cut held modulation static and came back clean; this run drives a LIVE variation-lane
// offset CV through the window change, the missing ingredient.
//
// V2 = centre control (no wander); V3..V5 = wandering. Voice len 8; dip on [DIP_ON,DIP_OFF).
#include <cstdio>
#include <cstdint>
#include <cmath>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
using redDot::PhiloxRng;
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.6f;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

static void seed(SequencerEngine& eng, int nVoices) {
    PhiloxRng rng; rng.seed64(12345u);
    eng.reset();
    eng.numPolyVoices = nVoices;
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]=rng.nextFloat(); eng.pe.octaveRandom[i]=rng.nextFloat();
        eng.pe.qmixRandom[i]=rng.nextFloat();   eng.pe.rhythmRandom[i]=rng.nextFloat();
        eng.pe.accentRandom[i]=rng.nextFloat();  eng.pe.variationRandom[i]=rng.nextFloat();
        eng.pe.legatoRandom[i]=rng.nextFloat();
    }
    for (int v = 2; v <= 16; ++v) {
        const auto vs = VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < 16; ++i)
                eng.pe.polyRandom(vs, LaneId{kLanes[lane]})[i] = rng.nextFloat();
        eng.voices[vs].restProb   = 0.20f;
        eng.voices[vs].accentProb = 0.25f;
        eng.voices[vs].legatoProb = 0.0f;
        eng.voices[vs].qmixLevel  = 0.5f;
        eng.voices[vs].variationProb = (v == 2) ? 0.0f : 0.5f;   // V2 centre control
        eng.voiceLorRef(vs, SequencerEngine::LOR_LEN) = 8;       // voice frame = 8
        eng.polyLORRef(vs, LaneId{SequencerEngine::PL_VARIATION}, SequencerEngine::LOR_LEN) = 8;
    }
}

static void stepB(SequencerEngine& eng, const PatternInput& in, bool rose, bool high) {
    StepResult r = eng.executeModeB(rose, high, 6.f, in);
    if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
}

int main() {
    const int N = 4;                 // voices V2..V5
    const int STEPS = 52;
    const int DIP_ON = 16, DIP_OFF = 26;
    std::printf("LIVE-MODULATION DIFFERENTIAL: A=global16, B=global 16->4->16 on [%d,%d)\n", DIP_ON, DIP_OFF);
    std::printf("gate mode, voice len 8, V2 centre; LIVE variation-lane offset CV swinging each step\n\n");

    SequencerEngine A, B; seed(A, N); seed(B, N);
    A.updateWindow(16.f,0.f,false,0.f,0.f,false);
    B.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const PatternInput in = makeInput();

    PhiloxRng grng; grng.seed64(999u);
    bool high = false;
    int firstDiverge = -1;

    std::printf(" step |     A adv[2..5]     |     B adv[2..5]     | B-A diff | varOff\n");
    for (int s = 0; s < STEPS; ++s) {
        bool rose = false;
        if (high) { if (grng.nextFloat() < 0.40f) high = false; }
        else      { if (grng.nextFloat() < 0.55f) { high = true; rose = true; } }

        // LIVE variation-lane offset modulation, identical for A and B, swinging each step.
        // advanceOffset_ index i (1-based voice number) <-> slot(i+1): V2=idx1 (CENTRE,
        // NOT modulated), V3=idx2, V4=idx3, V5=idx4. We modulate ONLY the WANDERING voices
        // V3..V5 (idx 2..4); if V2 (idx1, centre, un-modulated) diverges, that's cross-voice.
        const int varOff = (int)std::lround(3.0 * std::sin(s * 0.5));   // -3..+3
        for (int vn = 3; vn <= 1 + N; ++vn) {           // voice NUMBERS V3..V5 (the wandering ones)
            int d3[3] = {0, varOff, 0};
            A.pushLaneLorCvPoly(VoiceResolver::slot(vn), SequencerEngine::PL_VARIATION, d3);
            B.pushLaneLorCvPoly(VoiceResolver::slot(vn), SequencerEngine::PL_VARIATION, d3);
        }

        if (s == DIP_ON)  B.updateWindow(4.f,0.f,false,0.f,0.f,false);
        if (s == DIP_OFF) B.updateWindow(16.f,0.f,false,0.f,0.f,false);

        stepB(A, in, rose, high);
        stepB(B, in, rose, high);

        // idx 1..N : V2(centre) V3 V4 V5
        int da[5]={0}, db[5]={0}; bool diverged=false, centreDiverged=false;
        for (int i = 1; i <= N; ++i) {
            da[i] = (int)A.advanceOffset_[i]; db[i] = (int)B.advanceOffset_[i];
            if (da[i] != db[i]) { diverged = true; if (i == 1) centreDiverged = true; }
        }
        if (diverged && firstDiverge < 0) firstDiverge = s;
        const char* phase = (s>=DIP_ON && s<DIP_OFF) ? "dip" : "   ";
        const char* tag = centreDiverged ? "  <-- CENTRE V2 diverged (CROSS-VOICE)"
                        : diverged ? "  <-- diverge" : "";
        std::printf(" %2d %s | V2c%+d V3%+d V4%+d V5%+d | B-A %+d %+d %+d %+d | varOff%+d%s\n",
            s, phase, db[1],db[2],db[3],db[4],
            db[1]-da[1], db[2]-da[2], db[3]-da[3], db[4]-da[4], varOff, tag);
    }
    if (firstDiverge < 0)
        std::printf("\nRESULT: A==B throughout even with live modulation — dip does NOT perturb advanceOffset_.\n");
    else
        std::printf("\nRESULT: first divergence at step %d — live modulation + dip DOES perturb advanceOffset_ (reproduced).\n", firstDiverge);
    return 0;
}
