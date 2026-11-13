// test_stage5_combined.cpp — STEPS_32 STAGE 5 — VERIFY (the combined case)
//
// Per docs/design/STEPS_32_PLAN.md "### Stage 5 — verify (unchanged)": "<=16 goldens
// bit-identical; new 32-step pins: wrap at 32, both bars reachable, bar indicator tracks
// bar, mask bits address all 32 steps; combined subgate/advance/per-voice interaction at
// length 32 (the combined case, per the earlier lesson that clean merges can still have
// interaction bugs)."
//
// What each earlier stage already pinned (NOT duplicated here):
//   <=16 goldens bit-identical  — GATE 1 (test_stage2b_pregates, 260 digests vs the
//                                 committed 2a-tip oracle; runs in EVERY battery).
//   wrap at 32 / both bars      — test_stage2b_flip32 S1, via advancePlayhead alone.
//   bar indicator tracks bar    — test_ring_bar_indicator, deterministic-authoring loadout.
//   mask bits address 32 steps  — test_stage2b_flip32 S3, static push/loop per cell.
// THE GAP THIS SUITE CLOSES — the COMBINED case: those laws re-asserted while the FULL
// loadout runs through the REAL executors (mode A / mode B / mode B-subdivided), with:
//   - varied probability rows over ALL 32 cells (the GATE 1 mis-read detector, extended
//     past the old authored width — D3's extension is the feature under test),
//   - per-voice varied subgate thresholds (REST/LEG/ADV/ACC x RATCHET/GHOST),
//   - painted macro + east patterns with bits in BOTH bars (32-bit-clean),
//   - a MASK_DYNAMIC voice whose sound region is a BAR-2 window,
//   - lane LORs whose own walks cross the bar line (rhythm 20/0, melody 32/0, octave
//     7/27 — a bar-2 window that WRAPS the ring),
//   - per-voice poly LOR windows in bar 2, 15 poly voices, the legacy star graph,
//   - 96 steps = three full ring passes per executor.
//
// Sections (V for "verify"):
//   V0  the stage's preconditions (the flipped constants; the standing gate's oracle is
//       committed — its comparison runs in test_stage2b_pregates every battery).
//   V1  DETERMINISM at the combined loadout: every march built and run twice (same
//       seeds) — whole + per-step content digests and all per-stream digests
//       bit-identical — plus GATE 2's post-flip bite (the uint64 mask planes' bits past
//       the ring stay clear) sampled at t=0, every 8 steps, and end of run.
//   V2  WRAP AT 32 + BOTH BARS THROUGH THE REAL EXECUTORS: the mode-A march's walk
//       covers ALL 32 cells with period EXACTLY 32 (the seed is phase-dependent —
//       flip32's visited-set law — so the laws are the bound, the period, the
//       distinctness and the crossings); globalWalk_ tracks the landed cell; the walk
//       and the lane walks cross 15->16 and wrap 31->0; non-rest decisions land in
//       BOTH bars; a melody DRAW fills the ring. Plus the deterministic SUBGATE/
//       ADVANCE crossing: with subResLive and every ADVANCE column at +1, a RATCHET
//       edge walks the SHARED playhead 14->15->16 across the bar line and a GHOST edge
//       wraps 31->0 in the gap and plays (the three-level advance hierarchy at the
//       ring — the advance law's 32 face).
//   V3  THE DISPLAY LAW under the combined loadout (pure addition — the digests never
//       capture the display path, THE GOLDEN LAW's display face): the BAR LEDs stay
//       exclusive and follow stepIndex's bar layer on every landed frame of every
//       march; the lamp-0 tell; the playhead lamp is unique; the four standing levels
//       are the ONLY in-window levels; out-of-window current-lap masters ghost EXACTLY
//       at kStepGhostBrightness (the flip-table model's set at the (16,24) D3 wrap
//       window); the full window renders no ghosts and the trail lands on lamp 15 at
//       both ring wraps; mode D and the pre-start park silence both BAR LEDs.
//   V4  THE ONE MASK GATE, LIVE, ON THE MARCHES: outputMaskPasses evaluated at EVERY
//       landed frame (bar 2 included) equals the authored model — the painted macro
//       bit, the painted east bits, the dynamic voice's region — and the
//       effectiveOutputMask* projections match; a painted-off BAR-2 cell refuses the
//       mono output even with hot subgate thresholds (the mute multiplies post-engine,
//       in the OutputGenerator — the interaction the flip could have broken).
//   V5  THE BAR-1 EXTENSION EQUIVALENCE (engine-vs-engine, no oracle needed): the
//       (32,0) march's FIRST BAR is BIT-IDENTICAL to the (16,0) march's whole walk —
//       content + all per-voice substreams, frames 0..15 (the walk coordinate is
//       captured at its <=16 PROJECTION: setWindow re-anchors it ((tick-1) mod newLen),
//       the GATE 1 low16 discipline) — then the two diverge (one walks into bar 2, the
//       other wraps): D3's EXTENSION, not alteration, now under the combined loadout.
//       The pair uses the WALKING loadout (the mono MAIN-advance probability raised to
//       0.95 for BOTH twins — the varied draw mostly repeats and never leaves bar 1 in
//       96 frames).
//   V6  THE BAR-2 ISOLATION TWIN (the cross-row mis-read detector at 32): rotating
//       voice 6's bar-2 poly rows (its melody LOR is entirely bar 2) moves ONLY voice
//       6's stream; the mono lane and the other 14 leaf voices (the star graph's
//       leaves) are bit-identical for the WHOLE march — a bar-2 mis-read or cross-voice
//       bleed cannot land silently.
//   V7  THE COMBINED LOADOUT'S <=16 FACE: the same loadout at the (16,0) window keeps
//       every landing at -1 (the pre-start park before the first edge — the B/BS
//       streams can predate the seed) or inside [0,16) through all three executors,
//       BAR 1 lit throughout, no ghost renderable, and the bar-2 mirror cells stay
//       zero before any draw (GATE 1's domain is not disturbed by the Stage 5 loadout).
//
// No production code changed for Stage 5 — the plan's Stage 5 line is "verify".
//
// Build (run_all.sh): test_stage5_combined|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <set>
#include <vector>
#include <array>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
#include "star_seam.hpp"

using redDot::PhiloxRng;
namespace dm = dotModular;
namespace SK = dotModular::SubgateKnobs;

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
    void f32(float f) { uint32_t b; std::memcpy(&b, &f, 4); u32(b); }   // raw bits
    uint64_t digest() const {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < bytes.size(); ++i) h = fnv1a(h, bytes[i]);
        return h;
    }
    void clear() { bytes.clear(); }
};

// Per-step tee: every step's capture is digested alone AND appended to the march's
// whole stream (the fast-fail whole digest + the per-step comparison).
struct TeeCapture {
    Capture step, full;
    std::vector<uint64_t>* steps = nullptr;
    void commit() {
        if (steps) steps->push_back(step.digest());
        full.bytes.insert(full.bytes.end(), step.bytes.begin(), step.bytes.end());
    }
};

static int q12(float pitchV) { return (int)std::lrintf((double)pitchV * 12.0); }
static int slotIdx(int voiceNo) { return dm::VoiceResolver::slot(voiceNo).v; }   // voice 1..16 -> storage slot 0..15
static const bool DBG = std::getenv("STAGE5_DEBUG") != nullptr;

// ── the combined loadout: the authored width is the RING (32 cells) ─────────────
static constexpr int kCells = 32;
static void seedVariedRows32(SequencerEngine& eng, PhiloxRng& rng) {
    for (int i = 0; i < kCells; ++i) {   // ALL 32 cells varied — D3's extension is the feature under test
        eng.pe.melodyRandom[i]    = rng.nextFloat();
        eng.pe.octaveRandom[i]    = rng.nextFloat();
        eng.pe.qmixRandom[i]      = rng.nextFloat();
        eng.pe.rhythmRandom[i]    = rng.nextFloat();
        eng.pe.accentRandom[i]    = rng.nextFloat();
        eng.pe.variationRandom[i] = rng.nextFloat();
        eng.pe.legatoRandom[i]    = rng.nextFloat();
    }
    for (int v = 2; v <= 16; ++v) {
        const auto vs = dm::VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < kCells; ++i)
                eng.pe.polyRandom(vs, dm::LaneId{lane})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.05f + 0.60f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.55f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.80f * rng.nextFloat();
        eng.voices[vs].variationProb = 0.20f + 0.55f * rng.nextFloat();
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
    const auto m1 = dm::VoiceResolver::slot(1);
    eng.voices[m1].restProb      = 0.08f + 0.50f * rng.nextFloat();
    eng.voices[m1].accentProb    = 0.50f * rng.nextFloat();
    eng.voices[m1].legatoProb    = 0.85f * rng.nextFloat();
    eng.voices[m1].variationProb = 0.30f + 0.45f * rng.nextFloat();
    eng.voices[m1].qmixLevel     = rng.nextFloat();
}

// Non-default subgate thresholds, VARIED PER VOICE (a cross-row mis-read of the
// subRes plane reads the neighbour's threshold). subResLive stays FALSE (the pregates
// semantics — the rack-free engine reads the main values verbatim); the DETERMINISTIC
// advance driver in V2 sets subResLive itself.
static void varySubgates(SequencerEngine& eng, PhiloxRng& rng) {
    for (int v = 1; v <= 16; ++v) {
        const auto vs = dm::VoiceResolver::slot(v);
        for (int level = 0; level < SK::LEVELS; ++level) {
            eng.voices[vs].subRes[level][SK::REST]    = 0.10f + 0.75f * rng.nextFloat();
            eng.voices[vs].subRes[level][SK::LEGATO]  = 0.10f + 0.75f * rng.nextFloat();
            eng.voices[vs].subRes[level][SK::ADVANCE] = -0.80f + 1.60f * rng.nextFloat();
            eng.voices[vs].subRes[level][SK::ACCENT]  = 0.10f + 0.75f * rng.nextFloat();
        }
    }
}

// The painted loadout: 32-BIT-CLEAN patterns with bits in BOTH bars (no & 0xFFFF —
// bar-2 bits are the feature). followR stays FALSE everywhere (rotation is the 32-bit-
// ring feature pinned by the 32-step pins' own frame, not this stage's loadout).
static const uint64_t kMacroP  = 0x6D5A3C96ULL;   // mixed in both bars; cell 16 OFF (the V4 pin)
static const uint64_t kPatA    = 0xF00D4B1CULL;   // east voice 3, mixed both bars
static const uint64_t kPatB    = 0x1C4D00AFULL;   // east voice 7, mixed both bars
static const int      kEastA   = 3, kEastB = 7;
static const int      kDynVoice = 9, kDynLen = 10, kDynOff = 22;   // the sound region 22..31 — BAR 2

static void paint32(SequencerEngine& eng) {
    uint64_t macroBits[1] = { kMacroP };
    const float macroKind = 2.f;   // MASK_PAINTED
    uint64_t eastBits[16];
    float    eastKind[16], eastFollowR[16], localEast[16];
    for (int v = 0; v < 16; ++v) {
        eastBits[v] = dm::kMaskLenMask;   // CYCLE default — the all-on degenerate
        eastKind[v] = 0.f; eastFollowR[v] = 0.f; localEast[v] = 0.f;
    }
    eastBits[kEastA - 1] = kPatA; eastKind[kEastA - 1] = 2.f; localEast[kEastA - 1] = 1.f;
    eastBits[kEastB - 1] = kPatB; eastKind[kEastB - 1] = 2.f; localEast[kEastB - 1] = 1.f;
    eastBits[kDynVoice - 1] = dm::kMaskLenMask;                  // all-on overlay: the region IS the base
    eastKind[kDynVoice - 1] = 1.f; localEast[kDynVoice - 1] = 1.f;   // MASK_DYNAMIC (abs1)
    const auto vs = dm::VoiceResolver::slot(kDynVoice);
    eng.setVoiceLorLocalEast(vs, true);   // the LOR's LOCAL-EAST flag is its own switch (voiceLorLocalEast_) —
                                          // without it effectiveVoiceLor reads the MACRO LOR and the region is 0..15
    eng.voiceLorRef(vs, SequencerEngine::LOR_LEN) = kDynLen;
    eng.voiceLorRef(vs, SequencerEngine::LOR_OFF) = kDynOff;
    eng.pushMaskEditor(macroBits, macroKind, 0.f, eastBits, eastKind, eastFollowR, localEast);
}

// Lane LORs whose OWN walks cross the bar line / wrap the ring (Forward everywhere —
// the Stage 5 pins target length-32 interaction, not direction; directions are the lane
// suites' domain). Per-voice poly windows in bar 2; voice 6's melody LOR is ENTIRELY
// bar 2 (the V6 twin voice reads its bar-2 rows from the first frame).
static void authorLors32(SequencerEngine& eng) {
    eng.lorRef(dm::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 20;   // 0..19 — crosses 15->16
    eng.lorRef(dm::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 0;
    eng.lorRef(dm::STRAND_MELODY, SequencerEngine::LOR_LEN) = 32;   // the full-ring lane walk
    eng.lorRef(dm::STRAND_MELODY, SequencerEngine::LOR_OFF) = 0;
    eng.lorRef(dm::STRAND_OCTAVE, SequencerEngine::LOR_LEN) = 7;    // 27..31 + 0..2 — WRAPS the ring
    eng.lorRef(dm::STRAND_OCTAVE, SequencerEngine::LOR_OFF) = 27;
    const auto v3 = dm::VoiceResolver::slot(3);
    eng.polyLenERef(v3, dm::LaneId{SequencerEngine::PL_MELODY}) = 12;   // 20..31 — bar 2
    eng.polyOffERef(v3, dm::LaneId{SequencerEngine::PL_MELODY}) = 20;
    const auto v7 = dm::VoiceResolver::slot(7);
    eng.polyLenERef(v7, dm::LaneId{SequencerEngine::PL_REST}) = 8;      // 24..31 — bar 2
    eng.polyOffERef(v7, dm::LaneId{SequencerEngine::PL_REST})  = 24;
    const auto v6 = dm::VoiceResolver::slot(6);
    eng.polyLenERef(v6, dm::LaneId{SequencerEngine::PL_MELODY}) = 16;   // 16..31 — ENTIRELY bar 2
    eng.polyOffERef(v6, dm::LaneId{SequencerEngine::PL_MELODY}) = 16;
}

static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

// ── the march context + builder (GATE 1's discipline, at the combined loadout) ──
enum Mode { MODE_A, MODE_B, MODE_BS };

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

static void buildCombined(MarchCtx& c, uint64_t seed, int winLen, int winOff) {
    c.rng.seed64(seed);
    c.eng.reset();
    injectLegacyStarGraph(c.eng);
    c.eng.numPolyVoices = 15;
    seedVariedRows32(c.eng, c.rng);
    varySubgates(c.eng, c.rng);
    c.eng.setWindow(winLen, winOff);
    authorLors32(c.eng);
    paint32(c.eng);
    c.noteVal = 2.f + (float)(int)(c.rng.nextFloat() * 4.999f);
    c.in = makeInput(0.20f + 0.70f * c.rng.nextFloat());
}

// ── per-step captures ────────────────────────────────────────────────────────────
// The CONTENT stream: decisions/nv/gates/CV for mono + all 15 poly voices, the walk
// state, and the melody mirrors at their FULL 32-cell width (the post-flip projection —
// GATE 1's low16 projection was the cross-cap oracle's discipline, not ours).
static void captureContent(const SequencerEngine& eng, const StepResult& r, Capture& cap) {
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(r.stepped ? 1u : 0u);
    cap.u8(eng.gs.gateHeld ? 1u : 0u);
    cap.u8(eng.gsStep.gateHeld ? 1u : 0u);
    cap.i16(q12(eng.gs.currentPitchV));
    for (int v = 2; v <= 16; ++v) {
        const auto& pv = eng.voices[dm::VoiceResolver::slot(v)];
        cap.u8(pv.gs.gateHeld ? 1u : 0u);
        cap.u8(pv.gsStep.gateHeld ? 1u : 0u);
        cap.u8(pv.accented ? 1u : 0u);
        cap.u8(pv.participating ? 1u : 0u);
        cap.i16(q12(pv.gs.currentPitchV));
    }
    for (int s = 0; s < 7; ++s) cap.i16(eng.laneTick_[s]);
    cap.i16(eng.globalWalk_ & 15);   // the walk coordinate's <=16 PROJECTION (the GATE 1 low16
                                     // discipline): the raw coordinate is mod cachedLength — setWindow
                                     // re-anchors it ((tick-1) mod newLen), so the pre-first-tick state
                                     // reads 31 at (32,0) vs 15 at (16,0): the SAME transport state in
                                     // different moduli. The projection is the modulus-agnostic face the
                                     // V5 cross-window comparison needs; landed frames 0..15 are raw-equal.
    for (int i = 0; i < kCells; ++i) cap.i16(eng.pe.melodySemitone[i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.melodyPitchV[i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.cachedMelodyPitchV[i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.cachedQmix[i]);
}

// The MONO substream (V6's isolation law: a poly voice's bar-2 rows cannot feed it).
static void captureMonoSub(const SequencerEngine& eng, const StepResult& r, Capture& cap) {
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(r.stepped ? 1u : 0u);
    cap.u8(eng.gs.gateHeld ? 1u : 0u);
    cap.u8(eng.gsStep.gateHeld ? 1u : 0u);
    cap.i16(q12(eng.gs.currentPitchV));
    cap.i16(eng.scanOfsV_[0]);
    for (int s = 0; s < 7; ++s) cap.i16(eng.laneTick_[s]);
    cap.i16(eng.globalWalk_ & 15);   // the <=16 walk projection (see captureContent)
    for (int i = 0; i < kCells; ++i) cap.i16(eng.pe.melodySemitone[i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.melodyPitchV[i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.cachedMelodyPitchV[i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.cachedQmix[i]);
}

// The PER-VOICE substream (V6: each leaf voice's own sound — its gate state, its
// verdict/scan/ticks, and its published slew rows at full width).
static void captureVoiceSub(const SequencerEngine& eng, int voiceNo, Capture& cap) {
    const auto& pv = eng.voices[dm::VoiceResolver::slot(voiceNo)];
    const int s = slotIdx(voiceNo);
    cap.u8(pv.gs.gateHeld ? 1u : 0u);
    cap.u8(pv.gsStep.gateHeld ? 1u : 0u);
    cap.u8(pv.accented ? 1u : 0u);
    cap.u8(pv.participating ? 1u : 0u);
    cap.i16(q12(pv.gs.currentPitchV));
    cap.u8((unsigned)eng.polyVerdict_[s]);
    cap.i16(eng.polyNv_[s]);
    cap.i16(eng.scanOfsV_[s]);
    for (int l = 0; l < 7; ++l) cap.i16(eng.laneTickV_[s][l]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyRhythm[s][i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyMelody[s][i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyOctave[s][i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyAccent[s][i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyQmix[s][i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyVariation[s][i]);
    for (int i = 0; i < kCells; ++i) cap.f32(eng.pe.pubSlewedPolyLegato[s][i]);
}

// GATE 2's post-flip bite: the uint64 mask planes' bits past the ring stay clear.
// (The float rows' beyond-ring walk is vacuous since 2a — storage == ring == 32 —
// so the law that still bites is the 64-headroom mask law.)
static const char* leakMaskPlanes(const SequencerEngine& eng, char* msg) {
    const uint64_t beyond = ~dm::kMaskLenMask;
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

// ── the march report + driver ───────────────────────────────────────────────────
static const int kSteps = 96;   // three full ring passes

enum Probe { PROBE_NONE = 0, PROBE_FULL32, PROBE_BAR1, PROBE_WRAP24 };

struct MarchOut {
    TeeCapture content;
    TeeCapture monoSub;
    std::vector<TeeCapture> voice;          // 15, index voiceNo-2
    std::vector<uint64_t> contentSteps, monoSteps;
    std::vector<std::vector<uint64_t>> voiceSteps;
    std::vector<int> landings, walks;
    std::vector<std::array<int, 7>> ticks;
    std::vector<unsigned char> decisions;
    // V3's inline display-law accumulators (never digested — the GOLDEN LAW's display face)
    bool barFollow = true, barExcl = true, tellOK = true, noGhost = true;
    bool playUnique = true, levelsLegal = true, ghostSetOK = true;
    int  wrapLandings = 0;  bool trailAtWrap = true;
    // V4's inline mask-law accumulators
    bool maskMono = true, maskEA = true, maskEB = true, maskDyn = true, effMasks = true;
    // V2's accumulators
    bool landingLaw = true, walkTracks = true;
    bool crossBar = false, crossRing = false, laneCrossBar = false, laneWrapRing = false;
    bool octaveTickBounded = true;
    int  bar1Dec = 0, bar2Dec = 0;
    // the mirror faces: the bar-2 cells BEFORE any redraw (V7), and the ring-wide fill a
    // forced melody DRAW produces (V2 — the post-flip face of GATE 2's zero-fill law)
    bool mirrorBar2Zero = true;
    bool melodyBar2 = false;
    // V1's leak sampling
    int  leakSamples = 0;  bool leakOK = true;
    // DBG: the first 16 frames' content-step bytes (the V5 byte-diff)
    std::vector<std::vector<uint8_t>> dbgSteps;
};

static void marchCombined(MarchCtx& c, Mode mode, MarchOut& out, Probe probe) {
    redDot::RateManager rm;
    const std::set<int> winAll = [] { std::set<int> w; for (int i = 0; i < 32; ++i) w.insert(i); return w; }();
    char msg[256];
    for (int t = 0; t < kSteps; ++t) {
        StepResult r;
        if (mode == MODE_A) {
            rm.advance(redDot::Rate::Step, true);
            r = c.eng.executeModeA(rm, c.noteVal, c.in, +1);
            if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
        } else if (mode == MODE_B) {
            bool rose; c.main.step(c.rng, 0.50f, 0.40f, rose);
            r = c.eng.executeModeB(rose, c.main.high, c.noteVal, c.in);
            if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
            for (int i = 1; i <= c.eng.numPolyVoices; ++i)
                c.latch[i + 1] = c.eng.voices[dm::VoiceResolver::slot(i + 1)].gs.gateHeld;
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
                        monoOpen = monoOpen && c.eng.voices[dm::VoiceResolver::slot(u + 1)].gs.gateHeld;
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
                                             : c.eng.voices[dm::VoiceResolver::slot(u + 1)].gs.gateHeld);
                }
                const auto vs = dm::VoiceResolver::slot(i + 1);
                c.eng.voices[vs].gs.gateHeld = vOpen;
                if (!vOpen) { c.eng.voices[vs].gs.holdRemain = 0.f; c.eng.voices[vs].gs.gatePulseRemain = -1; }
            }
        } else {
            bool mainRose; c.main.step(c.rng, 0.45f, 0.35f, mainRose);
            const bool ratchetRise = c.main.high && (c.rng.nextFloat() < 0.45f);
            const bool inGap = !c.main.high && !c.prevMainHigh;
            bool ghostRose = false;
            if (inGap) c.ghost.step(c.rng, 0.40f, 0.50f, ghostRose);
            r = c.eng.executeModeBSubdivided(mainRose, c.main.high, ratchetRise, c.noteVal, c.in,
                                             ghostRose, c.ghost.high);
            if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
            for (int i = 1; i <= c.eng.numPolyVoices; ++i)
                c.latch[i + 1] = c.eng.voices[dm::VoiceResolver::slot(i + 1)].gs.gateHeld;
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
                        monoOpen = monoOpen && c.eng.voices[dm::VoiceResolver::slot(u + 1)].gs.gateHeld;
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
                                             : c.eng.voices[dm::VoiceResolver::slot(u + 1)].gs.gateHeld);
                }
                const auto vs = dm::VoiceResolver::slot(i + 1);
                c.eng.voices[vs].gs.gateHeld = vOpen;
                if (!vOpen) { c.eng.voices[vs].gs.holdRemain = 0.f; c.eng.voices[vs].gs.gatePulseRemain = -1; }
            }
        }

        // ── captures (the content + the per-stream substreams) ──
        captureContent(c.eng, r, out.content.step); out.content.commit(); out.content.step.clear();
        captureMonoSub(c.eng, r, out.monoSub.step); out.monoSub.commit(); out.monoSub.step.clear();
        for (int v = 2; v <= 16; ++v) {
            captureVoiceSub(c.eng, v, out.voice[v - 2].step);
            out.voice[v - 2].commit(); out.voice[v - 2].step.clear();
        }

        // ── logs ──
        out.landings.push_back(c.eng.stepIndex);
        out.walks.push_back(c.eng.globalWalk_);
        std::array<int, 7> tk;
        for (int s = 0; s < 7; ++s) tk[s] = c.eng.laneTick_[s];
        out.ticks.push_back(tk);
        out.decisions.push_back((unsigned char)r.decision);
        if (r.decision != MonoDecision::Rest) {
            if (c.eng.stepIndex < 16) ++out.bar1Dec; else ++out.bar2Dec;
        }

        // ── V2's walk laws (mode A: the deterministic +1 walk; the SEED is phase-
        // dependent — flip32 pinned the visited SET, not a start cell — so the law here
        // is the ring BOUND; the period + the distinctness are asserted from the log) ──
        if (mode == MODE_A && c.eng.stepIndex >= 0) {
            if (DBG && out.landingLaw && !(c.eng.stepIndex >= 0 && c.eng.stepIndex < 32))
                std::printf("  [dbg] landing out of ring t=%d step=%d\n", t, c.eng.stepIndex);
            out.landingLaw &= (c.eng.stepIndex >= 0 && c.eng.stepIndex < 32);
            out.walkTracks &= (c.eng.globalWalk_ == c.eng.stepIndex);
            if (t > 0) {
                const int prev = out.landings[t - 1];
                out.crossBar  |= (prev == 15 && c.eng.stepIndex == 16);
                out.crossRing |= (prev == 31 && c.eng.stepIndex == 0);
                const int prevR = out.ticks[t - 1][dm::STRAND_RHYTHM];
                const int curR  = c.eng.laneTick_[dm::STRAND_RHYTHM];
                out.laneCrossBar |= (prevR == 15 && curR == 16);   // the L=20 lane crosses the bar line
                const int prevM = out.ticks[t - 1][dm::STRAND_MELODY];
                const int curM  = c.eng.laneTick_[dm::STRAND_MELODY];
                out.laneWrapRing |= (prevM == 31 && curM == 0);    // the L=32 lane wraps the ring
            }
        }
        if (c.eng.laneTick_[dm::STRAND_OCTAVE] < 0 || c.eng.laneTick_[dm::STRAND_OCTAVE] >= 7)
            out.octaveTickBounded = false;   // the L=7 lane's own small ring

        // ── V3's display laws (live reads, never digested) ──
        if (probe != PROBE_NONE && c.eng.stepIndex >= 0) {
            const float b1 = c.eng.getBarLightBrightness(0), b2 = c.eng.getBarLightBrightness(1);
            out.barFollow &= ((b1 == 1.f) == (c.eng.stepIndex < 16)) &&
                             ((b2 == 1.f) == (c.eng.stepIndex >= 16));
            out.barExcl   &= (b1 != b2);
            if (c.eng.stepIndex % 16 == 0)
                out.tellOK &= ((c.eng.stepIndex < 16 && b1 == 1.f) || (c.eng.stepIndex >= 16 && b2 == 1.f));
            const int lamp = ((c.eng.stepIndex % 16) + 16) % 16;
            const int trailMaster = ((c.eng.stepIndex - c.eng.lastPlayDir) % dm::kMaskRing + dm::kMaskRing) % dm::kMaskRing;
            const int trailLamp  = trailMaster % 16;
            const int lapBase = (c.eng.stepIndex >= 16) ? 16 : 0;
            auto winHas = [&](int master) {
                if (probe == PROBE_FULL32) return true;
                if (probe == PROBE_BAR1)   return master < 16;
                return (((SequencerEngine::windowMaskBits(16, 24) >> master) & 1ULL) != 0ULL);
            };
            const bool trailInWin = winHas(trailMaster);
            for (int k = 0; k < 16; ++k) {
                const float br = c.eng.getStepLightBrightness(k);
                if (DBG && !(out.playUnique & ((br == 1.0f) == (k == lamp))))
                    std::printf("  [dbg] playUnique t=%d step=%d k=%d br=%.6f\n", t, c.eng.stepIndex, k, br);
                out.playUnique &= ((br == 1.0f) == (k == lamp));
                if (k == trailLamp && trailInWin && k != lamp) {
                    // the COMET TRAIL: the step just left — its level reads the TRAIL MASTER's
                    // own membership (across the bar line the master belongs to the other lap,
                    // so an out-of-window CURRENT-LAP master at 0.5 is the trail, not a bug)
                    if (DBG && out.levelsLegal && br != 0.5f)
                        std::printf("  [dbg] trail t=%d step=%d k=%d br=%.6f trailM=%d\n", t, c.eng.stepIndex, k, br, trailMaster);
                    out.levelsLegal &= (br == 0.5f);
                } else if (winHas(lapBase + k)) {
                    if (DBG && out.levelsLegal && !(br == 1.0f || br == 0.35f || br == 0.07f))
                        std::printf("  [dbg] levels t=%d step=%d k=%d br=%.6f master=%d\n", t, c.eng.stepIndex, k, br, lapBase + k);
                    out.levelsLegal &= (br == 1.0f || br == 0.35f || br == 0.07f);
                } else {
                    if (k != lamp) {   // the playhead lands in-window, but guard anyway
                        if (DBG && out.ghostSetOK && br != SequencerEngine::kStepGhostBrightness)
                            std::printf("  [dbg] ghost t=%d step=%d k=%d br=%.6f master=%d trailM=%d\n",
                                        t, c.eng.stepIndex, k, br, lapBase + k, trailMaster);
                        out.ghostSetOK &= (br == SequencerEngine::kStepGhostBrightness);   // the CURRENT-LAP ghost law
                    }
                    if (DBG && out.levelsLegal && !(br == SequencerEngine::kStepGhostBrightness || br == 1.0f))
                        std::printf("  [dbg] levels-out t=%d step=%d k=%d br=%.6f master=%d\n", t, c.eng.stepIndex, k, br, lapBase + k);
                    out.levelsLegal &= (br == SequencerEngine::kStepGhostBrightness || br == 1.0f);
                }
                if (probe != PROBE_WRAP24)
                    out.noGhost &= (br != SequencerEngine::kStepGhostBrightness);
            }
            if (c.eng.stepIndex == 0 && t > 0) {
                ++out.wrapLandings;
                out.trailAtWrap &= (c.eng.getStepLightBrightness(15) == 0.5f);
            }
        }

        // ── V4's mask law, live on the landed frames ──
        if (probe == PROBE_FULL32 && c.eng.stepIndex >= 0) {
            const int s = c.eng.stepIndex;
            const auto M  = dm::VoiceResolver::mono();
            const auto v3 = dm::VoiceResolver::slot(kEastA);
            const auto v7 = dm::VoiceResolver::slot(kEastB);
            const auto v9 = dm::VoiceResolver::slot(kDynVoice);
            const uint64_t dynWin = SequencerEngine::windowMaskBits(kDynLen, kDynOff);
            out.maskMono &= (c.eng.outputMaskPasses(M,  s) == ((kMacroP >> s) & 1ULL));
            out.maskEA   &= (c.eng.outputMaskPasses(v3, s) == ((kPatA   >> s) & 1ULL));
            out.maskEB   &= (c.eng.outputMaskPasses(v7, s) == ((kPatB   >> s) & 1ULL));
            out.maskDyn  &= (c.eng.outputMaskPasses(v9, s) == ((dynWin  >> s) & 1ULL));
            out.effMasks &= (c.eng.effectiveOutputMaskMacro() == kMacroP);
            out.effMasks &= (c.eng.effectiveOutputMask(v3) == kPatA);
            out.effMasks &= (c.eng.effectiveOutputMask(v9) == dynWin);
        }

        // ── V1's leak sampling (t=0 handled by the caller; every 8 + end here) ──
        if ((t + 1) % 8 == 0 || t == kSteps - 1) {
            const char* leak = leakMaskPlanes(c.eng, msg);
            out.leakOK &= (leak == nullptr);
            if (leak) std::printf("  (leak at step %d: %s)\n", t, leak);
            ++out.leakSamples;
        }
        if (DBG && t < 16) out.dbgSteps.push_back(out.content.step.bytes);
    }
    // PRE-draw: the mirrors' bar-2 cells (V7's law — nothing authored lands past the window)
    for (int i = 16; i < kCells; ++i)
        out.mirrorBar2Zero &= (c.eng.pe.melodySemitone[i] == 0) && (c.eng.pe.melodyPitchV[i] == 0.f) &&
                              (c.eng.pe.cachedMelodyPitchV[i] == 0.f) && (c.eng.pe.cachedQmix[i] == 0.f);
    // The melody DRAW fills the RING (V2's law): force a redraw AFTER the pre-draw scan —
    // the mirror's ring-wide fill is the post-flip face of GATE 2's law (cells 16..31 were
    // beyond-ring zero at cap 16; they are in-ring draws now).
    c.eng.pe.redrawMelody(c.in);
    for (int i = 16; i < kCells; ++i)
        out.melodyBar2 |= (c.eng.pe.melodySemitone[i] != 0);
}

static bool digestsEqual(const TeeCapture& a, const TeeCapture& b) {
    return a.full.bytes == b.full.bytes;
}

int main() {
    std::printf("STEPS_32 Stage 5 — the combined case (kPatternLengthCap=%d, kMaskRing=%d, kStepStorage=%d)\n",
                dm::kPatternLengthCap, dm::kMaskRing, dm::kStepStorage);

    // ── V0: the stage's preconditions ────────────────────────────────────────────
    check(dm::kPatternLengthCap == 32 && dm::kMaskRing == 32 && dm::kStepStorage == 32,
          "the 2b flip's constants stand (cap == ring == storage == 32)");
    {
        FILE* f = std::fopen("test/goldens/steps32_gate1_cap16_ref.txt", "r");
        check(f != nullptr, "the standing <=16 gate's oracle is committed (GATE 1 compares it in test_stage2b_pregates)");
        if (f) {
            char buf[64];
            check(std::fgets(buf, sizeof buf, f) != nullptr && buf[0] == '#',
                  "the oracle file is non-empty (the GATE 1 header line)");
            std::fclose(f);
        }
    }

    // ── V1: determinism at the combined loadout + the mask-plane zero-leak ───────
    struct Spec { const char* name; Mode mode; uint64_t seed; Probe probe; };
    static const Spec kSpecs[] = {
        {"A",  MODE_A,  0x5A6E50001ULL, PROBE_FULL32},
        {"B",  MODE_B,  0x5A6E50002ULL, PROBE_FULL32},
        {"BS", MODE_BS, 0x5A6E50003ULL, PROBE_FULL32},
    };
    char msg[256];
    std::vector<MarchOut> first;   // kept for V2/V3/V4 analysis
    for (const auto& sp : kSpecs) {
        MarchOut o1, o2;
        o1.voice.resize(15); o1.voiceSteps.resize(15);
        o2.voice.resize(15); o2.voiceSteps.resize(15);
        o1.content.steps = &o1.contentSteps; o1.monoSub.steps = &o1.monoSteps;
        o2.content.steps = &o2.contentSteps; o2.monoSub.steps = &o2.monoSteps;
        for (int v = 0; v < 15; ++v) { o1.voice[v].steps = &o1.voiceSteps[v]; o2.voice[v].steps = &o2.voiceSteps[v]; }

        MarchCtx c1; buildCombined(c1, sp.seed, 32, 0);
        { const char* leak = leakMaskPlanes(c1.eng, msg);   // the t=0 sample
          check(leak == nullptr, leak ? leak : "mask planes clear at t=0 (before any step)"); }
        marchCombined(c1, sp.mode, o1, sp.probe);

        MarchCtx c2; buildCombined(c2, sp.seed, 32, 0);
        marchCombined(c2, sp.mode, o2, PROBE_NONE);

        check(digestsEqual(o1.content, o2.content),
              "V1: the combined march is deterministic (content stream, built+run twice)");
        check(o1.contentSteps == o2.contentSteps,
              "V1: deterministic PER STEP too (every step's content digest)");
        check(o1.monoSub.full.bytes == o2.monoSub.full.bytes,
              "V1: the mono substream is deterministic");
        bool voicesDet = true;
        for (int v = 0; v < 15; ++v) voicesDet &= (o1.voice[v].full.bytes == o2.voice[v].full.bytes);
        check(voicesDet, "V1: all 15 per-voice substreams are deterministic");
        check(o1.leakOK && o1.leakSamples >= 12,
              "V1: the uint64 mask planes' bits past the ring stay clear (t=0 + every 8 + end)");
        first.push_back(std::move(o1));
    }

    // ── V2: wrap at 32 + both bars THROUGH the real executors ────────────────────
    {
        const MarchOut& a = first[0];
        // the walk's SEED is phase-dependent (flip32 pinned the visited SET, not a start
        // cell) — the laws are the BOUND, the PERIOD, the DISTINCTNESS, and the crossings
        bool period32 = true;
        for (int t = 0; t + 32 < kSteps; ++t) period32 &= (a.landings[t] == a.landings[t + 32]);
        std::set<int> seen32;
        for (int t = 0; t < 32; ++t) seen32.insert(a.landings[t]);
        check(a.landingLaw && period32 && (int)seen32.size() == 32,
              "V2: the mode-A executor's walk covers ALL 32 cells with period EXACTLY 32 (96 frames — the wrap law through the executor)");
        check(a.walkTracks, "V2: globalWalk_ tracks the landed cell on every frame");
        check(a.crossBar,   "V2: the walk crosses 15->16 in sequence (the bar line, not the old ring wrap)");
        check(a.crossRing,  "V2: the walk wraps 31->0 in sequence (the ring wrap at 32)");
        check(a.bar1Dec > 0 && a.bar2Dec > 0,
              "V2: non-rest decisions land in BOTH bars (bar 1 and bar 2 are live through the executor)");
        check(a.melodyBar2, "V2: a melody DRAW fills the RING (cells 16..31 written — the post-flip face of the draw mirror)");
        check(a.laneCrossBar, "V2: the rhythm lane's own walk (L=20) crosses the bar line (15->16)");
        check(a.laneWrapRing, "V2: the melody lane's own walk (L=32) wraps the ring (31->0)");
        check(a.octaveTickBounded, "V2: the octave lane's walk stays on its L=7 small ring (the wrapping bar-2 window's tick)");
    }

    // ── V2b: the deterministic subgate/advance crossing (the three-level hierarchy at 32) ──
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;   // the transport pin — mono only
        for (int v = 1; v <= 16; ++v)
            eng.voices[dm::VoiceResolver::slot(v)].restProb = 0.f;   // deterministic plays
        const auto M = dm::VoiceResolver::mono();
        eng.voices[M].variationProb = 1.f;        // the MAIN level always FORWARD (the p=1 identity)
        eng.voices[M].subResLive = true;          // the subgate columns go live
        eng.voices[M].subRes[SK::RATCHET][SK::ADVANCE] = 1.f;   // ratchet edges always FORWARD
        eng.voices[M].subRes[SK::GHOST][SK::ADVANCE]   = 1.f;   // ghost edges always FORWARD
        eng.voices[M].subRes[SK::GHOST][SK::REST]      = 0.f;   // ghosts play
        eng.setWindow(32, 0);
        PatternInput in = makeInput(0.f);

        bool seqOK = true; StepResult r{};
        r = eng.executeModeBSubdivided(true, true, false, 2.f, in);   // the seed edge
        seqOK &= (eng.stepIndex == 0);
        for (int i = 1; i <= 14; ++i) {   // main rises walk 1..14
            r = eng.executeModeBSubdivided(true, true, false, 2.f, in);
            seqOK &= (eng.stepIndex == i);
        }
        r = eng.executeModeBSubdivided(false, true, true, 2.f, in);   // a RATCHET edge
        seqOK &= (eng.stepIndex == 15);
        r = eng.executeModeBSubdivided(false, true, true, 2.f, in);
        const int ratchetCross = (eng.stepIndex == 16);   // THE BAR CROSSING ON A RATCHET EDGE
        for (int i = 17; i <= 31; ++i) {  // ratchet edges walk to the ring's end
            r = eng.executeModeBSubdivided(false, true, true, 2.f, in);
            seqOK &= (eng.stepIndex == i);
        }
        r = eng.executeModeBSubdivided(false, false, false, 2.f, in, /*ghostRise=*/true, /*ghostHigh=*/true);
        const bool ghostWrap = (eng.stepIndex == 0) && (r.decision != MonoDecision::Rest);   // THE RING WRAP ON A GHOST EDGE
        check(seqOK,         "V2b: with every ADVANCE column at +1 the edges walk the shared playhead exactly +1 per edge, 0..31");
        check(ratchetCross,  "V2b: the 15->16 bar crossing lands ON a RATCHET edge (the subgate tier drives the shared playhead across the bar line)");
        check(ghostWrap,     "V2b: the 31->0 ring wrap lands ON a GHOST edge and the ghost PLAYS (the gap tier crosses the ring wrap)");
    }

    // ── V3: the display law under the combined loadout ───────────────────────────
    {
        const MarchOut& a = first[0];
        const MarchOut& s = first[2];
        for (const MarchOut* o : { &a, &s }) {
            check(o->barFollow && o->barExcl,
                  "V3: the BAR LEDs are exclusive and follow stepIndex's bar layer on every landed frame (subgates + 15 voices live)");
            check(o->tellOK, "V3: at every lamp-0 landing the new bar's LED is already lit (invariant 2, combined loadout)");
            check(o->playUnique, "V3: exactly one lamp reads 1.0 per frame — the playhead landing (stepIndex % 16)");
            check(o->noGhost, "V3: the full window renders NO ghosts (P6 — the BAR LEDs alone carry the lap)");
            check(o->levelsLegal, "V3: every lamp's brightness is one of the standing levels (1.0 / 0.5 / 0.35 / 0.07 / ghost)");
        }
        // the ring-wrap trail law is a mode-A statement: the deterministic +1 walk wraps
        // exactly twice in 96 frames; the BS walk's repeats make its stepIndex-0 count
        // edge-dependent (its per-frame laws are asserted above).
        check(a.wrapLandings == 2 && a.trailAtWrap,
              "V3: at both mode-A ring-wrap landings the trail lands on lamp 15 (master 31's lamp, 0.5)");
    }

    // ── V3b: the ghost arc at the (16,24) D3 wrap window, combined loadout ───────
    {
        MarchOut o; o.voice.resize(15);
        MarchCtx c; buildCombined(c, 0x5A6E50004ULL, 16, 24);   // cells 24..31 + 0..7
        marchCombined(c, MODE_A, o, PROBE_WRAP24);
        check(o.ghostSetOK,
              "V3b: at the (16,24) wrap window the out-of-window current-lap masters ghost EXACTLY at kStepGhostBrightness (the flip-table model, combined loadout)");
        check(o.levelsLegal && o.playUnique && o.barFollow && o.barExcl,
              "V3b: the standing levels, the unique playhead lamp and the BAR LED law hold through the wrap");
    }

    // ── V4: the one mask gate, live, on the mode-A march ─────────────────────────
    {
        const MarchOut& a = first[0];
        check(a.maskMono, "V4: outputMaskPasses(mono) == the painted macro bit at EVERY landed frame (bar 2 included)");
        check(a.maskEA && a.maskEB,
              "V4: the painted east voices' outputs gate exactly at their own 32-bit patterns");
        check(a.maskDyn,  "V4: the MASK_DYNAMIC voice's output is confined to its bar-2 region (22..31) at every landed frame");
        check(a.effMasks, "V4: the effectiveOutputMask* projections match the authored patterns/region exactly");
        // the named interaction: cell 16 is painted OFF in kMacroP; the march lands it at t=16
        bool landedOff = false;
        for (int t = 0; t < kSteps; ++t)
            if (a.landings[t] == 16) landedOff = true;
        check(landedOff && ((kMacroP >> 16) & 1ULL) == 0ULL,
              "V4: the walk lands cell 16 and it is painted OFF in the authored macro pattern");
        SequencerEngine eng; eng.reset(); eng.setWindow(32, 0); paint32(eng);
        const auto M = dm::VoiceResolver::mono();
        check(!eng.outputMaskPasses(M, 16),
              "V4: a painted-off BAR-2 cell refuses the mono output even with hot subgate thresholds (the mute multiplies post-engine)");
    }

    // ── V5: the bar-1 extension equivalence — (32,0) bar 1 == the (16,0) walk ────
    {
        MarchOut w32, w16;
        w32.voice.resize(15); w16.voice.resize(15);
        w32.voiceSteps.resize(15); w16.voiceSteps.resize(15);
        w32.content.steps = &w32.contentSteps; w16.content.steps = &w16.contentSteps;
        w32.monoSub.steps = &w32.monoSteps;    w16.monoSub.steps = &w16.monoSteps;
        for (int v = 0; v < 15; ++v) { w32.voice[v].steps = &w32.voiceSteps[v]; w16.voice[v].steps = &w16.voiceSteps[v]; }
        MarchCtx c1; buildCombined(c1, 0x5A6E50003ULL, 32, 0);   // the BS seed — the richest executor
        MarchCtx c2; buildCombined(c2, 0x5A6E50003ULL, 16, 0);
        // THE WALKING LOADOUT: the varied mono variationProb draw (~0.3..0.75) mostly
        // REPEATS — in 96 frames the BS walk never leaves bar 1 and the pair would be
        // trivially identical. The extension claim needs a walk that CROSSES the bar
        // line: the mono MAIN-advance probability goes to 0.95 (the subgate tiers stay
        // as authored), IDENTICALLY for both twins — the equivalence comparison is
        // unaffected; the walk just actually walks.
        c1.eng.voices[dm::VoiceResolver::slot(1)].variationProb = 0.95f;
        c2.eng.voices[dm::VoiceResolver::slot(1)].variationProb = 0.95f;
        marchCombined(c1, MODE_BS, w32, PROBE_NONE);
        marchCombined(c2, MODE_BS, w16, PROBE_NONE);

        bool firstBar = true, firstBarVoices = true, firstBarMono = true;
        for (int t = 0; t < 16; ++t) {
            if (DBG && firstBar && w32.contentSteps[t] != w16.contentSteps[t])
                std::printf("  [dbg] V5 content diverges at frame %d (step32=%d step16=%d)\n",
                            t, w32.landings[t], w16.landings[t]);
            firstBar      &= (w32.contentSteps[t] == w16.contentSteps[t]);
            if (DBG && firstBarMono && w32.monoSteps[t] != w16.monoSteps[t])
                std::printf("  [dbg] V5 mono diverges at frame %d (step32=%d step16=%d)\n", t, w32.landings[t], w16.landings[t]);
            firstBarMono  &= (w32.monoSteps[t]    == w16.monoSteps[t]);
            for (int v = 0; v < 15; ++v) {
                if (DBG && firstBarVoices && w32.voiceSteps[v][t] != w16.voiceSteps[v][t])
                    std::printf("  [dbg] V5 voice %d diverges at frame %d\n", v + 2, t);
                firstBarVoices &= (w32.voiceSteps[v][t] == w16.voiceSteps[v][t]);
            }
        }
        check(firstBar,      "V5: the (32,0) march's first bar is BIT-IDENTICAL to the (16,0) march's whole walk (content, frames 0..15 — D3's extension, not alteration)");
        check(firstBarMono,  "V5: the mono substream matches too (frames 0..15)");
        check(firstBarVoices,"V5: all 15 per-voice substreams match (frames 0..15)");
        check(w32.content.full.digest() != w16.content.full.digest(),
              "V5: the marches then DIVERGE (bar 2 is new content, not a re-wrap — the sensitivity face)");
        if (DBG && (!firstBar || !firstBarMono || w32.content.full.digest() == w16.content.full.digest())) {
            std::printf("  [dbg] V5 landings32:");
            for (int t = 0; t < 48; ++t) std::printf(" %d", w32.landings[t]);
            std::printf("\n  [dbg] V5 landings16:");
            for (int t = 0; t < 48; ++t) std::printf(" %d", w16.landings[t]);
            std::printf("\n  [dbg] V5 len(c32)=%zu len(c16)=%zu\n", w32.content.full.bytes.size(), w16.content.full.bytes.size());
            for (int t = 0; t < 16; ++t) {
                const auto& A = w32.dbgSteps[t]; const auto& B = w16.dbgSteps[t];
                for (size_t b = 0; b < A.size() && b < B.size(); ++b)
                    if (A[b] != B[b]) {
                        std::printf("  [dbg] V5 frame %d first differing byte offset %zu (32: %02x 16: %02x)\n",
                                    t, b, (unsigned)A[b], (unsigned)B[b]);
                        break;
                    }
            }
        }
    }

    // ── V6: the bar-2 isolation twin ─────────────────────────────────────────────
    {
        MarchOut m1, m2;
        m1.voice.resize(15); m2.voice.resize(15);
        MarchCtx c1; buildCombined(c1, 0x5A6E50003ULL, 32, 0);
        marchCombined(c1, MODE_BS, m1, PROBE_NONE);
        MarchCtx c2; buildCombined(c2, 0x5A6E50003ULL, 32, 0);
        {   // the twin: voice 6's bar-2 poly rows rotate by one cell (cells 0..15 identical)
            const auto v6 = dm::VoiceResolver::slot(6);
            for (int lane = 0; lane < 7; ++lane) {
                auto& row = c2.eng.pe.polyRandom(v6, dm::LaneId{lane});
                const float keep = row[16];
                for (int i = 16; i < 31; ++i) row[i] = row[i + 1];
                row[31] = keep;
            }
        }
        marchCombined(c2, MODE_BS, m2, PROBE_NONE);

        check(m1.content.full.digest() != m2.content.full.digest(),
              "V6: rotating voice 6's bar-2 rows MOVES the march (bar-2 content is live — the sensitivity face)");
        check(m1.monoSub.full.digest() == m2.monoSub.full.digest(),
              "V6: the MONO lane's stream is bit-identical for the WHOLE march (a poly voice's bar-2 rows cannot feed it)");
        bool others = true;
        for (int v = 0; v < 15; ++v)
            if (v + 2 != 6) others &= (m1.voice[v].full.digest() == m2.voice[v].full.digest());
        check(others, "V6: the other 14 leaf voices' streams are bit-identical for the WHOLE march (no cross-voice bleed)");
        check(m1.voice[6 - 2].full.digest() != m2.voice[6 - 2].full.digest(),
              "V6: voice 6's own stream moved (its bar-2 rows are its own sound)");
    }

    // ── V7: the combined loadout's <=16 face ─────────────────────────────────────
    {
        for (const auto& sp : kSpecs) {
            MarchOut o; o.voice.resize(15);
            MarchCtx c; buildCombined(c, sp.seed, 16, 0);
            marchCombined(c, sp.mode, o, PROBE_BAR1);
            bool inBar1 = true;
            for (int t = 0; t < kSteps; ++t) {
                // the B/BS marches' FIRST frames can predate the seed edge (no rise yet ->
                // stepIndex -1, the pre-start park) — the law is -1 OR inside [0,16)
                const bool okLanding = (o.landings[t] == -1) || (o.landings[t] >= 0 && o.landings[t] < 16);
                if (DBG && inBar1 && !okLanding)
                    std::printf("  [dbg] V7 landing outside [0,16) at t=%d: %d (mode %s)\n", t, o.landings[t], sp.name);
                inBar1 &= okLanding;
            }
            check(inBar1, "V7: the (16,0) walk stays inside [0,16) through the combined loadout (GATE 1's domain)");
            check(o.barFollow && o.barExcl && o.tellOK,
                  "V7: BAR 1 lit / BAR 2 dim on EVERY frame — one-bar windows render STABLE");
            check(o.noGhost, "V7: no ghost can render (the current-lap masters 0..15 are all in-window)");
            const char* leak = leakMaskPlanes(c.eng, msg);
            check(leak == nullptr, leak ? leak : "V7: the mask planes stay clear at the (16,0) march's end");
            check(o.mirrorBar2Zero, "V7: the bar-2 mirror cells stay ZERO before any draw (nothing authored lands past the window)");
        }
    }

    std::printf("test_stage5_combined: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
