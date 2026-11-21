// diag_octave_step6.cpp — reproduce Rodney's EXACT setup.
//   global 16 steps, cycle mode; voice 1 = 8 steps; octave lane base 8 steps with
//   an offset CV; rotation zero on voice and octave.
//   Symptom: the octave offset jumps from the modulation when the voice lane
//   reaches STEP 6 of 8 — not the wrap.
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;
using LaneId = dotModular::LaneId;
static dotModular::VoiceSlot v1() { return VoiceResolver::slot(1); }   // "voice 1"
static constexpr int OCT = dotModular::STRAND_OCTAVE;

static void octCommitted(SequencerEngine& eng, int& L, int& O) {
    int R; SequencerEngine::LaneDir d;
    eng.effectiveLaneLor(v1(), LaneId{SequencerEngine::PL_OCTAVE}, L, O, R, d);
}
static int trueCellVoice(SequencerEngine& eng) {
    int rL, rO, rR; SequencerEngine::LaneDir rd;
    eng.effectiveVoiceLor(v1().v, rL, rO, rR, rd);
    return (int)SequencerEngine::laneTickFor(rd, (int64_t)eng.voiceFramePhase_[v1().v] + 1, rL);
}

int main() {
    std::printf("global=16 cycle, voice1 len=8, octave lane len=8, rot=0, octave OFFSET CV +3 at t=1\n");
    std::printf(" t | gPhrase vPhase vCell(wrap?) | octCommO octCommL | notes\n");
    std::printf("---+----------------------------+------------------+------\n");
    SequencerEngine eng; eng.reset();
    eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // global 16, cycle (no abs1)
    eng.numPolyVoices = 1;
    eng.setVoiceLorLocalEast(v1(), true);
    eng.voiceLorRef(v1(), SequencerEngine::LOR_LEN) = 8;    // voice 1 = 8 steps
    eng.voiceLorRef(v1(), SequencerEngine::LOR_ROT) = 0;    // rotation zero
    eng.polyLORRef(v1(), LaneId{SequencerEngine::PL_OCTAVE}, SequencerEngine::LOR_LEN) = 8; // octave lane base 8
    eng.polyLORRef(v1(), LaneId{SequencerEngine::PL_OCTAVE}, SequencerEngine::LOR_ROT) = 0;
    int prevO = 0; { int L; octCommitted(eng, L, prevO); }
    for (int t = 0; t < 34; ++t) {
        eng.advancePlayhead(1, nullptr);
        if (t == 1) { int d3[3] = {0, 3, 0}; eng.pushLaneLorCvPoly(v1(), SequencerEngine::PL_OCTAVE, d3); } // offset +3
        const int gp = eng.pos_.phrasePos();
        const int vph = eng.voiceFramePhase_[v1().v];
        const int vcell = trueCellVoice(eng);
        int cL, cO; octCommitted(eng, cL, cO);
        // the ACTUAL read index the octave strand consumes (what Rodney hears/sees):
        int rcL, rcO, rcR; SequencerEngine::LaneDir rcd;
        eng.effectiveLaneLor(v1(), LaneId{SequencerEngine::PL_OCTAVE}, rcL, rcO, rcR, rcd);
        const int octTick = eng.polyLaneTick(v1(), LaneId{SequencerEngine::PL_OCTAVE});
        const int octIdx  = eng.getStrandIdx(octTick, rcL, rcO, rcR);
        const bool pastAuthored = (octIdx >= cL);   // read slid past the authored lane length
        const bool wrap = (vcell == 0);
        const bool jumped = (cO != prevO);
        char note[96] = "";
        if (jumped && wrap)      std::snprintf(note, sizeof note, "octave OFFSET JUMPED @ voice wrap (cell0)");
        else if (jumped)         std::snprintf(note, sizeof note, "octave OFFSET JUMPED @ vCell=%d  <-- NOT the wrap", vcell);
        else if (wrap)           std::snprintf(note, sizeof note, "voice wrap (no jump)");
        char readnote[64] = "";
        if (pastAuthored) std::snprintf(readnote, sizeof readnote, "octIdx=%d >= len %d  <-- PAST AUTHORED (reads ring cell)", octIdx, cL);
        else              std::snprintf(readnote, sizeof readnote, "octIdx=%d", octIdx);
        std::printf("%2d | %5d  %6d  %4d %-5s | %6d  %6d | %-22s %s\n",
                    t, gp, vph, vcell, wrap?"WRAP":"", cO, cL, readnote, note);
        prevO = cO;
    }
    return 0;
}
