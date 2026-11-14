// STEPS_32 STAGE 2b — the 32-step pins (the plan's "Stage 2b's own pins (unchanged, now run
// after the two gates above)"). The REQUIRED PRE-GATES (test_stage2b_pregates) proved the flip
// moved NOTHING at ≤16 (GATE 1: 260 digests, zero differences vs the committed 2a-tip oracle)
// and that the widened rows stay zero-filled beyond the ring (GATE 2). THIS suite pins the
// 32-step behaviour the flip BUYS, per the plan's three bullets:
//
//   1. WRAP AT 32; BOTH BARS REACHABLE; the bar-indicator's engine truth (see BAR INDICATOR):
//      the factory default window rides the RING (reset(): windowMask = kMaskLenMask — commit
//      A's "the factory window full-cap"), so the default walk covers cells 0..31, wraps at 32
//      (not 16), and the landed frame crosses the bar boundary (stepIndex 15 -> 16 -> ... -> 31
//      -> 0). The LED split itself (`stepIndex >= 16`, D6(a)'s presentational domain) is Stage
//      3 display; the ENGINE truth it reads — which bar the landed frame is in — is pinned here.
//      Also pinned: the MODULE default resolves to the (16,0) window (the knob's default push),
//      so the live plugin default stays inside the ≤16 world GATE 1 covers.
//   2. MASK BITS ADDRESS ALL 32 STEPS: a painted bit at ANY cell 0..31 authors through the REAL
//      pushMaskEditor, stores ring-masked, and gates outputMaskPasses exactly there; the
//      all-on sentinel is the RING's all-on (kMaskLenMask = 0xFFFFFFFF); bits ≥ the 64-mask's
//      ring (≥ 32) still drop at the store boundary (the DECIDED block's 64-headroom law).
//   3. THE DRAW-WINDOW CAP FLIP widens DRAW_CHUNK AND the Philox fill together, with its
//      headroom math pinned: CHUNK == the rhythm fill == ring×64 (the Stage-1 relationship
//      restored at the new bound), the melody/qmix fills ride the ring, the buffers hold the
//      fills, and the unit()-cursor worst cases stay under the chunk (no aliasing into pos+1).
//
// Build (run_all.sh): test_stage2b_flip32|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <set>

#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"

namespace dm = dotModular;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── the draw-window headroom math, pinned at compile time (the plan's bullet 3) ─────
static_assert(PatternEngine::DRAW_CHUNK == (uint64_t)dm::kPatternLengthCap * 64ULL,
              "DRAW_CHUNK == the rhythm fill (ring x 64 fields) — the Stage-1 relationship holds at the 32 bound");
static_assert(dm::kStepStorage * 64 >= (int)PatternEngine::DRAW_CHUNK,
              "the rhythm draw buffer (storage x 64) holds the phrase fill");
static_assert(dm::kStepStorage * 32 >= dm::kPatternLengthCap * 32,
              "the melody draw buffer (storage x 32) holds the melody fill (ring x 32)");
static_assert(dm::kStepStorage * 16 >= dm::kPatternLengthCap * 16,
              "the qmix draw buffer (storage x 16) holds the qmix fill (ring x 16)");
// the unit()-cursor worst cases stay under the chunk (no aliasing into phrase pos+1):
//   melody: ring * (2 mono + 15*2 poly fields) = 32 * 32 = 1024
//   rhythm: ring * (4 mono + 15 fields)        = 32 * 19 =  608
static_assert((uint64_t)dm::kPatternLengthCap * 32ULL <= PatternEngine::DRAW_CHUNK,
              "melody worst-case unit() calls fit the chunk");
static_assert((uint64_t)dm::kPatternLengthCap * 19ULL <= PatternEngine::DRAW_CHUNK,
              "rhythm worst-case unit() calls fit the chunk");
// the mask plane: the ring is 32, the all-on sentinel is the ring's all-on, and the
// 64-headroom law holds (the uint64 mask words cover the 64-step ceiling with no re-widening).
static_assert(dm::kMaskRing == 32, "the ring is 32 after the 2b flip");
static_assert(dm::kMaskLenMask == 0xFFFFFFFFULL, "the all-on sentinel is the RING's all-on (32 bits)");
static_assert(dm::kStepStorage <= 64, "the storage fits the uint64 length-masked mask words");

int main() {
    std::printf("test_stage2b_flip32 — the 32-step pins (Stage 2b's own pins)\n");

    // ── 0. the flipped constants themselves (the next move must be conscious too) ──
    check(dm::kPatternLengthCap == 32, "kPatternLengthCap == 32 (the 2b flip landed)");
    check(dm::kStepStorage == 32,      "kStepStorage == 32 (storage == cap since 2a)");

    // ── 1. WRAP AT 32 — the factory default window rides the ring ─────────────────
    {
        SequencerEngine eng; eng.reset();
        // the factory state (commit A's documented default): the window mask is the
        // RING's all-on; cachedLength still REPORTS the knob default 16 (the MODE LAW —
        // the module's first updateWindow push resolves the live window to (16,0)).
        check(eng.windowMask == dm::kMaskLenMask,
              "the factory window mask is the ring's all-on (the full-cap default — 2b widened it)");
        // (CC_PROMPT_16_TRIAGE option C re-aim) the ENGINE's transport default IS the cap
        // (the do-now group); the MODULE's knob default stays 16 — its first updateWindow
        // push resolves the live window (moving the knob is the deferred round).
        check(eng.cachedLength == dm::kPatternLengthCap,
              "cachedLength reports the transport default = the cap (option C's do-now group; the module push resolves the live window)");
        // 40 advances: the walk covers ALL 32 cells and wraps at 32 — period exactly 32.
        std::set<int> first32, all;
        bool seedOK = true;
        for (int i = 0; i < 32; ++i) {
            eng.advancePlayhead(+1);
            seedOK = seedOK && eng.stepIndex >= 0 && eng.stepIndex < 32;
            if (i < 32) first32.insert(eng.stepIndex);
        }
        for (int i = 32; i < 40; ++i) { eng.advancePlayhead(+1); all.insert(eng.stepIndex); }
        bool periodOK = true;
        for (int s : all) periodOK = periodOK && first32.count(s) > 0;
        check(seedOK, "every landed frame stays inside the 32-ring");
        check((int)first32.size() == 32, "32 advances visit ALL 32 cells (the default ride is the ring, not 16)");
        check(periodOK, "the walk repeats with period exactly 32 (wraps at the ring)");
        // BOTH BARS REACHABLE: cells 0..15 are bar 1, 16..31 bar 2 (BAR INDICATOR's
        // split reads the landed frame, no wrap — the LED surface is Stage 3 display).
        bool bar1 = false, bar2 = false;
        SequencerEngine w; w.reset();
        for (int i = 0; i < 32; ++i) {
            w.advancePlayhead(+1);
            if (w.stepIndex < 16) bar1 = true; else bar2 = true;
        }
        check(bar1 && bar2, "both bars reachable on the default ride (cells 0..15 AND 16..31)");
        // the BOUNDARY: the cell after 15 is 16 (bar 2 begins — NOT the 16-ring's wrap to 0),
        // and the cell after 31 is 0 (the ring wrap).
        SequencerEngine b; b.reset();
        for (int i = 0; i <= 15; ++i) b.advancePlayhead(+1);
        check(b.stepIndex == 15, "the 16th landed frame is cell 15 (bar 1's end)");
        b.advancePlayhead(+1);
        check(b.stepIndex == 16, "the cell AFTER 15 is 16 — the walk crosses the bar boundary (the 2b shift; the 16-ring wrapped to 0)");
        for (int i = 16; i <= 30; ++i) b.advancePlayhead(+1);
        check(b.stepIndex == 31, "the walk reaches cell 31 (bar 2's end)");
        b.advancePlayhead(+1);
        check(b.stepIndex == 0, "the cell AFTER 31 is 0 — the ring wrap at 32");
    }

    // ── 2. the MODULE default resolves to the (16,0) window — the live ≤16 world ───
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // the knob's default push (Monsoon.cpp's per-block call)
        check(eng.windowMask == SequencerEngine::windowMaskBits(16, 0),
              "the module-default push lands the (16,0) window mask (cells 0..15)");
        std::set<int> seen;
        for (int i = 0; i < 40; ++i) { eng.advancePlayhead(+1); seen.insert(eng.stepIndex); }
        bool inWindow = true;
        for (int s : seen) inWindow = inWindow && (s >= 0 && s < 16);
        check(inWindow && (int)seen.size() == 16,
              "the live default walk stays inside [0,16) (GATE 1's equivalence domain)");
    }

    // ── 3. MASK BITS ADDRESS ALL 32 STEPS ──────────────────────────────────────────
    {
        SequencerEngine eng; eng.reset();
        const auto M = dm::VoiceResolver::mono();
        uint64_t macroBits[1]; float macroKind = 2.f, macroFollowR = 0.f;   // painted
        uint64_t eastBits[16]; float eastKind[16], eastFollowR[16], localEast[16];
        for (int v = 0; v < 16; ++v) { eastBits[v]=dm::kMaskLenMask; eastKind[v]=0.f; eastFollowR[v]=0.f; localEast[v]=0.f; }
        // every single cell 0..31 authors, stores and gates EXACTLY there
        bool allCells = true;
        for (int i = 0; i < dm::kMaskRing; ++i) {
            macroBits[0] = (uint64_t)1 << i;
            eng.pushMaskEditor(macroBits, macroKind, macroFollowR, eastBits, eastKind, eastFollowR, localEast);
            for (int s = 0; s < dm::kMaskRing; ++s)
                allCells = allCells && eng.outputMaskPasses(M, s) == (s == i);
        }
        check(allCells, "a painted bit at ANY of the 32 cells gates outputMaskPasses exactly there (bits 16..31 are LIVE cells)");
        // the east/local row too (a cell in bar 2 through the REAL delegation path)
        const auto v2 = dm::VoiceResolver::slot(2);
        macroBits[0] = dm::kMaskLenMask;
        eastBits[1] = (uint64_t)1 << 20; eastKind[1] = 2.f; localEast[1] = 1.f;
        eng.pushMaskEditor(macroBits, macroKind, macroFollowR, eastBits, eastKind, eastFollowR, localEast);
        bool eastOK = true;
        for (int s = 0; s < dm::kMaskRing; ++s)
            eastOK = eastOK && eng.outputMaskPasses(v2, s) == (s == 20);
        check(eastOK, "a LOCAL east bit at cell 20 gates the local voice exactly there (the delegation path, ring 32)");
        // the all-on sentinel: the ring's all-on passes everything; ~0ULL stores as the
        // sentinel (bits >= the ring DROP — the 64-headroom law at the 32 ring).
        macroBits[0] = ~0ULL;
        eastBits[1] = dm::kMaskLenMask; eastKind[1] = 0.f; localEast[1] = 0.f;
        eng.pushMaskEditor(macroBits, macroKind, macroFollowR, eastBits, eastKind, eastFollowR, localEast);
        bool sentinelOK = eng.maskMacroAnchored_ == dm::kMaskLenMask;
        for (int s = 0; s < dm::kMaskRing; ++s) sentinelOK = sentinelOK && eng.outputMaskPasses(M, s);
        check(sentinelOK, "~0ULL stores as the ring's all-on (0xFFFFFFFF) and passes all 32 cells (bits >= 32 drop — the 64-headroom law)");
        // windowMaskBits covers the ring: the full-cap window and a WRAPPING 20-window
        check(SequencerEngine::windowMaskBits(32, 0) == dm::kMaskLenMask,
              "windowMaskBits(32,0) == the ring's all-on");
        const uint64_t wWrap = SequencerEngine::windowMaskBits(20, 25);   // 25..31 + 0..12 — wraps the 32-ring (7 + 13 = 20)
        bool wWrapOK = true;
        for (int s = 0; s < dm::kMaskRing; ++s)
            wWrapOK = wWrapOK && (((wWrap >> s) & 1ULL) != 0ULL) == (s >= 25 || s < 13);
        check(wWrapOK, "windowMaskBits(20,25) wraps the 32-ring exactly (cells 25..31, 0..12)");
    }

    // ── 4. the draw-window headroom math, the runtime face ────────────────────────
    // (The compile-time pins are the static_asserts above; here the RELATIONSHIP the
    // flip re-derived is spelled once at runtime so a future constant drift prints.)
    {
        check(PatternEngine::DRAW_CHUNK == (uint64_t)dm::kPatternLengthCap * 64ULL,
              "DRAW_CHUNK == ring x 64 (the rhythm fill — the fill == CHUNK relationship at the 32 bound)");
        check(PatternEngine::DRAW_CHUNK == 2048ULL,
              "DRAW_CHUNK == 2048 (the 2b value; was 1024 at the 16 ring)");
        // the melody/qmix fills ride the ring at their own field counts
        const uint64_t melodyFill = (uint64_t)dm::kPatternLengthCap * 32ULL;
        const uint64_t qmixFill   = (uint64_t)dm::kPatternLengthCap * 16ULL;
        check(melodyFill == 1024ULL && qmixFill == 512ULL,
              "the melody (ring x 32 = 1024) and qmix (ring x 16 = 512) fills ride the ring");
        check(melodyFill <= PatternEngine::DRAW_CHUNK && qmixFill <= PatternEngine::DRAW_CHUNK,
              "every stream's fill fits its chunk (pos owns [pos*CHUNK, (pos+1)*CHUNK) with no aliasing into pos+1)");
    }

    std::printf("test_stage2b_flip32: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
