// diag_reverse_rot_edge — CC_PROMPT_REVERSE_VOICE_ROTATION_EDGE, reproduce-first.
// The doc's diagnostic table: headless, voice L=8, O=0, global 16, clean fwd clock
// via advancePlayhead, NO CV (nothing commits — this is pure read/position law).
// Per transport step, print the ENGINE's voice read (voiceRead_, content = voiceRead_-O),
// the per-lane tick the UI feeds (laneTickV_), the drawn bar (lanePlayheadBar = the
// calcPlayhead law), and the drawn bar's SLOT within the window ((bar-off) mod L).
// The doc's property: the step that reads rotation content R must sit at the
// direction-entry edge slot (LEFT 0 fwd / RIGHT L-1 rev).
//
// Compile: g++ -std=c++20 -msse4.1 -mavx -Itest -Isrc -Isrc/dsp/engines -Isrc/ui \
//   test/diag_reverse_rot_edge.cpp $SE $GS $PE $CE
#include <cstdio>
#include <cmath>
#include "engines/SequencerEngine.hpp"
#include "engines/PatternEngine.hpp"
#include "ui/SandsRangeGestureCore.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

static PatternInput mk(){ PatternInput in; for(int i=0;i<12;i++) in.semiWeights[i]=1.f; in.noteVariationMask=0b111; in.variationAmount=.6f; in.octaveLo=0; in.octaveHi=1; return in; }

static void run(const char* tag, SequencerEngine::LaneDir dir, int O, int R) {
    const int L = 8;
    SequencerEngine eng; eng.reset(); eng.numPolyVoices = 2;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f; eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    const auto v = VoiceResolver::slot(2);
    for (int l=0;l<7;l++) for (int i=0;i<16;i++) eng.pe.polyRandom(v, LaneId{kLanes[l]})[i]=.5f;
    eng.voiceLorRef(v, SequencerEngine::LOR_LEN)=L;
    eng.voiceLorRef(v, SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(v, SequencerEngine::LOR_ROT)=R;
    eng.polyLORRef(v, LaneId{SequencerEngine::PL_VARIATION}, SequencerEngine::LOR_LEN)=L;
    eng.setVoiceLorLocalEast(v, true);
    eng.voiceLorDir_[v.v]=dir;
    eng.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const int el = SequencerEngine::editorLane(SequencerEngine::PL_VARIATION);
    int laneLen, laneOff, laneRot; SequencerEngine::LaneDir laneDir;
    eng.effectiveLaneLor(v, LaneId{SequencerEngine::PL_VARIATION}, laneLen, laneOff, laneRot, laneDir);
    std::printf("\n== %s dir=%s O=%d R=%d | lane(len=%d off=%d rot=%d dir=%d) ==\n",
                tag, dir==SequencerEngine::LaneDir::Reverse?"REV":"FWD", O, R,
                laneLen, laneOff, laneRot, (int)laneDir);
    std::printf("step | voiceRead cont | laneTickV drawnBar slot\n");
    for (int s=0;s<16;s++){
        eng.advancePlayhead(1,nullptr);
        const int vr = eng.voiceRead_[v.v];
        const int cont = ((vr - O) % L + L) % L;
        const int tick = eng.laneTickV_[v.v][el];
        const int bar = redDot::SandsRangeGestureCore::lanePlayheadBar(tick, laneLen, laneOff, laneRot, dotModular::kMaskRing);
        const int slot = ((bar - laneOff) % L + L) % L;
        std::printf("%4d | %8d %4d | %8d %8d %4d\n", s, vr, cont, tick, bar, slot);
    }
}

int main(){
    run("fwd", SequencerEngine::LaneDir::Forward, 0, 3);
    run("rev", SequencerEngine::LaneDir::Reverse, 0, 3);
    return 0;
}
