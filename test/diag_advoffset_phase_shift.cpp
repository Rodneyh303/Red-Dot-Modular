#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;
static dotModular::VoiceSlot vv(){ return VoiceResolver::slot(2); }
int main(){
  SequencerEngine eng; eng.reset();
  eng.updateWindow(16.f,0.f,false,0.f,0.f,false);
  eng.numPolyVoices=2;
  eng.setVoiceLorLocalEast(vv(), true);
  eng.voiceLorRef(vv(), SequencerEngine::LOR_LEN)=8;
  eng.polyLORRef(vv(), LaneId{SequencerEngine::PL_OCTAVE}, SequencerEngine::LOR_LEN)=8;
  for(int t=0;t<8;t++) eng.advancePlayhead(1,nullptr);         // settle
  // INJECT a persistent divergence (as foldPerVoiceAdvance_ would accumulate when global<voice)
  eng.advanceOffset_[vv().v] += 2;   // simulate +2 wander that never re-seeds
  std::printf("injected advanceOffset_[v] += 2 after settle; push octave +3 at t=11\n");
  std::printf(" t | transportPhase(=t0 mod8) vPhase | committedOff\n");
  int prev=0;
  for(int t=8;t<28;t++){
    eng.advancePlayhead(1,nullptr);
    if(t==11){ int d3[3]={0,3,0}; eng.pushLaneLorCvPoly(vv(), SequencerEngine::PL_OCTAVE, d3); }
    int L,O,R; SequencerEngine::LaneDir d; eng.effectiveLaneLor(vv(), LaneId{SequencerEngine::PL_OCTAVE}, L,O,R,d);
    long tp = ((long)eng.transportTick_-1) % 8; if(tp<0) tp+=8;
    const char* tag=(O!=prev)?"<== COMMIT":"";
    std::printf("%2d | transportPhase=%ld  vPhase=%d | off=%d %s\n", t, tp, eng.voiceFramePhase_[vv().v], O, tag);
    prev=O;
  }
  return 0;
}
