// test_macro_lane_holder_retire.cpp — CC_PROMPT_RETIRE_MACRO_LANE_HOLDER
//
// The macro-tier lane holder (macroLOR_ → macroLanePhase_ → macroLaneTick_/
// macroLaneSign_) was a display-only parallel position machine — the last survivor
// of the globalWalk_/dual-position disease. RETIRED; the Macro strip reads the
// DERIVED VIEW (macroLaneDisplay): the macro ROW's frame authority (macroFrameT0_)
// mapped through the lane's FULL latched LOR from the per-voice path — see == play.
//
// Reproduce-first evidence (captured on the pre-fix engine, this suite's stub run):
//   P1-fact: with the holder's LEN agreeing with the per-voice latched store and the
//   row frames agreeing, the holder tick EQUALED the audio tick and the composed
//   display cell (lanePlayheadBar — the ONE law, which re-applies the O/R the holder
//   drops) equaled the played cell AT NONZERO O/R — the doc's "does NOT diverge"
//   branch is real (the O/R lossiness was harmless in the current composition), so
//   the retirement is ODR cleanup for that vector.
//   P2-bug: with the per-voice latched LEN diverged from macroLOR_ (the real state
//   when a voice's own LEN CV is patched — macroLOR_ carried only Macro's own
//   base+CV), the holder walked the WRONG domain: the holder tick diverged from the
//   audio's per-voice tick and the composed display cell diverged from the played
//   cell ("FAIL: P2: the holder tick ... diverged (8 vs 6)" — 2 checks). The rig's
//   row length (8) must exceed the lane lens or the frame fold is identity and the
//   divergence cannot show (the reproduce lesson).
//
// The final pins (post-retire): the view matches the audio tick+sign in BOTH states.
// AUDIO IS BYTE-IDENTICAL by construction: the holder fed no audio reader — the full
// battery staying byte-identical after the deletion is that proof.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "../src/ui/SandsRangeGestureCore.hpp"   // lanePlayheadBar — the ONE playhead law (GLFW-free)

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

static dotModular::VoiceSlot v2() { return dotModular::VoiceResolver::slot(2); }

// The composed display cell — exactly what the Macro strip draws (calcPlayhead's
// delegation; the widget passes Macro's own L/O/R, which the managers keep equal to
// the per-voice store's authored values when no per-voice CV diverges them).
static int cellOf(int tick, int len, int off, int rot) {
    return redDot::SandsRangeGestureCore::lanePlayheadBar(tick, len, off, rot, dotModular::kMaskRing);
}

// The shared harness: the macro row mapped {8,0,0} (the row walk — the frame value
// 0..7 — must EXCEED the lane lens or the fold is identity and a LEN divergence can
// never show: the reproduce lesson); V2 follows it (the row IS the macro row — the
// frames agree); the REST lane (editor lane 3 == strand 3) carries L/O/R in the
// per-voice store.
struct Rig {
    SequencerEngine eng;
    static constexpr int el = SequencerEngine::PL_REST;   // == STRAND_RHYTHM (editor-aligned 0..4)
    int v;
    int storeLen, off, rot;
    Rig(int storeLen_, int off_, int rot_) : v(v2().v), storeLen(storeLen_), off(off_), rot(rot_) {
        eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN]  = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF]  = 0;
        eng.macroVoiceLor_[SequencerEngine::LOR_ROT]  = 0;
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 8;   // V2 follows the macro row (localEast false by default)
        eng.polyLORRef(v2(), dotModular::LaneId{el}, SequencerEngine::LOR_LEN) = storeLen;
        eng.polyLORRef(v2(), dotModular::LaneId{el}, SequencerEngine::LOR_OFF) = off;
        eng.polyLORRef(v2(), dotModular::LaneId{el}, SequencerEngine::LOR_ROT) = rot;
        for (int t = 0; t < 4; ++t) eng.advancePlayhead(1, nullptr);   // land past the reset edges
    }
};

static void p1_view_matches_audio_equal_len() {
    std::printf("P1 the view == the audio at equal LEN, nonzero O/R (the redundancy fact: nothing diverged there pre-fix either)\n");
    char msg[160];
    Rig rig(6, 2, 1);
    bool ok = true, cellsOk = true;
    for (int t = 0; t < 12; ++t) {
        rig.eng.advancePlayhead(1, nullptr);
        int vt, vs;
        rig.eng.macroLaneDisplay(Rig::el, vt, vs);
        ok = ok && (vt == rig.eng.laneTickV_[rig.v][Rig::el])
                && (vs == rig.eng.laneSignV_[rig.v][Rig::el]);
        const int audioTick = rig.eng.laneTickV_[rig.v][Rig::el];
        cellsOk = cellsOk && (cellOf(vt, rig.storeLen, rig.off, rig.rot)
                              == cellOf(audioTick, rig.storeLen, rig.off, rig.rot));
    }
    std::snprintf(msg, sizeof(msg), "P1: the view does not match the audio tick/sign in the agreeing state (equal LEN, frames agree)");
    CHECK(ok, msg);
    CHECK(cellsOk, "P1: the view's composed cell does not match the played cell (nonzero O/R — the ONE law both sides)");
}

static void p2_view_follows_per_voice_truth() {
    std::printf("P2 the view follows the per-voice latched truth under the LEN divergence (the lying display fixed)\n");
    char msg[160];
    Rig rig(6, 2, 1);   // the store L=6 — the holder walked macroLOR_'s 8; the view reads THIS store
    rig.eng.polyLORRef(v2(), dotModular::LaneId{Rig::el}, SequencerEngine::LOR_LEN) = 6;
    bool tickOk = true, cellOk = true;
    for (int t = 0; t < 20; ++t) {
        rig.eng.advancePlayhead(1, nullptr);
        int vt, vs;
        rig.eng.macroLaneDisplay(Rig::el, vt, vs);
        const int audioTick = rig.eng.laneTickV_[rig.v][Rig::el];
        tickOk = tickOk && (vt == audioTick) && (vs == rig.eng.laneSignV_[rig.v][Rig::el]);
        cellOk = cellOk && (cellOf(vt, 6, 2, 1) == cellOf(audioTick, 6, 2, 1));
    }
    std::snprintf(msg, sizeof(msg), "P2: the view tick does not match the audio tick under the LEN divergence — the view must read the per-voice latched LOR (see == play)");
    CHECK(tickOk, msg);
    CHECK(cellOk, "P2: the view's composed cell does not match the played cell under the LEN divergence");
}

int main() {
    std::printf("== test_macro_lane_holder_retire (CC_PROMPT_RETIRE_MACRO_LANE_HOLDER) ==\n");
    p1_view_matches_audio_equal_len();
    p2_view_follows_per_voice_truth();
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
