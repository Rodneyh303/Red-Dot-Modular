// test_rhythm_decision_differential.cpp — STAGE R, R-1: the reference oracle.
//
// RHYTHM_DECISION_MANAGER_PLAN.md §2.3 R-1: BEFORE any engine line moves, capture the
// per-voice rhythm-decision streams this engine produces, so every Stage-R centralisation
// commit (R-2..R-5) is gated BYTE-IDENTICAL, and Stage F's star-graph marches are gated
// against the same oracle.
//
// What it captures, per march step, after the executor cascade + the module-layer drive:
//   mono  : decision, nvIdx, accented, gs.gateHeld, gsStep.gateHeld, pitch (semitone-quantised)
//   poly  : per voice V2..V16 — gs.gateHeld, gsStep.gateHeld, accented, participating,
//           pitch (semitone-quantised)
// accumulated across 24 seeded marches x 64 steps and hashed into a per-mode FNV-1a 64
// GOLDEN DIGEST. The digests are the byte-identity pin for the whole refactor.
//
// Golden-independent STRUCTURAL invariants asserted per step (the oracle's teeth even
// before the digests are pinned):
//   INV-C2  : mono rests  ->  every poly voice's fused gate is closed (the V1-rest close;
//             there is no term that can reopen a closed envelope — the asymmetry itself).
//   INV-ENV : gate modes — a poly voice's open gate never exceeds the mono envelope
//             (post-AND: vOpen = gateOpen && latch; the C5 bound).
//   INV-DET : the whole battery runs TWICE on fresh engines from the same seeds ->
//             identical digests (determinism is the precondition for a digest oracle).
//
// Harness idioms follow test_subgate.cpp / test_gate_smoke.cpp: the REAL engine, the REAL
// executors (executeModeA / executeModeB / executeModeBSubdivided / executePolyVoices),
// and the module layer's IMPL 2b per-sample drive mirrored exactly as shipped
// (Monsoon.cpp:1162-1213: ghostSounding, gapBridge, gateOpen, the closed-gate zeroing,
// the polyVoiceActive latch + the C5 AND). Draws seeded via PhiloxRng (counter-based,
// integer-math -> platform-stable); pitches quantised to semitones for FP-hash safety.
//
// Build (see test/run_all.sh, entry "test_rhythm_decision_differential|$SE $GS $PE $CE"):
//   g++ -std=c++20 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/ui
//       test/test_rhythm_decision_differential.cpp
//       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp
//       src/dsp/gates/GateState.cpp src/dsp/engines/ClockEngine.cpp -o /tmp/trdd && /tmp/trdd

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
#include "star_seam.hpp"   // the D2 test seam (§3.1) — the star authored INTO pe.containmentParents

using redDot::PhiloxRng;   // the house counter-based RNG (namespace redDot)

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── FNV-1a 64 ───────────────────────────────────────────────────────────────────
static uint64_t fnv1a(uint64_t h, uint8_t b) { return (h ^ (uint64_t)b) * 0x100000001b3ULL; }

// ── The capture stream ──────────────────────────────────────────────────────────
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

// ── Seeding ─────────────────────────────────────────────────────────────────────
static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

// The seven draw lanes: the lorStore order (MEL,OCT,QMIX,REST,ACC,VAR,LEG).
static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO
};

static void seedEngine(SequencerEngine& eng, PhiloxRng& rng) {
    // Mono rows (the named aliases of the unified store, PatternEngine.hpp:123-129).
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]    = rng.nextFloat();
        eng.pe.octaveRandom[i]    = rng.nextFloat();
        eng.pe.qmixRandom[i]      = rng.nextFloat();
        eng.pe.rhythmRandom[i]    = rng.nextFloat();
        eng.pe.accentRandom[i]    = rng.nextFloat();
        eng.pe.variationRandom[i] = rng.nextFloat();
        eng.pe.legatoRandom[i]    = rng.nextFloat();
    }
    // Per-voice rows + thresholds, V2..V16 (slots are 1-based: slot(V) = voice V).
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
    // V1's own thresholds (the mono slot carries V1's PARAMS; the mono GATE state is eng.gs).
    const auto m1 = dotModular::VoiceResolver::slot(1);
    eng.voices[m1].restProb      = 0.10f + 0.55f * rng.nextFloat();
    eng.voices[m1].accentProb    = 0.60f * rng.nextFloat();
    eng.voices[m1].legatoProb    = 0.85f * rng.nextFloat();
    eng.voices[m1].variationProb = 0.25f + 0.50f * rng.nextFloat();
    eng.voices[m1].qmixLevel     = rng.nextFloat();
}

// ── Per-step capture + invariants ───────────────────────────────────────────────
static int q12(float pitchV) { return (int)std::lrintf((double)pitchV * 12.0); }

static void captureAndCheck(const SequencerEngine& eng, const StepResult& r, int gateOpen,
                            Capture& cap, int& c2n, int& envn) {
    // Mono.
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(eng.gs.gateHeld ? 1u : 0u);
    cap.u8(eng.gsStep.gateHeld ? 1u : 0u);
    cap.i16(q12(eng.gs.currentPitchV));
    // Poly (V2..V16 = slots 2..16).
    for (int v = 2; v <= 16; ++v) {
        const auto& pv = eng.voices[dotModular::VoiceResolver::slot(v)];
        cap.u8(pv.gs.gateHeld ? 1u : 0u);
        cap.u8(pv.gsStep.gateHeld ? 1u : 0u);
        cap.u8(pv.accented ? 1u : 0u);
        cap.u8(pv.participating ? 1u : 0u);
        cap.i16(q12(pv.gs.currentPitchV));
    }
    // INV-C2: the V1-rest close — mono rests -> every poly voice's fused gate is closed.
    if (r.decision == MonoDecision::Rest) {
        bool anyOpen = false;
        for (int v = 2; v <= 16; ++v)
            anyOpen = anyOpen || eng.voices[dotModular::VoiceResolver::slot(v)].gs.gateHeld;
        check(!anyOpen, "INV-C2: mono rests -> all poly gates closed");
        ++c2n;
    }
    // INV-ENV (gate modes only): a poly gate never exceeds the mono envelope (post-AND).
    // gateOpen < 0 marks "mode A — no per-sample envelope drive" and skips the check.
    if (gateOpen >= 0) {
        for (int v = 2; v <= 16; ++v) {
            const auto& pv = eng.voices[dotModular::VoiceResolver::slot(v)];
            check(!pv.gs.gateHeld || gateOpen == 1,
                  "INV-ENV: poly gate open implies mono envelope open");
            ++envn;
        }
    }
}

// ── The three marches (each appends to the mode's Capture) ──────────────────────
static const int kMarches = 24;
static const int kSteps   = 64;

// Mode A (clock origin): one Step-tier tick per march step, direction +1.
static void marchModeA(int seed, Capture& cap, int& c2n, int& envn) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);   // D2 seam: the legacy sound as an authored graph
    eng.numPolyVoices = 15;
    seedEngine(eng, rng);
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);   // 1/4 .. 1/32
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    redDot::RateManager rm;
    for (int step = 0; step < kSteps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = eng.executeModeA(rm, noteVal, in, +1);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        captureAndCheck(eng, r, /*gateOpen=*/-1, cap, c2n, envn);
    }
}

// Simple two-state gate stream: rise p while low, fall p while high.
struct GateStream {
    bool high = false;
    void step(PhiloxRng& rng, float pRise, float pFall, bool& rose) {
        rose = false;
        if (high) { if (rng.nextFloat() < pFall) high = false; }
        else      { if (rng.nextFloat() < pRise) { high = true; rose = true; } }
    }
};

// Mode B (plain gate origin): gate1 rise/high drives onsets; IMPL 2b mono drive + C5 AND.
static void marchModeB(int seed, Capture& cap, int& c2n, int& envn) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 1u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);   // D2 seam: the legacy sound as an authored graph
    eng.numPolyVoices = 15;
    seedEngine(eng, rng);
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    GateStream main; bool prevMainHigh = false;
    bool latch[17] = {};   // indexed by VOICE number 2..16 ([0..1] unused)
    for (int step = 0; step < kSteps; ++step) {
        bool rose; main.step(rng, 0.50f, 0.40f, rose);
        StepResult r = eng.executeModeB(/*gate1Rise=*/rose, /*gate1High=*/main.high, noteVal, in);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        // The polyVoiceActive latch (Monsoon.cpp:1009-1012): after the stepped execution.
        for (int i = 1; i <= eng.numPolyVoices; ++i)
            latch[i + 1] = eng.voices[dotModular::VoiceResolver::slot(i + 1)].gs.gateHeld;
        // IMPL 2b mono drive (Monsoon.cpp:1162-1196) — plain B has no ghost term.
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
        // The C5 FOREST AND + closed-gate zeroing (the F-3 drive, Monsoon.cpp — D8 + D7): the
        // MATERIALIZED compose, visited in the cached topological order. F-3/D7: the order now
        // INCLUDES slot 0 — the mono voice's own branch (parentCount[0] == 0 on the injected
        // star: the mono write above IS the envelope, the branch exits before touching
        // anything — the goldens pin that bit-identity). Free voices (no containers) are
        // skipped: their envelope is their own. gateOpen is the mono envelope cell.
        for (int k = 0; k < eng.rdm.topoCount; ++k) {
            const int i = eng.rdm.topoOrder[k];
            if (i == 0) {   // D7 mirror: the mono voice inside the algebra (a no-op on the star)
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
        captureAndCheck(eng, r, gateOpen ? 1 : 0, cap, c2n, envn);
    }
}

// Mode B subdivided (gate origin + ratchet/ghost subgates): three streams, the full IMPL 2b.
static void marchModeBS(int seed, Capture& cap, int& c2n, int& envn) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 2u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);   // D2 seam: the legacy sound as an authored graph
    eng.numPolyVoices = 15;
    seedEngine(eng, rng);
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    GateStream main, ghost; bool prevMainHigh = false;
    bool latch[17] = {};
    for (int step = 0; step < kSteps; ++step) {
        bool mainRose; main.step(rng, 0.45f, 0.35f, mainRose);
        // Ratchet: fires only IN-gate (Monsoon.cpp:878).
        const bool ratchetRise = main.high && (rng.nextFloat() < 0.45f);
        // Ghost: fires only genuinely in-gap, one sample clear of the fall (887-888).
        const bool inGap = !main.high && !prevMainHigh;
        bool ghostRose = false;
        if (inGap) ghost.step(rng, 0.40f, 0.50f, ghostRose);
        StepResult r = eng.executeModeBSubdivided(/*mainGateRise=*/mainRose, /*mainGateHigh=*/main.high,
                                                  ratchetRise, noteVal, in,
                                                  /*ghostRise=*/ghostRose, /*ghostHigh=*/ghost.high);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        for (int i = 1; i <= eng.numPolyVoices; ++i)
            latch[i + 1] = eng.voices[dotModular::VoiceResolver::slot(i + 1)].gs.gateHeld;
        // IMPL 2b with the ghost term (Monsoon.cpp:1162-1196).
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
        // The F-3 drive mirror (same materialized compose incl. the D7 V1 branch as mode B above).
        for (int k = 0; k < eng.rdm.topoCount; ++k) {
            const int i = eng.rdm.topoOrder[k];
            if (i == 0) {   // D7 mirror: the mono voice inside the algebra (a no-op on the star)
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
        captureAndCheck(eng, r, gateOpen ? 1 : 0, cap, c2n, envn);
    }
}

// ── Golden digests (the byte-identity oracle) ───────────────────────────────────
// History: R-1 captured A/B/BS on lane-expander-refactor @ 257fb351; the feat/step-advance
// Stages 1+3 re-pinned B/BS (the probabilistic gate-mode advance); CC_PROMPT_STEP_ADVANCE_
// FIX_DRAW_SOURCE re-pins them AGAIN — the mid-range B/BS marches (variationProb
// 0.25..0.75) legitimately moved BOTH times: first the probabilistic advance, now the
// draw source (the fresh reserved-chunk uniform → the variation lane's step-addressed
// r_vary, the per-step threshold) + the ghost-candidacy gate's deletion (candidacy is
// the subgate REST decision). GOLDEN_A is UNTOUCHED through every re-pin (mode A has no
// step-advance probability — the unchanged digest is the regression guard that the
// feature stays gate-mode-only). The p=1 identity (byte-identity with the PRE-feature
// engine) is pinned separately by test_step_advance (the spec's golden pin).
static const uint64_t GOLDEN_A  = 0xa843e2ff14883ab9ULL;
static const uint64_t GOLDEN_B  = 0x9e87dbdea46a4886ULL;   // re-captured (draw-source fix)
static const uint64_t GOLDEN_BS = 0xf2ef8f431ab33e71ULL;   // re-captured (draw-source fix)

int main() {
    std::printf("\033[1mrhythm decision differential — the R-1 oracle (stage R)\033[0m\n");

    // Pass 1: accumulate the three per-mode streams + invariant counts.
    Capture ca, cb, cbs; int c2a = 0, eva = 0, c2b = 0, evb = 0, c2s = 0, evs = 0;
    for (int seed = 0; seed < kMarches; ++seed) {
        marchModeA (seed, ca,  c2a, eva);
        marchModeB (seed, cb,  c2b, evb);
        marchModeBS(seed, cbs, c2s, evs);
    }
    const uint64_t digA = ca.digest(), digB = cb.digest(), digBS = cbs.digest();

    // Pass 2 (INV-DET): fresh buffers, same seeds -> byte-identical digests.
    Capture da, db, dbs; int rc2a = 0, reva = 0, rc2b = 0, revb = 0, rc2s = 0, revs = 0;
    for (int seed = 0; seed < kMarches; ++seed) {
        marchModeA (seed, da,  rc2a, reva);
        marchModeB (seed, db,  rc2b, revb);
        marchModeBS(seed, dbs, rc2s, revs);
    }
    check(digA  == da.digest(),  "INV-DET: mode A byte-identical across runs");
    check(digB  == db.digest(),  "INV-DET: mode B byte-identical across runs");
    check(digBS == dbs.digest(), "INV-DET: mode B-subdivided byte-identical across runs");

    // The goldens (placeholders on the first run — paste and re-commit).
    check(digA  == GOLDEN_A,  "GOLDEN mode A digest matches the pre-refactor engine");
    check(digB  == GOLDEN_B,  "GOLDEN mode B digest matches the pre-refactor engine");
    check(digBS == GOLDEN_BS, "GOLDEN mode B-subdivided digest matches the pre-refactor engine");

    std::printf("  C2-rest-close fired: A=%d B=%d BS=%d steps; ENV checks: B=%d BS=%d\n",
                c2a, c2b, c2s, evb, evs);
    std::printf("  GOLDEN_A  = 0x%016llxULL\n", (unsigned long long)digA);
    std::printf("  GOLDEN_B  = 0x%016llxULL\n", (unsigned long long)digB);
    std::printf("  GOLDEN_BS = 0x%016llxULL\n", (unsigned long long)digBS);
    std::printf("%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
