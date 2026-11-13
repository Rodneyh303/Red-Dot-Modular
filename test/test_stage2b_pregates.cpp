// test_stage2b_pregates.cpp — STEPS_32 Stage 2b REQUIRED PRE-GATES (GATE 1 + GATE 2)
//
// Per docs/design/STEPS_32_PLAN.md "STAGE 2b — cap flip (kPatternLengthCap 16 → 32):
// REQUIRED PRE-GATES (from the 2a review)". Stage 2b must not start until both gates
// are in place and green on the 2a tip; both belong to Stage 2b's gate list.
//
// GATE 1 — non-default ≤16 equivalence pin (the mis-read detector).
//   2a's flat-stride bug was caught only because a mis-read HAPPENED to hit a cell;
//   default-valued patterns compare equal across a mis-read, so the ≤16 goldens are
//   not yet a real guarantee. This pin runs the engine on patterns with VARIED
//   probabilities (every lane, every step, every voice row different — a cross-row
//   read reads a NEIGHBOUR's value, which a digest catches), varied mask bits
//   (painted macro + east patterns, an offset/length master window, a MASK_DYNAMIC
//   voice), and non-default subgate thresholds (ratchet/ghost × REST/LEG/ADV/ACC),
//   and compares EVERY observable output STEP BY STEP (decision, note index,
//   gate/CV for mono + all 15 poly voices, the playhead/walk display state, the
//   mask-projection display state, and the melody + published-slew row mirrors)
//   against a recorded reference.
//   The reference is generated ONCE at the 2a tip (cap 16, pre-flip), COMMITTED
//   (test/goldens/steps32_gate1_cap16_ref.txt), and is the oracle for 2b:
//   after the flip, the EQUIV section must show ZERO differences. If the flip
//   changes any ≤16 output, Stage 2b is wrong.
//
//   THE EQUIVALENCE DOMAIN (why two sections). The plan's wrap-regime formulation
//   ("The boundary vs the ring"): the BOUNDARY (what triggers a wrap) is live
//   parameter; the RING (the frame the arithmetic lands on, & 0x0F → & 0x1F) is a
//   constant that the flip widens; and D3: "a live window that wraps the 16-ring
//   today EXTENDS instead at frame 32". So a configuration whose window/LOR wraps
//   the 16-ring is NOT the same musical request after the flip — it is a documented,
//   conscious change. The oracle therefore records two sections:
//     EQUIV    — non-wrapping ≤16 configs (every landing stays in [0,16)): compared
//                at EVERY cap. This is the equivalence domain.
//     CAP16WRAP— wrapping configs (LOR len+off crossing 15→0, master window
//                wrapping): pinned ONLY while kPatternLengthCap == 16 (the 2a tip
//                and any future cap-16 build). They still run post-flip (they feed
//                GATE 2 and exercise the extension path) but their digests are not
//                compared across the flip.
//   follow-R painted masks are likewise excluded from EQUIV (rotMaskRing rotates in
//   the RING's width: a 16-bit pattern rotated at ring 32 projects differently onto
//   cells 0..15 than at ring 16 — rotation is a 32-bit-ring feature, pinned by the
//   32-step pins, not an equivalence domain). The length-CV knob map is likewise
//   excluded (D1's *31 rescale is a conscious re-pin): windows are authored through
//   the setWindow/lorRef APIs, never through CV.
//
// GATE 2 — beyond-ring zero-fill pin.
//   The design depends on cells ≥ kPatternLengthCap of every per-step row staying
//   ZERO while the ring is 16; a write leaking past the ring would corrupt the ≤16
//   output silently. After full runs across varied patterns this walks EVERY
//   per-step row — every lane × every voice row (random_, warped_, the slewed /
//   preRemapSlewed / remapSnap / published / source-cache planes), the melody
//   mirrors (melodySemitone / melodyPitchV / cachedMelodyPitchV / cachedQmix), the
//   three Philox draw scratch buffers, the cached-draw z rows, the z0/z1 window
//   caches, and the widened uint64 mask planes (bits ≥ the ring must be clear:
//   maskPainted_ / maskAnchored_ / maskLastPushed_ per voice, the three macro
//   twins, windowMask) — and asserts exact zero. Sampled at intermediate steps
//   AND at each run's end (a leak can be transient). Post-flip the float-row walk
//   is vacuous (storage == ring) but the uint64 mask law keeps biting: bits ≥ 32
//   of every plane must stay zero (the 64-step ceiling's headroom).
//
// The Monsoon module's display mirrors of these rows are module-level copies of
// the engine authority rows audited here (the Stage-3 display gate + the Rack
// review cover them); the engine rows are the write authority this pin owns.
//
// Oracle protocol:
//   <bin>            compare against test/goldens/steps32_gate1_cap16_ref.txt
//                    (missing file = FAIL with instructions)
//   <bin> --record   run everything, WRITE the oracle, then re-verify against it
//                    in the same invocation (self-check). Used ONCE at the 2a tip.
//
// Build (test/run_all.sh entry "test_stage2b_pregates|$SE $GS $PE $CE").

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
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

// ── FNV-1a 64 + capture stream (the differential oracle's discipline) ──────────
static uint64_t fnv1a(uint64_t h, uint8_t b) { return (h ^ (uint64_t)b) * 0x100000001b3ULL; }
struct Capture {
    std::vector<uint8_t> bytes;
    void u8(unsigned v)  { bytes.push_back((uint8_t)(v & 0xFFu)); }
    void i16(int v) {
        int16_t s = (int16_t)v;
        bytes.push_back((uint8_t)((s >> 0) & 0xFF));
        bytes.push_back((uint8_t)((s >> 8) & 0xFF));
    }
    void u32(uint32_t v) {
        bytes.push_back((uint8_t)((v >> 0)  & 0xFF));
        bytes.push_back((uint8_t)((v >> 8)  & 0xFF));
        bytes.push_back((uint8_t)((v >> 16) & 0xFF));
        bytes.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    void f32(float f) { uint32_t b; std::memcpy(&b, &f, 4); u32(b); }   // raw bits — a last-bit drift must trip the pin
    uint64_t digest() const {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < bytes.size(); ++i) h = fnv1a(h, bytes[i]);
        return h;
    }
    void clear() { bytes.clear(); }
};

// Per-step tee: every step's capture is digested alone (step-by-step comparison)
// AND appended to the march's whole stream (the fast-fail whole digest).
struct TeeCapture {
    Capture step, full;
    std::vector<uint64_t>* steps = nullptr;   // optional per-step digest sink
    void commit() {
        if (steps) steps->push_back(step.digest());
        full.bytes.insert(full.bytes.end(), step.bytes.begin(), step.bytes.end());
    }
};

static int q12(float pitchV) { return (int)std::lrintf((double)pitchV * 12.0); }
static uint32_t low16(uint64_t m) { return (uint32_t)(m & 0xFFFFULL); }   // the ≤16-cell projection (see header: all-on/rotated planes legitimately widen ≥16)

// ── The varied input rows (GATE 1's whole point: nothing default) ───────────────
// THE ≤16 EQUIVALENCE DOMAIN'S AUTHORED WIDTH: the rows are seeded over a FIXED 16
// cells — the same VALUES land in cells 0..15 at EVERY cap, so a cap flip compares
// the same authored pattern (a cap-relative seed loop would consume a different
// number of rng draws at each cap and shift every downstream input: the first GATE 1
// run at cap 32 failed on exactly that harness artifact, not on the engine). Cells
// 16..cap-1 stay authored-ZERO: below the flip that is GATE 2's beyond-ring law;
// above it, the ring-wide compute of zero-probability cells is D3's documented
// extension behaviour, outside the ≤16 equivalence domain.
static constexpr int kEquivSeedCells = 16;
static void seedVariedRows(SequencerEngine& eng, PhiloxRng& rng) {
    for (int i = 0; i < kEquivSeedCells; ++i) {   // the FIXED authored width — never kPatternLengthCap (cap-relative draw counts)
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
            for (int i = 0; i < kEquivSeedCells; ++i)
                eng.pe.polyRandom(vs, dotModular::LaneId{lane})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.05f + 0.60f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.55f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.80f * rng.nextFloat();
        eng.voices[vs].variationProb = 0.20f + 0.55f * rng.nextFloat();
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
    const auto m1 = dotModular::VoiceResolver::slot(1);
    eng.voices[m1].restProb      = 0.08f + 0.50f * rng.nextFloat();
    eng.voices[m1].accentProb    = 0.50f * rng.nextFloat();
    eng.voices[m1].legatoProb    = 0.85f * rng.nextFloat();
    eng.voices[m1].variationProb = 0.30f + 0.45f * rng.nextFloat();
    eng.voices[m1].qmixLevel     = rng.nextFloat();
}

// Non-default subgate thresholds, VARIED PER VOICE (a cross-row mis-read of the
// subRes plane reads the neighbour's threshold — the stream catches it).
static void varySubgates(SequencerEngine& eng, PhiloxRng& rng) {
    for (int v = 1; v <= 16; ++v) {
        const auto vs = dotModular::VoiceResolver::slot(v);
        for (int level = 0; level < 2; ++level) {           // SubgateKnobs::RATCHET, GHOST
            eng.voices[vs].subRes[level][0] = 0.10f + 0.75f * rng.nextFloat();   // REST  [0,1]
            eng.voices[vs].subRes[level][1] = 0.10f + 0.75f * rng.nextFloat();   // LEG   [0,1]
            eng.voices[vs].subRes[level][2] = -0.80f + 1.60f * rng.nextFloat();  // ADV   [-1,+1]
            eng.voices[vs].subRes[level][3] = 0.10f + 0.75f * rng.nextFloat();   // ACC   [0,1]
        }
    }
}

// Painted masks through the REAL authoring push (pure + rack-free). followR stays
// FALSE everywhere (see header: rotation is not an equivalence domain). The painted
// PATTERNS are 16-BIT-CLEAN (& 0xFFFF — the same bits land in the store at every
// cap; a pattern with bits above 15 would author different cells at ring 32 than
// the store-masked cells at ring 16 — the first GATE 1 run's effMacro divergence).
static void pushPaintedMasks(SequencerEngine& eng, bool macroPainted, int eastVoiceA, int eastVoiceB,
                             int dynamicVoice, int dynLen, int dynOff) {
    uint64_t macroBits[1] = { dotModular::kMaskLenMask };
    if (macroPainted) macroBits[0] = 0xB2D5A46BULL & 0xFFFFULL;   // mixed on/off, 16-bit clean
    const float macroKind = macroPainted ? 2.f : 0.f;                            // MASK_PAINTED / MASK_CYCLE

    uint64_t eastBits[16];
    float    eastKind[16], eastFollowR[16], localEast[16];
    for (int v = 0; v < 16; ++v) {
        eastBits[v] = dotModular::kMaskLenMask;   // CYCLE default — the all-on degenerate
        eastKind[v] = 0.f; eastFollowR[v] = 0.f; localEast[v] = 0.f;
    }
    if (eastVoiceA > 0) {   // voice number 1..16 → array index v-1
        eastBits[eastVoiceA - 1] = 0x6D5A3C96ULL & 0xFFFFULL;
        eastKind[eastVoiceA - 1] = 2.f; localEast[eastVoiceA - 1] = 1.f;
    }
    if (eastVoiceB > 0) {
        eastBits[eastVoiceB - 1] = 0xA96B4D2FULL & 0xFFFFULL;
        eastKind[eastVoiceB - 1] = 2.f; localEast[eastVoiceB - 1] = 1.f;
    }
    if (dynamicVoice > 0) {   // MASK_DYNAMIC (abs1): the region IS the voice LOR window
        eastKind[dynamicVoice - 1] = 1.f; localEast[dynamicVoice - 1] = 1.f;
        const auto vs = dotModular::VoiceResolver::slot(dynamicVoice);
        eng.voiceLorRef(vs, SequencerEngine::LOR_LEN) = dynLen;
        eng.voiceLorRef(vs, SequencerEngine::LOR_OFF) = dynOff;
    }
    eng.pushMaskEditor(macroBits, macroKind, 0.f, eastBits, eastKind, eastFollowR, localEast);
}

// Voice-LOR windows (the lanes' OWN wrap boundary — live parameter, ≤ cap here).
// Non-wrapping: len+off ≤ 16 (the landing stays in [0,16) — the EQUIV domain).
// Wrapping:     len+off  > 16 (crosses 15→0 today; D3 extends at frame 32 — CAP16WRAP).
static void authorLaneLors(SequencerEngine& eng, bool wrapping) {
    if (wrapping) {
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 16;   // full-ring wrap (off 5)
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 5;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 3;
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_LEN) = 7;    // 12+7 crosses 15→0
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_OFF) = 12;
        eng.laneDir_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Pendulum;
        eng.laneDirPending_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Pendulum;
        eng.lorRef(dotModular::STRAND_OCTAVE, SequencerEngine::LOR_LEN) = 5;    // 3+5 wraps
        eng.lorRef(dotModular::STRAND_OCTAVE, SequencerEngine::LOR_OFF) = 3;
        eng.laneDir_[dotModular::STRAND_OCTAVE] = SequencerEngine::LaneDir::PingPong;
        eng.laneDirPending_[dotModular::STRAND_OCTAVE] = SequencerEngine::LaneDir::PingPong;
    } else {
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 7;    // 2+7 ≤ 16
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 2;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 3;
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_LEN) = 9;    // 4+9 ≤ 16
        eng.lorRef(dotModular::STRAND_MELODY, SequencerEngine::LOR_OFF) = 4;
        eng.laneDir_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Pendulum;
        eng.laneDirPending_[dotModular::STRAND_MELODY] = SequencerEngine::LaneDir::Pendulum;
        eng.lorRef(dotModular::STRAND_OCTAVE, SequencerEngine::LOR_LEN) = 5;    // 3+5 ≤ 16
        eng.lorRef(dotModular::STRAND_OCTAVE, SequencerEngine::LOR_OFF) = 3;
        eng.laneDir_[dotModular::STRAND_OCTAVE] = SequencerEngine::LaneDir::PingPong;
        eng.laneDirPending_[dotModular::STRAND_OCTAVE] = SequencerEngine::LaneDir::PingPong;
    }
    const auto v3 = dotModular::VoiceResolver::slot(3);
    eng.polyLenERef(v3, dotModular::LaneId{SequencerEngine::PL_MELODY}) = wrapping ? 9 : 6;
    eng.polyOffERef(v3, dotModular::LaneId{SequencerEngine::PL_MELODY}) = wrapping ? 11 : 1;   // 11+9 wraps / 1+6 ≤ 16
    eng.polyRotERef(v3, dotModular::LaneId{SequencerEngine::PL_MELODY}) = 2;
    const auto v7 = dotModular::VoiceResolver::slot(7);
    eng.polyLenERef(v7, dotModular::LaneId{SequencerEngine::PL_REST}) = 8;
    eng.polyOffERef(v7, dotModular::LaneId{SequencerEngine::PL_REST})  = wrapping ? 12 : 0;    // 12+8 wraps / 0+8 ≤ 16
}

static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

// ── Per-step capture: note, gate, CV, display state, row mirrors ────────────────
static void captureStep(const SequencerEngine& eng, const StepResult& r, Capture& cap) {
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(r.stepped ? 1u : 0u);
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
    // Display state: the walks + the window + the mask projections (the ≤16 cells).
    for (int s = 0; s < 7; ++s) cap.i16(eng.laneTick_[s]);
    cap.i16(eng.globalWalk_);
    cap.i16(eng.cachedLength);
    cap.u32(low16(eng.windowMask));
    cap.u32(low16(eng.effectiveOutputMaskMacro()));
    for (int v = 1; v <= 16; ++v)
        cap.u32(low16(eng.effectiveOutputMask(dotModular::VoiceResolver::slot(v))));
    // The melody mirrors (16-cell projections).
    for (int i = 0; i < 16; ++i) cap.i16(eng.pe.melodySemitone[i]);
    for (int i = 0; i < 16; ++i) cap.f32(eng.pe.melodyPitchV[i]);
    for (int i = 0; i < 16; ++i) cap.f32(eng.pe.cachedMelodyPitchV[i]);
    for (int i = 0; i < 16; ++i) cap.f32(eng.pe.cachedQmix[i]);
    // The published slew planes (the prob-out gate's content — the flat-stride bug
    // class) + the engine slew rows, two voice slots so a cross-row read trips.
    const int vsA = 0, vsB = 5;   // V1 and V6
    for (int i = 0; i < 16; ++i) {
        cap.f32(eng.pe.pubSlewedPolyRhythm[vsA][i]);     cap.f32(eng.pe.pubSlewedPolyRhythm[vsB][i]);
        cap.f32(eng.pe.pubSlewedPolyMelody[vsA][i]);     cap.f32(eng.pe.pubSlewedPolyMelody[vsB][i]);
        cap.f32(eng.pe.pubSlewedPolyOctave[vsA][i]);     cap.f32(eng.pe.pubSlewedPolyOctave[vsB][i]);
        cap.f32(eng.pe.pubSlewedPolyAccent[vsA][i]);     cap.f32(eng.pe.pubSlewedPolyAccent[vsB][i]);
        cap.f32(eng.pe.pubSlewedPolyQmix[vsA][i]);       cap.f32(eng.pe.pubSlewedPolyQmix[vsB][i]);
        cap.f32(eng.pe.pubSlewedPolyVariation[vsA][i]);  cap.f32(eng.pe.pubSlewedPolyVariation[vsB][i]);
        cap.f32(eng.pe.pubSlewedPolyLegato[vsA][i]);     cap.f32(eng.pe.pubSlewedPolyLegato[vsB][i]);
    }
    for (int i = 0; i < 16; ++i) {
        cap.f32(eng.pe.slewedRhythm[vsA][i]);    cap.f32(eng.pe.slewedMelody[vsA][i]);
        cap.f32(eng.pe.slewedOctave[vsA][i]);    cap.f32(eng.pe.slewedAccent[vsA][i]);
        cap.f32(eng.pe.slewedVariation[vsA][i]); cap.f32(eng.pe.slewedLegato[vsA][i]);
        cap.f32(eng.pe.slewedQmix[vsA][i]);
    }
}

// ── The marches (the REAL executors, the differential harness's discipline) ─────
static const int kSteps = 64;

struct GateStream {
    bool high = false;
    void step(PhiloxRng& rng, float pRise, float pFall, bool& rose) {
        rose = false;
        if (high) { if (rng.nextFloat() < pFall) high = false; }
        else      { if (rng.nextFloat() < pRise) { high = true; rose = true; } }
    }
};

struct MarchCtx {
    SequencerEngine eng;
    PatternInput    in;
    float           noteVal = 2.f;
    GateStream      main, ghost;
    bool            prevMainHigh = false;
    bool            latch[17] = {};
    PhiloxRng       rng;
};

static void marchModeA(MarchCtx& c, TeeCapture& t) {
    redDot::RateManager rm;
    for (int step = 0; step < kSteps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = c.eng.executeModeA(rm, c.noteVal, c.in, +1);
        if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
        captureStep(c.eng, r, t.step); t.commit(); t.step.clear();
    }
}

static void marchModeB(MarchCtx& c, TeeCapture& t) {
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
        captureStep(c.eng, r, t.step); t.commit(); t.step.clear();
    }
}

static void marchModeBS(MarchCtx& c, TeeCapture& t) {
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
        captureStep(c.eng, r, t.step); t.commit(); t.step.clear();
    }
}

enum Mode { MODE_A, MODE_B, MODE_BS };

// ── The march table ──────────────────────────────────────────────────────────────
// EQUIV: the ≤16 equivalence domain (non-wrapping, followR-free) — compared at
// EVERY cap. CAP16: the wrap-detection configs — pinned while cap == 16 only (D3).
struct MarchSpec {
    const char* name;
    Mode        mode;
    uint64_t    seed;
    int         winLen, winOff;      // the master window (setWindow)
    bool        paintedMacro;
    int         eastA, eastB;        // painted east voices (voice numbers, 0 = none)
    int         dynVoice, dynLen, dynOff;
    bool        wrappingLors;
    bool        subgates;
    bool        cap16Only;
};

static const MarchSpec kMarches[] = {
    // ── EQUIV (the oracle's core; zero differences required after the flip) ──
    {"E1_A_painted_window",     MODE_A,  0xA11CE0001ULL, 10, 3, true,  4,  6, 8, 5, 2, false, true,  false},
    {"E2_B_lane_lors",          MODE_B,  0xB0B000002ULL, 16, 0, false, 5,  9, 0, 0, 0, false, false, false},
    {"E3_BS_subgates",          MODE_BS, 0xC4AFE0003ULL, 12, 4, true,  3, 11, 9, 6, 2, false, true,  false},
    {"E4_B_all_voices_painted", MODE_B,  0xD00D0004ULL, 16, 0, true,  0,  0, 0, 0, 0, false, false, false},
    // ── CAP16WRAP (the wrap-detection configs; D3: they extend at frame 32) ──
    {"W1_A_wrapping_window",    MODE_A,  0xE1110005ULL, 12, 8, false, 2,  0, 0, 0, 0, true,  false, true },
    {"W2_BS_wrapping_lors",     MODE_BS, 0xF00F0006ULL, 16, 0, true,  7, 12, 0, 0, 0, true,  true,  true },
};
static const int kNumMarches = (int)(sizeof(kMarches) / sizeof(kMarches[0]));

// ── GATE 2: the beyond-ring zero-fill walk ───────────────────────────────────────
// Every per-step row's cells at index ≥ kPatternLengthCap exactly zero, for every
// lane and every voice row; the uint64 mask planes' bits ≥ kMaskRing clear. Runs
// mid-run AND at the end (a leak can be transient).
#define WALK_VOICESTRIDE(arr, what)                                                    \
    for (int v = 0; v < SequencerEngine::kVoiceSlots; ++v)                             \
        for (int i = dotModular::kPatternLengthCap; i < dotModular::kStepStorage; ++i) \
            if (arr[v][i] != 0.f) {                                                    \
                std::snprintf(msg, 256, "%s [%d][%d] = %.9g (must be 0)", what, v, i, (double)arr[v][i]); \
                return msg;                                                            \
            }
#define WALK_MATRIX2(arr, what) WALK_VOICESTRIDE(arr, what)

static const char* walkBeyondRing(const SequencerEngine& eng, char* msg) {
    const PatternEngine& pe = eng.pe;
    // The probability planes (lane × voice × step).
    for (int v = 0; v < SequencerEngine::kVoiceSlots; ++v)
        for (int s = 0; s < dotModular::NUM_STRANDS; ++s)
            for (int i = dotModular::kPatternLengthCap; i < dotModular::kStepStorage; ++i) {
                if (pe.random_[v][s][i] != 0.f) { std::snprintf(msg, 256, "random_ [%d][%d][%d] = %.9g", v, s, i, (double)pe.random_[v][s][i]); return msg; }
                if (pe.warped_[v][s][i] != 0.f) { std::snprintf(msg, 256, "warped_ [%d][%d][%d] = %.9g", v, s, i, (double)pe.warped_[v][s][i]); return msg; }
            }
    // The slew planes (engine + pre-remap + published + snapshots + sources).
    WALK_VOICESTRIDE(pe.preRemapSlewedRhythm,     "preRemapSlewedRhythm")
    WALK_VOICESTRIDE(pe.preRemapSlewedVariation,  "preRemapSlewedVariation")
    WALK_VOICESTRIDE(pe.preRemapSlewedLegato,     "preRemapSlewedLegato")
    WALK_VOICESTRIDE(pe.preRemapSlewedAccent,     "preRemapSlewedAccent")
    WALK_VOICESTRIDE(pe.preRemapSlewedMelody,     "preRemapSlewedMelody")
    WALK_VOICESTRIDE(pe.preRemapSlewedOctave,     "preRemapSlewedOctave")
    WALK_VOICESTRIDE(pe.preRemapSlewedQmix,       "preRemapSlewedQmix")
    WALK_VOICESTRIDE(pe.slewedRhythm,             "slewedRhythm")
    WALK_VOICESTRIDE(pe.slewedVariation,          "slewedVariation")
    WALK_VOICESTRIDE(pe.slewedLegato,             "slewedLegato")
    WALK_VOICESTRIDE(pe.slewedAccent,             "slewedAccent")
    WALK_VOICESTRIDE(pe.slewedMelody,             "slewedMelody")
    WALK_VOICESTRIDE(pe.slewedOctave,             "slewedOctave")
    WALK_VOICESTRIDE(pe.slewedQmix,               "slewedQmix")
    WALK_VOICESTRIDE(pe.pubSlewedPolyRhythm,      "pubSlewedPolyRhythm")
    WALK_VOICESTRIDE(pe.pubSlewedPolyMelody,      "pubSlewedPolyMelody")
    WALK_VOICESTRIDE(pe.pubSlewedPolyOctave,      "pubSlewedPolyOctave")
    WALK_VOICESTRIDE(pe.pubSlewedPolyAccent,      "pubSlewedPolyAccent")
    WALK_VOICESTRIDE(pe.pubSlewedPolyQmix,        "pubSlewedPolyQmix")
    WALK_VOICESTRIDE(pe.pubSlewedPolyVariation,   "pubSlewedPolyVariation")
    WALK_VOICESTRIDE(pe.pubSlewedPolyLegato,      "pubSlewedPolyLegato")
    WALK_MATRIX2(pe.remapSnapR, "remapSnapR")
    WALK_MATRIX2(pe.remapSnapM, "remapSnapM")
    WALK_MATRIX2(pe.remapSnapO, "remapSnapO")
    WALK_MATRIX2(pe.remapSnapQ, "remapSnapQ")
    WALK_MATRIX2(pe.remapSnapA, "remapSnapA")
    WALK_MATRIX2(pe.remapSnapV, "remapSnapV")
    WALK_MATRIX2(pe.remapSnapL, "remapSnapL")
    WALK_VOICESTRIDE(pe.rhythmSource,    "rhythmSource")
    WALK_VOICESTRIDE(pe.variationSource, "variationSource")
    WALK_VOICESTRIDE(pe.legatoSource,    "legatoSource")
    WALK_VOICESTRIDE(pe.accentSource,    "accentSource")
    WALK_VOICESTRIDE(pe.melodySource,    "melodySource")
    WALK_VOICESTRIDE(pe.octaveSource,    "octaveSource")
    WALK_VOICESTRIDE(pe.qmixSource,      "qmixSource")
    // The melody mirrors.
    for (int i = dotModular::kPatternLengthCap; i < dotModular::kStepStorage; ++i) {
        if (pe.melodySemitone[i] != 0)       { std::snprintf(msg, 256, "melodySemitone[%d] = %d", i, (int)pe.melodySemitone[i]); return msg; }
        if (pe.melodyPitchV[i] != 0.f)       { std::snprintf(msg, 256, "melodyPitchV[%d] = %.9g", i, (double)pe.melodyPitchV[i]); return msg; }
        if (pe.cachedMelodyPitchV[i] != 0.f) { std::snprintf(msg, 256, "cachedMelodyPitchV[%d] = %.9g", i, (double)pe.cachedMelodyPitchV[i]); return msg; }
        if (pe.cachedQmix[i] != 0.f)         { std::snprintf(msg, 256, "cachedQmix[%d] = %.9g", i, (double)pe.cachedQmix[i]); return msg; }
    }
    // The three Philox draw scratch buffers (the fill is RING-bounded).
    for (int i = dotModular::kPatternLengthCap * 64; i < dotModular::kStepStorage * 64; ++i)
        if (pe.rhythmDrawBuf[i] != 0.f) { std::snprintf(msg, 256, "rhythmDrawBuf[%d] = %.9g", i, (double)pe.rhythmDrawBuf[i]); return msg; }
    for (int i = dotModular::kPatternLengthCap * 32; i < dotModular::kStepStorage * 32; ++i)
        if (pe.melodyDrawBuf[i] != 0.f) { std::snprintf(msg, 256, "melodyDrawBuf[%d] = %.9g", i, (double)pe.melodyDrawBuf[i]); return msg; }
    for (int i = dotModular::kPatternLengthCap * 16; i < dotModular::kStepStorage * 16; ++i)
        if (pe.qmixDrawBuf[i] != 0.f)   { std::snprintf(msg, 256, "qmixDrawBuf[%d] = %.9g", i, (double)pe.qmixDrawBuf[i]); return msg; }
    // The cached draw tables: the scattered draw rows + the z rows (RING-bounded).
    for (const auto& kv : pe.rhythmDrawCache_) {
        const auto& d = kv.second.draw;
        WALK_MATRIX2(d.rhythm,    "cached rhythm draw")
        WALK_MATRIX2(d.variation, "cached variation draw")
        WALK_MATRIX2(d.legato,    "cached legato draw")
        WALK_MATRIX2(d.accent,    "cached accent draw")
        for (int v = 0; v < SequencerEngine::kVoiceSlots; ++v)
            for (int i = dotModular::kPatternLengthCap; i < dotModular::kStepStorage; ++i) {
                if (kv.second.zRhythm[v][i] != 0.0)    { std::snprintf(msg, 256, "zRhythm [%d][%d] = %.9g", v, i, kv.second.zRhythm[v][i]); return msg; }
                if (kv.second.zVariation[v][i] != 0.0) { std::snprintf(msg, 256, "zVariation [%d][%d] = %.9g", v, i, kv.second.zVariation[v][i]); return msg; }
                if (kv.second.zLegato[v][i] != 0.0)    { std::snprintf(msg, 256, "zLegato [%d][%d] = %.9g", v, i, kv.second.zLegato[v][i]); return msg; }
                if (kv.second.zAccent[v][i] != 0.0)    { std::snprintf(msg, 256, "zAccent [%d][%d] = %.9g", v, i, kv.second.zAccent[v][i]); return msg; }
            }
    }
    for (const auto& kv : pe.melodyDrawCache_) {
        const auto& d = kv.second.draw;
        WALK_MATRIX2(d.melody, "cached melody draw")
        WALK_MATRIX2(d.octave, "cached octave draw")
        for (int v = 0; v < SequencerEngine::kVoiceSlots; ++v)
            for (int i = dotModular::kPatternLengthCap; i < dotModular::kStepStorage; ++i) {
                if (kv.second.zMelody[v][i] != 0.0) { std::snprintf(msg, 256, "zMelody [%d][%d] = %.9g", v, i, kv.second.zMelody[v][i]); return msg; }
                if (kv.second.zOctave[v][i] != 0.0) { std::snprintf(msg, 256, "zOctave [%d][%d] = %.9g", v, i, kv.second.zOctave[v][i]); return msg; }
            }
    }
    for (const auto& kv : pe.qmixDrawCache_) {
        const auto& d = kv.second.draw;
        WALK_MATRIX2(d.qmix, "cached qmix draw")
        for (int v = 0; v < SequencerEngine::kVoiceSlots; ++v)
            for (int i = dotModular::kPatternLengthCap; i < dotModular::kStepStorage; ++i)
                if (kv.second.zQmix[v][i] != 0.0) { std::snprintf(msg, 256, "zQmix [%d][%d] = %.9g", v, i, kv.second.zQmix[v][i]); return msg; }
    }
    // The z0/z1 window caches (RING × MAX_FIELDS active cells).
    for (int i = dotModular::kPatternLengthCap * PatternEngine::ZWindowCache::MAX_FIELDS;
             i < dotModular::kStepStorage  * PatternEngine::ZWindowCache::MAX_FIELDS; ++i) {
        if (pe.rhythmZCache_.z0[i] != 0.0 || pe.rhythmZCache_.z1[i] != 0.0) { std::snprintf(msg, 256, "rhythmZCache z0/z1[%d] nonzero", i); return msg; }
        if (pe.melodyZCache_.z0[i] != 0.0 || pe.melodyZCache_.z1[i] != 0.0) { std::snprintf(msg, 256, "melodyZCache z0/z1[%d] nonzero", i); return msg; }
        if (pe.qmixZCache_.z0[i]   != 0.0 || pe.qmixZCache_.z1[i]   != 0.0) { std::snprintf(msg, 256, "qmixZCache z0/z1[%d] nonzero", i); return msg; }
    }
    // The widened uint64 mask planes: bits ≥ the RING must stay clear (the DECIDED
    // length-masked law; post-flip this is the 64-headroom bite).
    const uint64_t beyond = ~dotModular::kMaskLenMask;
    for (int v = 0; v < SequencerEngine::kVoiceSlots; ++v) {
        if (eng.maskAnchored_[v]    & beyond) { std::snprintf(msg, 256, "maskAnchored_[%d] bits beyond ring", v); return msg; }
        if (eng.maskAnchored_[v]   & beyond) { std::snprintf(msg, 256, "maskAnchored_[%d] bits beyond ring", v); return msg; }
        if (eng.maskLastPushed_[v] & beyond) { std::snprintf(msg, 256, "maskLastPushed_[%d] bits beyond ring", v); return msg; }
    }
    if (eng.maskMacroAnchored_    & beyond) { std::snprintf(msg, 256, "maskMacroAnchored_ bits beyond ring");    return msg; }
    if (eng.maskMacroAnchored_   & beyond) { std::snprintf(msg, 256, "maskMacroAnchored_ bits beyond ring");   return msg; }
    if (eng.maskMacroLastPushed_ & beyond) { std::snprintf(msg, 256, "maskMacroLastPushed_ bits beyond ring"); return msg; }
    if (eng.windowMask           & beyond) { std::snprintf(msg, 256, "windowMask bits beyond ring");           return msg; }
    return nullptr;
}

// ── main ─────────────────────────────────────────────────────────────────────────
static const char* kOraclePath = "test/goldens/steps32_gate1_cap16_ref.txt";

int main(int argc, char** argv) {
    const bool record = (argc > 1 && std::strcmp(argv[1], "--record") == 0);

    std::printf("STEPS_32 Stage 2b pre-gates (kPatternLengthCap=%d, kStepStorage=%d)\n",
                dotModular::kPatternLengthCap, dotModular::kStepStorage);

    // ── GATE 1: run every march once, keep per-step + whole digests ──
    std::vector<std::vector<uint64_t>> stepDig(kNumMarches);
    std::vector<uint64_t> wholeDig(kNumMarches);
    for (int m = 0; m < kNumMarches; ++m) {
        MarchCtx c;
        c.rng.seed64(kMarches[m].seed);
        c.eng.reset();
        injectLegacyStarGraph(c.eng);
        c.eng.numPolyVoices = 15;
        seedVariedRows(c.eng, c.rng);
        if (kMarches[m].subgates) varySubgates(c.eng, c.rng);
        c.eng.setWindow(kMarches[m].winLen, kMarches[m].winOff);
        authorLaneLors(c.eng, kMarches[m].wrappingLors);
        pushPaintedMasks(c.eng, kMarches[m].paintedMacro, kMarches[m].eastA, kMarches[m].eastB,
                         kMarches[m].dynVoice, kMarches[m].dynLen, kMarches[m].dynOff);
        c.noteVal = 2.f + (float)(int)(c.rng.nextFloat() * 4.999f);
        c.in = makeInput(0.20f + 0.70f * c.rng.nextFloat());

        TeeCapture t;
        t.steps = &stepDig[m];   // the tee records each step's digest at commit
        if      (kMarches[m].mode == MODE_A) marchModeA(c, t);
        else if (kMarches[m].mode == MODE_B) marchModeB(c, t);
        else                                 marchModeBS(c, t);
        wholeDig[m] = t.full.digest();
    }

    char msg[256];
    if (record) {
        FILE* f = std::fopen(kOraclePath, "w");
        if (!f) { std::printf("FAIL: cannot write %s (run from repo root)\n", kOraclePath); return 1; }
        std::fprintf(f, "# STEPS_32 GATE 1 — non-default <=16 equivalence oracle (the mis-read detector)\n");
        std::fprintf(f, "# Recorded per STEPS_32_PLAN.md \"STAGE 2b — cap flip: REQUIRED PRE-GATES\".\n");
        std::fprintf(f, "# EQUIV lines compare at EVERY cap (the <=16 equivalence domain).\n");
        std::fprintf(f, "# CAP16WRAP lines compare only while kPatternLengthCap == 16 (D3: a window\n");
        std::fprintf(f, "#  that wraps the 16-ring extends instead at frame 32 — not an equivalence\n");
        std::fprintf(f, "#  domain across the flip; pinned at cap 16).\n");
        for (int m = 0; m < kNumMarches; ++m) {
            std::fprintf(f, "%s %s whole 0x%016llx\n",
                         kMarches[m].cap16Only ? "CAP16WRAP" : "EQUIV", kMarches[m].name,
                         (unsigned long long)wholeDig[m]);
            for (int s = 0; s < (int)stepDig[m].size(); ++s)
                std::fprintf(f, "%s %s step%02d 0x%016llx\n",
                             kMarches[m].cap16Only ? "CAP16WRAP" : "EQUIV", kMarches[m].name,
                             s, (unsigned long long)stepDig[m][s]);
        }
        std::fclose(f);
        std::printf("recorded %d marches (whole + %zu steps each) -> %s\n",
                    kNumMarches, (size_t)kSteps, kOraclePath);
    }

    // Compare (also re-verified right after --record).
    {
        FILE* f = std::fopen(kOraclePath, "r");
        if (!f) {
            std::printf("FAIL: oracle %s missing — run with --record once at the 2a tip to generate it\n", kOraclePath);
            ++g_fail;
        } else {
            char line[256];
            int compared = 0, mismatches = 0;
            while (std::fgets(line, sizeof line, f)) {
                if (line[0] == '#' || line[0] == '\n') continue;
                char tag[32] = "", name[64] = "", kind[16] = ""; unsigned long long d = 0;
                if (std::sscanf(line, "%31s %63s %15s %llx", tag, name, kind, &d) != 4) continue;
                if (std::strcmp(tag, "CAP16WRAP") == 0 && dotModular::kPatternLengthCap != 16) continue;   // D3
                int found = -1;
                for (int m = 0; m < kNumMarches; ++m)
                    if (std::strcmp(kMarches[m].name, name) == 0) { found = m; break; }
                if (found < 0) { std::printf("FAIL: oracle names unknown march '%s'\n", name); ++g_fail; continue; }
                uint64_t actual;
                int stepIdx = -1;
                if (std::sscanf(kind, "step%d", &stepIdx) == 1) {
                    if (stepIdx < 0 || stepIdx >= (int)stepDig[found].size()) { std::printf("FAIL: oracle step %d out of range for '%s'\n", stepIdx, name); ++g_fail; continue; }
                    actual = stepDig[found][stepIdx];
                } else {
                    actual = wholeDig[found];
                }
                ++compared;
                if (actual != (uint64_t)d) {
                    ++mismatches;
                    if (mismatches <= 8) {
                        char where[48];
                        if (stepIdx >= 0) std::snprintf(where, sizeof where, "step %d", stepIdx);
                        else              std::snprintf(where, sizeof where, "whole stream");
                        std::printf("FAIL: march '%s' %s digest mismatch:\n  oracle 0x%016llx\n  actual 0x%016llx\n",
                                    name, where, d, (unsigned long long)actual);
                    }
                }
            }
            std::fclose(f);
            check(compared > 0, "oracle compared at least one digest");
            if (mismatches == 0) std::printf("GATE 1: %d digests compared (whole + per-step), ZERO differences\n", compared);
            else                 std::printf("GATE 1: %d digests compared, %d MISMATCHES (first divergent steps listed above)\n", compared, mismatches);
            g_fail += mismatches;
        }
    }

    // ── GATE 2: the beyond-ring zero-fill walks (intermediate + end samples) ──
    const int kSampleEvery = 8;
    struct Gate2Run { const char* name; Mode mode; uint64_t seed; int winLen, winOff;
                      bool paintedMacro; int eastA, eastB; int dynVoice, dynLen, dynOff;
                      bool wrappingLors; bool subgates; };
    static const Gate2Run g2[] = {
        {"E1-like", MODE_A,  0xA11CE0001ULL, 10, 3, true,  4,  6, 8, 5, 2, false, true },
        {"E3-like", MODE_BS, 0xC4AFE0003ULL, 12, 4, true,  3, 11, 9, 6, 2, false, true },
        {"W1-like", MODE_A,  0xE1110005ULL, 12, 8, false, 2,  0, 0, 0, 0, true,  false},
        {"E2-like", MODE_B,  0xB0B000002ULL, 16, 0, false, 5,  9, 0, 0, 0, false, false},
    };
    for (const auto& g : g2) {
        MarchCtx c;
        c.rng.seed64(g.seed);
        c.eng.reset();
        injectLegacyStarGraph(c.eng);
        c.eng.numPolyVoices = 15;
        seedVariedRows(c.eng, c.rng);
        if (g.subgates) varySubgates(c.eng, c.rng);
        c.eng.setWindow(g.winLen, g.winOff);
        authorLaneLors(c.eng, g.wrappingLors);
        pushPaintedMasks(c.eng, g.paintedMacro, g.eastA, g.eastB, g.dynVoice, g.dynLen, g.dynOff);
        c.noteVal = 2.f;
        c.in = makeInput(0.5f);

        int sampled = 0;
        const char* leak = walkBeyondRing(c.eng, msg);   // t=0 (pre-first-step) too
        check(leak == nullptr, leak ? leak : "beyond-ring zero at t=0");
        if (leak) std::printf("  (%s, %s)\n", g.name, leak);

        for (int step = 0; step < kSteps; ++step) {
            StepResult r;
            if (g.mode == MODE_A) {
                redDot::RateManager rm;
                rm.advance(redDot::Rate::Step, true);
                r = c.eng.executeModeA(rm, c.noteVal, c.in, +1);
                if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
            } else if (g.mode == MODE_B) {
                bool rose; c.main.step(c.rng, 0.50f, 0.40f, rose);
                r = c.eng.executeModeB(rose, c.main.high, c.noteVal, c.in);
                if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
            } else {
                bool mainRose; c.main.step(c.rng, 0.45f, 0.35f, mainRose);
                const bool ratchetRise = c.main.high && (c.rng.nextFloat() < 0.45f);
                const bool inGap = !c.main.high && !c.prevMainHigh;
                bool ghostRose = false;
                if (inGap) c.ghost.step(c.rng, 0.40f, 0.50f, ghostRose);
                r = c.eng.executeModeBSubdivided(mainRose, c.main.high, ratchetRise, c.noteVal, c.in, ghostRose, c.ghost.high);
                if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
                c.prevMainHigh = c.main.high;
            }
            if ((step + 1) % kSampleEvery == 0) {   // intermediate sample
                leak = walkBeyondRing(c.eng, msg);
                check(leak == nullptr, leak ? leak : "beyond-ring zero at intermediate sample");
                if (leak) std::printf("  (%s, step %d: %s)\n", g.name, step, leak);
                ++sampled;
            }
        }
        leak = walkBeyondRing(c.eng, msg);   // the END-of-run sample
        check(leak == nullptr, leak ? leak : "beyond-ring zero at end of run");
        if (leak) std::printf("  (%s, end: %s)\n", g.name, leak);
        std::printf("GATE 2: %s walked (%d intermediate samples + end)\n", g.name, sampled);
    }

    std::printf("%d passed %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
