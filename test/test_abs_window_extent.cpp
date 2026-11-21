// test_abs_window_extent — ABSOLUTE mode must honour the voice's LENGTH as its window extent.
//
// THE DESIGN (UNIFIED_ADDRESSING_SYSTEM.md, the original ideas; Rodney confirmed 2026-11-21):
// L = LENGTH = the number of 1/16 steps the lane spans; O + L define the WINDOW [O, O+L) — L steps —
// in ANY mode. ABSOLUTE mode changes the ANCHORING (the window is pinned to absolute global steps), NOT
// the window LENGTH. So a voice range 9..16 (O=8, L=8) must read only cells 8..15, in cycle OR absolute.
//
// THE BUG (reproduced in diag_abs_window_extent): ABS1 / MASK_DYNAMIC keeps voiceRead_ = wnow + O with
// wnow spanning the full global phrase (0..G-1) because the L = cachedLength overrides
// (SequencerEngine.cpp:986/:1116, SequencerEngine.hpp:705) widen the walk period from L to G. So the read
// runs O..O+G-1 (cells 8..23 = steps 9..24) instead of the window [O, O+L).
//
// RULING (Rodney 2026-11-21, option A): abs1 = cycle with the cycling off. It sounds once as the global
// playhead passes the window; rotation wraps INSIDE the window. So every SOUNDING step reads within [O,O+L).
// INVARIANT: over a full global phrase every sounding voice read stays inside its window [O, O+L). Cycle already satisfies it (the control below); abs1 must too. This is the window
// EXTENT only — the within-window anchoring is a separate concern (feel-tested in Rack).
//
// Reproduce-first: FAILS on the current abs1 arm (reads O..O+G-1), PASSES once the walk is bounded to L.
// Compile: see test/run_all.sh.

#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

// Sweep a full global phrase; return the number of reads that fell OUTSIDE the window [O, O+L).
static int bleedCount(bool abs1, int O, int L, int R, SequencerEngine::LaneDir dir, int* soundingOut = nullptr) {
    SequencerEngine eng; eng.reset(); eng.numPolyVoices = 2;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
        eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    const auto v = VoiceResolver::slot(2);
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(v, LaneId{kLanes[l]})[i]=.5f;
    eng.voiceLorRef(v, SequencerEngine::LOR_LEN)=L;
    eng.voiceLorRef(v, SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(v, SequencerEngine::LOR_ROT)=R;
    eng.setVoiceLorLocalEast(v, true);
    eng.voiceLorDir_[v.v]=dir;
    if (abs1) { eng.maskLocalEast_[v.v]=true; eng.maskKind_[v.v]=SequencerEngine::MASK_DYNAMIC; }
    eng.updateWindow(16.f,0.f,false,0.f,0.f,false);
    int outside=0, sounding=0;
    for (int s=0;s<16;s++){
        eng.advancePlayhead(1,nullptr);
        int rd = eng.voiceRead_[v.v];
        // ABS1 sounds only where the output gate passes (the global step is in the window);
        // silent steps may read anywhere. CYCLE has no base gate: every step sounds.
        const bool sounds = abs1 ? eng.outputMaskPasses(v, eng.pos_.playedColumn()) : true;
        // ABS1 outside the window: silent, and the playhead (the strip's read) FOLLOWS THE GLOBAL STEP.
        if (abs1 && !sounds && rd != eng.pos_.playedColumn()) { ++outside; }
        if (!sounds) continue;
        ++sounding;
        if (rd < O || rd >= O+L) ++outside;
    }
    if (soundingOut) *soundingOut = sounding;
    return outside;
}

int main(){
    std::printf("test_abs_window_extent: an absolute-mode voice reads only cells in its window [O,O+L)\n");
    char msg[200];
    const int L = 8;
    const int Os[] = { 0, 4, 8 };
    const int Rs[] = { 0, 3, 5 };           // R-sweep (R==0 is degenerate), mid rotations
    const SequencerEngine::LaneDir dirs[2] = { SequencerEngine::LaneDir::Forward, SequencerEngine::LaneDir::Reverse };

    // Control: CYCLE mode must already be bounded (guards the test setup + the harness).
    for (int oi=0; oi<3; ++oi) for (int ri=0; ri<3; ++ri) for (int di=0; di<2; ++di) {
        int bleed = bleedCount(false, Os[oi], L, Rs[ri], dirs[di]);
        std::snprintf(msg, sizeof msg, "CYCLE control bounded (O=%d R=%d %s): %d cells outside window",
                      Os[oi], Rs[ri], di?"REV":"FWD", bleed);
        CHECK(bleed == 0, msg);
    }
    // The property under test: ABSOLUTE mode must be bounded to [O, O+L) too.
    for (int oi=0; oi<3; ++oi) for (int ri=0; ri<3; ++ri) for (int di=0; di<2; ++di) {
        int snd = 0;
        int bleed = bleedCount(true, Os[oi], L, Rs[ri], dirs[di], &snd);
        std::snprintf(msg, sizeof msg, "ABS1 sounding read in window + silent read follows global step (O=%d R=%d %s): %d cells outside",
                      Os[oi], Rs[ri], di?"REV":"FWD", bleed);
        CHECK(bleed == 0, msg);
        std::snprintf(msg, sizeof msg, "ABS1 sounds exactly L steps once (O=%d R=%d %s): %d", Os[oi], Rs[ri], di?"REV":"FWD", snd);
        CHECK(snd == L, msg);
    }

    std::printf(g_fail ? "\nFAILED (%d)\n" : "\nOK\n", g_fail);
    return g_fail ? 1 : 0;
}
