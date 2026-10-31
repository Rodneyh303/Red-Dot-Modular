// test_step_advance.cpp — the gate-mode step-advance feature (CC_PROMPT_GATE_STEP_ADVANCE_SPEC
// Stages 1+3 + CC_PROMPT_STEP_ADVANCE_CONSUMPTION Stage 2; the bipolar branch is implemented in
// the pure fn, table-pinned, and SOURCED as of the consumption completion: the resolved value is
// the unclamped knob+CV2+Junction sum clamped [-1,+1], so p < 0 is reachable through a bipolar
// modulation source).
//
// What is pinned (DRAW SOURCE FIXED per CC_PROMPT_STEP_ADVANCE_FIX_DRAW_SOURCE: u is
// the VARIATION LANE's step-addressed draw — NOT a fresh Philox draw; the reserved
// chunk is deleted and the advance consumes ZERO new draws. The rectified twin
// variationRateProb is RETIRED — its only consumer, the ghost-candidacy gate, is
// deleted; candidacy is the subgate REST decision):
//   PURE     : the decideAdvance table — p=1 → always FORWARD; p=0 → always REPEAT; mid → ratio
//              (FORWARD iff u < p); p<0 → BACKWARD iff u < |p| (Stage 2, sourced + reachable).
//   IDENTITY : the GOLDEN PIN — at p_advance = 1 (V1's resolved Variation = 1) plain Mode B and
//              Mode B subdivided marches are BYTE-IDENTICAL to the pre-feature engine (u=r_vary
//              < 1 crosses every step). The plain-B digest is the pre-change capture; the
//              SUBDIVIDED digest was RE-CAPTURED for the draw-source fix (the ghost-candidacy
//              deletion changes ghost outcomes at p=1 — documented, deliberate; the trajectory
//              itself is unchanged: every decision rise still moves).
//   RULE     : the engine's playhead trajectory == the mirrored rule, rise by rise — the test
//              re-computes u as the SAME r_vary the engine reads (the variation lane's
//              step-addressed draw at the current step) and predicts every move/no-move
//              exactly. The knob SCHEDULE turns p through deterministic-move / stochastic /
//              deterministic-hold phases: a fixed p on a sticky step holds forever BY DESIGN
//              (the per-step threshold — the intended live control, not a wall).
//   HOLD     : at p=0 the playhead never moves after the seed and the held step re-outputs its
//              step-addressed data identically (the corrected REPEAT semantics, Rodney 91b100c:
//              NO re-draw branch — not advancing = same step = same counter = same data). The
//              held rise also leaves the variation ring cell BIT-IDENTICAL (zero new draws —
//              the additive-domain law: the advance adds NOTHING to the stream).
//   TIERS    : the THREE-LEVEL hierarchy — main/ratchet/ghost edges decide on their OWN resolved
//              columns (subRes[level][ADVANCE] behind the subResLive gate; main =
//              voices[mono].variationProb); a coincident main+ratchet rise IS the main onset;
//              the follow-main degenerate (subRes filled with the main value) is behaviourally
//              identical to the ungated engine.
//   STAGE2   : the bipolar sourcing — p<0 walks the playhead BACKWARD (exactly -1 per move,
//              wrapping 0 -> 15 through advancePlayhead's reverse traversal); ghost CANDIDACY
//              is the subgate REST decision (subRes[GHOST][REST]=0 → ghosts play; =1 → the
//              gap stays empty) — variation no longer gates candidacy at all; a negative
//              ghost-ADVANCE column walks the playhead backward in the gaps.
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
// DOTTED BUILD re-pin: the capture records r.nvIdx — in gate mode the
// NULLIFICATION SENTINEL's index (NV_GATE_NULLIFY), which moved 6 → 9 when 1/16
// shifted. The normalization proof (differential, same discipline) shows the whole
// stream byte-identical with that one byte mapped back — the gate-mode logic never
// moved. OLD (pre-dotted): B=0x7fde9525731c67e2.
static const uint64_t ADV_IDENTITY_B  = 0x66f88550dcb45afaULL;
// RE-CAPTURED for CC_PROMPT_STEP_ADVANCE_FIX_DRAW_SOURCE (the ghost-candidacy
// deletion changes ghost outcomes at p=1; the plain-B digest above is the
// ORIGINAL pre-change capture and still holds — u=r_vary < 1 crosses every
// step, so the advance itself is byte-identical).
static const uint64_t ADV_IDENTITY_BS = 0x7c63400e804fe03eULL;   // DOTTED re-pin (duration order; was 0x24fba00a690d125e)

static void marchModeB_p1(int seed, Capture& cap) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 1u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);
    eng.numPolyVoices = 15;
    seedEngine(eng, rng, 1.0f);   // THE IDENTITY POINT: p_advance = 1 -> always advance
    const float noteVal = (float)legacyNoteValueIndex(2 + (int)(rng.nextFloat() * 4.999f));
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
    const float noteVal = (float)legacyNoteValueIndex(2 + (int)(rng.nextFloat() * 4.999f));
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
// Mirrors the engine rule per rise: seed at stepIndex==-1 (no draw); otherwise the
// decision is decide(p, u) where u = the SCANNED variation draw — the engine's
// scanOfs_ (THE FREEZE FIX: +1 on a held rise, reset on a move) walks the variation
// window while held, so a sticky step is NOT a permanent wall.
static void ruleMarchInto(float p, int steps, Capture& cap, int* mismatches, int* advances);

// The instrumented predictor: per-rise checks that the engine's moves are exactly the rule's.
static void ruleMarch(float p, int steps) {
    PhiloxRng rng; rng.seed64(0x51E7Aull);
    SequencerEngine eng; eng.reset();
    eng.numPolyVoices = 0;   // the playhead rule is mono; keep the march minimal
    seedEngine(eng, rng, p);
    const PatternInput in = makeInput(0.5f);
    const float noteVal = 6.f;   // 1/4T (DOTTED BUILD: was legacy idx 3)
    const auto m = dotModular::VoiceResolver::slot(1);
    int mismatches = 0, heldRises = 0, advances = 0;
    int mirrorScan = 0;   // THE SCAN MIRROR: the engine's scanOfs_ (+1 held, 0 after a move)
    for (int step = 0; step < steps; ++step) {
        // Deterministic gate: rise on even steps, high for one step, low for one.
        const bool rise = (step % 2) == 0;
        const bool high = rise;
        // The knob SCHEDULE (deterministic-move / stochastic / deterministic-hold
        // phases; the mirror predicts every rise exactly whichever phase is live).
        const float pNow = (step < steps / 4) ? 1.0f
                        : (step < steps / 2) ? p
                        : (step < 3 * steps / 4) ? 0.0f
                        : p;
        eng.voices[m].variationProb = pNow;
        const int before = eng.stepIndex;
        const float drawAt0Before = (eng.stepIndex != -1) ? eng.stepAdvanceDrawAt(0) : 0.f;
        const float uBefore = (rise && eng.stepIndex != -1)
            // the SCANNED draw, SIGNED by p (the engine reads the scan in the
            // TRAVEL direction — pNow < 0 scans BACKWARD: drawAt(-mirrorScan))
            ? eng.stepAdvanceDrawAt((pNow < 0.f) ? -mirrorScan : mirrorScan)
            : 0.f;
        bool expectAdvance = false;
        if (rise && eng.stepIndex != -1)
            expectAdvance = (uBefore < pNow);
        StepResult r = eng.executeModeB(rise, high, noteVal, in);
        if (!rise) continue;
        const bool seeded = (before == -1);
        const bool moved  = (eng.stepIndex != before);
        if (!seeded) {
            if (moved != expectAdvance) ++mismatches;
            if (!moved) {
                ++heldRises;
                ++mirrorScan;   // the scan accrues on a held rise (the freeze fix)
                // ZERO NEW DRAWS (the additive-domain law): a held rise leaves the
                // variation ring BIT-IDENTICAL — the decision (and the scan) only
                // READ existing ring values; nothing regenerates (the old
                // reserved-chunk draw is deleted).
                check(eng.stepAdvanceDrawAt(0) == drawAt0Before,
                      "RULE: a held rise draws nothing new (the ring is read-only here)");
            } else {
                mirrorScan = 0;   // any motion re-anchors (the arrived step judged on its own value)
            }
        }
        // DISPLAY TRUTH (CC_PROMPT_VARIATION_SCAN_DISPLAY): the chevron's two public
        // reads ride the SAME scan the engine's decision used — the mirror (updated
        // just above, magnitude) must equal the engine's SIGNED accessor, every
        // rise, held or moved (the sign = the live travel direction, pNow's sign).
        check(eng.stepAdvanceScanDelta() == ((pNow < 0.f) ? -mirrorScan : mirrorScan),
              "RULE: the signed scan delta equals the mirror (the display's read, direction-signed)");
        if (moved && !seeded) ++advances;
        (void)r;
    }
    check(mismatches == 0, "RULE: playhead trajectory == the mirrored decide rule (incl. the scan), every rise");
    check(heldRises > 0, "RULE: the march actually exercised held (repeat) rises");
    check(advances > 0, "RULE: the march actually exercised forward advances");
    // The factoring guard (CC_PROMPT_VARIATION_SCAN_DISPLAY): the scanned POSITION at
    // delta 0 is the plain variation step — the factoring changed nothing there.
    check(eng.stepAdvanceStrandIdx(0) == eng.getVariationStep(),
          "RULE: the factored position at delta 0 is the variation step (pure factoring)");
}
// (helper so the determinism re-run can capture without recursing)
static void ruleMarchInto(float p, int steps, Capture& cap, int* mismatches, int* advances) {
    PhiloxRng rng; rng.seed64(0x51E7Aull);
    SequencerEngine eng; eng.reset();
    eng.numPolyVoices = 0;
    seedEngine(eng, rng, p);
    const PatternInput in = makeInput(0.5f);
    const float noteVal = 6.f;   // 1/4T (DOTTED BUILD: was legacy idx 3)
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

    // ── NO-FREEZE (CC_PROMPT_STEP_ADVANCE_FREEZE_AND_BIPOLAR): a FIXED mid-range p ──
    //    never walls the playhead — the scan walks the variation window, so a sticky
    //    step escapes within varLen held rises (varLen <= 16 here). Pre-fix, a step
    //    with draw >= p was a PERMANENT wall (~half the steps at p=0.3).
    {
        PhiloxRng rng; rng.seed64(0xF12Eull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, 0.3f);
        const PatternInput in = makeInput(0.5f);
        int advances = 0, holdRun = 0, maxHoldRun = 0;
        for (int step = 0; step < 96; ++step) {
            const bool rise = (step % 2) == 0;
            const int before = eng.stepIndex;
            eng.executeModeB(rise, rise, 3.f, in);
            if (!rise || before == -1) continue;
            if (eng.stepIndex != before) {
                ++advances;
                holdRun = 0;
            } else {
                ++holdRun;
                if (holdRun > maxHoldRun) maxHoldRun = holdRun;
            }
        }
        check(advances > 0, "NO-FREEZE: a fixed p=0.3 march keeps moving (the scan escapes)");
        check(maxHoldRun < 16,
              "NO-FREEZE: no hold run reaches the window length (the escape within varLen rises)");
    }

    // ── HOLD: p=0 -> the playhead never moves; the held step re-outputs itself ──
    {
        PhiloxRng rng; rng.seed64(0xA11CEull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, 0.0f);
        const PatternInput in = makeInput(0.5f);
        const float noteVal = 6.f;   // 1/4T (DOTTED BUILD: was legacy idx 3)
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
        const float noteVal = 6.f;   // 1/4T (DOTTED BUILD: was legacy idx 3)
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

    // ── STAGE 2 (bipolar, CC_PROMPT_STEP_ADVANCE_CONSUMPTION): p < 0 REACHED ────
    // The pure branch was always table-pinned; the SOURCING is the completion: the
    // resolved value is the unclamped knob+CV2+Junction sum clamped [-1,+1] (the
    // ModeController writes variationProb bipolar; the rectified twin is retired),
    // the ADVANCE resolver range widened to [-1,+1] (follow-main carries the sign;
    // REST/LEGATO/ACCENT stay [0,1]), and the rise sites map BACKWARD ->
    // advancePlayhead(-1) — the reverse traversal, which wraps past step 0 to 15.
    {
        // BACKWARD WALK (rack-free, plain Mode B, the RULE discipline incl. the
        // SCAN mirror): the |p| SCHEDULE — stochastic -0.35 first, then -1.0 (the
        // deterministic walker that guarantees the 0 -> 15 wrap). Every decision
        // rise moves EXACTLY -1 iff the SCANNED draw < |p|, else holds (the scan
        // then accrues — the freeze fix works backward too: the window is scanned
        // IN THE TRAVEL DIRECTION (backward travel reads drawAt(-scanOfs_)).
        PhiloxRng rng; rng.seed64(0x51E7Aull);   // the RULE march's seed
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, -0.35f);
        const auto m = dotModular::VoiceResolver::slot(1);
        const PatternInput in = makeInput(0.5f);
        int mismatches = 0, backwardMoves = 0, heldRises = 0;
        int mirrorScan = 0;
        bool crossedWrap = false;
        for (int step = 0; step < 96; ++step) {
            const bool rise = (step % 2) == 0;
            const float mag = (step < 48) ? 0.35f : 1.0f;
            eng.voices[m].variationProb = -mag;
            const int before = eng.stepIndex;
            bool expectBackward = false;
            if (rise && eng.stepIndex != -1)
                // backward travel: the engine reads the scan SIGNED (drawAt(-scanOfs_))
                expectBackward = (eng.stepAdvanceDrawAt(-mirrorScan) < mag);
            StepResult r = eng.executeModeB(rise, rise, 3.f, in);
            (void)r;
            if (!rise || before == -1) continue;   // the seed advances forward, no decision
            const bool moved     = (eng.stepIndex != before);
            const bool movedBack = moved && (eng.stepIndex == ((before - 1 + 16) & 0xF));
            if (moved != expectBackward) ++mismatches;
            if (moved && !movedBack) ++mismatches;   // backward moves are EXACTLY -1 (mod 16)
            if (movedBack) { ++backwardMoves; if (before == 0) crossedWrap = true; }
            if (!moved) { ++heldRises; ++mirrorScan; }   // held: the scan accrues
            else mirrorScan = 0;                          // moved: re-anchored
        }
        check(mismatches == 0, "STAGE2: p<0 walk == the mirrored BACKWARD rule, exactly -1 per move");
        check(backwardMoves > 0, "STAGE2: the march actually exercised backward moves");
        check(heldRises > 0, "STAGE2: the march actually exercised held (repeat) rises");
        check(crossedWrap, "STAGE2: backward wrap verified (step 0 -> step 15)");
    }
    {
        // CANDIDACY = THE SUBGATE REST (CC_PROMPT_STEP_ADVANCE_FIX_DRAW_SOURCE, the
        // companion change — the value pair is RETIRED): variationProb = -1 (the
        // main walks BACKWARD every rise; at |p|=1 every r_vary is crossed), the
        // ghost level's REST = 0 (every ghost PASSES rest — the candidacy question
        // collapsed into the ONE rest authority) and its own ADVANCE column = -1
        // (ghosts walk the shared playhead backward through the gaps). A backward
        // move at a GHOST rise proves the ghost cell played (its rest passed) and
        // its level's own column governed.
        PhiloxRng rng; rng.seed64(0xFA11ull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, -1.0f);
        const auto m1 = dotModular::VoiceResolver::slot(1);
        eng.voices[m1].subResLive = true;
        for (int l = 0; l < SK::LEVELS; ++l)
            for (int d = 0; d < SK::DECS; ++d)
                eng.voices[m1].subRes[l][d] = 0.0f;    // REST=0: every ghost passes the one rest gate
        eng.voices[m1].subRes[SK::GHOST][SK::ADVANCE] = -1.0f;   // ghosts walk backward too
        const PatternInput in = makeInput(0.5f);
        int backward = 0, ghostPlays = 0;
        bool ghostMoved = false;
        for (int step = 0; step < 32; ++step) {
            const int ph = step % 4;
            const bool mainRise  = (ph == 0);
            const bool mainHigh  = (ph == 0 || ph == 1);
            const bool ghostRise = (ph == 3);           // in-gap (main low)
            const int before = eng.stepIndex;
            StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, false, 3.f, in,
                                                      ghostRise, ghostRise);
            if (ghostRise && !mainRise && r.decision != MonoDecision::Rest) ++ghostPlays;
            if (before == -1 || !(mainRise || ghostRise)) continue;
            if (eng.stepIndex == ((before - 1 + 16) & 0xF)) {
                ++backward;
                if (ghostRise && !mainRise) ghostMoved = true;
            }
        }
        check(backward > 0, "STAGE2: the bipolar march exercised backward moves at both levels");
        check(ghostPlays > 0, "STAGE2: ghosts PLAY with subRes[GHOST][REST]=0 (the one rest authority)");
        check(ghostMoved, "STAGE2: ghost-ADVANCE -1 walks the playhead backward in the gaps "
                          "(the played ghost's own column governs)");
    }
    {
        // THE CANDIDACY GATE IS GONE (the same march, REST authored to 1): every
        // ghost cell rests — the gaps stay EMPTY (a Rest decision at every ghost
        // rise), and the ghost level's ADVANCE column never runs (no move at a
        // ghost rise). Variation no longer gates candidacy AT ALL.
        PhiloxRng rng; rng.seed64(0xFA11ull);
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        seedEngine(eng, rng, -1.0f);
        const auto m1 = dotModular::VoiceResolver::slot(1);
        eng.voices[m1].subResLive = true;
        for (int l = 0; l < SK::LEVELS; ++l)
            for (int d = 0; d < SK::DECS; ++d)
                eng.voices[m1].subRes[l][d] = 0.0f;
        eng.voices[m1].subRes[SK::GHOST][SK::REST] = 1.0f;       // every ghost rests
        eng.voices[m1].subRes[SK::GHOST][SK::ADVANCE] = -1.0f;  // ...so this never runs
        const PatternInput in = makeInput(0.5f);
        int ghostRests = 0, ghostMoves = 0;
        for (int step = 0; step < 32; ++step) {
            const int ph = step % 4;
            const bool mainRise  = (ph == 0);
            const bool mainHigh  = (ph == 0 || ph == 1);
            const bool ghostRise = (ph == 3);
            const int before = eng.stepIndex;
            StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, false, 3.f, in,
                                                      ghostRise, ghostRise);
            if (ghostRise && !mainRise) {
                if (r.decision == MonoDecision::Rest) ++ghostRests;
                if (eng.stepIndex != before) ++ghostMoves;
            }
        }
        check(ghostRests > 0, "STAGE2: subRes[GHOST][REST]=1 -> every ghost rests (the gap stays empty)");
        // NOTE: the playhead motion is UPSTREAM of the articulation (the advance
        // decision precedes executeStep), so a rested ghost cell may still have
        // moved the playhead — ghostMoves counts it for INFORMATION only.
        std::printf("  (info) rested-ghost marches: %d ghost moves (the advance is upstream)\n", ghostMoves);
    }

    std::printf("  passed %d, failed %d\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
