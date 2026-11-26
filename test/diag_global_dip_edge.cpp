// test/diag_global_dip_edge.cpp — the reseed-hunt MEASUREMENT (not a pass/fail guard).
//
// WHY. The 173 test_global_dip_differential failures when the setWindow in-window
// re-SEED is removed are all "no commit off the live anchor". The claim to VERIFY
// (not reason about): the identity-row commit edge rides the COUNTER
//     atPhraseEdge() == (netSteps_ % len_ == 0)
// while the live anchor the commit must land on is the COLUMN
//     phrasePos() == 0   (col_ == startStep_)
// and the re-seed is the ONLY thing holding them equal (netSteps_ ≡ col_-startStep_).
// Remove it and the two edges split on a global length change — that split is the
// 173-failure mechanism. This harness drives the engine through the 16->4->16 dip
// and PRINTS, per step, the counter edge vs the column edge, WITH the re-seed and
// WITHOUT it (PositionResolver::reseedInWindowOnReauthor_), and counts the splits.
//
// Reproduce-first expectation: WITH re-seed → 0 splits (they agree every step);
// WITHOUT → splits appear at/after the dip, each a step where the engine's
// identity-row commit (counter) fires at a tick the live column anchor does not
// sanction — exactly "no commit off the live anchor".
//
// Measurement only. It asserts nothing except the harness's own internal sanity
// (WITH-reseed must show zero splits — if THAT breaks, the harness is wrong, not
// the engine). Compile: see test/run_all.sh.
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
    in.noteVariationMask = 0b111; in.variationAmount = 0.6f;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

// Identity-delegated rows (mappedRows=false): the anchor IS the global phrase entry
// — the branch of voiceFrameEdgeLive_ that reads the counter. Mirror of the test's seed().
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
        eng.voices[vs].restProb = 0.20f; eng.voices[vs].accentProb = 0.25f;
        eng.voices[vs].legatoProb = 0.0f; eng.voices[vs].qmixLevel = 0.5f;
        eng.voices[vs].variationProb = (v == 2) ? 0.0f : 0.5f;
        eng.voiceLorRef(vs, SequencerEngine::LOR_LEN) = 8;
        eng.polyLORRef(vs, LaneId{SequencerEngine::PL_VARIATION}, SequencerEngine::LOR_LEN) = 8;
    }
}
static void stepB(SequencerEngine& eng, const PatternInput& in, bool rose, bool high) {
    StepResult r = eng.executeModeB(rose, high, 6.f, in);
    if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
}

// One run of the dip. Returns the number of COUNTER-vs-COLUMN edge splits.
static int run(bool reseed, bool print) {
    const int N = 4, STEPS = 60, DIP_ON = 16, DIP_OFF = 26;
    SequencerEngine B; seed(B, N);
    B.pos_.reseedInWindowOnReauthor_ = reseed;   // the DIAG toggle
    B.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const PatternInput in = makeInput();
    PhiloxRng grng; grng.seed64(999u);
    bool high = false;
    int splits = 0;

    std::printf("\n  === run: in-window re-SEED %s ===\n", reseed ? "ON (shipped)" : "OFF (free-run)");
    std::printf("  step tick lenG col netSteps phrasePos | counterEdge columnEdge\n");
    for (int s = 0; s < STEPS; ++s) {
        bool rose = false;
        if (high) { if (grng.nextFloat() < 0.40f) high = false; }
        else      { if (grng.nextFloat() < 0.55f) { high = true; rose = true; } }
        const int varOff = (int)std::lround(3.0 * std::sin(s * 0.5));
        for (int vn = 3; vn <= 1 + N; ++vn) { int d3[3] = {0, varOff, 0};
            B.pushLaneLorCvPoly(VoiceResolver::slot(vn), SequencerEngine::PL_VARIATION, d3); }

        if (s == DIP_ON)  B.updateWindow(4.f, 0.f,false,0.f,0.f,false);
        if (s == DIP_OFF) B.updateWindow(16.f,0.f,false,0.f,0.f,false);
        stepB(B, in, rose, high);

        const bool counterEdge = B.pos_.atPhraseEdge();      // the engine identity-row commit
        const bool columnEdge  = (B.pos_.phrasePos() == 0);  // the live anchor the test demands
        const bool split = (counterEdge != columnEdge);
        if (split) ++splits;
        if (print && (split || s == DIP_ON || s == DIP_OFF || s < 2)) {
            std::printf("  %4d %4lld %4d %3d %8lld %9d |    %d          %d   %s%s\n",
                        s, (long long)B.transportTick_, B.cachedLength, B.pos_.playedColumn(),
                        (long long)B.pos_.netSteps_, B.pos_.phrasePos(),
                        (int)counterEdge, (int)columnEdge,
                        split ? "<<< SPLIT" : "",
                        (s==DIP_ON) ? "  [dip 16->4]" : (s==DIP_OFF) ? "  [restore 4->16]" : "");
        }
    }
    std::printf("  --> %d counter-vs-column edge splits\n", splits);
    return splits;
}

int main() {
    std::printf("[diag_global_dip_edge] identity rows, 16->4->16 global dip.\n");
    std::printf("Measuring: does the identity-row commit edge (atPhraseEdge, the COUNTER)\n");
    std::printf("track the live anchor (phrasePos()==0, the COLUMN) across the dip?\n");

    const int withReseed = run(/*reseed=*/true,  /*print=*/true);
    const int noReseed   = run(/*reseed=*/false, /*print=*/true);

    std::printf("\n[diag_global_dip_edge] SUMMARY: WITH re-seed = %d splits, WITHOUT = %d splits\n",
                withReseed, noReseed);
    std::printf("  Reading: WITH should be 0 (re-seed holds counter==column). WITHOUT > 0 means\n");
    std::printf("  the counter edge fires where the live column anchor does not — the\n");
    std::printf("  'no commit off the live anchor' mechanism. If WITHOUT is also 0, the\n");
    std::printf("  divergence is elsewhere (not this identity-row counter edge).\n");

    // Harness sanity ONLY: WITH the re-seed the two edges MUST agree every step.
    if (withReseed != 0) {
        std::printf("\n  HARNESS ERROR: WITH-reseed showed %d splits — the mirror is wrong, not the engine.\n", withReseed);
        return 1;
    }
    return 0;
}
