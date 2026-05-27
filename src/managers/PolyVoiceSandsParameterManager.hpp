#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"
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
 *   - Target 1: Average of all poly voices (creates cohesion)
 *   - Target 2: Mono voice draw (creates mono-to-poly blending)
 *   
 *   Each voice has independent spread control per lane.
 * 
 * Data Storage:
 *   Probabilities: PatternEngine.polyRhythmRandom[voiceIdx][16], etc.
 *   Spread: SpreadManager (per voice, per lane)
 * 
 * Voice Mapping:
 *   Editor voice index 0-6 → Monsoon voices 2-8 (East)
 *   Editor voice index 0-7 → Monsoon voices 9-16 (West)
 *   PatternEngine polyRandom uses 0-14 indexing (voices 2-16)
 * 
 * Usage:
 *   PolyVoiceSandsParameterManager mgr(patternEngine, numVoices);
 *   mgr.setInterpolationTarget(SpreadManager::AVERAGE_POLY);
 *   mgr.setSpread(voiceIdx, lane, spreadValue);
 *   mgr.syncEditorToPatternEngine(voiceIdx, editor->currentState);
 *   mgr.syncPatternEngineToEditor(voiceIdx, editor->currentState);
 *   
 *   // Get spread-adjusted display value
 *   float displayProb = mgr.getDisplayProbability(voiceIdx, lane, step);
 */

struct PolyVoiceSandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  int numVoices = 7;  // 7 for East, 8 for West
  
  // Spread manager handles all interpolation
  SpreadManager spreadMgr;
  
  PolyVoiceSandsParameterManager(PatternEngine* pe = nullptr, int nVoices = 7) 
    : patternEngine(pe), numVoices(nVoices), spreadMgr(pe, nVoices) {}
  
  // Set spread value for a voice/lane combination
  void setSpread(int voiceIdx, int lane, float value) {
    spreadMgr.setSpread(voiceIdx, lane, value);
  }
  
  // Get spread value for a voice/lane combination
  float getSpread(int voiceIdx, int lane) const {
    return spreadMgr.getSpread(voiceIdx, lane);
  }
  
  // Set interpolation target (AVERAGE_POLY or MONO_DRAW)
  void setInterpolationTarget(SpreadManager::InterpolationTarget target) {
    spreadMgr.setInterpolationTarget(target);
  }
  
  // Sync visual editor to PatternEngine for a specific voice
  void syncEditorToPatternEngine(int voiceIdx, const SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return;
    
    // Rest lane (uses polyRhythmRandom)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRhythmRandom[voiceIdx][i] = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      patternEngine->polyRhythmSource[voiceIdx][i] = patternEngine->polyRhythmRandom[voiceIdx][i];
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyMelodyRandom[voiceIdx][i] = editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i];
      patternEngine->polyMelodySource[voiceIdx][i] = patternEngine->polyMelodyRandom[voiceIdx][i];
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyOctaveRandom[voiceIdx][i] = editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i];
      patternEngine->polyOctaveSource[voiceIdx][i] = patternEngine->polyOctaveRandom[voiceIdx][i];
    }
  }
  
  // Sync PatternEngine to visual editor for a specific voice
  // Shows interpolated values with spread applied
  void syncPatternEngineToEditor(int voiceIdx, SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return;
    
    // Rest lane - show interpolated values
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::REST].probabilities[i] = 
        spreadMgr.getInterpolatedValue(voiceIdx, 0, i);
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i] = 
        spreadMgr.getInterpolatedValue(voiceIdx, 1, i);
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i] = 
        spreadMgr.getInterpolatedValue(voiceIdx, 2, i);
    }
  }
  
  // Get spread-adjusted probability for display (voice-specific)
  float getDisplayProbability(int voiceIdx, int lane, int step) const {
    if (voiceIdx < 0 || voiceIdx >= numVoices) return 0.0f;
    if (lane < 0 || lane > 2 || step < 0 || step >= 16) return 0.0f;
    
    return spreadMgr.getInterpolatedValue(voiceIdx, lane, step);
  }
};

}  // namespace redDot
