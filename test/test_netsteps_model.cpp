// test_netsteps_model — the NET-STEPS model (Rodney 2026-11-25) reproduces the engine's CYCLE
// read with a SINGLE signed counter: read == (pos_.netSteps() + R) mod L.
//
// netSteps_ is +1 per forward step, -1 per back, 0 per hold, the first motion only "enters"
// (read lands at netSteps_==0), reset re-initialises it, and it NEVER re-parks / re-arms on a
// window change. The claim (pinned here against the live engine): it equals the engine's read
// across forward, a mid-phrase FLIP (retrace), HOLDs, and rotation / offset / length changes —
// and diverges ONLY for a reverse that begins immediately after a window change (the re-park the
// engine still does, the same drift PR #365 removed on the forward edge). That one case is the
// deliberate C3 decision; this test locks the equivalence everywhere else and documents the gap.
// Compile: see test/run_all.sh.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)
static const int kLanes[7] = { SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST, SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION, SequencerEngine::PL_LEGATO };

static SequencerEngine eng;
static dotModular::VoiceSlot V = VoiceResolver::slot(1);
static int L = 8, O = 0, R = 2;

static void setup() {
    eng.reset(); eng.numPolyVoices = 1;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
        eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(V, LaneId{kLanes[l]})[i]=.5f;
    eng.setVoiceLorLocalEast(V, true);
    eng.voiceLorDir_[V.v] = SequencerEngine::LaneDir::Forward;
    L=8; O=0; R=2;
}
static void win() { eng.voiceLorRef(V,SequencerEngine::LOR_LEN)=L; eng.voiceLorRef(V,SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(V,SequencerEngine::LOR_ROT)=R; eng.updateWindow((float)L,0.f,false,(float)O,0.f,false); }

static int model() { long long n = eng.pos_.netSteps(); return (int)(((n + R) % L + L) % L); }
static int step(int motion) { eng.advancePlayhead(motion, nullptr); return eng.voiceRead_[V.v]; }

int main() {
    std::printf("test_netsteps_model: one signed counter reproduces the engine CYCLE read\n");
    char msg[200];

    // ── T1: the model tracks the engine across every dynamic change EXCEPT reverse-after-window.
    {
        setup(); win();
        int bad = 0, checked = 0;
        auto tick = [&](int motion){ int e = step(motion); int m = model(); if (e != m) ++bad; ++checked; };
        for (int i=0;i<6;i++) tick(+1);                 // forward
        for (int i=0;i<4;i++) tick(-1);                 // mid-phrase FLIP -> reverse (retrace)
        tick(0); tick(0);                               // holds (directionless)
        for (int i=0;i<2;i++) tick(+1);                 // forward
        R=5; win(); for (int i=0;i<3;i++) tick(+1);     // rotation change
        O=3; win(); for (int i=0;i<3;i++) tick(+1);     // offset change (placement)
        L=4; O=0; win(); for (int i=0;i<4;i++) tick(+1);// shrink length mid-forward
        std::snprintf(msg,sizeof msg,"T1: %d/%d steps where (netSteps+R)%%L != engine read (expect 0)", bad, checked);
        CHECK(bad == 0, msg);
    }

    // ── T2: the model is deterministic & drift-free on a cold/flip reverse from zero — and equals
    //    the engine there too (the from-zero reverse is NOT a re-park case). Fresh run.
    {
        setup(); win();
        int bad = 0;
        for (int i=0;i<8;i++){ int e = step(-1); if (e != model()) ++bad; }   // cold reverse from transport zero
        std::snprintf(msg,sizeof msg,"T2 cold reverse from zero: %d/8 steps differ (expect 0)", bad);
        CHECK(bad == 0, msg);
    }

    // ── T3: the ONE documented divergence — reverse that begins right after a mid-run window
    //    change. The engine re-parks; the net-steps model keeps running (drift-free). We assert
    //    the model stays deterministic (strictly stepping) AND record that the engine differs, so
    //    any future change to this behaviour trips the test and gets a deliberate review.
    {
        setup(); L=4; O=0; win();                       // start in a length-4 window
        for (int i=0;i<6;i++) step(+1);                 // run forward a while (arbitrary history)
        L=8; O=0; win();                                // a REAL window re-author mid-run (clears entryDir)
        int diverged = 0; long long prevN = eng.pos_.netSteps();
        bool firstRev = true;
        for (int i=0;i<4;i++){
            int e = step(-1);
            long long n = eng.pos_.netSteps();
            if (!firstRev) { if (n != prevN - 1) { std::printf("  FAIL: netSteps not strictly decrementing on reverse\n"); ++g_fail; } }
            prevN = n; firstRev = false;
            if (e != model()) ++diverged;               // engine re-parks -> expected to differ from the model here
        }
        std::snprintf(msg,sizeof msg,"T3 reverse-after-window-change: engine diverges from model on %d/4 steps (the re-park; drift-free model is the C3 choice)", diverged);
        CHECK(diverged > 0, msg);                        // lock in that this IS the divergence case
    }

    std::printf(g_fail ? "\nFAILED (%d)\n" : "\nOK\n", g_fail);
    return g_fail ? 1 : 0;
}
