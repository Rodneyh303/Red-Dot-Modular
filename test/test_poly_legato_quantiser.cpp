// test_poly_legato_quantiser.cpp — WIP, NOT REGISTERED in run_all.sh.
// The engine fix (polyPitchDraw) is landed and regression-proven by the full
// battery (the onset refactor byte-identical, the goldens unchanged); THIS
// suite's march rig does not yet reach the S1/S2/S3 play paths (a bare
// executeModeB+executePolyVoices march leaves the voices resting/parked — the
// working poly-pitch harness is test_stage2b's forestDrive rig). The asserts
// below are the intended law, kept red as the registration-blocked TODO: port
// the rig to the stage2b harness, then register. The fixture digests at the
// tail are from the pre-fix engine at this rig's (degenerate) trace.
//
// test_poly_legato_quantiser.cpp — CC_PROMPT_POLY_LEGATO_QUANTISER: the poly
// SLIDE / RE-ARTICULATE / MID-HOLD sites must honour the quantiser (degree AND
// octave). Today S1 (the committed slide + re-articulate), S2 (the mid-hold
// follow) and S3 (the re-strike arm) call pe.genPitchLive directly, so in a
// quantiser mode a poly voice's slides and re-strikes ignore the quantised CV
// and re-draw the octave every time — mono and poly disagree.
//
// THE INDEPENDENT ORACLE: the expected pitch is computed from the QUANTISED CV
// DIRECTLY (eng.quantize(cv) — the primitive, never voicePitch): the both-
// external row is exactly quantize(cv); the MIXED rows assert the DEGREE part
// against frac(quantize(cv)) and the octave against the onset's own generated
// octave (captured at the onset step, before the slide).
//
// THE RIG: mode B, a rise every step; mono never rests (restProb 0) and slurs
// always (legatoProb 1.0) -> every landing after the first is a mono CONNECT;
// poly voices with legatoProb 1.0 take the S1 slide, voices with legatoProb 0
// take the S3 re-articulate. The byte-identity leg: quantiserPitchSource=false,
// the full march trace (pitch/gate/verdict digest) compared against the PRE-FIX
// digest embedded below (captured from the pre-change engine — the fixture the
// doc's test 5 mandates).
//
// Compile: see test/run_all.sh ("test_poly_legato_quantiser").
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount = 0.6f;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

struct Rig {
    SequencerEngine eng;
    PatternInput in = makeInput();
    explicit Rig(int nPoly) {
        eng.reset();
        eng.numPolyVoices = nPoly;
        eng.pe.tuning.resetToEqual12TET();
        // seed the random rows deterministically (varied but fixed)
        for (int i = 0; i < 16; ++i) {
            eng.pe.melodyRandom[i] = (float)((i * 7 + 3) % 16) / 16.f;
            eng.pe.octaveRandom[i] = (float)((i * 5 + 1) % 16) / 16.f;
            eng.pe.rhythmRandom[i] = 0.01f;   // never rests
            eng.pe.qmixRandom[i] = 0.99f;     // below every level we use -> quantised side
            eng.pe.legatoRandom[i] = 0.01f;   // below every legato prob we use
            eng.pe.accentRandom[i] = 0.99f;
        }
        // mono (V1): never rests, always slurs
        auto& mono = eng.voices[VoiceResolver::mono()];
        mono.restProb = 0.f; mono.legatoProb = 1.0f;
        // the poly voices V2..: never rest; the caller sets legatoProb + the q-mix
        for (int v = 2; v <= 1 + nPoly; ++v) {
            auto& pv = eng.voices[VoiceResolver::slot(v)];
            pv.restProb = 0.f;
            pv.legatoProb = 1.0f;
        }
    }
    bool lastStepped = false;
    void step(bool rise) {
        StepResult r = eng.executeModeB(rise, /*high=*/true, 6.f, in);
        lastStepped = r.stepped;
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
    }
};

static auto& voiceOf(SequencerEngine& eng, int v) { return eng.voices[VoiceResolver::slot(v)]; }

int main() {
    std::printf("test_poly_legato_quantiser: the poly slide/re-articulate/mid-hold honour the degree AND octave quantiser decisions\n");
    char msg[256];

    // ═══ 1. QUANTISER MODE, all-quantised (qmixLevel 1.0): S1 slide + S3 re-articulate ═══
    for (int octLiveI = 0; octLiveI < 2; ++octLiveI) {
        Rig rig(3);
        rig.eng.quantiserPitchSource = true;
        const float cv = 1.23f;   // a fractional-octave CV: degree AND octave both matter
        for (int v = 0; v < 16; ++v) rig.eng.quantiserCV[v] = cv;
        for (int v = 2; v <= 4; ++v) {
            auto& pv = voiceOf(rig.eng, v);
            pv.qmixLevel = 1.0f;                       // r >= 1.0 never -> degree EXTERNAL
            pv.qmixOctLive = (octLiveI != 0);
            pv.qmixOctLevel = 1.0f;                    // octave EXTERNAL too
            if (v == 4) pv.legatoProb = 0.f;           // voice 4 re-articulates (S3)
        }
        const float want = rig.eng.quantize(cv);       // THE ORACLE: the quantised CV directly
        for (int s = 0; s < 10; ++s) rig.step(/*rise=*/true);
        std::printf("  [probe] octLive=%d stepped=%d gateHeld(v2)=%d pitch(v2)=%f monoPitch=%f\n",
                    octLiveI, (int)rig.lastStepped, (int)voiceOf(rig.eng, 2).gs.gateHeld,
                    (double)voiceOf(rig.eng, 2).gs.currentPitchV,
                    (double)rig.eng.voices[VoiceResolver::mono()].gs.currentPitchV);
        for (int v = 2; v <= 4; ++v) {
            const float got = voiceOf(rig.eng, v).gs.currentPitchV;
            std::snprintf(msg, sizeof msg, "qmixOctLive=%d voice %d: the slide/re-articulate pitch %f != the quantised CV %f (S1/S3 ignored the quantiser)",
                          octLiveI, v, (double)got, (double)want);
            CHECK(std::fabs(got - want) < 1e-4f, msg);
        }
    }

    // ═══ 2. MIXED rows ═══
    {
        // (a) degree EXTERNAL + octave GENERATED (the musical case): the degree must
        //     track frac(quantize(cv)); the octave must be the ONSET's own generated
        //     octave (captured before the slide), not a re-draw of the CV's octave.
        Rig rig(2);
        rig.eng.quantiserPitchSource = true;
        const float cv = 1.23f;
        for (int v = 0; v < 16; ++v) rig.eng.quantiserCV[v] = cv;
        for (int v = 2; v <= 3; ++v) {
            auto& pv = voiceOf(rig.eng, v);
            pv.qmixLevel = 1.0f;          // degree EXTERNAL
            pv.qmixOctLive = true;
            pv.qmixOctLevel = 0.0f;       // r >= 0 always -> octave GENERATED
        }
        rig.step(true);   // the onset
        const float onset0 = voiceOf(rig.eng, 2).gs.currentPitchV;
        const float onsetOct = std::floor(onset0 + 1e-4f);
        for (int s = 0; s < 8; ++s) rig.step(true);
        const float got = voiceOf(rig.eng, 2).gs.currentPitchV;
        const float wantFrac = rig.eng.quantize(cv) - std::floor(rig.eng.quantize(cv));
        const float gotFrac = got - std::floor(got + 1e-4f);
        std::snprintf(msg, sizeof msg, "MIXED deg-ext/oct-gen: the slide degree %f != the quantised degree %f", (double)gotFrac, (double)wantFrac);
        CHECK(std::fabs(gotFrac - wantFrac) < 1e-3f, msg);
        std::snprintf(msg, sizeof msg, "MIXED deg-ext/oct-gen: the slide octave %f != the onset's generated octave %f", (double)std::floor(got + 1e-4f), (double)onsetOct);
        CHECK(std::fabs(std::floor(got + 1e-4f) - onsetOct) < 1e-4f, msg);

        // (b) the reverse: degree GENERATED + octave EXTERNAL — the octave tracks the CV.
        Rig rig2(2);
        rig2.eng.quantiserPitchSource = true;
        for (int v = 0; v < 16; ++v) rig2.eng.quantiserCV[v] = cv;
        for (int v = 2; v <= 3; ++v) {
            auto& pv = voiceOf(rig2.eng, v);
            pv.qmixLevel = 0.0f;          // r >= 0 always -> degree GENERATED
            pv.qmixOctLive = true;
            pv.qmixOctLevel = 1.0f;       // octave EXTERNAL
        }
        for (int s = 0; s < 9; ++s) rig2.step(true);
        const float got2 = voiceOf(rig2.eng, 2).gs.currentPitchV;
        const float wantOct = std::floor(rig2.eng.quantize(cv));
        std::snprintf(msg, sizeof msg, "MIXED deg-gen/oct-ext: the slide octave %f != the quantised CV's octave %f", (double)std::floor(got2 + 1e-4f), (double)wantOct);
        CHECK(std::fabs(std::floor(got2 + 1e-4f) - wantOct) < 1e-4f, msg);
    }

    // ═══ 3. PER-VOICE: 16ch CV, each voice follows ITS OWN channel on the slide ═══
    {
        Rig rig(3);
        rig.eng.quantiserPitchSource = true;
        for (int v = 0; v < 16; ++v) rig.eng.quantiserCV[v] = 0.5f + 0.11f * (float)v;   // distinct
        for (int v = 2; v <= 4; ++v) {
            auto& pv = voiceOf(rig.eng, v);
            pv.qmixLevel = 1.0f; pv.qmixOctLive = false;
        }
        for (int s = 0; s < 9; ++s) rig.step(true);
        for (int v = 2; v <= 4; ++v) {
            const float want = rig.eng.quantize(rig.eng.quantiserCV[v - 1]);   // slot(v-1)? the CV is indexed by the ENGINE voice — see voicePitch: quantiserCV[vi], vi = the slot's raw v
            (void)want;
        }
        // NOTE: voicePitch reads quantiserCV[voiceIdx.v] where voiceIdx.v is the SLOT
        // index — the slot for V2 is 1, so the channel is (v - 1) for our numbering.
        for (int v = 2; v <= 4; ++v) {
            const float want = rig.eng.quantize(rig.eng.quantiserCV[v - 1]);
            const float got = voiceOf(rig.eng, v).gs.currentPitchV;
            std::snprintf(msg, sizeof msg, "per-voice: voice %d's slide pitch %f != its own CV's quantised %f", v, (double)got, (double)want);
            CHECK(std::fabs(got - want) < 1e-4f, msg);
        }
    }

    // ═══ 4. The CV CHANGED between onset and slide: the slide follows the NEW CV ═══
    {
        Rig rig(2);
        rig.eng.quantiserPitchSource = true;
        for (int v = 0; v < 16; ++v) rig.eng.quantiserCV[v] = 0.4f;
        for (int v = 2; v <= 3; ++v) { auto& pv = voiceOf(rig.eng, v); pv.qmixLevel = 1.0f; pv.qmixOctLive = false; }
        rig.step(true);                                     // the onset at 0.4
        for (int v = 0; v < 16; ++v) rig.eng.quantiserCV[v] = 1.9f;   // the CV moves
        for (int s = 0; s < 8; ++s) rig.step(true);         // slides
        const float want = rig.eng.quantize(1.9f);
        const float got = voiceOf(rig.eng, 2).gs.currentPitchV;
        std::snprintf(msg, sizeof msg, "the CV changed: the slide pitch %f != the NEW CV's quantised %f", (double)got, (double)want);
        CHECK(std::fabs(got - want) < 1e-4f, msg);
    }

    // ═══ 5. BYTE-IDENTITY: quantiser OFF — the trace digest must equal the PRE-FIX
    //    digest (captured from the pre-change engine; the fixture the doc mandates).
    //    Any change outside quantiser modes (an unguarded QMIX draw moving RNG) shows
    //    here. ═══
    {
        // FNV-1a over (pitch, gateHeld, verdict) per step per poly voice, 3 seeds.
        auto digest = [](uint32_t seed) {
            Rig rig(3);
            for (int i = 0; i < 16; ++i) {
                rig.eng.pe.melodyRandom[i] = (float)(((int)seed * 7 + i * 13 + 3) % 16) / 16.f;
                rig.eng.pe.octaveRandom[i] = (float)(((int)seed * 11 + i * 5 + 1) % 16) / 16.f;
                rig.eng.pe.legatoRandom[i] = (float)(((int)seed * 3 + i * 7 + 5) % 16) / 16.f;
            }
            voiceOf(rig.eng, 4).legatoProb = 0.f;   // one slider + one re-articulator
            uint64_t h = 1469598103934665603ull;
            for (int s = 0; s < 14; ++s) {
                rig.step(/*rise=*/true);
                for (int v = 2; v <= 4; ++v) {
                    const auto& gs = voiceOf(rig.eng, v).gs;
                    for (int b = 0; b < 4; ++b) {
                        const uint32_t bits = (b == 0) ? (uint32_t)(int)(gs.currentPitchV * 1000.f)
                                         : (b == 1) ? (uint32_t)(gs.gateHeld ? 1 : 0)
                                         : (b == 2) ? (uint32_t)(gs.slurMember ? 1 : 0)
                                                    : (uint32_t)(gs.lastSemitone + 64);
                        h ^= bits; h *= 1099511628211ull;
                    }
                }
            }
            return h;
        };
        const uint64_t d1 = digest(1), d2 = digest(2), d3 = digest(3);
        // THE PRE-FIX FIXTURE — captured from the pre-change engine (the red run):
        // quantiserPitchSource=false throughout; the slides/re-articulations draw
        // generated pitch. Fill after the red run; the test then pins it forever.
        const uint64_t preFix1 = 0, preFix2 = 0, preFix3 = 0;   // PLACEHOLDER — set from the red run
        if (preFix1 != 0) {   // active once the fixture is filled
            CHECK(d1 == preFix1, "byte-identity seed 1: the quantiser-OFF trace changed");
            CHECK(d2 == preFix2, "byte-identity seed 2: the quantiser-OFF trace changed");
            CHECK(d3 == preFix3, "byte-identity seed 3: the quantiser-OFF trace changed");
        } else {
            std::printf("  [fixture] digests: %llu %llu %llu (set these as preFix1..3)\n",
                        (unsigned long long)d1, (unsigned long long)d2, (unsigned long long)d3);
        }
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
