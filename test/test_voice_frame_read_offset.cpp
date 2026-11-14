// test_voice_frame_read_offset.cpp — CC_FIX_VOICE_FRAME_READ_OFFSET
//
// The voice/macro frame READ cell (voiceRead_ / macroFrameRead_ — what the voice-
// range strip's playhead draws) was a SECOND representation of "where the voice
// reads", separate from the frame VALUE the lanes seed off (frameT0 = wnow). It
// recomputed its own placement as `wnow + (abs1 ? O : offG + O)` — adding the
// GLOBAL offset in CYCLE mode only. So with a non-zero global offset the strip
// playhead landed on the rotation + offG instead of the rotation (Rodney, Rack:
// "the offset gets added to the rotation"). The frame value (and the audio/lanes)
// never had the offG; the read cell drifted from it.
//
// The fix collapses the read cell to ONE frame-derived law — read = wnow + O
// (== the frame value folded into the window), no offG, no mode branch — so the
// strip read can no longer drift from the frame value / the lanes. abs1 already
// used `wnow + O` (its offG rides in wnow via the phrase walk); CYCLE now matches.
//
// Reproduce-first: these gates FAIL on the pre-fix code for offG != 0 and PASS
// after. offG == 0 is bit-identical either way. Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/SandsRangeGestureCore.hpp"

using redDot::SandsRangeGestureCore;
using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_fail; } } while (0)

static constexpr int RING = 32;

struct Row { int L, O, R; };
static const Row kRows[]    = { {7, 2, 4}, {6, 1, 3}, {8, 16, 0}, {5, 9, 2} };
static const int kOffsets[] = { 0, 2, 3, 5, 7 };

// ── GATE 1: MACRO — the row's cycle read lands on the rotation at global step 1
//    and stays inside [O,O+L) for the whole run, for every global offset. ───────
static void gate1_macro() {
    std::printf("gate 1: macro cycle read anchors to the rotation under any offset\n");
    for (int offG : kOffsets) {
        for (const Row& row : kRows) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(16.f, 0.f, false, (float)offG, 0.f, false);
            eng.numPolyVoices = 4;
            eng.maskMacroKind_ = SequencerEngine::MASK_CYCLE;   // CYCLE (the complex mode)
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = row.L;
            eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = row.O;
            eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = row.R;
            const int anchor = SandsRangeGestureCore::wrapMod(row.O + (row.R % row.L), RING);
            for (int t = 0; t < 2 * 16 + 1; ++t) {
                eng.advancePlayhead(1, nullptr);
                const int read = eng.macroFrameRead();
                if (read < 0) continue;
                CHECK(SandsRangeGestureCore::windowContains(row.O, row.L, read, RING),
                      "macro read left the window [O,O+L) (offG leaked into the read)");
                if ((t % 16) == 0)   // global step 1 (T=1,17,33)
                    CHECK(read == anchor, "macro read != rotation (O + R%L) at global step 1");
            }
        }
    }
}

// ── GATE 2: EAST V1 (mono) — same law on the per-voice path. ──────────────────
static void gate2_east_v1() {
    std::printf("gate 2: east V1 cycle read anchors to the rotation under any offset\n");
    for (int offG : kOffsets) {
        for (const Row& row : kRows) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(16.f, 0.f, false, (float)offG, 0.f, false);
            eng.numPolyVoices = 4;
            // East V1 (mono slot 0): its OWN window, CYCLE (local mask kind).
            eng.voiceLorLocalEast_[0] = true;
            eng.maskLocalEast_[0]     = true;
            eng.maskKind_[0]          = SequencerEngine::MASK_CYCLE;
            eng.voiceLorStore_[0][SequencerEngine::LOR_LEN] = row.L;
            eng.voiceLorStore_[0][SequencerEngine::LOR_OFF] = row.O;
            eng.voiceLorStore_[0][SequencerEngine::LOR_ROT] = row.R;
            const int anchor = SandsRangeGestureCore::wrapMod(row.O + (row.R % row.L), RING);
            const auto v1 = VoiceResolver::slot(1);
            for (int t = 0; t < 2 * 16 + 1; ++t) {
                eng.advancePlayhead(1, nullptr);
                const int read = eng.voiceReadPosition(v1);
                if (read < 0) continue;
                CHECK(SandsRangeGestureCore::windowContains(row.O, row.L, read, RING),
                      "east V1 read left the window [O,O+L) (offG leaked into the read)");
                if ((t % 16) == 0)
                    CHECK(read == anchor, "east V1 read != rotation (O + R%L) at global step 1");
            }
        }
    }
}

int main() {
    std::printf("== test_voice_frame_read_offset ==\n");
    gate1_macro();
    gate2_east_v1();
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
