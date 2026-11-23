// test_unified_addressing.cpp — Stage 1 (CC_PROMPT_UNIFIED_ADDRESSING_SPEC.md)
//
// The unified pattern-addressing Stage-1 suite: the nested global->voice->lane chain
// with the GLOBAL map in place (driverTickAt) and the voice map still pass-through.
//
// TWO golden layers:
//   1. PASS-THROUGH IDENTITY (the Stage-1 GOLDEN PIN): the six UA_* digests below were
//      captured on the PRE-CHANGE engine (scripts/capture_unified_addressing_identity.cpp,
//      the test_rhythm_decision_differential discipline — the REAL executors, 24 seeded
//      marches x 64 steps x modes A/B/BS, the same capture stream). UA_IDENTITY_* are the
//      default config (and equal GOLDEN_A/B/BS — the harness is byte-faithful);
//      UA_NONDIV_* author NON-DIVISIVE lane lengths (mono REST len=7 off=2 rot=3, mono
//      MELODY Pendulum len=7, mono OCTAVE PingPong len=5, poly V3 MELODY len=5, poly V7
//      REST len=6) — the exact case where a bounded intermediate driver would change the
//      reads. The chain must pass the raw DNA tick through at the default window
//      (length 16, offset 0), so ALL SIX must hold bit-identically.
//   2. MAPPED RULES (the KNOWN-ISSUE fix): with an authored window (length < 16 or
//      offset > 0) the driver becomes the window walk, so
//        - a default lane (len 16, off 0) reads EXACTLY the global window's steps —
//          Sands reads the window, not the full 16;
//        - a lane with its own window composes against the walk (the polymeter beat
//          against the container length): read == ((walk + rot) mod L + off) every step;
//        - the walk is a pure function of the clock: reverse marches retrace forward
//          ones bit-exactly (reversibility), and re-authoring the window re-bounds the
//          reads from the next step.
//      These digests are NEW pins (the fix deliberately changes reads at authored
//      windows; no pre-change golden exists for length != 16 — verified: no suite drives
//      the real setWindow with length != 16).
//
// Build (test/run_all.sh entry "test_unified_addressing|$SE $GS $PE $CE").

#include <cstdio>
#include <cstdint>
#include <vector>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
#include "star_seam.hpp"

using redDot::PhiloxRng;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── FNV-1a 64 + capture stream (verbatim the differential oracle's domain) ─────
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

// ── The Stage-1 golden pins (captured PRE-CHANGE — do not touch) ────────────────
// DOTTED BUILD re-pin: the capture records r.nvIdx, and the duration-interleave
// renumbered indices 2..7 — the representation moved, the music did not (the
// differential's NORMALIZATION PROOF: the whole stream byte-identical with the
// nvIdx byte mapped back; the marches' draws mapped through legacyNoteValueIndex).
// UA_IDENTITY_* still equal the differential's GOLDEN_* (byte-faithful harness).
static const uint64_t UA_IDENTITY_A  = 0x6a57a9766799c379ULL;   // UNMOVED (clock/phase — the wander fix touches gate-mode poly only)
static const uint64_t UA_IDENTITY_B  = 0xf6311c05776c0467ULL;   // re-pinned (per-voice wander, CC_FIX_PER_VOICE_ADVANCE_WANDER; then tie-emergent CC_PROMPT_POLY_TIE_EMERGENT — the mode-B trace moves: same-degree/different-octave mono joins slide to the drawn octave, poly S1 derives its own tie)
static const uint64_t UA_IDENTITY_BS = 0xb01edb7cad398f9fULL;   // re-pinned (per-voice wander; then tie-emergent — same move as UA_IDENTITY_B)
static const uint64_t UA_NONDIV_A    = 0xe2726ae1498b2cc5ULL;   // UNMOVED (mode A)
static const uint64_t UA_NONDIV_B    = 0x7c047fff3fbb9f9eULL;   // re-pinned (per-voice wander; then tie-emergent CC_PROMPT_POLY_TIE_EMERGENT — same move as UA_IDENTITY_B)
static const uint64_t UA_NONDIV_BS   = 0x6da2c94e66763ba0ULL;   // re-pinned (per-voice wander; then tie-emergent — same move as UA_IDENTITY_B)
// The MAPPED-mode digests (new behaviour — captured on the Stage-1 engine).
static const uint64_t UA_WIN10_A     = 0xc40b7ba28a8bc0baULL;   // re-pinned (1/16T insert)
static const uint64_t UA_WIN10_B     = 0x561c1a7f4c6799daULL;   // re-pinned (per-voice wander; then tie-emergent CC_PROMPT_POLY_TIE_EMERGENT — same move as UA_IDENTITY_B)
static const uint64_t UA_WIN10O4_A   = 0x7124d7417c03fa9fULL;   // re-pinned (1/16T insert)

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
    eng.voices[m1].variationProb = 0.25f + 0.50f * rng.nextFloat();
    eng.voices[m1].qmixLevel     = rng.nextFloat();
}

static void authorNonDivLor(SequencerEngine& eng) {
    eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 7;
    eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 2;
    eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 3;
    eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_LEN) = 7;
    eng.laneDir_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Pendulum;
    eng.laneDirPending_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Pendulum;
    eng.lorRef(dotModular::STRAND_OCTAVE, SequencerEngine::LOR_LEN) = 5;
    eng.laneDir_[dotModular::STRAND_OCTAVE] = SequencerEngine::LaneDir::PingPong;
    eng.laneDirPending_[dotModular::STRAND_OCTAVE] = SequencerEngine::LaneDir::PingPong;
    const auto v3 = dotModular::VoiceResolver::slot(3);
    eng.polyLenERef(v3, dotModular::LaneId{SequencerEngine::PL_MELODY}) = 5;
    eng.polyOffERef(v3, dotModular::LaneId{SequencerEngine::PL_MELODY}) = 1;
    eng.polyRotERef(v3, dotModular::LaneId{SequencerEngine::PL_MELODY}) = 2;
    const auto v7 = dotModular::VoiceResolver::slot(7);
    eng.polyLenERef(v7, dotModular::LaneId{SequencerEngine::PL_REST}) = 6;
}

static int q12(float pitchV) { return (int)std::lrintf((double)pitchV * 12.0); }

static void captureStep(const SequencerEngine& eng, const StepResult& r, Capture& cap) {
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(eng.gs.gateHeld ? 1u : 0u);
    cap.u8(eng.gsStep.gateHeld ? 1u : 0u);
    cap.i16(q12(eng.gs.currentPitchV));
    for (int v = 2; v <= 16; ++v) {
        const auto& pv = eng.voices[dotModular::VoiceResolver::slot(v)];
        cap.u8(pv.gs.gateHeld ? 1u : 0u);
        cap.u8(pv.gsStep.gateHeld ? 1u : 0u);
        cap.u8(pv.accented ? 1u : 0u);
        cap.u8(pv.participating ? 1u : 0u);
        cap.i16(q12(pv.gs.currentPitchV));
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

// Shared march preamble: fresh engine + star graph + seeds + optional config.
struct MarchCtx {
    SequencerEngine eng;
    PatternInput    in;
    float           noteVal = 2.f;
    GateStream      main, ghost;
    bool            prevMainHigh = false;
    bool            latch[17] = {};
    PhiloxRng       rng;
};

static void marchModeA(MarchCtx& c, Capture& cap) {
    redDot::RateManager rm;
    for (int step = 0; step < kSteps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = c.eng.executeModeA(rm, c.noteVal, c.in, +1);
        if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
        captureStep(c.eng, r, cap);
    }
}

static void marchModeB(MarchCtx& c, Capture& cap) {
    for (int step = 0; step < kSteps; ++step) {
        bool rose; c.main.step(c.rng, 0.50f, 0.40f, rose);
        StepResult r = c.eng.executeModeB(/*gate1Rise=*/rose, /*gate1High=*/c.main.high, c.noteVal, c.in);
        if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
        for (int i = 1; i <= c.eng.numPolyVoices; ++i)
            c.latch[i + 1] = c.eng.voices[dotModular::VoiceResolver::slot(i + 1)].gs.gateHeld;
        const bool isRest    = (r.decision == MonoDecision::Rest);
        const bool gapBridge = c.eng.tieAcrossRests ? c.eng.gs.slurForward : c.prevMainHigh;
        const bool gateOpen  = !isRest && (c.main.high || gapBridge);
        c.eng.gs.gateHeld     = gateOpen;
        c.eng.gsStep.gateHeld = !isRest && c.main.high;
        if (!gateOpen) {
            c.eng.gs.holdRemain = 0.f;       c.eng.gs.gatePulseRemain = -1;
            c.eng.gsStep.holdRemain = 0.f;   c.eng.gsStep.gatePulseRemain = -1;
        }
        c.prevMainHigh = c.main.high;
        for (int k = 0; k < c.eng.rdm.topoCount; ++k) {
            const int i = c.eng.rdm.topoOrder[k];
            if (i == 0) {
                if (c.eng.rdm.parentCount[0] == 0) continue;
                bool monoOpen = gateOpen;
                for (int p = 0; p < c.eng.rdm.parentCount[0]; ++p) {
                    const int u = c.eng.rdm.parentSlot[0][p];
                    if (u > c.eng.numPolyVoices) continue;
                    monoOpen = monoOpen && c.eng.voices[dotModular::VoiceResolver::slot(u + 1)].gs.gateHeld;
                }
                c.eng.gs.gateHeld = monoOpen;
                if (!monoOpen) {
                    c.eng.gs.holdRemain = 0.f;       c.eng.gs.gatePulseRemain = -1;
                    c.eng.gsStep.holdRemain = 0.f;   c.eng.gsStep.gatePulseRemain = -1;
                }
                continue;
            }
            if (i > c.eng.numPolyVoices || c.eng.rdm.parentCount[i] == 0) continue;
            bool vOpen = c.latch[i + 1];
            for (int p = 0; p < c.eng.rdm.parentCount[i]; ++p) {
                const int u = c.eng.rdm.parentSlot[i][p];
                vOpen = vOpen && (u == 0 ? gateOpen
                                         : c.eng.voices[dotModular::VoiceResolver::slot(u + 1)].gs.gateHeld);
            }
            const auto vs = dotModular::VoiceResolver::slot(i + 1);
            c.eng.voices[vs].gs.gateHeld = vOpen;
            if (!vOpen) { c.eng.voices[vs].gs.holdRemain = 0.f; c.eng.voices[vs].gs.gatePulseRemain = -1; }
        }
        captureStep(c.eng, r, cap);
    }
}

static void marchModeBS(MarchCtx& c, Capture& cap) {
    for (int step = 0; step < kSteps; ++step) {
        bool mainRose; c.main.step(c.rng, 0.45f, 0.35f, mainRose);
        const bool ratchetRise = c.main.high && (c.rng.nextFloat() < 0.45f);
        const bool inGap = !c.main.high && !c.prevMainHigh;
        bool ghostRose = false;
        if (inGap) c.ghost.step(c.rng, 0.40f, 0.50f, ghostRose);
        StepResult r = c.eng.executeModeBSubdivided(/*mainGateRise=*/mainRose, /*mainGateHigh=*/c.main.high,
                                                    ratchetRise, c.noteVal, c.in,
                                                    /*ghostRise=*/ghostRose, /*ghostHigh=*/c.ghost.high);
        if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
        for (int i = 1; i <= c.eng.numPolyVoices; ++i)
            c.latch[i + 1] = c.eng.voices[dotModular::VoiceResolver::slot(i + 1)].gs.gateHeld;
        const bool isRest        = (r.decision == MonoDecision::Rest);
        const bool ghostSounding = c.eng.ghostActive && c.ghost.high;
        const bool gapBridge     = c.eng.tieAcrossRests ? c.eng.gs.slurForward : c.prevMainHigh;
        const bool gateOpen      = !isRest && (c.main.high || ghostSounding || gapBridge);
        c.eng.gs.gateHeld     = gateOpen;
        c.eng.gsStep.gateHeld = !isRest && (c.main.high || ghostSounding);
        if (!gateOpen) {
            c.eng.gs.holdRemain = 0.f;       c.eng.gs.gatePulseRemain = -1;
            c.eng.gsStep.holdRemain = 0.f;   c.eng.gsStep.gatePulseRemain = -1;
        }
        c.prevMainHigh = c.main.high;
        for (int k = 0; k < c.eng.rdm.topoCount; ++k) {
            const int i = c.eng.rdm.topoOrder[k];
            if (i == 0) {
                if (c.eng.rdm.parentCount[0] == 0) continue;
                bool monoOpen = gateOpen;
                for (int p = 0; p < c.eng.rdm.parentCount[0]; ++p) {
                    const int u = c.eng.rdm.parentSlot[0][p];
                    if (u > c.eng.numPolyVoices) continue;
                    monoOpen = monoOpen && c.eng.voices[dotModular::VoiceResolver::slot(u + 1)].gs.gateHeld;
                }
                c.eng.gs.gateHeld = monoOpen;
                if (!monoOpen) {
                    c.eng.gs.holdRemain = 0.f;       c.eng.gs.gatePulseRemain = -1;
                    c.eng.gsStep.holdRemain = 0.f;   c.eng.gsStep.gatePulseRemain = -1;
                }
                continue;
            }
            if (i > c.eng.numPolyVoices || c.eng.rdm.parentCount[i] == 0) continue;
            bool vOpen = c.latch[i + 1];
            for (int p = 0; p < c.eng.rdm.parentCount[i]; ++p) {
                const int u = c.eng.rdm.parentSlot[i][p];
                vOpen = vOpen && (u == 0 ? gateOpen
                                         : c.eng.voices[dotModular::VoiceResolver::slot(u + 1)].gs.gateHeld);
            }
            const auto vs = dotModular::VoiceResolver::slot(i + 1);
            c.eng.voices[vs].gs.gateHeld = vOpen;
            if (!vOpen) { c.eng.voices[vs].gs.holdRemain = 0.f; c.eng.voices[vs].gs.gatePulseRemain = -1; }
        }
        captureStep(c.eng, r, cap);
    }
}

enum Mode { MODE_A, MODE_B, MODE_BS };

static Capture runMarches(Mode mode, bool nonDiv, int lenWin, int offWin) {
    Capture cap;
    for (int s = 0; s < kMarches; ++s) {
        MarchCtx c;
        c.rng.seed64((uint64_t)s * 747796405u + 2891336453u
                     + (mode == MODE_A ? 0u : mode == MODE_B ? 1u : 2u));
        c.eng.reset();
        injectLegacyStarGraph(c.eng);
        c.eng.numPolyVoices = 15;
        seedEngine(c.eng, c.rng);
        if (nonDiv) authorNonDivLor(c.eng);
        if (lenWin != 16 || offWin != 0) c.eng.updateWindow((float)lenWin, 0.f, false, (float)offWin, 0.f, false);
        c.noteVal = (float)legacyNoteValueIndex(2 + (int)(c.rng.nextFloat() * 4.999f));
        const float varAmt = 0.20f + 0.70f * c.rng.nextFloat();
        c.in = makeInput(varAmt);
        if      (mode == MODE_A)  marchModeA(c, cap);
        else if (mode == MODE_B)  marchModeB(c, cap);
        else                      marchModeBS(c, cap);
    }
    return cap;
}

// ── MAPPED-mode rule marches ─────────────────────────────────────────────────────
// A minimal deterministic Mode-A march returning the per-step mono REST-lane read
// (getRhythmStep) + the playhead stepIndex, for the window-walk assertions.
static void marchReadsA(SequencerEngine& eng, int steps, std::vector<int>& reads,
                        std::vector<int>& stepsIdx) {
    PhiloxRng rng; rng.seed64(0xC0FFEEu);
    for (int i = 0; i < 16; ++i) eng.pe.rhythmRandom[i] = rng.nextFloat();
    const auto m1 = dotModular::VoiceResolver::slot(1);
    eng.voices[m1].restProb = 0.5f;
    PatternInput in = makeInput(0.3f);
    redDot::RateManager rm;
    for (int step = 0; step < steps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = eng.executeModeA(rm, 2.f, in, +1);
        (void)r;
        reads.push_back(eng.getRhythmStep());
        stepsIdx.push_back(eng.playedColumn());
    }
}

int main() {
    // ── 1. PASS-THROUGH IDENTITY — the Stage-1 golden pin (six digests) ──────────
    // (Computed first + printed — the differential's re-pin facility: a documented
    // behaviour move re-captures in place. History: the six were captured on the
    // PRE-CHANGE engine at this branch's fork point (5647e7cc); the mode-B rows
    // were re-pinned ONCE after merging fix/subgate-merge-onto-straits-base — the
    // step-advance draw-source fix (74897d64) + the freeze-fix scan (8679c2d3)
    // legitimately moved every mid-range mode-B march, the SAME two moves the
    // differential's GOLDEN_B/BS were re-pinned for (its history comment documents
    // all three of its moves). Mode A never moved — the gate-mode-only guard; the
    // A rows passing un-re-pinned is the evidence the moves are mode-B-scoped.)
    const uint64_t digIdA   = runMarches(MODE_A,  false, 16, 0).digest();
    const uint64_t digIdB   = runMarches(MODE_B,  false, 16, 0).digest();
    const uint64_t digIdBS  = runMarches(MODE_BS, false, 16, 0).digest();
    const uint64_t digNdA   = runMarches(MODE_A,  true,  16, 0).digest();
    const uint64_t digNdB   = runMarches(MODE_B,  true,  16, 0).digest();
    const uint64_t digNdBS  = runMarches(MODE_BS, true,  16, 0).digest();
    const uint64_t digW10A  = runMarches(MODE_A,  false, 10, 0).digest();
    const uint64_t digW10B  = runMarches(MODE_B,  false, 10, 0).digest();
    const uint64_t digW10O4 = runMarches(MODE_A,  false, 10, 4).digest();
    std::printf("  UA_IDENTITY_A  = 0x%016llxULL\n", (unsigned long long)digIdA);
    std::printf("  UA_IDENTITY_B  = 0x%016llxULL\n", (unsigned long long)digIdB);
    std::printf("  UA_IDENTITY_BS = 0x%016llxULL\n", (unsigned long long)digIdBS);
    std::printf("  UA_NONDIV_A    = 0x%016llxULL\n", (unsigned long long)digNdA);
    std::printf("  UA_NONDIV_B    = 0x%016llxULL\n", (unsigned long long)digNdB);
    std::printf("  UA_NONDIV_BS   = 0x%016llxULL\n", (unsigned long long)digNdBS);
    std::printf("  UA_WIN10_A     = 0x%016llxULL\n", (unsigned long long)digW10A);
    std::printf("  UA_WIN10_B     = 0x%016llxULL\n", (unsigned long long)digW10B);
    std::printf("  UA_WIN10O4_A   = 0x%016llxULL\n", (unsigned long long)digW10O4);
    check(digIdA == UA_IDENTITY_A,
          "IDENTITY: default config, mode A march == the pre-change engine (== GOLDEN_A)");
    check(digIdB == UA_IDENTITY_B,
          "IDENTITY: default config, mode B march == the pre-change engine (== GOLDEN_B)");
    check(digIdBS == UA_IDENTITY_BS,
          "IDENTITY: default config, mode B subdivided == the pre-change engine (== GOLDEN_BS)");
    check(digNdA == UA_NONDIV_A,
          "IDENTITY: non-divisive LOR (7/5/6 + pendulum/pingpong), mode A is BIT-IDENTICAL");
    check(digNdB == UA_NONDIV_B,
          "IDENTITY: non-divisive LOR (7/5/6 + pendulum/pingpong), mode B is BIT-IDENTICAL");
    check(digNdBS == UA_NONDIV_BS,
          "IDENTITY: non-divisive LOR (7/5/6 + pendulum/pingpong), mode B subdivided is BIT-IDENTICAL");

    // ── 2. MAPPED digests (the fix's new pins) ───────────────────────────────────
    check(digW10A == UA_WIN10_A,
          "MAPPED: window 10, mode A march == the Stage-1 pin");
    check(digW10B == UA_WIN10_B,
          "MAPPED: window 10, mode B march == the Stage-1 pin");
    check(digW10O4 == UA_WIN10O4_A,
          "MAPPED: window 10 offset 4, mode A march == the Stage-1 pin");

    // ── 3. driverTickAt unit table ───────────────────────────────────────────────
    check(SequencerEngine::driverTickAt(12345, 16, 0) == 12345,
          "driver: pass-through at the default window (raw tick, unbounded)");
    check(SequencerEngine::driverTickAt(-1, 16, 0) == -1,
          "driver: pass-through keeps negative ticks raw (today's prv expression)");

    // ── 3b. KILL_DNA_LCM: the unbounded/NEGATIVE counter, wrapped LOCALLY ────────
    // The old global DNA_LCM wrap's divisibility argument is replaced by LOCAL
    // periodicity: every position function is periodic in t with its OWN period,
    // negative t included (a backward walk from a fresh start goes -1, -2, ...).
    {
        const int64_t big = (int64_t)1 << 40;   // far beyond the old DNA_LCM = 1441440
        bool okDrive = true, okShift = true, okBounce = true;
        for (int len = 1; len <= 16; ++len) {
            for (int off = 0; off < 16; ++off) {
                if (len == 16 && off == 0) continue;   // pass-through: see the shift check below
                okDrive &= SequencerEngine::driverTickAt(big - 17, len, off)
                        == SequencerEngine::driverTickAt(big - 17 + len, len, off);
                okDrive &= SequencerEngine::driverTickAt(-12345, len, off)
                        == SequencerEngine::driverTickAt(-12345 + len, len, off);
            }
            const int64_t t = -98765;   // a negative counter
            // Forward/Reverse move by exactly len under t -> t - len: congruent through
            // getStrandIdx/calcPlayhead, which mod by the same length.
            okShift &= SequencerEngine::laneTickFor(SequencerEngine::LaneDir::Forward, t, len)
                     - SequencerEngine::laneTickFor(SequencerEngine::LaneDir::Forward, t - len, len) == len;
            okShift &= SequencerEngine::laneTickFor(SequencerEngine::LaneDir::Reverse, t, len)
                     - SequencerEngine::laneTickFor(SequencerEngine::LaneDir::Reverse, t - len, len) == -len;
            // Pendulum/PingPong return their wrapped triangle: exactly periodic.
            const int64_t Pp = 2 * (len > 1 ? len - 1 : 1);
            const int64_t Pg = 2 * len;
            okBounce &= SequencerEngine::laneTickFor(SequencerEngine::LaneDir::Pendulum, t, len)
                     == SequencerEngine::laneTickFor(SequencerEngine::LaneDir::Pendulum, t - Pp, len);
            okBounce &= SequencerEngine::laneTickFor(SequencerEngine::LaneDir::PingPong, t, len)
                     == SequencerEngine::laneTickFor(SequencerEngine::LaneDir::PingPong, t - Pg, len);
        }
        check(okDrive, "KILL_DNA_LCM: the window walk is periodic in t (negative t included)");
        check(okShift, "KILL_DNA_LCM: Forward/Reverse shift by exactly len under t -> t-len (congruent through the local mods)");
        check(okBounce, "KILL_DNA_LCM: Pendulum/PingPong are exactly periodic with their own periods (negative t included)");
    }
    check(SequencerEngine::driverTickAt(1, 10, 0) == 1
          && SequencerEngine::driverTickAt(10, 10, 0) == 10
          && SequencerEngine::driverTickAt(11, 10, 0) == 1
          && SequencerEngine::driverTickAt(20, 10, 0) == 10
          && SequencerEngine::driverTickAt(21, 10, 0) == 1,
          "driver: mapped window walk wraps at length (1..10, 1..10, ...)");
    check(SequencerEngine::driverTickAt(1, 10, 4) == 5
          && SequencerEngine::driverTickAt(10, 10, 4) == 14
          && SequencerEngine::driverTickAt(11, 10, 4) == 5,
          "driver: mapped window walk carries the offset (5..14)");
    check(SequencerEngine::driverTickAt(0, 10, 0) == 10
          && SequencerEngine::driverTickAt(-9, 10, 0) == 1,
          "driver: negative raw ticks normalize into the walk (DNA wrap / reverse seed)");

    // ── 4. THE FIX: the window bounds a default lane's reads ─────────────────────
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 0.f, false, false);
        std::vector<int> reads, stepsIdx;
        marchReadsA(eng, 40, reads, stepsIdx);
        bool inWin = true, isWalk = true;
        for (size_t k = 0; k < reads.size(); ++k) {
            const long T = (long)k + 1;                      // the 1-based DNA tick
            const int  walk = (int)(((T - 1) % 10 + 10) % 10);
            inWin = inWin && reads[k] >= 0 && reads[k] < 10;
            isWalk = isWalk && reads[k] == walk;             // default lane: read == the walk
        }
        check(inWin, "FIX: window 10 — a default lane never reads outside [0,10)");
        check(isWalk, "FIX: window 10 — a default lane's read IS the window walk");
        // The playhead wraps the same window (the playhead and the read agree).
        bool phInWin = true;
        for (int s : stepsIdx) phInWin = phInWin && s >= 0 && s < 10;
        check(phInWin, "FIX: window 10 — the playhead walks the same window");
    }
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 4.f, false, false);
        std::vector<int> reads, stepsIdx;
        marchReadsA(eng, 40, reads, stepsIdx);
        bool inWin = true, isWalk = true;
        for (size_t k = 0; k < reads.size(); ++k) {
            const long T = (long)k + 1;
            const int  walk = (int)(((T - 1) % 10 + 10) % 10) + 4;
            inWin = inWin && reads[k] >= 4 && reads[k] < 14;
            isWalk = isWalk && reads[k] == (walk & 0x0F);
        }
        check(inWin, "FIX: window 10 offset 4 — a default lane reads inside [4,14)");
        check(isWalk, "FIX: window 10 offset 4 — the read is the offset walk (mod 16)");
        bool phInWin = true;
        for (int s : stepsIdx) phInWin = phInWin && s >= 4 && s <= 13;
        check(phInWin, "FIX: window 10 offset 4 — the playhead walks [4..13]");
    }

    // ── 5. THE COMPOSITION RULE at an authored window: a lane with its own window ─
    // read == ((walk + rot) mod L + off) mod 16, walk = the window walk — the lane's
    // own LOR applies ON TOP of the global map (the nested-map composition).
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 0.f, false, false);
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 6;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 3;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 2;
        std::vector<int> reads, stepsIdx;
        marchReadsA(eng, 60, reads, stepsIdx);
        bool isComposed = true;
        for (size_t k = 0; k < reads.size(); ++k) {
            const long T = (long)k + 1;
            const int  walk = (int)(((T - 1) % 10 + 10) % 10);
            (void)walk; const int  expect = (int)((((T - 1) + 2) % 6) + 3) & 0x0F;   // RULED 2026-11-21: a lane shorter than G free-runs (t mod L), no restart at the global edge
            isComposed = isComposed && reads[k] == expect;
        }
        check(isComposed,
              "RULE: len-6 lane at window 10 — read == ((walk+rot) mod 6 + off), the beat vs the container");
    }

    // ── 6. REVERSIBILITY: the walk is a pure function of the clock ───────────────
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 2.f, false, false);
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 6;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 2;
        std::vector<int> fwd, back;
        for (int k = 0; k < 24; ++k) { eng.advancePlayhead(+1); fwd.push_back(eng.getRhythmStep()); }
        for (int k = 0; k < 24; ++k) { eng.advancePlayhead(-1); back.push_back(eng.getRhythmStep()); }
        // After the forward march T=24; the j-th backward step lands at T=23-j, whose read
        // must equal the forward read at the same T (= fwd[23-j-1] = fwd[22-j]).
        bool retraces = true;
        for (int j = 0; j < 23; ++j) retraces = retraces && back[j] == fwd[22 - j];
        check(retraces, "REVERSIBILITY: mapped window + authored lane — backward retraces forward bit-exactly");
    }

    // ── 7. RE-AUTHORING the window re-bounds the reads from the next step ────────
    {
        SequencerEngine eng; eng.reset();
        std::vector<int> reads, stepsIdx;
        marchReadsA(eng, 8, reads, stepsIdx);              // default window: full 16
        bool full16 = true;
        for (int r : reads) full16 = full16 && r >= 0 && r < 16;
        check(full16, "default window: reads span the full 16 (pass-through)");
        eng.updateWindow(5.f, 0.f, false, 0.f, false, false);
        std::vector<int> reads2, stepsIdx2;
        marchReadsA(eng, 12, reads2, stepsIdx2);
        bool bounded = true;
        for (int r : reads2) bounded = bounded && r >= 0 && r < 5;
        check(bounded, "RE-AUTHOR: after updateWindow(5), the reads bound to [0,5) from the next step");
    }

    // ── 8. STAGE 2 — the VOICE map (the global->voice level) ─────────────────────
    {
        // THE WORKED TABLE, re-pinned by CC_FIX_VOICE_WINDOW_CYCLE_AND_ROTATION (the
        // doc's declared behaviour change: rotation is now WINDOW-RELATIVE, rho = R —
        // the phase into the window at global step 1 — not the old (R-O) absolute
        // fold; voices with R != 0 or O != 0 read differently by design). Voice: L=8
        // O=5 R=7 (window 5..12): at global 1 the voice reads WINDOW-STEP 7 = phrase
        // 12, wrapping within 5..12 — reads 12,5,6,...,11,12,5,... Lane A = the mono
        // REST lane: L=4 O=1 R=3. Lane B = the mono MELODY lane: L=6 O=3 R=6. The
        // lanes hit THEIR rotations exactly when the VOICE is at its window start
        // (read 5) — the lane R re-base to voice-step-1, now reached one step after
        // the wrap (the R=7 phase consumes the window's tail first).
        static const int kVoiceReads[16] = {12,5,6,7,8,9,10,11,12,5,6,7,8,9,10,11};
        static const int kLaneA[16]      = {2,3,4,1,2,3,4,1,2,3,4,1,2,3,4,1};
        static const int kLaneB[16]      = {7,6,7,8,3,4,5,6,7,6,7,8,3,4,5,6};
        SequencerEngine eng; eng.reset();
        const auto M = dotModular::VoiceResolver::mono();
        eng.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;   // the Macro row: all voices follow
        eng.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 5;
        eng.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 7;
        // The LANES' stored rot is MECHANICAL (getStrandIdx applies it directly); the doc's
        // lane R values are ANCHOR-form (the read at voice-step-1): rot = (R_anchor - O) mod L
        // -> lane A: (3-1) mod 4 = 2; lane B: (6-3) mod 6 = 3. The VOICE row's R is stored
        // WINDOW-RELATIVE (rho = R — the CC fix; the engine folds it directly).
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 4;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 1;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 2;
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_LEN) = 6;
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_OFF) = 3;
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_ROT) = 3;
        bool voiceOK = true, laneAOK = true, laneBOK = true;
        for (int k = 0; k < 16; ++k) {
            eng.advancePlayhead(+1);
            voiceOK = voiceOK && eng.voiceReadPosition(M) == kVoiceReads[k];
            laneAOK = laneAOK && eng.getRhythmStep()  == kLaneA[k];
            laneBOK = laneBOK && eng.getMelodyStep()  == kLaneB[k];
        }
        check(voiceOK, "STAGE2: the worked table — the voice reads exactly the doc's column");
        check(laneAOK, "STAGE2: the worked table — lane A (win 1..4, R=3) matches every row");
        check(laneBOK, "STAGE2: the worked table — lane B (win 3..8, R=6) matches every row");
        // THE ANCHOR LAW: the lanes read their R when the VOICE is at its window start —
        // wherever in the phrase that falls. Change the voice R to 6: the voice's phase
        // shifts (rho = R), voice-step-1 moves (now global 3/11), and the lanes'
        // rotations move WITH it (voice R propagates).
        eng.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 6;
        bool anchorOK = true; int atVoiceStart = 0;
        for (int k = 0; k < 16; ++k) {
            eng.advancePlayhead(+1);
            if (eng.voiceReadPosition(M) == 5) {   // the voice at its window start
                ++atVoiceStart;
                anchorOK = anchorOK && eng.getRhythmStep() == 3 && eng.getMelodyStep() == 6;
            }
        }
        check(anchorOK && atVoiceStart == 2,
              "STAGE2: lanes read their R exactly at voice-step-1 (the re-based anchor, voice R propagates)");
    }
    {
        // DIRECTION — the sign product falls out of the chain (voice dir is RELATIVE to
        // the parent's motion; the lanes inherit by composition). Voice Reverse x lane
        // Forward reads BACKWARD in real time; voice Reverse x lane Reverse reads
        // FORWARD (the double negative). Voice L=8 O=0 R=0 (rho 0).
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;
        eng.macroVoiceLorDirRef() = SequencerEngine::LaneDir::Reverse;
        // Lane A: the mono REST lane FORWARD len 4; Lane B: the mono MELODY lane REVERSE len 4.
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 4;
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_LEN) = 4;
        eng.laneDir_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Reverse;
        eng.laneDirPending_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Reverse;
        int prevA = -1, prevB = -1; bool aBackward = true, bForward = true;
        for (int k = 0; k < 12; ++k) {
            eng.advancePlayhead(+1);
            const int a = eng.getRhythmStep(), b = eng.getMelodyStep();
            if (prevA >= 0) aBackward = aBackward && a == ((prevA + 3) & 0x03);  // -1 mod 4
            if (prevB >= 0) bForward  = bForward  && b == ((prevB + 1) & 0x03);  // +1
            prevA = a; prevB = b;
        }
        check(aBackward, "STAGE2: voice Reverse x lane Forward reads BACKWARD (sign product)");
        check(bForward,  "STAGE2: voice Reverse x lane Reverse reads FORWARD (double negative)");
    }
    {
        // TURNAROUND STYLES: a Pendulum voice reads its window END once per turn
        // (pre-direction); a PingPong voice reads it TWICE (the double-tap). L=8 O=5 R=5.
        // CC_FIX re-pin: rho = R shifts the phase — the walk's SHAPE (and the turnaround
        // timing) is unchanged, the reads start at window-step 5 = phrase 10.
        SequencerEngine pend; pend.reset();
        pend.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;
        pend.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 5;
        pend.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 5;
        pend.macroVoiceLorDirRef() = SequencerEngine::LaneDir::Pendulum;
        SequencerEngine pong; pong.reset();
        pong.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;
        pong.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 5;
        pong.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 5;
        pong.macroVoiceLorDirRef() = SequencerEngine::LaneDir::PingPong;
        bool pendOK = true, pongOK = true;
        for (int k = 0; k < 17; ++k) {
            pend.advancePlayhead(+1); pong.advancePlayhead(+1);
            // Pendulum: walk 0,1,...,7,6,...,1 phase-shifted by R=5: reads 10,11,12,5,...
            const int p = pend.voiceReadPosition(dotModular::VoiceResolver::mono());
            const int tri = (k % 14 < 8) ? (k % 14) : (14 - (k % 14));   // 0..7..1 triangle
            pendOK = pendOK && p == ((tri + 5) % 8) + 5;
            // PingPong: walk 0,...,7,7,6,... (the endpoint repeats), phase-shifted by R=5
            const int g = pong.voiceReadPosition(dotModular::VoiceResolver::mono());
            const int per = k % 16;
            const int walk = (per < 8) ? per : (15 - per);               // 0..7,7..0
            pongOK = pongOK && g == ((walk + 5) % 8) + 5;
        }
        check(pendOK, "STAGE2: Pendulum voice — the window end read ONCE per turn (no double-tap) [rho=R re-pin]");
        check(pongOK, "STAGE2: PingPong voice — the window end read TWICE (the double-tap) [rho=R re-pin]");
    }
    {
        // ROUND-TRIP under a mapped voice: the voice map is a pure function of the clock.
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;
        eng.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 2;
        eng.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 6;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 6;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 2;
        std::vector<int> fwd;
        for (int k = 0; k < 24; ++k) { eng.advancePlayhead(+1); fwd.push_back(eng.getRhythmStep()); }
        // (TRUE-RETRACE, Rodney 2026-11-21 — supersedes the SLICE 2 re-aim, which had replaced the retrace
        // model with "reverse re-enters at R"). A mid-phrase reversal now walks the SAME read coordinate
        // backwards (PositionResolver::readCoord is anchored to the traversal's ENTRY direction), so the backward
        // march REPLAYS the forward reads bit-exactly on the composed chain (mapped voice x authored lane): the
        // j-th backward step lands on forward step 22-j. A reverse that starts FRESH at the phrase entry still
        // enters at R (test_reverse_rotation_anchor gate 2).
        bool retraces = true;
        for (int k = 0; k < 23; ++k) {
            eng.advancePlayhead(-1);
            retraces = retraces && (eng.getRhythmStep() == fwd[22 - k]);
        }
        check(retraces, "STAGE2: REVERSIBILITY (true retrace, 2026-11-21) — a mid-phrase reversal replays the forward "
                        "reads of the composed chain (mapped voice x authored lane) bit-exactly");
    }
    {
        // DELEGATION: the Macro row is the default for every voice; a local-East claim
        // overrides for that voice alone.
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 2;
        eng.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;   // V2 (follow) and V3 (follow)
        eng.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 5;
        eng.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 7;
        const auto v2 = dotModular::VoiceResolver::slot(2);   // bank.v = 1
        const auto v3 = dotModular::VoiceResolver::slot(3);   // bank.v = 2
        eng.setVoiceLorLocalEast(v2, true);                   // V2 claims its own range
        eng.voiceLorRef(v2, SequencerEngine::LOR_LEN) = 4;    // V2: L=4 O=0 R=0
        bool v2OK = true, v3OK = true;
        for (int k = 0; k < 16; ++k) {
            eng.advancePlayhead(+1);
            const long T = (long)k + 1;
            v2OK = v2OK && eng.voiceReadPosition(v2) == (int)((T - 1) % 4);
            v3OK = v3OK && eng.voiceReadPosition(v3) == (((T + 6) % 8 + 5) & 0x0F);   // rho=R re-pin: (T-1+R) mod 8 + O
        }
        check(v2OK, "STAGE2: a local-East voice reads ITS OWN window (L=4)");
        check(v3OK, "STAGE2: a following voice reads the Macro window (L=8 O=5 R=7) [rho=R re-pin]");
        // V2's lanes are driven by V2's driver: a default REST lane reads (driver-1).
        bool laneOK = true;
        SequencerEngine e2; e2.reset(); e2.numPolyVoices = 2;
        e2.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;
        e2.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 5;
        e2.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 7;
        e2.setVoiceLorLocalEast(v2, true);
        e2.voiceLorRef(v2, SequencerEngine::LOR_LEN) = 4;
        for (int k = 0; k < 16; ++k) {
            e2.advancePlayhead(+1);
            laneOK = laneOK && e2.laneTickV_[1][dotModular::STRAND_RHYTHM] == (int)k % 4;
        }
        check(laneOK, "STAGE2: the claimed voice's LANES are driven by its own voice walk");
    }
    {
        // IDENTITY VALUES = PASS-THROUGH (not the mapped form): a voice authored at
        // exactly {cap,0,0,Forward} keeps the raw-tick driver, so a non-divisive lane
        // length free-runs exactly as the pre-unification engine. (bug 23: the identity
        // sentinel is the CAP — the full voice window; {16,0,0} is now a genuine
        // 16-window, the mapped walk.)
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLorRef(SequencerEngine::LOR_LEN) = dotModular::kPatternLengthCap;   // authored, but identity values (the cap)
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 7;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 3;
        bool isPassthrough = true;
        for (int k = 0; k < 40; ++k) {
            eng.advancePlayhead(+1);
            const long T = (long)k + 1;
            const int expect = (int)(((T - 1 + 3) % 7 + 7) % 7);
            isPassthrough = isPassthrough && eng.getRhythmStep() == expect;
        }
        check(isPassthrough,
              "STAGE2: identity values = PASS-THROUGH — a len-7 lane free-runs on the raw tick");
    }

    // ── STAGE 2b: the AUTHORING PUSH (pushVoiceLorEditor — the editor-store path) ──
    // The Monsoon float rows reach the map ONLY through this one copy; these pins
    // prove the lift is faithful (identity floats = reset bytes; authored floats land
    // on the same store values the Stage-2a direct-ref tests pinned; clamps mirror
    // the lane LOR ranges; the delegation flag maps 0=follow / nonzero=local) and
    // byte-neutral at defaults (a pushed-identity engine marches bit-identically to
    // an untouched one — the goldens' precondition on the module path).
    {
        // IDENTITY PUSH = RESET BYTES: every row reads exactly what reset() seeded.
        SequencerEngine eng; eng.reset();
        // (CC_PROMPT_16_TRIAGE option C re-aim) the EDITOR's identity floats are the
        // CAP (Monsoon.hpp's EditorState, the bug-23 round) — the 16.f literals here
        // were the 16-era leftovers, pushing a NON-identity 16-row against the
        // cap-seeded store (the march below then diverged at bar 2).
        float macroLor[3] = {(float)dotModular::kPatternLengthCap, 0.f, 0.f}; float macroDir = 0.f;
        float eastLor[48], eastDir[16], localEast[16];
        for (int v = 0; v < 16; ++v) {
            eastLor[v*3+0] = (float)dotModular::kPatternLengthCap; eastLor[v*3+1] = 0.f; eastLor[v*3+2] = 0.f;
            eastDir[v] = 0.f; localEast[v] = 0.f;
        }
        eng.pushVoiceLorEditor(macroLor, macroDir, eastLor, eastDir, localEast);
        bool ident = true;
        // slot(v+1): the resolver is VOICE-NUMBER based (V1..V16 -> bank.v 0..15).
        for (int v = 0; v < 16; ++v) {
            const auto vs = dotModular::VoiceResolver::slot(v + 1);
            ident = ident && eng.voiceLor(vs, SequencerEngine::LOR_LEN) == dotModular::kPatternLengthCap;
            ident = ident && eng.voiceLor(vs, SequencerEngine::LOR_OFF) == 0;
            ident = ident && eng.voiceLor(vs, SequencerEngine::LOR_ROT) == 0;
            ident = ident && eng.voiceLorDirRef(vs) == SequencerEngine::LaneDir::Forward;
            ident = ident && eng.voiceLorFollowsMacro(vs);
        }
        ident = ident && eng.macroVoiceLorRef(SequencerEngine::LOR_LEN) == dotModular::kPatternLengthCap;
        ident = ident && eng.macroVoiceLorRef(SequencerEngine::LOR_OFF) == 0;
        ident = ident && eng.macroVoiceLorRef(SequencerEngine::LOR_ROT) == 0;
        ident = ident && eng.macroVoiceLorDirRef() == SequencerEngine::LaneDir::Forward;
        check(ident, "STAGE2b: identity floats push to EXACTLY the reset() bytes");
    }
    {
        // BYTE-NEUTRAL MARCH: a pushed-identity engine and an untouched one march
        // bit-identically (lane ticks + voice reads + the mono row) — the push at
        // defaults cannot move anything the Stage-1/2a goldens pinned.
        SequencerEngine a; a.reset(); a.numPolyVoices = 4;
        SequencerEngine b; b.reset(); b.numPolyVoices = 4;
        float macroLor[3] = {(float)dotModular::kPatternLengthCap, 0.f, 0.f}; float macroDir = 0.f;   // (option C re-aim) the editor's identity floats = the cap
        float eastLor[48], eastDir[16], localEast[16];
        for (int v = 0; v < 16; ++v) {
            eastLor[v*3+0] = (float)dotModular::kPatternLengthCap; eastLor[v*3+1] = 0.f; eastLor[v*3+2] = 0.f;
            eastDir[v] = 0.f; localEast[v] = 0.f;
        }
        b.pushVoiceLorEditor(macroLor, macroDir, eastLor, eastDir, localEast);
        bool same = true;
        for (int k = 0; k < 64; ++k) {
            a.advancePlayhead(+1); b.advancePlayhead(+1);
            same = same && a.getRhythmStep() == b.getRhythmStep();
            for (int l = 0; l < dotModular::NUM_STRANDS; ++l)
                same = same && a.laneTick_[l] == b.laneTick_[l];
            for (int v = 0; v < 4; ++v) {
                const auto vs = dotModular::VoiceResolver::slot(v + 1);   // V1..V4
                same = same && a.voiceReadPosition(vs) == b.voiceReadPosition(vs);
                for (int l = 0; l < dotModular::NUM_STRANDS; ++l)
                    same = same && a.laneTickV_[v][l] == b.laneTickV_[v][l];
            }
        }
        check(same, "STAGE2b: pushed-identity march == untouched march BIT-EXACTLY");
    }
    {
        // FAITHFUL LIFT: floats through the push == the direct-ref authoring the
        // Stage-2a pins used (the worked-table Macro row + one local-East voice),
        // marched bit-identically against the direct-store engine.
        SequencerEngine direct; direct.reset(); direct.numPolyVoices = 4;
        const auto v2 = dotModular::VoiceResolver::slot(2);   // voice 2 -> bank.v 1 (the Stage-2a convention)
        direct.macroVoiceLorRef(SequencerEngine::LOR_LEN) = 8;
        direct.macroVoiceLorRef(SequencerEngine::LOR_OFF) = 5;
        direct.macroVoiceLorRef(SequencerEngine::LOR_ROT) = 7;
        direct.setVoiceLorLocalEast(v2, true);
        direct.voiceLorRef(v2, SequencerEngine::LOR_LEN) = 4;
        direct.voiceLorRef(v2, SequencerEngine::LOR_OFF) = 1;
        direct.voiceLorRef(v2, SequencerEngine::LOR_ROT) = 2;
        direct.voiceLorDirRef(v2) = SequencerEngine::LaneDir::Reverse;

        SequencerEngine pushed; pushed.reset(); pushed.numPolyVoices = 4;
        float macroLor[3] = {8.f, 5.f, 7.f}; float macroDir = 0.f;
        float eastLor[48], eastDir[16], localEast[16];
        for (int v = 0; v < 16; ++v) {
            eastLor[v*3+0] = 16.f; eastLor[v*3+1] = 0.f; eastLor[v*3+2] = 0.f;
            eastDir[v] = 0.f; localEast[v] = 0.f;
        }
        eastLor[1*3+0] = 4.f; eastLor[1*3+1] = 1.f; eastLor[1*3+2] = 2.f;
        eastDir[1] = 1.f;          // Reverse (the LaneDir order)
        localEast[1] = 1.f;        // V2 claims its own East row
        pushed.pushVoiceLorEditor(macroLor, macroDir, eastLor, eastDir, localEast);

        bool same = true;
        for (int k = 0; k < 64; ++k) {
            direct.advancePlayhead(+1); pushed.advancePlayhead(+1);
            same = same && direct.getRhythmStep() == pushed.getRhythmStep();
            for (int l = 0; l < dotModular::NUM_STRANDS; ++l)
                same = same && direct.laneTick_[l] == pushed.laneTick_[l];
            for (int v = 0; v < 4; ++v) {
                const auto vs = dotModular::VoiceResolver::slot(v + 1);   // V1..V4
                same = same && direct.voiceReadPosition(vs) == pushed.vo