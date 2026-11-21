// test_abs_cycle_consistent — ABSOLUTE mode (range base, MASK_DYNAMIC) is CYCLE mode with a mute.
//
// THE LOCKED MODEL (UNIFIED_ADDRESSING_SYSTEM.md, "[RESOLVED — Rodney 2026-11-21] CYCLE-CONSISTENT
// ABSOLUTE MODE"): content is ALWAYS the cycle read (the voice's own L, rotation R window-relative);
// the range base only MUTES the global steps outside [O, O+L). So, for the SAME voice row:
//   (1) at every SOUNDING step (the output gate passes) the voice READ equals cycle mode's read;
//   (2) at every sounding step the per-strand LANE reads equal cycle mode's;
//   (3) the probability-lane LATCH edge (voiceFrameEdgeLive_) fires at the SAME steps as cycle mode —
//       the voice's own range-edge predicate, NOT the global phrase edge and NOT the mid-range entry;
//   (4) outside the window the voice is silent and its playhead follows the GLOBAL step.
// CYCLE is the oracle: it is the shipped, golden-pinned path and is never modified by this change.
// Reproduce-first: FAILS on the old abs1 arm (full-phrase read + global-edge latch).
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;
using LD = SequencerEngine::LaneDir;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { if (g_fail < 40) std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

static void setup(SequencerEngine& eng, bool abs1, int G, int O, int L, int R, LD dir) {
    eng.reset(); eng.numPolyVoices = 2;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
        eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    const auto v = VoiceResolver::slot(2);
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(v, LaneId{kLanes[l]})[i]=.5f;
    eng.voiceLorRef(v, SequencerEngine::LOR_LEN)=L;
    eng.voiceLorRef(v, SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(v, SequencerEngine::LOR_ROT)=R;
    eng.setVoiceLorLocalEast(v, true);
    eng.voiceLorDir_[v.v]=dir;
    eng.maskLocalEast_[v.v]=true;
    eng.maskKind_[v.v] = abs1 ? SequencerEngine::MASK_DYNAMIC : SequencerEngine::MASK_CYCLE;
    eng.updateWindow((float)G,0.f,false,0.f,0.f,false);
}

struct Cfg { int G, O, L, R; };

static void compare(const Cfg& c, LD dir, int tdir) {
    SequencerEngine cyc, abs;
    setup(cyc, false, c.G, c.O, c.L, c.R, dir);
    setup(abs, true,  c.G, c.O, c.L, c.R, dir);
    const auto v = VoiceResolver::slot(2);
    char msg[220];
    const char* dn = dir==LD::Forward?"FWD":dir==LD::Reverse?"REV":dir==LD::Pendulum?"PEND":"PING";
    int sounding = 0;
    for (int t = 0; t < 3 * c.G; ++t) {          // three global passes
        cyc.advancePlayhead(tdir, nullptr);
        abs.advancePlayhead(tdir, nullptr);
        const int col = abs.pos_.playedColumn();
        const bool snd = abs.outputMaskPasses(v, col);
        // (3) the latch edge: identical to cycle's at EVERY step (sounding or not)
        std::snprintf(msg, sizeof msg, "latch edge differs (G=%d O=%d L=%d R=%d %s t=%d): abs=%d cycle=%d",
                      c.G, c.O, c.L, c.R, dn, t, (int)abs.voiceFrameEdgeLive_(v.v), (int)cyc.voiceFrameEdgeLive_(v.v));
        CHECK(abs.voiceFrameEdgeLive_(v.v) == cyc.voiceFrameEdgeLive_(v.v), msg);
        if (snd) {
            ++sounding;
            // (1) the read
            std::snprintf(msg, sizeof msg, "sounding READ != cycle (G=%d O=%d L=%d R=%d %s t=%d col=%d): abs=%d cycle=%d",
                          c.G, c.O, c.L, c.R, dn, t, col, abs.voiceRead_[v.v], cyc.voiceRead_[v.v]);
            CHECK(abs.voiceRead_[v.v] == cyc.voiceRead_[v.v], msg);
            // (2) the lanes
            for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
                std::snprintf(msg, sizeof msg, "sounding LANE %d tick != cycle (G=%d O=%d L=%d R=%d %s t=%d)",
                              s, c.G, c.O, c.L, c.R, dn, t);
                CHECK(abs.laneTickV_[v.v][s] == cyc.laneTickV_[v.v][s], msg);
            }
        } else {
            // (4) silent: the playhead follows the global step
            std::snprintf(msg, sizeof msg, "silent playhead != global step (G=%d O=%d L=%d R=%d %s t=%d): %d vs col %d",
                          c.G, c.O, c.L, c.R, dn, t, abs.voiceRead_[v.v], col);
            CHECK(abs.voiceRead_[v.v] == col, msg);
        }
    }
    std::snprintf(msg, sizeof msg, "the window sounds exactly min(L,G) steps per pass (G=%d O=%d L=%d R=%d %s): %d over 3 passes",
                  c.G, c.O, c.L, c.R, dn, sounding);
    CHECK(sounding == 3 * (c.L < c.G ? c.L : c.G), msg);
}

int main() {
    std::printf("test_abs_cycle_consistent: abs1 == cycle read + lanes + latch at sounding steps; a mute elsewhere\n");
    const Cfg cfgs[] = {
        {16, 8, 8, 0}, {16, 8, 8, 3}, {16, 8, 8, 5},      // Rodney's rig (window 9..16)
        {16, 6, 8, 2}, {16, 6, 8, 4},                      // O not a multiple of L: where enter-at-R and cycle-consistent differ
        {16, 0, 8, 3}, {16, 4, 8, 5},
        {16, 12, 8, 3},                                    // the window wraps the ring (O+L > G)
        {16, 3, 6, 2},                                     // L does not divide G
    };
    for (const Cfg& c : cfgs)
        for (LD d : { LD::Forward, LD::Reverse })
            compare(c, d, +1);
    std::printf(g_fail ? "\nFAILED (%d)\n" : "\nOK\n", g_fail);
    return g_fail ? 1 : 0;
}
