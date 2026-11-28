// test_netsteps_model — the NET-STEPS model (Rodney 2026-11-25) reproduces the engine's CYCLE
// read with a SINGLE signed counter. The full read law, verified below:
//
//     read = O + ((netSteps + R + advanceOffset) mod L)
//
//   - netSteps : +1 per forward step, -1 per back, 0 per hold; first motion enters (netSteps==0);
//                reset re-initialises it; it NEVER re-parks / re-arms on a window change.
//   - R, advanceOffset : window-relative PHASE — added INSIDE mod L (rotation; per-voice phase).
//   - O (voice LOR offset) : window PLACEMENT — added OUTSIDE mod L.
//   - the GLOBAL transport offset is NOT in the read value — it places the played COLUMN only.
//
// The claim, pinned against the live engine: the counter equals the engine read for forward, a
// mid-phrase FLIP (retrace), HOLDs, rotation, static voice/global offset, and per-voice
// advanceOffset. It diverges ONLY on a re-anchor: a reverse that begins right after a window
// change, and (same family) a mid-run OFFSET change. Those are the deliberate C3 cases.
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
static int L = 8, O = 0, R = 2, AOFF = 0, GOFF = 0;

static void setup() {
    eng.reset(); eng.numPolyVoices = 1;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
        eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(V, LaneId{kLanes[l]})[i]=.5f;
    eng.setVoiceLorLocalEast(V, true);
    eng.voiceLorDir_[V.v] = SequencerEngine::LaneDir::Forward;
    L=8; O=0; R=2; AOFF=0; GOFF=0;
}
static void win() { eng.voiceLorRef(V,SequencerEngine::LOR_LEN)=L; eng.voiceLorRef(V,SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(V,SequencerEngine::LOR_ROT)=R; eng.updateWindow((float)L,0.f,false,(float)GOFF,0.f,false);
    eng.advanceOffset_[V.v]=AOFF; }

static int model() { long long n = eng.pos_.netSteps(); long long w = (((n + R + AOFF) % L) + L) % L; return (int)((O + (int)w) % 16); }
static int step(int motion) { eng.advancePlayhead(motion, nullptr); return eng.voiceRead_[V.v]; }

int main() {
    std::printf("test_netsteps_model: read == O + ((netSteps + R + advanceOffset) mod L)\n");
    char m[160];

    // T1: direction + hold at a FIXED window — forward, a mid-phrase FLIP (retrace), HOLDs, forward.
    { setup(); win(); int bad=0,c=0; auto t=[&](int mo){ if (step(mo)!=model()) ++bad; ++c; };
      for(int i=0;i<6;i++) t(+1); for(int i=0;i<4;i++) t(-1); t(0); t(0); for(int i=0;i<2;i++) t(+1);
      std::snprintf(m,sizeof m,"T1 forward/flip/hold (fixed window): %d/%d differ (expect 0)",bad,c); CHECK(bad==0,m); }

    // T2: cold reverse from transport zero (aligns at R, steps back).
    { setup(); win(); int bad=0; for(int i=0;i<8;i++) if (step(-1)!=model()) ++bad;
      std::snprintf(m,sizeof m,"T2 cold reverse from zero: %d/8 differ (expect 0)",bad); CHECK(bad==0,m); }

    // T3: THE divergence — reverse right after a REAL window change (engine re-parks; model stays drift-free).
    { setup(); L=4; win(); for(int i=0;i<6;i++) step(+1); L=8; win();
      int diverged=0; long long prevN=eng.pos_.netSteps(); bool first=true;
      for(int i=0;i<4;i++){ int e=step(-1); long long n=eng.pos_.netSteps();
        if(!first && n!=prevN-1){ std::printf("  FAIL: netSteps not strictly decrementing\n"); ++g_fail; } prevN=n; first=false;
        if(e!=model()) ++diverged; }
      // INTEGRATION (netsteps edge, PR #369): the re-park divergence this test originally pinned
      // (>0) is the exact gap the arc closes — with the edges on the counter the engine is
      // drift-free across the window change, agreeing with the model everywhere. Expect 0.
      // (#380 then RETIRES the now-inert in-window re-seed; measured: already 0/4 without it.)
      std::snprintf(m,sizeof m,"T3 reverse-after-window-change: engine diverges on %d/4 (arc closed the re-park; expect 0)",diverged);
      CHECK(diverged==0,m); }

    // T4: static voice LOR offset — O places the read OUTSIDE mod L.
    { setup(); O=3; R=0; win(); int bad=0; for(int i=0;i<8;i++) if (step(+1)!=model()) ++bad;
      std::snprintf(m,sizeof m,"T4 static voice offset O=3 (outside mod L): %d/8 differ (expect 0)",bad); CHECK(bad==0,m); }

    // T5: per-voice advanceOffset — PHASE shift INSIDE mod L.
    { setup(); R=0; AOFF=2; win(); int bad=0; for(int i=0;i<8;i++) if (step(+1)!=model()) ++bad;
      std::snprintf(m,sizeof m,"T5 advanceOffset=2 (inside mod L): %d/8 differ (expect 0)",bad); CHECK(bad==0,m); }

    // T6: the GLOBAL transport offset is NOT in the read — it moves the played column only.
    { setup(); O=0; R=0; GOFF=3; win(); int bad=0, colShift=0;
      for(int i=0;i<8;i++){ int e=step(+1); if(e!=model()) ++bad; if(eng.pos_.playedColumn()!=(int)eng.pos_.netSteps()) ++colShift; }
      std::snprintf(m,sizeof m,"T6 global offset: read diffs %d/8 (expect 0), column shifted %d/8 (expect >0)",bad,colShift);
      CHECK(bad==0 && colShift>0,m); }

    // T7: a mid-run ROTATION change is window-relative — clean (no re-anchor), unlike an offset change.
    { setup(); win(); int bad=0; for(int i=0;i<4;i++) if(step(+1)!=model()) ++bad;
      R=5; win(); for(int i=0;i<4;i++) if(step(+1)!=model()) ++bad;
      std::snprintf(m,sizeof m,"T7 mid-run rotation R 2->5: %d/8 differ (expect 0)",bad); CHECK(bad==0,m); }

    // ── T8: THE SWAP-EQUIVALENCE PIN (slice 1). The eventual swap replaces readCoord() (the col_
    //    coordinate) with netSteps() as the input to the read fold. The engine reduces the
    //    coordinate mod lenG BEFORE any fold (vCoord = wrapMod(coord + advanceOffset, lenG)), so if
    //    netSteps ≡ readCoord (mod lenG) the swap is byte-identical — for EVERY downstream fold
    //    (per-voice advanceOffset, per-lane walkFold incl. pendulum/ping-pong, getStrandIdx,
    //    sub-windows), because the fold is unchanged and sees the same reduced value. Pin that
    //    congruence across forward, a mid-phrase FLIP (incl. wrapping past the window start), HOLD,
    //    and cold reverse. lenG == L here. ──
    auto congr = [&](){ long long n=eng.pos_.netSteps(), r=eng.pos_.readCoord();
        return (int)(((n%L)+L)%L) == (int)(((r%L)+L)%L); };
    {
        setup(); win(); int bad=0,c=0; auto t=[&](int mo){ step(mo); if(!congr()) ++bad; ++c; };
        for(int i=0;i<6;i++) t(+1); for(int i=0;i<6;i++) t(-1); t(0); for(int i=0;i<3;i++) t(+1);
        std::snprintf(m,sizeof m,"T8 netSteps ≡ readCoord (mod lenG) [swap-equivalence]: %d/%d violate (expect 0)",bad,c);
        CHECK(bad==0,m);
    }

    // ── T9: and it must DIVERGE on the one re-anchor case — reverse after a real window change —
    //    so the swap is NOT a silent no-op there (that is the deliberate drift-free C3 delta). ──
    {
        setup(); L=4; win(); for(int i=0;i<6;i++) step(+1); L=8; win();
        int diverged=0; for(int i=0;i<4;i++){ step(-1); if(!congr()) ++diverged; }
        // INTEGRATION (#369): counter and fold stay congruent through the window change. Expect 0.
        std::snprintf(m,sizeof m,"T9 reverse-after-window: netSteps/readCoord diverge on %d/4 (arc closed; expect 0)",diverged);
        CHECK(diverged==0,m);
    }

    // ── T10: the swap only changes the TRANSPORT coordinate (pos_). Lane LOR (L/O/R), lane
    //    direction (pendulum/ping-pong), and poly advanceOffset are all DOWNSTREAM folds, unchanged
    //    by the swap, and the engine reduces the coordinate mod lenG before each — so T8's congruence
    //    covers them by construction. The one way that could break is if POLY advance fed back into
    //    the shared pos_. Pin that it does not: two voices, the transport congruence still holds. ──
    {
        eng.reset(); eng.numPolyVoices = 2;
        for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
            eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
        for (int vv=1; vv<=2; ++vv){ auto S=VoiceResolver::slot(vv); for(int l=0;l<7;l++) for(int i=0;i<16;i++) eng.pe.polyRandom(S,LaneId{kLanes[l]})[i]=.5f; }
        L=8; O=0; R=0; GOFF=0; eng.updateWindow(8.f,0.f,false,0.f,0.f,false);
        int bad=0; auto t=[&](int mo){ eng.advancePlayhead(mo,nullptr); if(!congr()) ++bad; };
        for(int i=0;i<6;i++) t(+1); for(int i=0;i<4;i++) t(-1);
        std::snprintf(m,sizeof m,"T10 poly (2 voices) leaves transport congruence intact: %d violate (expect 0)",bad);
        CHECK(bad==0,m);
    }

    std::printf(g_fail ? "\nFAILED (%d)\n" : "\nOK\n", g_fail);
    return g_fail ? 1 : 0;
}
