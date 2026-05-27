#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"

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
 * Plus: SPREAD control per lane per voice (0.0-1.0)
 *   Spread interpolates the probability draws, creating variation within voice.
 * 
 * Data Storage:
 *   Probabilities: PatternEngine.polyRhythmRandom[voiceIdx][16], etc.
 *   Spread: Separate parameters (new, per lane per voice)
 * 
 * Voice Mapping:
 *   Editor voice index 0-6 → Monsoon voices 2-8 (East)
 *   Editor voice index 0-7 → Monsoon voices 9-16 (West)
 *   PatternEngine polyRandom uses 0-14 indexing (voices 2-16)
 * 
 * Usage:
 *   PolyVoiceSandsParameterManager mgr(patternEngine, numVoices);
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
  
  // Spread parameters: [voice][lane]
  // Voice index: 0-6 (maps to Monsoon voices 2-8) or 0-7 (voices 9-16)
  std::array<std::array<float, 3>, 8> spread = {};  // [8 voices][3 lanes]
  
  PolyVoiceSandsParameterManager(PatternEngine* pe = nullptr, int nVoices = 7) 
    : patternEngine(pe), numVoices(nVoices) {
    // Initialize all spreads to 0
    for (int v = 0; v < 8; ++v) {
      for (int l = 0; l < 3; ++l) {
        spread[v][l] = 0.0f;
      }
    }
  }
  
  // Set spread value for a voice/lane combination
  void setSpread(int voiceIdx, int lane, float value) {
    if (voiceIdx >= 0 && voiceIdx < numVoices && lane >= 0 && lane < 3) {
      spread[voiceIdx][lane] = rack::math::clamp(value, 0.0f, 1.0f);
    }
  }
  
  // Get spread value for a voice/lane combination
  float getSpread(int voiceIdx, int lane) const {
    if (voiceIdx >= 0 && voiceIdx < numVoices && lane >= 0 && lane < 3) {
      return spread[voiceIdx][lane];
    }
    return 0.0f;
  }
  
  // Apply spread interpolation to a probability value
  float applySpread(float probability, float spreadValue) const {
    if (spreadValue <= 0.0f) return probability;
    
    // Spread creates variation: pulls toward center (0.5) based on spread amount
    // At spread=0: no change
    // At spread=1: full interpolation toward 0.5 (flat distribution)
    float target = 0.5f;
    return probability + (target - probability) * spreadValue;
  }
  
  // Sync visual editor to PatternEngine for a specific voice
  void syncEditorToPatternEngine(int voiceIdx, const SandsVisualEditorV4::VoiceState& editorState,
                                 bool applySpreadToDisplay = false) {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return;
    
    // Rest lane (uses polyRhythmRandom)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      float prob = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      if (applySpreadToDisplay) {
        prob = applySpread(prob, spread[voiceIdx][SandsVisualEditorV4::REST]);
      }
      patternEngine->polyRhythmRandom[voiceIdx][i] = prob;
      patternEngine->polyRhythmSource[voiceIdx][i] = prob;
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      float prob = editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i];
      if (applySpreadToDisplay) {
        prob = applySpread(prob, spread[voiceIdx][SandsVisualEditorV4::MELODY]);
      }
      patternEngine->polyMelodyRandom[voiceIdx][i] = prob;
      patternEngine->polyMelodySource[voiceIdx][i] = prob;
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      float prob = editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i];
      if (applySpreadToDisplay) {
        prob = applySpread(prob, spread[voiceIdx][SandsVisualEditorV4::OCTAVE]);
      }
      patternEngine->polyOctaveRandom[voiceIdx][i] = prob;
      patternEngine->polyOctaveSource[voiceIdx][i] = prob;
    }
  }
  
  // Sync PatternEngine to visual editor for a specific voice
  void syncPatternEngineToEditor(int voiceIdx, SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return;
    
    // Rest lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::REST].probabilities[i] = patternEngine->polyRhythmRandom[voiceIdx][i];
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i] = patternEngine->polyMelodyRandom[voiceIdx][i];
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i] = patternEngine->polyOctaveRandom[voiceIdx][i];
    }
  }
  
  // Get spread-adjusted probability for display (voice-specific)
  float getDisplayProbability(int voiceIdx, int lane, int step) const {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return 0.0f;
    if (lane < 0 || lane > 2 || step < 0 || step >= SandsVisualEditorV4::STEP_COUNT) return 0.0f;
    
    float prob = 0.0f;
    float spreadValue = spread[voiceIdx][lane];
    
    switch (lane) {
      case SandsVisualEditorV4::REST:
        prob = patternEngine->polyRhythmRandom[voiceIdx][step];
        break;
      case SandsVisualEditorV4::MELODY:
        prob = patternEngine->polyMelodyRandom[voiceIdx][step];
        break;
      case SandsVisualEditorV4::OCTAVE:
        prob = patternEngine->polyOctaveRandom[voiceIdx][step];
        break;
      default:
        return 0.0f;
    }
    
    return applySpread(prob, spreadValue);
  }
};

}  // namespace redDot
