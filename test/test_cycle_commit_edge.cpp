// test_cycle_commit_edge — the cycle-mode probability-lane commit fires at the
// VOICE-LANE STEP EDGE (left in forward, right in reverse), NOT at rotation R.
//
// THE DESIGN (Rodney, crystal): rotation R sets where the voice sits at its first
// step (the read/note anchor). The lanes are ALLOWED TO MOVE at the voice-lane
// step edge — the LEFT edge in forward (read-content == R), the RIGHT edge in
// reverse (read-content == R-1, one cell past the anchor). Confirmed as the
// known-good behaviour at 74cf5c14 (the cycle fix landing).
//
// THE REGRESSION (7aae5ee9, the reverse-anchor read fix): anchorBias(+1) correctly
// moved the reverse NOTE read to R, but the commit predicate voiceFrameEdgeLive_
// did not carry the same bias, so the reverse commit decoupled from the range edge
// onto rotation R. Forward was never affected.
//
// THE FIX: voiceFrameEdgeLive_'s cycle arm keys on wrapMod(vCoord - anchorBias(d),
// walkPeriod(d,L)) == 0 — forward unchanged (bias 0, left edge, read R); reverse
// fires one step past the anchor (the right edge, read R-1).
//
// Reproduce-first: the REV rows FAIL on HEAD (commit at R, not R-1); PASS after.
// The FWD rows are the never-moved control. Compile: see test/run_all.sh.

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

// Returns the voice read-content at the step the probability lane commits (the
// first commit in the march), or -1 if none seen.
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
    std::printf("test_cycle_commit_edge: lanes commit at the voice-lane step edge (fwd left / rev right), not rotation\n");
    char msg[200];
    // 74cf5c14 known-good commit-read contents (captured by probe on that commit):
    struct Cfg { int O, R, fwd, rev; } cfgs[] = {
        { 0, 3, 3, 2 },   // left edge = R=3; right edge = R-1 = 2
        { 2, 5, 7, 6 },   // O!=0, R!=O; the edge asymmetry holds: fwd = rev + 1
    };
    for (const auto& c : cfgs) {
        const int f = commitReadCell(SequencerEngine::LaneDir::Forward, c.O, c.R);
        const int r = commitReadCell(SequencerEngine::LaneDir::Reverse, c.O, c.R);
        std::snprintf(msg, sizeof msg, "FWD O=%d R=%d: commit read-cell %d, want %d (the LEFT edge)", c.O, c.R, f, c.fwd);
        CHECK(f == c.fwd, msg);
        std::snprintf(msg, sizeof msg, "REV O=%d R=%d: commit read-cell %d, want %d (the RIGHT edge, R-1 — NOT rotation R)", c.O, c.R, r, c.rev);
        CHECK(r == c.rev, msg);
        std::snprintf(msg, sizeof msg, "O=%d R=%d: the edge asymmetry fwd==rev+1 (left is one content-cell above right): fwd=%d rev=%d", c.O, c.R, f, r);
        CHECK(f == ((r + 1) % 8) || f == r + 1, msg);
    }
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
