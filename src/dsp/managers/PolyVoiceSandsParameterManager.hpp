#pragma once
#include <rack.hpp>
#include "../../ui/SandsVisualEditorV4.hpp"
#include "../engines/PatternEngine.hpp"
#include "../engines/SequencerEngine.hpp"
#include "SpreadManager.hpp"

namespace redDot {

/**
 * PolyVoiceSandsParameterManager (Per-Voice)
 * 
 * Wires SandsVisualEditorV4 to per-voice DNA control.
 * Used by StraitsEastSands (7 voices: 2-8) and StraitsWestSands (8 voices: 9-16).
 * 
 * Per-voice DNA: 3 lanes only (REST, MELODY, OCTAVE)
 * Each voice has independent probability distributions.
 * 
 * Spread Control (via SpreadManager):
 *   Interpolates between original poly voice draw and:
 *   - Target 1: Average of ACTIVE poly voices (creates cohesion)
 *   - Target 2: Mono voice draw (creates mono-to-poly blending)
 *   
 *   IMPORTANT: Average uses only actively requested voices!
 *   If polyphony=4 voices, average is calculated from those 4 voices only.
 *   If polyphony=7 voices, average uses all 7 voices.
 *   This requires SequencerEngine reference for polyphony tracking.
 * 
 * Data Storage:
 *   Probabilities: PatternEngine.polyRandom(voiceIdx, SequencerEngine::PL_REST)[16], etc.
 *   Spread: SpreadManager (per voice, per lane)
 * 
 * Voice Mapping:
 *   Editor voice index 0-6 → Monsoon voices 2-8 (East)
 *   Editor voice index 0-7 → Monsoon voices 9-16 (West)
 *   PatternEngine polyRandom uses 0-14 indexing (voices 2-16)
 * 
 * Usage:
 *   SequencerEngine* seqEngine = nullptr; // from monsoon
 *   PolyVoiceSandsParameterManager mgr(patternEngine, seqEngine, 7, 0);  // East
 *   mgr.setSpread(voiceIdx, lane, spreadValue);
 *   mgr.syncPatternEngineToEditor(voiceIdx, editor->currentState);
 *   
 *   // Get spread-adjusted display value
 *   float displayProb = mgr.getDisplayProbability(voiceIdx, lane, step);
 */

struct PolyVoiceSandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  SequencerEngine* sequencerEngine = nullptr;  // For active voice count
  int numVoices = 7;  // 7 for East, 8 for West
  
  // Spread manager handles all interpolation
  // Uses SequencerEngine to determine average from active voices only
  SpreadManager spreadMgr;
  
  PolyVoiceSandsParameterManager(PatternEngine* pe = nullptr, SequencerEngine* se = nullptr, 
                                  int nVoices = 7, int startVoice = 0) 
    : patternEngine(pe), sequencerEngine(se), numVoices(nVoices), 
      spreadMgr(pe, nVoices, startVoice) {
    // Set SequencerEngine so SpreadManager can track active voices
    // Voice-count bookkeeping uses only requested polyphony
    if (se) {
      spreadMgr.setSequencerEngine(se);
    }
  }
  
  // Set spread value for a voice/lane combination
  void setSpread(int voiceIdx, int lane, float value) {
    spreadMgr.setSpread(voiceIdx, lane, value);
  }
  
  // Get spread value for a voice/lane combination
  float getSpread(int voiceIdx, int lane) const {
    return spreadMgr.getSpread(voiceIdx, lane);
  }
  
  
  // Sync visual editor to PatternEngine for a specific voice
  void syncEditorToPatternEngine(int voiceIdx, const SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return;
    
    // Rest lane (uses polyRhythmRandom)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRandom(voiceIdx, SequencerEngine::PL_REST)[i] = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      patternEngine->polyRhythmSource[voiceIdx][i] = patternEngine->polyRandom(voiceIdx, SequencerEngine::PL_REST)[i];
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRandom(voiceIdx, SequencerEngine::PL_MELODY)[i] = editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i];
      patternEngine->polyMelodySource[voiceIdx][i] = patternEngine->polyRandom(voiceIdx, SequencerEngine::PL_MELODY)[i];
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRandom(voiceIdx, SequencerEngine::PL_OCTAVE)[i] = editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i];
      patternEngine->polyOctaveSource[voiceIdx][i] = patternEngine->polyRandom(voiceIdx, SequencerEngine::PL_OCTAVE)[i];
    }
  }
  
  // Sync PatternEngine to visual editor for a specific voice
  // Shows interpolated values with spread applied
  // Average is calculated from only the active/requested voices
  // syncPatternEngineToEditor(voiceIdx, editorState) was DELETED (cleanup/dead-poly-sync).
  //
  // It filled only REST/MEL/OCT — never ACCENT — from spreadMgr.getInterpolatedValue(), i.e. the
  // PRE-spread values. Both behaviours are bugs that were fixed elsewhere: East's step() reads
  // VoiceResolver::laneProbabilityAtStep() for all four poly lanes, post-spread, because reading the
  // raw drawn pattern meant "moving spread changed the audio but NOT the visible bars", and it also
  // fixed blank lanes under Macro ownership.
  //
  // Both call sites (East step() + onVoiceTabChanged) were overwritten by that resolver fill in the
  // same frame, so the method had no observable effect — but anyone calling it in good faith would
  // have got stale accent and pre-spread probabilities. Do not reintroduce it.
  //
  // The WRITE direction, syncEditorToPatternEngine(), is live and load-bearing. Leave it alone.

  
  // Get spread-adjusted probability for display (voice-specific)
  // Average is calculated from only the active voices at this moment
  float getDisplayProbability(int voiceIdx, int lane, int step) const {
    if (voiceIdx < 0 || voiceIdx >= numVoices) return 0.0f;
    if (lane < 0 || lane > 2 || step < 0 || step >= 16) return 0.0f;
    
    return spreadMgr.getInterpolatedValue(voiceIdx, lane, step);
  }
};

}  // namespace redDot
