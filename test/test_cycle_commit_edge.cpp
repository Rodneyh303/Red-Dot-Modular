// test_cycle_commit_edge — the cycle-mode probability-lane commit fires at the
// VOICE-LANE STEP EDGE (left window edge in forward, right in reverse), NOT at
// rotation R.
//
// THE DESIGN (Rodney, crystal): rotation R sets where the voice sits at its first
// step — the read/note anchor, the rotation TICK (a content position). The
// probability lanes are ALLOWED TO MOVE at the voice-lane STEP EDGE — the LEFT
// window edge in forward, the RIGHT in reverse. The edge is a fixed POSITION
// (box-offset 0 or L-1); rotation only moves the content under the playhead, so
// the commit STEP shifts with R while the cell it lands on does NOT.
//
// THE BUG (every build, incl. 74cf5c14): voiceFrameEdgeLive_ keyed the commit on
// vCoord==0 — box-offset R, the rotation tick — so the lanes moved when the voice
// hit its rotation, not its range edge.
//
// WHY IT SLIPPED: the whole suite ran R==0, where rotation IS the left edge, so no
// test could distinguish the two. This test SWEEPS R: the commit read-cell must be
// the window-edge cell (O forward / O+L-1 reverse) for EVERY R — a cell that does
// NOT track rotation. That is the discriminator.
//
// Reproduce-first: on the rotation-phase engine the commit read-cell == (R+O) for
// every R (tracks rotation) — this FAILS; after the fix it is O / O+L-1 for every
// R. Compile: see test/run_all.sh.

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

static PatternInput mk(){ PatternInput in; for(int i=0;i<12;i++) in.semiWeights[i]=1.f; in.noteVariationMask=0b111; in.variationAmount=.6f; in.octaveLo=0; in.octaveHi=1; return in; }

// Returns the voice read-cell at the first probability-lane commit, or -1.
static int commitReadCell(SequencerEngine::LaneDir dir, int O, int R) {
    SequencerEngine eng; eng.reset(); eng.numPolyVoices = 2;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f; eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    const auto v = VoiceResolver::slot(2);
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(v, LaneId{kLanes[l]})[i]=.5f;
    eng.voices[v].restProb=0; eng.voices[v].accentProb=0; eng.voices[v].legatoProb=0; eng.voices[v].qmixLevel=.5f; eng.voices[v].variationProb=.5f;
    eng.voiceLorRef(v, SequencerEngine::LOR_LEN)=8;
    eng.voiceLorRef(v, SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(v, SequencerEngine::LOR_ROT)=R;
    eng.polyLORRef(v, LaneId{SequencerEngine::PL_VARIATION}, SequencerEngine::LOR_LEN)=8;
    eng.setVoiceLorLocalEast(v, true);
    eng.voiceLorDir_[v.v]=dir;
    eng.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const int el = SequencerEngine::editorLane(SequencerEngine::PL_VARIATION);
    const PatternInput in = mk();
    int prev = eng.laneLorCv_[v.v][el][SequencerEngine::LOR_OFF];
    for (int s=0;s<16;s++){
        int d3[3]={0,(s%5)+1,0}; eng.pushLaneLorCvPoly(v, SequencerEngine::PL_VARIATION, d3);
        eng.advancePlayhead(1,nullptr);
        int now = eng.laneLorCv_[v.v][el][SequencerEngine::LOR_OFF];
        if (now != prev) return eng.voiceRead_[v.v];
    }
    return -1;
}

int main(){
    std::printf("test_cycle_commit_edge: lanes commit at the voice-lane window edge (fwd left=O / rev right=O+L-1), NOT rotation\n");
    char msg[220];
    const int L = 8;
    const int Os[] = { 0, 2 };
    for (int O : Os) {
        const int leftEdge  = O;              // forward: left window-edge cell
        const int rightEdge = O + L - 1;      // reverse: right window-edge cell
        // Sweep rotation across the whole window — the commit cell must NOT move.
        for (int R = 0; R < L; ++R) {
            const int f = commitReadCell(SequencerEngine::LaneDir::Forward, O, R);
            const int r = commitReadCell(SequencerEngine::LaneDir::Reverse, O, R);
            std::snprintf(msg, sizeof msg, "FWD O=%d R=%d: commit read-cell %d, want %d (the LEFT edge — must NOT track rotation R=%d)", O, R, f, leftEdge, R);
            CHECK(f == leftEdge, msg);
            std::snprintf(msg, sizeof msg, "REV O=%d R=%d: commit read-cell %d, want %d (the RIGHT edge — must NOT track rotation R=%d)", O, R, r, rightEdge, R);
            CHECK(r == rightEdge, msg);
        }
    }
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
