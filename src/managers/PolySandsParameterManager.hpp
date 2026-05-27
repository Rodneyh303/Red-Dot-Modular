#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"
#include "SpreadManager.hpp"

namespace redDot {

/**
 * PolySandsParameterManager (Global/Macro)
 * 
 * Wires SandsVisualEditorV4 to MonsoonStraitSandsExpander (Poly Sands - Global).
 * 
 * Global poly DNA control: 3 lanes only (REST, MELODY, OCTAVE)
 * All voices share the same probability distribution.
 * 
 * Spread Control (via SpreadManager):
 *   Interpolates between original poly draw and:
 *   - Target 1: Average of all poly voices
 *   - Target 2: Mono voice draw (reference)
 *   
 *   At spread=0.0: Uses original poly distribution
 *   At spread=1.0: Uses target (average or mono)
 * 
 * Data Storage:
 *   Probabilities: PatternEngine rhythmRandom/melodyRandom/octaveRandom
 *   Spread: MacroSpreadManager (same spread applied to all voices)
 * 
 * Usage:
 *   PolySandsParameterManager mgr(patternEngine, 7);
 *   mgr.spreadMgr.setInterpolationTarget(SpreadManager::AVERAGE_POLY);
 *   mgr.spreadMgr.setSpread(lane, spread);
 *   mgr.syncPatternEngineToEditor(editor->currentState);
 * 
 * Voices affected: All voices 2-16
 */

struct PolySandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  rack::Module* monsoonModule = nullptr;
  
  // Spread manager handles interpolation
  MacroSpreadManager spreadMgr;
  
  PolySandsParameterManager(PatternEngine* pe = nullptr, rack::Module* m = nullptr,
                             int numVoices = 7)
    : patternEngine(pe), monsoonModule(m), spreadMgr(pe, numVoices) {}
  
  // Sync visual editor to PatternEngine (editor → DSP)
  void syncEditorToPatternEngine(const SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine) return;
    
    // Rest lane (global rhythm for all voices)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->rhythmRandom[i] = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      patternEngine->rhythmSource[i] = patternEngine->rhythmRandom[i];
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->melodyRandom[i] = editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i];
      patternEngine->melodySource[i] = patternEngine->melodyRandom[i];
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->octaveRandom[i] = editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i];
      patternEngine->octaveSource[i] = patternEngine->octaveRandom[i];
    }
  }
  
  // Sync PatternEngine to visual editor (DSP → editor)
  // Shows interpolated values with spread applied
  void syncPatternEngineToEditor(SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine) return;
    
    // Rest lane - show interpolated values
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      // Use voice 0 as reference for global display
      editorState.lanes[SandsVisualEditorV4::REST].probabilities[i] = 
        spreadMgr.getInterpolatedValue(0, 0, i);
    }
    
    // Melody lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i] = 
        spreadMgr.getInterpolatedValue(0, 1, i);
    }
    
    // Octave lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i] = 
        spreadMgr.getInterpolatedValue(0, 2, i);
    }
  }
  
  // Get interpolated probability for display (with spread applied)
  float getDisplayProbability(int lane, int step) const {
    if (lane < 0 || lane > 2 || step < 0 || step >= 16) return 0.0f;
    
    // For global display, use voice 0 as representative
    return spreadMgr.getInterpolatedValue(0, lane, step);
  }
};

}  // namespace redDot
