#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"

namespace redDot {

/**
 * MonoSandsParameterManager
 * 
 * Wires SandsVisualEditorV4 to MonsoonSandsExpander (Mono Sands).
 * 
 * Data Flow:
 *   PatternEngine (6 strands × 16 steps) ←→ Visual Editor ↔ Knobs (existing)
 * 
 * What we manage:
 *   ✅ Sync 16 probability values to/from PatternEngine
 *   ✅ Keep length/offset/rotation in sync
 *   ✅ Don't touch existing knob controls
 * 
 * Mono: 6 lanes (REST, MELODY, OCTAVE, LEGATO, ACCENT, VARIATION)
 * 1 voice (voice 1)
 * 
 * Usage:
 *   MonoSandsParameterManager mgr(patternEngine);
 *   mgr.syncEditorToPatternEngine(editor->currentState);
 *   mgr.syncPatternEngineToEditor(editor->currentState);
 */

struct MonoSandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  
  MonoSandsParameterManager(PatternEngine* pe = nullptr) 
    : patternEngine(pe) {}
  
  // Sync visual editor to PatternEngine (editor → DSP)
  void syncEditorToPatternEngine(const SandsVisualEditorV4::VoiceState& editorState) {
    if (!patternEngine) return;
    
    // Rest lane (uses rhythmRandom)
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->rhythmRandom[i] = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      patternEngine->rhythmSource[i] = patternEngine->rhythmRandom[i];  // Keep source in sync
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
    
    // Legato lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->legatoRandom[i] = editorState.lanes[SandsVisualEditorV4::LEGATO].probabilities[i];
      patternEngine->legatoSource[i] = patternEngine->legatoRandom[i];
    }
    
    // Accent lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->accentRandom[i] = editorState.lanes[SandsVisualEditorV4::ACCENT].probabilities[i];
      patternEngine->accentSource[i] = patternEngine->accentRandom[i];
    }
    
    // Variation lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->variationRandom[i] = editorState.lanes[SandsVisualEditorV4::VARIATION].probabilities[i];
      patternEngine->variationSource[i] = patternEngine->variationRandom[i];
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
    
    // Legato lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::LEGATO].probabilities[i] = patternEngine->legatoRandom[i];
    }
    
    // Accent lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::ACCENT].probabilities[i] = patternEngine->accentRandom[i];
    }
    
    // Variation lane
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      editorState.lanes[SandsVisualEditorV4::VARIATION].probabilities[i] = patternEngine->variationRandom[i];
    }
  }
};

}  // namespace redDot
