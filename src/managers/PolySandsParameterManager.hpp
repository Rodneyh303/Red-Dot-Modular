#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"
#include "../dsp/engines/SequencerEngine.hpp"
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
 * Spread Control (via MacroSpreadManager):
 *   Same spread applied to all voices (global mode)
 *   Interpolates toward switchable targets:
 *   - Target 1: Average of ACTIVE poly voices (creates cohesion)
 *   - Target 2: Mono voice draw (creates mono-to-poly blending)
 * 
 *   IMPORTANT: Average uses only actively requested voices!
 *   If polyphony=4 voices, average is calculated from those 4 voices.
 *   If polyphony=7 voices, average uses all 7 voices.
 *   This requires SequencerEngine reference for polyphony tracking.
 * 
 * Data Storage:
 *   Probabilities: PatternEngine rhythmRandom/melodyRandom/octaveRandom
 *   Spread: MacroSpreadManager (same for all voices)
 * 
 * Usage:
 *   SequencerEngine* seqEngine = /* from monsoon */;
 *   PolySandsParameterManager mgr(patternEngine, seqEngine, monsoonModule);
 *   mgr.spreadMgr.setInterpolationTarget(SpreadManager::AVERAGE_POLY);
 *   mgr.spreadMgr.setSpread(lane, spread);
 *   mgr.syncPatternEngineToEditor(editor->currentState);
 * 
 * Voices affected: All voices 2-16 (uses only active polyphony)
 */

struct PolySandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  SequencerEngine* sequencerEngine = nullptr;  // For active voice count
  rack::Module* monsoonModule = nullptr;
  
  // Spread manager handles interpolation
  // Uses SequencerEngine to determine average from active voices only
  MacroSpreadManager spreadMgr;
  
  PolySandsParameterManager(PatternEngine* pe = nullptr, SequencerEngine* se = nullptr,
                             rack::Module* m = nullptr, int numVoices = 7)
    : patternEngine(pe), sequencerEngine(se), monsoonModule(m), spreadMgr(pe, numVoices) {
    // Set SequencerEngine so SpreadManager can track active voices
    // This makes AVERAGE_POLY target use only requested polyphony
    if (se) {
      spreadMgr.setSequencerEngine(se);
    }
  }
  
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
  // Shows interpolated values with spread applied to all voices
  // Average is calculated from only the active/requested voices
  void syncPatternEngineToEditor(SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine) return;
    
    // Rest lane - show interpolated values (using voice 0 as representative)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
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
  // Average is calculated from only the active voices at this moment
  float getDisplayProbability(int lane, int step) const {
    if (lane < 0 || lane > 2 || step < 0 || step >= 16) return 0.0f;
    
    // For global display, use voice 0 as representative
    return spreadMgr.getInterpolatedValue(0, lane, step);
  }
};

}  // namespace redDot
