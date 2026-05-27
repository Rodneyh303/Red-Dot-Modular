#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"

namespace redDot {

/**
 * PolySandsParameterManager (Global/Macro)
 * 
 * Wires SandsVisualEditorV4 to MonsoonStraitSandsExpander (Poly Sands - Global).
 * 
 * Global poly DNA control: 3 lanes only (REST, MELODY, OCTAVE)
 * All voices share the same probability distribution.
 * 
 * Plus: SPREAD control per lane (0.0-1.0)
 *   Spread interpolates the probability draws, creating variation.
 *   The visual editor can display spread-adjusted probabilities.
 * 
 * Data Storage:
 *   Probabilities: PatternEngine rhythmRandom/melodyRandom/octaveRandom
 *   Spread: Separate parameters (new, per lane)
 * 
 * Usage:
 *   PolySandsParameterManager mgr(patternEngine, monsoonModule);
 *   mgr.syncEditorToPatternEngine(editor->currentState, spreadValues);
 *   mgr.syncPatternEngineToEditor(editor->currentState, spreadValues);
 * 
 * Voices affected: All voices 2-16
 */

struct PolySandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  rack::Module* monsoonModule = nullptr;
  
  // Spread parameters (per-lane spread control, 0.0-1.0)
  // These should be added as new parameters to the expander
  float spreadRest = 0.0f;
  float spreadMelody = 0.0f;
  float spreadOctave = 0.0f;
  
  PolySandsParameterManager(PatternEngine* pe = nullptr, rack::Module* m = nullptr) 
    : patternEngine(pe), monsoonModule(m) {}
  
  // Apply spread interpolation to a probability value
  float applySpread(float probability, float spread) const {
    if (spread <= 0.0f) return probability;
    
    // Spread creates variation: pulls toward center (0.5) based on spread amount
    // At spread=0: no change
    // At spread=1: full interpolation toward 0.5 (flat distribution)
    // This creates a "probability smearing" effect
    float target = 0.5f;
    return probability + (target - probability) * spread;
  }
  
  // Sync visual editor to PatternEngine (editor → DSP)
  // displayProbs: if true, apply spread to displayed values
  void syncEditorToPatternEngine(const SandsVisualEditorV4::VoiceState& editorState, 
                                  bool applySpreadToDisplay = false) {
    if (!patternEngine) return;
    
    // Rest lane (global rhythm for all voices)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      float prob = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      if (applySpreadToDisplay) {
        prob = applySpread(prob, spreadRest);
      }
      patternEngine->rhythmRandom[i] = prob;
      patternEngine->rhythmSource[i] = prob;
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      float prob = editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i];
      if (applySpreadToDisplay) {
        prob = applySpread(prob, spreadMelody);
      }
      patternEngine->melodyRandom[i] = prob;
      patternEngine->melodySource[i] = prob;
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      float prob = editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i];
      if (applySpreadToDisplay) {
        prob = applySpread(prob, spreadOctave);
      }
      patternEngine->octaveRandom[i] = prob;
      patternEngine->octaveSource[i] = prob;
    }
  }
  
  // Sync PatternEngine to visual editor (DSP → editor)
  void syncPatternEngineToEditor(SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine) return;
    
    // Rest lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::REST].probabilities[i] = patternEngine->rhythmRandom[i];
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i] = patternEngine->melodyRandom[i];
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i] = patternEngine->octaveRandom[i];
    }
  }
  
  // Get spread-adjusted probability for display
  float getDisplayProbability(int lane, int step) const {
    if (!patternEngine) return 0.0f;
    
    float prob = 0.0f;
    float spread = 0.0f;
    
    switch (lane) {
      case SandsVisualEditorV4::REST:
        prob = patternEngine->rhythmRandom[step];
        spread = spreadRest;
        break;
      case SandsVisualEditorV4::MELODY:
        prob = patternEngine->melodyRandom[step];
        spread = spreadMelody;
        break;
      case SandsVisualEditorV4::OCTAVE:
        prob = patternEngine->octaveRandom[step];
        spread = spreadOctave;
        break;
      default:
        return 0.0f;
    }
    
    return applySpread(prob, spread);
  }
};

}  // namespace redDot
