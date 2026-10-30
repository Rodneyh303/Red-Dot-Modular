// test_step_advance.cpp — the gate-mode step-advance feature (CC_PROMPT_GATE_STEP_ADVANCE_SPEC,
// Stages 1+3; Stage 2's bipolar branch is implemented in the pure fn and table-pinned, but no
// [0,1] source can reach it this build — it is a sourcing change, not a logic change).
//
// What is pinned:
//   PURE     : the decideAdvance table — p=1 → always FORWARD; p=0 → always REPEAT; mid → ratio
//              (FORWARD iff u < p); p<0 → BACKWARD iff u < |p| (Stage 2, reserved/unreachable).
//   IDENTITY : the GOLDEN PIN — at p_advance = 1 (V1's resolved Variation = 1) plain Mode B and
//              Mode B subdivided marches are BYTE-IDENTICAL to the pre-feature engine. The two
//              digests below were captured on the PRE-CHANGE source (24 seeded marches x 64 steps
//              through the REAL executors + the shipped IMPL 2b drive; see
//              scripts/capture_step_advance_identity.cpp) and must never move again — they are
//              the identity-with-today regression gate. Only p<1 changes behaviour.
//   RULE     : the engine's playhead trajectory == the mirrored rule, rise by rise — the test
//              re-computes u from the SAME stream at the SAME counter (PatternEngine::
//              stepRateUniform + stepAdvanceRiseCtr_) and predicts every move/no-move exactly.
//   HOLD     : at p=0 the playhead never moves after the seed and the held step re-outputs its
//              step-addressed data identically (the corrected REPEAT semantics, Rodney 91b100c:
//              NO re-draw branch — not advancing = same step = same counter = same data).
//   TIERS    : the THREE-LEVEL hierarchy — main/ratchet/ghost edges decide on their OWN resolved
//              columns (subRes[level][ADVANCE] behind the subResLive gate; main =
//              voices[mono].variationProb); a coincident main+ratchet rise IS the main onset;
//              the follow-main degenerate (subRes filled with the main value) is behaviourally
//              identical to the ungated engine.
//   SCOPE    : clock/phase (executeModeA) untouched — the playhead advances unconditionally there
//              (Variation keeps its existing clock/phase meaning; p_advance is gate-mode only).
//
// Build (see test/run_all.sh, entry "test_step_advance|$SE $GS $PE $CE").

#include <cstdio>
#include <cstdint>
#include <vector>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
#include "star_seam.hpp"

using redDot::PhiloxRng;
using SA = dotModular::StepAdvance;   // the pure decision struct (SA::decide / SA::FORWARD ...)
namespace SK = dotModular::SubgateKnobs;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── FNV-1a 64 + capture stream (the differential harness's discipline) ──────────
static uint64_t fnv1a(uint64_t h, uint8_t b) { return (h ^ (uint64_t)b) * 0x100000001b3ULL; }
struct Capture {
    std::vector<uint8_t> bytes;
    void u8(unsigned v)  { bytes.push_back((uint8_t)(v & 0xFFu)); }
    void i16(int v) {
        int16_t s = (int16_t)v;
        bytes.push_back((uint8_t)((s >> 0) & 0xFF));
        bytes.push_back((uint8_t)((s >> 8) & 0xFF));
    }
    uint64_t digest() const {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < bytes.size(); ++i) h = fnv1a(h, bytes[i]);
        return h;
    }
};

static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO
};

// The differential test's seedEngine, with V1's variationProb a PARAMETER (the advance
// threshold lives on the mono slot — the playhead's own row).
static void seedEngine(SequencerEngine& eng, PhiloxRng& rng, float v1VariationProb) {
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
        const auto vs = dotModular::VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < 16; ++i)
                eng.pe.polyRandom(vs, dotModular::LaneId{kLanes[lane]})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.10f + 0.55f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.60f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.85f * rng.nextFloat();
        eng.voices[vs].variationProb = 0.25f + 0.50f * rng.nextFloat();
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
    const auto m1 = dotModular::VoiceResolver::slot(1);
    eng.voices[m1].restProb      = 0.10f + 0.55f * rng.nextFloat();
    eng.voices[m1].accentProb    = 0.60f * rng.nextFloat();
    eng.voices[m1].legatoProb    = 0.85f * rng.nextFloat();
    eng.voices[m1].variationProb = v1VariationProb;
    eng.voices[m1].qmixLevel     = rng.nextFloat();
}

static int q12(float pitchV) { return (int)std::lrintf((double)pitchV * 12.0); }

static void captureStep(const SequencerEngine& eng, const StepResult& r, Capture& cap) {
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(eng.gs.gateHeld ? 1u : 0u);
    cap.u8(eng.gsStep.gateHeld ? 1u : 0u);
    cap.i16(q12(eng.gs.currentPitchV));
    cap.u8((unsigned)eng.stepIndex & 0xFFu);
    for (int v = 2; v <= 16; ++v) {
        const auto& pv = eng.voices[dotModular::VoiceResolver::slot(v)];
        cap.u8(pv.gs.gateHeld ? 1u : 0u);
        cap.u8(pv.gsStep.gateHeld ? 1u : 0u);
        cap.u8(pv.accented ? 1u : 0u);
        cap.u8(pv.participating ? 1u : 0u);
        cap.i16(q12(pv.gs.currentPitchV));
    }
}

// Shared F-3/C5 forest drive (mirrors Monsoon.cpp — same as the differential harness).
static void forestDrive(SequencerEngine& eng, bool gateOpen, const bool* latch) {
    for (int k = 0; k < eng.rdm.topoCount; ++k) {
        const int i = eng.rdm.topoOrder[k];
        if (i == 0) {
            if (eng.rdm.parentCount[0] == 0) continue;
            bool monoOpen = gateOpen;
            for (int p = 0; p < eng.rdm.parentCount[0]; ++p) {
                const int u = eng.rdm.parentSlot[0][p];
                if (u > eng.numPolyVoices) continue;
                monoOpen = monoOpen && eng.voices[dotModular::VoiceResolver::slot(u + 1)].gs.gateHeld;
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
                                     : eng.voices[dotModular::VoiceResolver::slot(u + 1)].gs.gateHeld);
        }
        const auto vs = dotModular::VoiceResolver::slot(i + 1);
        eng.voices[vs].gs.gateHeld = vOpen;
        if (!vOpen) { eng.voices[vs].gs.holdRemain = 0.f; eng.voices[vs].gs.gatePulseRemain = -1; }
    }
}

static const int kMarches = 24;
static const int kSteps   = 64;

struct GateStream {
    bool high = false;
    void step(PhiloxRng& rng, float pRise, float pFall, bool& rose) {
        rose = false;
        if (high) { if (rng.nextFloat() < pFall) high = false; }
        else      { if (rng.nextFloat() < pRise) { high = true; rose = true; } }
    }
};

// ── IDENTITY marches (must byte-match the PRE-CHANGE capture — do not touch) ────
static const uint64_t ADV_IDENTITY_B  = 0x7fde9525731c67e2ULL;
static const uint64_t ADV_IDENTITY_BS = 0xb946a03b9c7e06a0ULL;

static void marchModeB_p1(int seed, Capture& cap) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 1u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);
    eng.numPolyVoices = 15;
    seedEngine(eng, rng, 1.0f);   // THE IDENTITY POINT: p_advance = 1 -> always advance
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    GateStream main; bool prevMainHigh = false;
    bool latch[17] = {};
    for (int step = 0; step < kSteps; ++step) {
        bool rose; main.step(rng, 0.50f, 0.40f, rose);
        StepResult r = eng.executeModeB(rose, main.high, noteVal, in);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        for (int i = 1; i <= eng.numPolyVoices; ++i)
            latch[i + 1] = eng.voices[dotModular::VoiceResolver::slot(i + 1)].gs.gateHeld;
        const bool isRest    = (r.decision == MonoDecision::Rest);
        const bool gapBridge = eng.tieAcrossRests ? eng.gs.slurForward : prevMainHigh;
        const bool gateOpen  = !isRest && (main.high || gapBridge);
        eng.gs.gateHeld     = gateOpen;
        eng.gsStep.gateHeld = !isRest && main.high;
        if (!gateOpen) {
            eng.gs.holdRemain = 0.f;       eng.gs.gatePulseRemain = -1;
            eng.gsStep.holdRemain = 0.f;   eng.gsStep.gatePulseRemain = -1;
        }
        prevMainHigh = main.high;
        forestDrive(eng, gateOpen, latch);
        captureStep(eng, r, cap);
    }
}

static void marchModeBS_p1(int seed, Capture& cap) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 2u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);
    eng.numPolyVoices = 15;
    seedEngine(eng, rng, 1.0f);
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    GateStream main, ghost; bool prevMainHigh = false;
    bool latch[17] = {};
    for (int step = 0; step < kSteps; ++step) {
        bool mainRose; main.step(rng, 0.45f, 0.35f, mainRose);
        const bool ratchetRise = main.high && (rng.nextFloat() < 0.45f);
        const bool inGap = !main.high && !prevMainHigh;
        bool ghostRose = false;
        if (inGap) ghost.step(rng, 0.40f, 0.50f, ghostRose);
        StepResult r = eng.executeModeBSubdivided(mainRose, main.high, ratchetRise, noteVal, in,
                                                  ghostRose, ghost.high);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        for (int i = 1; i <= eng.numPolyVoices; ++i)
            latch[i + 1] = eng.voices[dotModular::VoiceResolver::slot(i + 1)].gs.gateHeld;
        const bool isRest        = (r.decision == MonoDecision::Rest);
        const bool ghostSounding = eng.ghostActive && ghost.high;
        const bool gapBridge     = eng.tieAcrossRests ? eng.gs.slurForward : prevMainHigh;
        const bool gateOpen      = !isRest && (main.high || ghostSounding || gapBridge);
        eng.gs.gateHeld     = gateOpen;
        eng.gsStep.gateHeld = !isRest && (main.high || ghostSounding);
        if (!gateOpen) {
            eng.gs.holdRemain = 0.f;       eng.gs.gatePulseRemain = -1;
            eng.gsStep.holdRemain = 0.f;   eng.gsStep.gatePulseRemain = -1;
        }
        prevMainHigh = main.high;
        forestDrive(eng, gateOpen, latch);
        captureStep(eng, r, cap);
    }
}

// ── RULE march: plain Mode B, deterministic gates, the trajectory predicted exactly ──
// Mirrors the engine rule per rise: seed at stepIndex==-1 (no draw); otherwise advance
// iff stepRateUniform(stepAdvanceRiseCtr_) < p — read BEFORE the executor consumes it.
static void ruleMarchInto(float p, int steps, Capture& cap, int* mismatches, int* advances);

// The instrumented predictor: per-rise checks that the engine's moves are exactly the rule's.
static void ruleMarch(float p, int steps) {
    PhiloxRng rng; rng.seed64(0x51E7Aull);
    SequencerEngine eng; eng.reset();
    eng.numPolyVoices = 0;   // the playhead rule is mono; keep the march minimal
    seedEngine(eng, rng, p);
    const PatternInput in = makeInput(0.5f);
    const float noteVal = 3.f;
    int mismatches = 0, heldRises = 0, advances = 0;
    for (int step = 0; step < steps; ++step) {
        // Deterministic gate: rise on even steps, high for one step, low for one.
        const bool rise = (step % 2) == 0;
        const bool high = rise;
        const int before = eng.stepIndex;
        const uint64_t ctrBefore = eng.stepAdvanceRiseCtr_;
        bool expectAdvance = false;
        if (rise && eng.stepIndex != -1)
            expectAdvance = (eng.pe.stepRateUniform(eng.stepAdvanceRiseCtr_) < p);
        StepResult r = eng.executeModeB(rise, high, noteVal, in);
        if (!rise) { check(eng.stepAdvanceRiseCtr_ == ctrBefore, "RULE: no rise -> no draw consumed"); continue; }
        const bool seeded = (before == -1);
        const bool moved  = (eng.stepIndex != before);
        if (!seeded) {
            check(eng.stepAdvanceRiseCtr_ == ctrBefore + 1,
                  "RULE: a decision rise consumes exactly one draw");
            if (moved != expectAdvance) ++mismatches;
        }
        if (moved && !seeded) ++advances;
        if (!moved && !seeded) ++heldRises;
        (void)r;
    }
    check(mismatches == 0, "RULE: playhead trajectory == the mirrored decide rule, every rise");
    check(heldRises > 0, "RULE: the march actually exercised held (repeat) rises");
    check(advances > 0, "RULE: the march actually exercised forward advances");
}
// (helper so the determinism re-run can capture without recursing)
static void ruleMarchInto(float p, int steps, Capture& cap, int* mismatches, int* advances) {
    PhiloxRng rng; rng.seed64(0x51E7Aull);
    SequencerEngine eng; eng.reset();
    eng.numPolyVoices = 0;
    seedEngine(eng, rng, p);
    const PatternInput in = makeInput(0.5f);
    const float noteVal = 3.f;
    *mismatches = 0; *advances = 0;
    for (int step = 0; step < steps; ++step) {
        const bool rise = (step % 2) == 0;
        const bool high = rise;
        const int before = eng.stepIndex;
        StepResult r = eng.executeModeB(rise, high, noteVal, in);
        const bool seeded = (before == -1);
        const bool moved  = (eng.stepIndex != before);
        if (moved && !seeded) ++(*advances);
        cap.u8(rise ? 1u : 0u);
        cap.u8((unsigned)eng.stepIndex & 0xFFu);
        cap.u8((unsigned)r.decision);
    }
}

int main() {
    std::printf("test_step_advance — the gate-mode step-advance feature (Stages 1+3)\n");

    // ── PURE: the decideAdvance table ────────────────────────────────────────────
    {
        bool ok1 = true, ok0 = true, okMid = true, okNeg = true, okNeg1 = true;
        for (int i = 0; i < 64; ++i) {
            const float u = (float)i / 64.f;   // [0, 1)
            ok1    = ok1    && (SA::decide( 1.0f, u) == SA::FORWARD);
            ok0    = ok0    && (SA::decide( 0.0f, u) == SA::REPEAT);
            okMid  = okMid  && (SA::decide( 0.25f, u) == (u < 0.25f ? SA::FORWARD : SA::REPEAT));
            okNeg  = okNeg  && (SA::decide(-0.5f, u) == (u < 0.5f ? SA::BACKWARD : SA::REPEAT));
            okNeg1 = okNeg1 && (SA::decide(-1.0f, u) == SA::BACKWARD);
        }
        check(ok1,    "PURE: p=+1 -> FORWARD for every u in [0,1) (the identity point)");
        check(ok0,    "PURE: p=0 -> REPEAT for every u (stutter)");
        check(okMid,  "PURE: p=0.25 -> FORWARD iff u < p (mid -> ratio)");
        check(SA::decide(0.25f, 0.249999f) == SA::FORWARD && SA::decide(0.25f, 0.25f) == SA::REPEAT,
              "PURE: the threshold is inclusive-exact at u == p (u < p)");
        check(okNeg,  "PURE: p=-0.5 -> BACKWARD iff u < |p| (Stage 2 bipolar, reserved)");
        check(okNeg1, "PURE: p=-1 -> BACKWARD for every u in [0,1) (Stage 2 extreme)");
    }

    // ── IDENTITY: the golden pin at p=1 ─────────────────────────────────────────
    {
        Capture b, bs;
        for (int s = 0; s < kMarches; ++s) { marchModeB_p1(s, b); marchModeBS_p1(s, bs); }
        check(b.digest()  == ADV_IDENTITY_B,
              "IDENTITY: plain Mode B at p=1 is byte-identical to the pre-feature engine");
        check(bs.digest() == ADV_IDENTITY_BS,
              "IDENTITY: Mode B subdivided at p=1 is byte-identical to the pre-feature engine");
    }

    // ── RULE: the trajectory IS the mirrored rule (+ double-run determinism) ────
    {
        ruleMarch(0.618034f, 96);
        Capture c1, c2; int m1 = 0, a1 = 0, m2 = 0, a2 = 0;
        ruleMarchInto(0.618034f, 96, c1, &m1, &a1);
        ruleMarchInto(0.618034f, 96, c2, &m2, &a2);
        check(c1.digest() == c2.digest() && m1 == 0, "RULE: double-run determinism (same seed, fresh engines)");
    }

    // ── HOLD: p=0 -> the playhead never moves; the held step re-outputs itself ──
    {
        PhiloxRng rng; rng.seed64(0xA11CEull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, 0.0f);
        const PatternInput in = makeInput(0.5f);
        const float noteVal = 3.f;
        int firstDecision = -1;
        bool heldConst = true, pitchConst = true, movedAfterSeed = false;
        float firstPitch = 0.f;
        int parkedIdx = -1;
        for (int step = 0; step < 48; ++step) {
            const bool rise = (step % 2) == 0;
            StepResult r = eng.executeModeB(rise, rise, noteVal, in);
            if (!r.stepped) continue;
            if (firstDecision < 0) {
                firstDecision = (int)r.decision;
                firstPitch = eng.gs.currentPitchV;
            } else {
                heldConst = heldConst && ((int)r.decision == firstDecision);
                pitchConst = pitchConst && (eng.gs.currentPitchV == firstPitch);
            }
            if (parkedIdx < 0) parkedIdx = eng.stepIndex;
            else movedAfterSeed = movedAfterSeed || (eng.stepIndex != parkedIdx);
        }
        check(heldConst, "HOLD: the held step re-outputs the SAME decision every repeat rise");
        check(pitchConst, "HOLD: the held step re-outputs the SAME pitch (same step-addressed data)");
        check(!movedAfterSeed, "HOLD: p=0 -> the playhead never moves after the seed");
    }

    // ── TIERS: main/ratchet/ghost decide on their OWN columns ───────────────────
    {
        // variationProb = 0 (main + follow-main subgate columns all 0), but the RATCHET
        // column authored to 1 via the resolved store (subResLive). Deterministic edges:
        // per 4-step period — main rise at 0 (high 0,1), ratchet rise at 1, ghost rise
        // at 3 (in-gap). Expected: the playhead advances ONLY at ratchet rises.
        PhiloxRng rng; rng.seed64(0x71E25ull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, 0.0f);
        const auto m1 = dotModular::VoiceResolver::slot(1);
        eng.voices[m1].subResLive = true;
        for (int l = 0; l < SK::LEVELS; ++l)
            for (int d = 0; d < SK::DECS; ++d)
                eng.voices[m1].subRes[l][d] = 0.0f;          // everything follow-main-at-0
        eng.voices[m1].subRes[SK::RATCHET][SK::ADVANCE] = 1.0f;   // the authored ratchet tier
        const PatternInput in = makeInput(0.5f);
        const float noteVal = 3.f;
        int advances = 0, seedAdvances = 0, wrongSite = 0;
        for (int step = 0; step < 64; ++step) {
            const int ph = step % 4;
            const bool mainRise  = (ph == 0);
            const bool mainHigh  = (ph == 0 || ph == 1);
            const bool ratchRise = (ph == 1);           // in-gate, non-coincident
            const bool ghostRise = (ph == 3);           // in-gap (prev low, main low)
            const int before = eng.stepIndex;
            StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, ratchRise, noteVal, in,
                                                      ghostRise, ghostRise);
            (void)r;
            if (eng.stepIndex != before) {
                if (before == -1) ++seedAdvances;
                else { ++advances; if (ph != 1) ++wrongSite; }   // only ratchet sites may move it
            }
        }
        check(wrongSite == 0, "TIERS: the playhead moved ONLY at ratchet edges (own column governs)");
        check(advances == 16, "TIERS: exactly one advance per ratchet rise (16 ratchet rises, all post-seed)");
        check(seedAdvances == 1, "TIERS: the seed advanced exactly once");
    }

    // ── TIERS (follow-main degenerate): subRes filled with the main values behaves
    //    identically to the ungated engine (the subResLive gate's byte-identity). ──
    {
        auto run = [&](bool live) {
            PhiloxRng rng; rng.seed64(0xBEEF5ull);
            SequencerEngine eng; eng.reset();
            eng.numPolyVoices = 0;
            seedEngine(eng, rng, 0.4f);
            const auto m1 = dotModular::VoiceResolver::slot(1);
            if (live) {
                eng.voices[m1].subResLive = true;
                for (int l = 0; l < SK::LEVELS; ++l)
                    for (int d = 0; d < SK::DECS; ++d)
                        eng.voices[m1].subRes[l][d] = 0.4f;   // == the main value everywhere
            }
            const PatternInput in = makeInput(0.5f);
            Capture cap;
            int adv = 0;
            for (int step = 0; step < 64; ++step) {
                const bool rise = (step % 3) == 0;
                const int before = eng.stepIndex;
                StepResult r = eng.executeModeB(rise, rise, 3.f, in);
                if (rise && before != -1 && eng.stepIndex != before) ++adv;
                cap.u8(rise ? 1u : 0u);
                cap.u8((unsigned)eng.stepIndex & 0xFFu);
                cap.u8((unsigned)r.decision);
            }
            struct R { uint64_t d; int a; };
            return R{cap.digest(), adv};
        };
        const auto off = run(false);
        const auto on  = run(true);
        check(off.d == on.d, "TIERS: follow-main degenerate (subRes == main) == ungated engine (digest)");
        check(off.a == on.a, "TIERS: follow-main degenerate == ungated engine (trajectory)");
    }

    // ── SCOPE: clock/phase (executeModeA) is untouched — the playhead advances
    //    unconditionally there even at variationProb = 0 ─────────────────────────
    {
        PhiloxRng rng; rng.seed64(0xC10Cull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, 0.0f);
        const PatternInput in = makeInput(0.5f);
        redDot::RateManager rm;
        bool alwaysMoved = true;   // the ring wraps every 16 — assert MOVEMENT, not monotonicity
        for (int step = 0; step < 20; ++step) {
            const int before = eng.stepIndex;
            rm.advance(redDot::Rate::Step, true);
            eng.executeModeA(rm, 3.f, in, +1);
            if (eng.stepIndex == before) alwaysMoved = false;
        }
        check(alwaysMoved, "SCOPE: clock/phase advances unconditionally (Variation meaning unchanged there)");
    }

    std::printf("  passed %d, failed %d\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
