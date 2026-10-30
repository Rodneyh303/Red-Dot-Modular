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
static const uint64_t UA_IDENTITY_A  = 0xa843e2ff14883ab9ULL;
static const uint64_t UA_IDENTITY_B  = 0xcc4d19d45293b134ULL;
static const uint64_t UA_IDENTITY_BS = 0x7c00a247c8040799ULL;
static const uint64_t UA_NONDIV_A    = 0x73a9b8753ca0c395ULL;
static const uint64_t UA_NONDIV_B    = 0xd107b614f138d27fULL;
static const uint64_t UA_NONDIV_BS   = 0xd2796ca5aa236a1aULL;
// The MAPPED-mode digests (new behaviour — captured on the Stage-1 engine).
static const uint64_t UA_WIN10_A     = 0x8ed5fcb47ac148a2ULL;
static const uint64_t UA_WIN10_B     = 0xed6a06beb47d30a7ULL;
static const uint64_t UA_WIN10O4_A   = 0xc83f5b01041db8dbULL;

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
        c.noteVal = 2.f + (float)(int)(c.rng.nextFloat() * 4.999f);
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
        stepsIdx.push_back(eng.stepIndex);
    }
}

int main() {
    // ── 1. PASS-THROUGH IDENTITY — the Stage-1 golden pin (six digests) ──────────
    check(runMarches(MODE_A,  false, 16, 0).digest() == UA_IDENTITY_A,
          "IDENTITY: default config, mode A march == the pre-change engine (== GOLDEN_A)");
    check(runMarches(MODE_B,  false, 16, 0).digest() == UA_IDENTITY_B,
          "IDENTITY: default config, mode B march == the pre-change engine (== GOLDEN_B)");
    check(runMarches(MODE_BS, false, 16, 0).digest() == UA_IDENTITY_BS,
          "IDENTITY: default config, mode B subdivided == the pre-change engine (== GOLDEN_BS)");
    check(runMarches(MODE_A,  true,  16, 0).digest() == UA_NONDIV_A,
          "IDENTITY: non-divisive LOR (7/5/6 + pendulum/pingpong), mode A is BIT-IDENTICAL");
    check(runMarches(MODE_B,  true,  16, 0).digest() == UA_NONDIV_B,
          "IDENTITY: non-divisive LOR (7/5/6 + pendulum/pingpong), mode B is BIT-IDENTICAL");
    check(runMarches(MODE_BS, true,  16, 0).digest() == UA_NONDIV_BS,
          "IDENTITY: non-divisive LOR (7/5/6 + pendulum/pingpong), mode B subdivided is BIT-IDENTICAL");

    // ── 2. MAPPED digests (the fix's new pins) ───────────────────────────────────
    check(runMarches(MODE_A, false, 10, 0).digest() == UA_WIN10_A,
          "MAPPED: window 10, mode A march == the Stage-1 pin");
    check(runMarches(MODE_B, false, 10, 0).digest() == UA_WIN10_B,
          "MAPPED: window 10, mode B march == the Stage-1 pin");
    check(runMarches(MODE_A, false, 10, 4).digest() == UA_WIN10O4_A,
          "MAPPED: window 10 offset 4, mode A march == the Stage-1 pin");

    // ── 3. driverTickAt unit table ───────────────────────────────────────────────
    check(SequencerEngine::driverTickAt(12345, 16, 0) == 12345,
          "driver: pass-through at the default window (raw tick, unbounded)");
    check(SequencerEngine::driverTickAt(-1, 16, 0) == -1,
          "driver: pass-through keeps negative ticks raw (today's prv expression)");
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
            const int  expect = ((walk + 2) % 6 + 3) & 0x0F;
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

    std::printf("test_unified_addressing: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
