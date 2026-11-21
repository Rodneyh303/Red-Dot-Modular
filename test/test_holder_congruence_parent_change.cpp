// test_holder_congruence_parent_change — CA5 key-completion GATE (docs/design/CC_PROMPT_REVERSE_COMPLETIONS_THEN_CA5.md Part 2).
// (adapted from debug/global-dip-logging diag_global_dip_advoffset.cpp)
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
        eng.setVoiceLorLocalEast(vs, true); eng.voiceLorRef(vs, SequencerEngine::LOR_LEN) = 8;
        eng.polyLORRef(vs, LaneId{SequencerEngine::PL_VARIATION}, SequencerEngine::LOR_LEN) = 8;
    }
}

static void stepB(SequencerEngine& eng, const PatternInput& in, bool rose, bool high) {
    StepResult r = eng.executeModeB(rose, high, 6.f, in);
    if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
}

static long g_bad=0, g_chk=0; static int g_first=-1;
static int g_fail=0;
#define CHECK(c,m) do{ if(!(c)){ if(g_fail<20) std::printf("  FAIL: %s\n",(m)); ++g_fail; } }while(0)
static void congr(SequencerEngine& e, const char* tag, int step, int N) {
    const int64_t t0 = (int64_t)e.transportTick_ - 1;
    for (int v = 1; v <= N; ++v) {            // voice index (0 = mono); V2..V5 = idx 1..4
        int L,O,R; SequencerEngine::LaneDir vd; e.effectiveVoiceLor(v, L,O,R, vd);
        if (L==dotModular::kPatternLengthCap && O==0 && R==0 && vd==SequencerEngine::LaneDir::Forward) continue;
        if (L < 1) L = 1;
        const int per = SequencerEngine::walkPeriod(vd, L);
        const int64_t t0e = t0 + e.advanceOffset_[v];
        int64_t d = ((int64_t)e.voiceFramePhase_[v] - t0e) % per; if (d<0) d+=per;
        ++g_chk;
        if (d != 0) { ++g_bad; if (g_first<0) g_first=step; if (g_bad<6) std::printf("  NONCONGRUENT %s step %d voice idx %d: phase %d, t0eff %lld, per %d\n", tag, step, v, e.voiceFramePhase_[v], (long long)t0e, per); }
    }
}
int main() {
    std::printf("test_holder_congruence_parent_change: the phase holders stay congruent to the transport when the PARENT length/offset changes under them\n");
    // The audit (CONCEPT_ODR_AUDIT CA5) claimed walkKeyOf(dir,len) omitting the parent cycle lets a child phase drift. The
    // holder law is a CONGRUENCE (phase = t0Eff mod period), maintained by + k (the raw delta) and re-seeded to the closed
    // form on a key change; a parent change moves t0Eff only through advanceOffset_, and k carries that delta. So the key
    // never needs the parent. PIN that: same-seed run, live variation CV, voices MAPPED (so the frame holder runs), the
    // global length and offset changed repeatedly mid-march — every checked step must stay congruent.
    const int N = 4, STEPS = 160;
    SequencerEngine B; seed(B, N);
    B.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const PatternInput in = makeInput();
    PhiloxRng grng; grng.seed64(999u);
    bool high = false;
    const float gl[] = {16.f, 4.f, 16.f, 8.f, 12.f, 32.f, 5.f, 16.f};
    const float go[] = {0.f, 0.f, 3.f, 0.f, 7.f, 0.f, 2.f, 0.f};
    for (int s = 0; s < STEPS; ++s) {
        bool rose = false;
        if (high) { if (grng.nextFloat() < 0.40f) high = false; }
        else      { if (grng.nextFloat() < 0.55f) { high = true; rose = true; } }
        const int varOff = (int)std::lround(3.0 * std::sin(s * 0.5));
        for (int vn = 3; vn <= 1 + N; ++vn) { int d3[3] = {0, varOff, 0}; B.pushLaneLorCvPoly(VoiceResolver::slot(vn), SequencerEngine::PL_VARIATION, d3); }
        if (s % 9 == 0) { const int k = (s / 9) % 8; B.updateWindow(gl[k], go[k], false, 0.f, 0.f, false); }
        stepB(B, in, rose, high);
        congr(B, "B", s, N);
    }
    char msg[160];
    std::snprintf(msg, sizeof msg, "enough congruence checks ran (%ld)", g_chk);
    CHECK(g_chk > 300, msg);
    std::snprintf(msg, sizeof msg, "%ld of %ld checks were non-congruent (first at step %d)", g_bad, g_chk, g_first);
    CHECK(g_bad == 0, msg);
    if (g_fail) { std::printf("test_holder_congruence_parent_change: %d FAIL\n", g_fail); return 1; }
    std::printf("test_holder_congruence_parent_change: PASS (%ld checks)\n", g_chk);
    return 0;
}
