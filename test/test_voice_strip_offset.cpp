// test_voice_strip_offset.cpp — CC_FIX_VOICE_STRIP_GLOBAL_OFFSET_PLACEMENT
//
// The voice-range strip drew its window tint + rotation chevron VOICE-LOCAL [O,O+L)
// (the same frame the probability lanes use) but its playhead from the ABSOLUTE
// engine read (voiceReadPosition / macroFrameRead — it carries the global offset
// offG), so with offG != 0 the playhead rendered offG steps OUTSIDE the drawn
// window. The fix: SandsRangeGestureCore::stripPlayheadCell folds offG out in window
// mode (and leaves the read absolute under the output mask, whose tint is the
// absolute global-column mask). Engine UNCHANGED — the read is correct; this pins
// the DISPLAY law and drives the engine to prove the folded read is in-window.
//
// Registered in run_all with $SE $GS $PE $CE.
#include <cstdio>
#include <cstdlib>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/SandsRangeGestureCore.hpp"

using redDot::SandsRangeGestureCore;
using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_fail; } } while (0)

static constexpr int RING = 32;   // kMaskRing == kPatternLengthCap == STEP_COUNT

// ── GATE 1: the pure fold law ───────────────────────────────────────────────
static void gate1_fold_law() {
    std::printf("gate 1: stripPlayheadCell fold law\n");
    for (int offG = 0; offG < RING; ++offG) {
        for (int O = 0; O < RING; ++O) {
            for (int phase = 0; phase < RING; ++phase) {
                const int engineRead = SandsRangeGestureCore::wrapMod(offG + O + phase, RING);
                // window mode: offG folded out → voice-local O+phase
                const int win = SandsRangeGestureCore::stripPlayheadCell(engineRead, offG, /*mask*/false, RING);
                CHECK(win == SandsRangeGestureCore::wrapMod(O + phase, RING), "window-mode fold != O+phase");
                // mask mode: absolute read passes through unchanged
                const int msk = SandsRangeGestureCore::stripPlayheadCell(engineRead, offG, /*mask*/true, RING);
                CHECK(msk == engineRead, "mask-mode must not fold");
            }
        }
    }
    // idle sentinel passes through in both modes
    CHECK(SandsRangeGestureCore::stripPlayheadCell(-1, 7, false, RING) == -1, "idle sentinel (window) must pass through");
    CHECK(SandsRangeGestureCore::stripPlayheadCell(-1, 7, true,  RING) == -1, "idle sentinel (mask) must pass through");
}

// ── GATE 2: the folded read is INSIDE the drawn window; the raw read is NOT ──
// Drives the engine with a global offset and a cycling MACRO row, exactly the
// path the strip shows (macroFrameRead). Asserts, across >=2 phrases:
//   folded  = stripPlayheadCell(read, offG, false) ∈ windowContains(O, L)
//   raw     = read is NOT in windowContains(O, L)   (offG != 0 — the bug the fold fixes)
static void gate2_read_in_window() {
    std::printf("gate 2: engine read folds into the drawn window\n");
    struct Row { int L, O, R; };
    const Row rows[] = { {7, 2, 4}, {6, 1, 3}, {8, 16, 0}, {5, 29, 2} };
    const int offsets[] = { 0, 3, 5, 7, 13 };
    for (int offG : offsets) {
        for (const Row& row : rows) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(16.f, 0.f, false, (float)offG, 0.f, false);
            eng.numPolyVoices = 4;
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = row.L;
            eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = row.O;
            eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = row.R;
            bool sawRawOutside = false;
            for (int t = 0; t < 2 * 16 + 1; ++t) {
                eng.advancePlayhead(1, nullptr);
                const int read = eng.macroFrameRead();
                if (read < 0) continue;
                const int folded = SandsRangeGestureCore::stripPlayheadCell(read, offG, /*mask*/false, RING);
                CHECK(SandsRangeGestureCore::windowContains(row.O, row.L, folded, RING),
                      "folded strip read is OUTSIDE the drawn window");
                if (offG != 0 && !SandsRangeGestureCore::windowContains(row.O, row.L, read, RING))
                    sawRawOutside = true;
            }
            // with a non-zero offset the UNFIXED (raw) read must land outside the
            // voice-local window at least once — proves the fold is doing real work
            if (offG != 0)
                CHECK(sawRawOutside, "raw read never left the window — the bug isn't reproduced");
        }
    }
}

// ── GATE 3: first-step anchor — at the global phrase start the folded cell is the
// rotation chevron (O + R%L), tying the DISPLAY to the landed phrase anchor. ─────
static void gate3_first_step_anchor() {
    std::printf("gate 3: phrase-start folded cell == rotation chevron (O + R%%L)\n");
    const int offsets[] = { 0, 3, 5, 7 };
    const int L = 7, O = 2, R = 4;
    for (int offG : offsets) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, (float)offG, 0.f, false);
        eng.numPolyVoices = 4;
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = L;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = O;
        eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = R;
        const int chevron = SandsRangeGestureCore::wrapMod(O + (R % L), RING);
        for (int t = 0; t < 3 * 16; ++t) {
            eng.advancePlayhead(1, nullptr);
            const bool phraseStart = (t % 16) == 0;   // T=1,17,33 → global step 1
            if (!phraseStart) continue;
            const int folded = SandsRangeGestureCore::stripPlayheadCell(eng.macroFrameRead(), offG, false, RING);
            CHECK(folded == chevron, "phrase-start folded cell != rotation chevron");
        }
    }
}

int main() {
    std::printf("== test_voice_strip_offset ==\n");
    gate1_fold_law();
    gate2_read_in_window();
    gate3_first_step_anchor();
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
