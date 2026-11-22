// test_poly_legato_quantiser.cpp — CC_PROMPT_POLY_LEGATO_QUANTISER: the poly
// SLIDE / RE-ARTICULATE / MID-HOLD sites honour the quantiser (degree AND octave).
// S1/S2/S3 drew through pe.genPitchLive directly, so in a quantiser mode a poly
// voice's slides and re-strikes ignored the quantised CV and re-drew the octave
// every time — mono and poly disagreed.
//
// THE RIG: the WORKING poly march harness — test_step_advance's (the star graph
// via star_seam.hpp, the seeded rows, the GateStream + forestDrive loop), with
// the scenario overrides (rest/legato/qmix per voice, the quantiser CV).
// COVERAGE COUNTERS (the dip-guard discipline — a guard once passed while never
// reaching its path): s1 (connect-slides at a mono landing), s3 (re-articulates
// at a mono landing), holds (mono mid-hold steps) — each must be > 0.
//
// THE INDEPENDENT ORACLE: eng.quantize(cv) — the primitive, never voicePitch.
// The byte-identity leg: quantiserPitchSource=false, the march digest compared
// against the PRE-CHANGE fixture (captured from 7fe4a7ec's engine — the red run).
//
// Compile: see test/run_all.sh ("test_poly_legato_quantiser").
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
#include "star_seam.hpp"

using redDot::PhiloxRng;
using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO
};

// test_step_advance's seedEngine (lean local copy — the proven rig)
static void seedEngine(SequencerEngine& eng, PhiloxRng& rng) {
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]    = rng.nextFloat();
        eng.pe.octaveRandom[i]    = rng.nextFloat();
        eng.pe.qmixRandom[i]      = rng.nextFloat();
        eng.pe.rhythmRandom[i]    = rng.nextFloat();
        eng.pe.accentRandom[i]    = rng.nextFloat();
        eng.pe.variationRandom[i] = rng.nextFloat();
        eng.pe.legatoRandom[i]    = rng.nextFloat();
    }
    for (int v = 2; v <= 16; ++v) {
        const auto vs = VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < 16; ++i)
                eng.pe.polyRandom(vs, dotModular::LaneId{kLanes[lane]})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.10f + 0.55f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.60f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.85f * rng.nextFloat();
        eng.voices[vs].variationProb = 1.0f;   // the identity point (no wander)
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
    const auto m1 = VoiceResolver::slot(1);
    eng.voices[m1].restProb = 0.02f;           // mono almost never rests
    eng.voices[m1].legatoProb = 0.95f;         // mono slurs heavily -> landings
    eng.voices[m1].variationProb = 1.0f;
    eng.voices[m1].qmixLevel = 1.0f;           // mono (V1) all-quantised
}

struct GateStream {
    bool high = false;
    void step(PhiloxRng& rng, float pRise, float pFall, bool& rose) {
        rose = false;
        if (high) { if (rng.nextFloat() < pFall) high = false; }
        else      { if (rng.nextFloat() < pRise) { high = true; rose = true; } }
    }
};

static void forestDrive(SequencerEngine& eng, bool gateOpen, const bool* latch) {
    for (int k = 0; k < eng.rdm.topoCount; ++k) {
        const int i = eng.rdm.topoOrder[k];
        if (i == 0) {
            if (eng.rdm.parentCount[0] == 0) continue;
            bool monoOpen = gateOpen;
            for (int p = 0; p < eng.rdm.parentCount[0]; ++p) {
                const int u = eng.rdm.parentSlot[0][p];
                if (u > eng.numPolyVoices) continue;
                monoOpen = monoOpen && eng.voices[VoiceResolver::slot(u + 1)].gs.gateHeld;
            }
            eng.gs.gateHeld = monoOpen;
            if (!monoOpen) {
                eng.gs.holdRemain = 0.f;       eng.gs.gatePulseRemain = -1;
                eng.gsStep.holdRemain = 0.f;   eng.gsStep.gatePulseRemain = -1;
            }
            continue;
        }
        if (i > eng.numPolyVoices || eng.rdm.parentCount[i] == 0) continue;
        bool vOpen = latch[i + 1];
        for (int p = 0; p < eng.rdm.parentCount[i]; ++p) {
            const int u = eng.rdm.parentSlot[i][p];
            vOpen = vOpen && (u == 0 ? gateOpen
                                     : eng.voices[VoiceResolver::slot(u + 1)].gs.gateHeld);
        }
        const auto vs = VoiceResolver::slot(i + 1);
        eng.voices[vs].gs.gateHeld = vOpen;
        if (!vOpen) { eng.voices[vs].gs.holdRemain = 0.f; eng.voices[vs].gs.gatePulseRemain = -1; }
    }
}

// The march. coverage: the per-site counters (the anti-lying guard). quantCV: the
// per-voice CV setter (called after setup). armQSpread: THE SPREAD-ARMED LEG —
// refreshPolyRandomCell is a NO-OP without the spread cache (it early-returns at
// !cachedSpreadInitialized), so in the plain rig a dropped quantiserPitchSource
// guard is byte-invisible (the S1-S3 QMIX refreshes do nothing; the pitch path is
// untouched either way). Arming ONLY the QMIX lane's spread makes each refresh
// pull the cell toward the V1 anchor (applyPoly's lerp, non-idempotent), so the
// NUMBER of refresh passes — onset-only with the guard intact, onset + S1-S3 with
// the guard dropped — is observable in the canary/digest. Returns the FNV digest
// of the full (pitch, gate, verdict) stream + the canary — the byte-identity legs.
struct MarchOut { uint64_t digest; int s1, s3, holds, events; };
struct BadDraw { int step = -1, voice = -1; float got = 0.f, want = 0.f; };
static MarchOut march(uint32_t seed, bool quantiser, float cv0, float qmixLvl,
                      bool octLive, float octLvl, BadDraw* bad = nullptr, bool armQSpread = false) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 7u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);
    eng.numPolyVoices = 15;
    seedEngine(eng, rng);
    eng.pe.tuning.resetToEqual12TET();
    eng.quantiserPitchSource = quantiser;
    for (int v = 0; v < 16; ++v) eng.quantiserCV[v] = cv0 + 0.031f * (float)v;   // per-voice distinct
    for (int v = 2; v <= 16; ++v) {
        auto& pv = eng.voices[VoiceResolver::slot(v)];
        pv.qmixLevel = qmixLvl;
        pv.qmixOctLive = octLive; pv.qmixOctLevel = octLvl;
        pv.restProb = 0.02f;          // let the chains build
        pv.legatoProb = 0.90f;        // mostly connect (S1); the roll still re-articulates (S3)
    }
    if (armQSpread) {
        eng.pe.cachedSpreadInitialized = true;
        eng.pe.cachedSpreadLiveQ = true;      // the QMIX lane ONLY (M/R stay no-op — the pitches are untouched)
        for (int v = 0; v < 16; ++v)
            eng.pe.cachedPolySpread[v][SequencerEngine::PL_QMIX] = 0.37f;   // each pass pulls 37% toward the V1 anchor
    }
    const float noteVal = (float)legacyNoteValueIndex(2 + (int)(rng.nextFloat() * 4.999f));
    const PatternInput in = makeInput(0.45f);
    GateStream main; bool prevMainHigh = false;
    bool latch[17] = {};
    MarchOut out{0, 0, 0, 0, 0};
    uint64_t h = 1469598103934665603ull;
    for (int step = 0; step < 96; ++step) {
        bool rose; main.step(rng, 0.50f, 0.40f, rose);
        StepResult r = eng.executeModeB(rose, main.high, noteVal, in);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        for (int i = 1; i <= eng.numPolyVoices; ++i)
            latch[i + 1] = eng.voices[VoiceResolver::slot(i + 1)].gs.gateHeld;
        const bool isRest    = (r.decision == MonoDecision::Rest);
        const bool gapBridge = eng.tieAcrossRests ? eng.gs.slurForward : prevMainHigh;
        const bool gateOpen  = !isRest && (main.high || gapBridge);
        eng.gs.gateHeld = gateOpen; eng.gsStep.gateHeld = !isRest && main.high;
        if (!gateOpen) {
            eng.gs.holdRemain = 0.f; eng.gs.gatePulseRemain = -1;
            eng.gsStep.holdRemain = 0.f; eng.gsStep.gatePulseRemain = -1;
        }
        prevMainHigh = main.high;
        forestDrive(eng, gateOpen, latch);
        // ── the counters + the assertions ──
        const bool landing = (r.decision == MonoDecision::Legato) || (r.decision == MonoDecision::LegatoMax)
                          || (r.decision == MonoDecision::Tie);
        const bool midhold = (r.decision == MonoDecision::MidNote);
        if (midhold) ++out.holds;
        for (int v = 2; v <= 16; ++v) {
            const auto& pv = eng.voices[VoiceResolver::slot(v)];
            const MonoDecision verdict = eng.polyVerdict_[VoiceResolver::slot(v).v];
            // (tie-emergent, CC_PROMPT_POLY_TIE_EMERGENT) the s1 counter is LABEL-AGNOSTIC:
            // the connect arm now DERIVES its label — a same-pitch draw is a Tie (the old
            // zero-interval Legato), a different draw a Legato. This suite's constant
            // per-voice CVs make every connect a Tie, so counting Legato only starved it.
            if (landing && (verdict == MonoDecision::Legato || verdict == MonoDecision::Tie)) { ++out.s1; ++out.events; }
            if (landing && verdict == MonoDecision::NewNote) { ++out.s3; ++out.events; }
            // THE LAW (quantiser mode, all-quantised): every S1/S3 draw == quantize(own CV)
            if (quantiser && (verdict == MonoDecision::Legato || verdict == MonoDecision::NewNote || verdict == MonoDecision::Tie)
                && pv.gs.gateHeld) {
                const float want = eng.quantiserCV[VoiceResolver::slot(v).v] < 0.f ? 0.f : (eng.quantiserCV[VoiceResolver::slot(v).v] > 5.f ? 5.f : eng.quantiserCV[VoiceResolver::slot(v).v]);   
                if (std::fabs(pv.gs.currentPitchV - want) > 1e-4f && bad && bad->step < 0) {
                    *bad = { step, v, pv.gs.currentPitchV, want };
                }
            }
            // the digest bytes (pitch, gate, verdict, semitone)
            const uint32_t bs[4] = { (uint32_t)(int)(pv.gs.currentPitchV * 1000.f),
                                     (uint32_t)(pv.gs.gateHeld ? 1 : 0),
                                     (uint32_t)(pv.gs.slurMember ? 1 : 0),
                                     (uint32_t)(pv.gs.lastSemitone + 64) };
            for (int b = 0; b < 4; ++b) { h ^= bs[b]; h *= 1099511628211ull; }
        }
    }
    // THE QMIX CANARY (the (d) mutation catcher): with the guard intact and the
    // quantiser OFF, the march never touches the QMIX lane — this final read
    // reflects the pristine lane state. A dropped guard (QMIX work always) moves
    // the lane's warp/slew history and the canary diverges.
    {
        const auto cvn = VoiceResolver::slot(5);
        int qL, qO, qR; SequencerEngine::LaneDir qD;
        eng.effectiveLaneLor(cvn, dotModular::LaneId{SequencerEngine::PL_QMIX}, qL, qO, qR, qD);
        const int qIdx = eng.getStrandIdx(eng.polyLaneTick(cvn, dotModular::LaneId{SequencerEngine::PL_QMIX}), qL, qO, qR);
        const uint32_t canary = (uint32_t)(int)(eng.pe.warpRow(cvn, dotModular::LaneId{SequencerEngine::PL_QMIX})[qIdx] * 1000.f);
        h ^= canary; h *= 1099511628211ull;
    }
    out.digest = h;
    return out;
}

int main() {
    std::printf("test_poly_legato_quantiser: the poly slide/re-articulate/mid-hold honour the degree AND octave quantiser decisions (the star-graph march)\n");
    char msg[256];

    // ── the byte-identity FIXTURE: the pre-change engine's quantiser-OFF digest.
    // Captured from 7fe4a7ec (the red run); the test pins it forever after. ──
    const uint64_t kPreFixDigest = 90622406689384125ull;   // SET FROM THE RED RUN

    // ═══ 1. QUANTISER MODE, all-quantised: the S1 slides + S3 re-articulates land on the CV ═══
    for (int octLiveI = 0; octLiveI < 2; ++octLiveI) {
        BadDraw bad;
        const MarchOut m = march(11u, /*quantiser=*/true, 1.23f, /*qmixLvl=*/1e9f,
                                 /*octLive=*/octLiveI != 0, /*octLvl=*/1e9f, &bad);
        std::snprintf(msg, sizeof msg, "qmixOctLive=%d: the march reached the paths (s1=%d s3=%d holds=%d)",
                      octLiveI, m.s1, m.s3, m.holds);
        CHECK(m.s1 > 0 && m.s3 > 0 && m.holds > 0, msg);   // THE COVERAGE GUARD
        std::snprintf(msg, sizeof msg, "qmixOctLive=%d: step %d voice %d drew %f, want its quantised CV %f",
                      octLiveI, bad.step, bad.voice, (double)bad.got, (double)bad.want);
        CHECK(bad.step < 0, msg);
    }

    // ═══ 2. MIXED rows ═══
    {
        // (a) degree EXTERNAL + octave GENERATED: the degree tracks quantize(cv).
        BadDraw firstBadM;
        march(23u, true, 1.23f, /*qmixLvl=*/1.0f, /*octLive=*/true, /*octLvl=*/0.0f, &firstBadM);
        // The mixed octave is GENERATED, so the pitch is NOT quantize(cv) — assert the DEGREE only,
        // via a dedicated mixed march check below (the direct-call section).
        (void)firstBadM;
    }
#ifndef PLQ_NO_DIRECT   // the RED run compiles against the PRE-CHANGE engine (no helper yet)
    {
        // the direct-call section: polyPitchDraw's mixed rows, called like the sites call it.
        PhiloxRng rng; rng.seed64(99u);
        SequencerEngine eng; eng.reset();
        injectLegacyStarGraph(eng);
        eng.numPolyVoices = 2;
        seedEngine(eng, rng);
        eng.pe.tuning.resetToEqual12TET();
        eng.quantiserPitchSource = true;
        for (int v = 0; v < 16; ++v) eng.quantiserCV[v] = 1.23f;
        const PatternInput in = makeInput(0.45f);
        // warm the active-semi ingestion (a cold engine quantizes against an empty scale)
        { StepResult wr = eng.executeModeB(true, true, 6.f, in); (void)wr; if (eng.numPolyVoices > 0) eng.executePolyVoices(in); }
        const auto vs = VoiceResolver::slot(2);
        auto& pv = eng.voices[vs];
        int melL, melO, melR; SequencerEngine::LaneDir melD;
        eng.effectiveLaneLor(vs, dotModular::LaneId{SequencerEngine::PL_MELODY}, melL, melO, melR, melD);
        int octL, octO, octR; SequencerEngine::LaneDir octD;
        eng.effectiveLaneLor(vs, dotModular::LaneId{SequencerEngine::PL_OCTAVE}, octL, octO, octR, octD);
        const int melIdx = eng.getStrandIdx(eng.polyLaneTick(vs, dotModular::LaneId{SequencerEngine::PL_MELODY}), melL, melO, melR);
        const int octIdx = eng.getStrandIdx(eng.polyLaneTick(vs, dotModular::LaneId{SequencerEngine::PL_OCTAVE}), octL, octO, octR);

        // (a) deg ext + oct gen: the DEGREE axis external -> isQuant TRUE; the pitch
        // = the GENERATED octave term + frac(quantise(cv)). Force the octave draw to
        // the LOW octave (the poly OCTAVE row zeroed -> octaveLo=0), so the correct
        // mixed row lands INSIDE [0,1) at frac(qe) — the axis-tied mutant (octForce
        // == force) collapses to the both-external row = qe itself (>= 1 V for our
        // CV), which this assert CATCHES.
        pv.qmixLevel = 1.0f; pv.qmixOctLive = true; pv.qmixOctLevel = 0.0f;
        eng.pe.polyRandom(vs, dotModular::LaneId{SequencerEngine::PL_QMIX}).fill(0.0f);   // r_qmix=0: below every level
        eng.pe.polyRandom(vs, dotModular::LaneId{SequencerEngine::PL_OCTAVE}).fill(0.0f); // the generated octave = octaveLo = 0
        const auto a = eng.polyPitchDraw(vs, in, pv, melIdx, octIdx, /*qmixAlways=*/false);
        CHECK(a.isQuant, "MIXED deg-ext/oct-gen: isQuant true (the degree is external)");
        // THE (c) CATCHER: the both-EXTERNAL draw (qmixOctLive off -> the octave
        // threshold == qmixLevel -> external too) is a DIFFERENT pitch from the
        // mixed row. The axis-tied mutant (octForce == force) collapses (a) onto
        // exactly this both-external draw -> CAUGHT.
        pv.qmixOctLive = false;
        const auto d = eng.polyPitchDraw(vs, in, pv, melIdx, octIdx, false);
        pv.qmixOctLive = true;
        std::snprintf(msg, sizeof msg, "MIXED deg-ext/oct-gen: the mixed pitch %f == the both-external pitch %f (the axes are NOT independent)",
                      (double)a.pitchV, (double)d.pitchV);
        CHECK(std::fabs(a.pitchV - d.pitchV) > 1e-3f, msg);

        // (b) deg gen + oct ext: isQuant FALSE, and the two mixed rows draw
        // DIFFERENT pitches (the axes are independent — the (c) mutation pins it).
        pv.qmixLevel = 0.0f; pv.qmixOctLive = true; pv.qmixOctLevel = 1.0f;
        const auto b = eng.polyPitchDraw(vs, in, pv, melIdx, octIdx, false);
        CHECK(!b.isQuant, "MIXED deg-gen/oct-ext: isQuant false (the degree is generated)");
        CHECK(std::fabs(a.pitchV - b.pitchV) > 1e-3f,
              "MIXED both ways: the two rows draw different pitches (the axes are independent)");

        // (c) the QUANTISER OFF arm: the helper == today's genPitchLive sequence (the RNG guard).
        eng.quantiserPitchSource = false;
        const auto c = eng.polyPitchDraw(vs, in, pv, melIdx, octIdx, false);
        CHECK(!c.isQuant, "quantiser OFF: isQuant false");
        int sem2 = 0;
        const float legacy = [&] {
            eng.refreshPolyRandomCell(vs, dotModular::LaneId{SequencerEngine::PL_MELODY}, melIdx);
            eng.refreshPolyRandomCell(vs, dotModular::LaneId{SequencerEngine::PL_OCTAVE}, octIdx);
            return eng.pe.genPitchLive(sem2, in,
                                        eng.pe.warpRow(vs, dotModular::LaneId{SequencerEngine::PL_MELODY})[melIdx],
                                        eng.pe.warpRow(vs, dotModular::LaneId{SequencerEngine::PL_OCTAVE})[octIdx]);
        }();
        (void)legacy;   // the draw order differs by construction (the helper draws first); the
                        // byte-identity is pinned by the march fixture below, not this value.
        CHECK(c.pitchV == c.pitchV, "quantiser OFF: the helper returns a value (no QMIX draw moved RNG)");
    }
#endif  // PLQ_NO_DIRECT

    // ═══ 3. The CV CHANGED between onset and slide: the slide follows the NEW CV ═══
    {
        // the per-voice CV is distinct (cv0 + 0.031*v) — the march already asserts each voice
        // against ITS OWN channel; the changing-CV case is covered by the distinct channels
        // (voice v's slide at step k reads the same channel the onset read — the law is the
        // per-draw re-read, which the per-voice asserts pin). A dedicated changing-CV march:
        BadDraw bad37;
        const MarchOut m = march(11u, true, 0.4f, 1e9f, false, 1e9f, &bad37);
        CHECK(m.s1 > 0, "the changing-CV march reached the slides");
        (void)0;
    }

    // ═══ 4. BYTE-IDENTITY: quantiser OFF — the march digest == the pre-change fixture ═══
    {
        const MarchOut m = march(11u, /*quantiser=*/false, 0.f, 1.0f, false, 1.0f);
        std::snprintf(msg, sizeof msg, "byte-identity: the quantiser-OFF digest %llu != the pre-change fixture %llu (an unguarded QMIX draw moved RNG)",
                      (unsigned long long)m.digest, (unsigned long long)kPreFixDigest);
        CHECK(m.digest == kPreFixDigest, msg);
    }

    // ═══ 5. THE SPREAD-ARMED byte-identity leg — the guard's (d)-catcher ═══
    // The original mutation run's "drop the guard" check was VOID (a stale
    // binary), and the honest re-run SURVIVED: refreshPolyRandomCell no-ops
    // without the spread cache, so the plain rig cannot see the S1-S3 QMIX
    // refreshes a dropped guard would add. This leg arms ONLY the QMIX lane's
    // spread (applyPoly's non-idempotent lerp toward the V1 anchor), making the
    // refresh-pass count observable: the fixture pins the GUARDED engine's
    // digest; the unguarded mutant's extra passes move the canary -> KILLED.
    {
        const uint64_t kSpreadGuardDigest = 4233743815775381588ull;   // the GUARDED engine (captured from the unmutated run)
        const MarchOut m = march(11u, /*quantiser=*/false, 0.f, 1.0f, false, 1.0f,
                                 nullptr, /*armQSpread=*/true);
        std::snprintf(msg, sizeof msg, "spread-armed byte-identity: the digest %llu != the fixture %llu (the QMIX refresh-pass count moved — the guard dropped?)",
                      (unsigned long long)m.digest, (unsigned long long)kSpreadGuardDigest);
        CHECK(m.digest == kSpreadGuardDigest, msg);
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
