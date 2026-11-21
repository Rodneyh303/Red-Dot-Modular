// test_follow_r_retired — the painted mask is STATIC; the stored follow-R flag is read and IGNORED.
//
// (UAS "[RESOLVED] CYCLE-CONSISTENT ABSOLUTE MODE", Rodney 2026-11-21.) Follow-R (abs3) is retired: the
// painted pattern is fixed to global-step positions whatever R and the voice direction do. Measured
// before retiring it: the old re-pin shifted the figure EARLIER for every voice direction, which is right
// for Forward only — a Reverse voice's rhythm slid AGAINST its content, and Pendulum/PingPong reflect the
// content so no single shift tracks it. A loaded patch with the flag ON therefore plays STATIC.
// Reproduce-first: FAILS on the pre-retirement push (R moved the figure when the flag was on).
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LD = SequencerEngine::LaneDir;
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

int main() {
    std::printf("test_follow_r_retired: a patch with follow-R ON plays static for every voice direction\n");
    char msg[160];
    const auto V3 = VoiceResolver::slot(3);
    for (LD d : { LD::Forward, LD::Reverse, LD::Pendulum, LD::PingPong }) {
        SequencerEngine eng; eng.reset(); eng.numPolyVoices = 4;
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.setVoiceLorLocalEast(V3, true);
        eng.voiceLorRef(V3, SequencerEngine::LOR_LEN) = 8;
        eng.voiceLorDir_[V3.v] = d;
        uint64_t macroBits[1] = { ~0ULL }; float macroKind = 0.f, macroFollowR = 0.f;
        uint64_t eastBits[16]; float eastKind[16], eastFollowR[16], localEast[16];
        for (int v = 0; v < 16; ++v) { eastBits[v] = dotModular::kMaskLenMask; eastKind[v] = 0.f; eastFollowR[v] = 0.f; localEast[v] = 0.f; }
        eastBits[V3.v] = (1ULL << 1) | (1ULL << 4) | (1ULL << 6);   // painted: steps 2,5,7
        eastKind[V3.v] = 2.f; eastFollowR[V3.v] = 1.f; localEast[V3.v] = 1.f;   // the flag is ON
        bool staticOK = true;
        for (int R = 0; R < 8; ++R) {
            eng.voiceLorRef(V3, SequencerEngine::LOR_ROT) = R;
            eng.pushMaskEditor(macroBits, macroKind, macroFollowR, eastBits, eastKind, eastFollowR, localEast);
            for (int s = 0; s < 8; ++s)
                staticOK = staticOK && (eng.outputMaskPasses(V3, s) == (s == 1 || s == 4 || s == 6));
        }
        std::snprintf(msg, sizeof msg, "voice dir %d: with follow-R pushed ON the painted mask stays on steps 2,5,7 for every R", (int)d);
        CHECK(staticOK, msg);
        CHECK(!eng.maskFollowR_[V3.v], "the engine never arms follow-R from the push");
    }
    std::printf(g_fail ? "\nFAILED (%d)\n" : "\nOK\n", g_fail);
    return g_fail ? 1 : 0;
}
