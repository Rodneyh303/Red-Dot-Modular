#pragma once
#include "../plugin.hpp"
#include "StraitsVisualEditorPanel.hpp"

namespace redDot {

/**
 * MonsoonParameterManager
 * 
 * Concrete implementation for Monsoon module.
 * Maps visual editor lanes/steps to actual Monsoon.hpp parameter IDs.
 * 
 * IMPORTANT: Update POLY_*_BASE values to match your Monsoon.hpp enum ParamId
 */

struct MonsoonParameterManager : SandsParameterManager {
  
  // ===== PARAMETER ID BASES (from Monsoon.hpp) =====
  // CUSTOMIZE THESE for your actual parameter layout
  
  static constexpr int POLY_DNA_VOICE_1_LEN = 0;          // First Rest lane param
  static constexpr int POLY_DNA_VOICE_1_OFF = 1;          // Offset
  static constexpr int POLY_DNA_VOICE_1_ROT = 2;          // Rotation
  
  static constexpr int POLY_MELODY_VOICE_1_LEN = 48;      // First Melody lane param
  static constexpr int POLY_MELODY_VOICE_1_OFF = 49;
  static constexpr int POLY_MELODY_VOICE_1_ROT = 50;
  
  static constexpr int POLY_OCTAVE_VOICE_1_LEN = 96;      // First Octave lane param
  static constexpr int POLY_OCTAVE_VOICE_1_OFF = 97;
  static constexpr int POLY_OCTAVE_VOICE_1_ROT = 98;
  
  static constexpr int POLY_LEGATO_VOICE_1_LEN = 144;     // First Legato lane param
  static constexpr int POLY_LEGATO_VOICE_1_OFF = 145;
  static constexpr int POLY_LEGATO_VOICE_1_ROT = 146;
  
  static constexpr int POLY_VARIATION_VOICE_1_LEN = 192;  // First Variation lane param
  static constexpr int POLY_VARIATION_VOICE_1_OFF = 193;
  static constexpr int POLY_VARIATION_VOICE_1_ROT = 194;
  
  // Stride: Each voice gets 3 params (len, off, rot)
  static constexpr int VOICE_STRIDE = 3;
  
  // ===== LANE TO BASE MAPPING =====
  
  int getLaneBase(int lane, bool isRot = false) {
    // Lane: 0=Rest, 1=Melody, 2=Octave, 3=Legato, 4=Variation
    switch (lane) {
      case 0:  // Rest
        return isRot ? POLY_DNA_VOICE_1_ROT : POLY_DNA_VOICE_1_LEN;
      case 1:  // Melody
        return isRot ? POLY_MELODY_VOICE_1_ROT : POLY_MELODY_VOICE_1_LEN;
      case 2:  // Octave
        return isRot ? POLY_OCTAVE_VOICE_1_ROT : POLY_OCTAVE_VOICE_1_LEN;
      case 3:  // Legato
        return isRot ? POLY_LEGATO_VOICE_1_ROT : POLY_LEGATO_VOICE_1_LEN;
      case 4:  // Variation
        return isRot ? POLY_VARIATION_VOICE_1_ROT : POLY_VARIATION_VOICE_1_LEN;
      default:
        return -1;
    }
  }
  
  // ===== PARAMETER ID CALCULATION =====
  
  int getStepParamId(int lane, int voice, int step) override {
    // Lane: 0=Rest, 1=Melody, 2=Octave, 3=Legato, 4=Variation
    // Voice: 2-16 (human-readable), but index as 0-14 internally
    // Step: 0-15
    
    int laneBase = getLaneBase(lane, false);
    if (laneBase < 0) return -1;
    
    // Voices are 1-indexed, convert to 0-based
    int voiceIndex = voice - 1;
    int voiceOffset = voiceIndex * VOICE_STRIDE;
    
    // Step values are sequential: LEN + step
    int paramId = laneBase + voiceOffset + step;
    
    return paramId;
  }
  
  int getStartParamId(int lane, int voice) override {
    // Start offset stored in OFF parameter
    // In some designs, OFF = start offset
    // In others, it might be calculated from length
    
    int laneBase = getLaneBase(lane, false);
    if (laneBase < 0) return -1;
    
    int voiceIndex = voice - 1;
    int voiceOffset = voiceIndex * VOICE_STRIDE;
    
    // OFF is at position +1 in stride
    int paramId = laneBase + voiceOffset + 1;
    
    return paramId;
  }
  
  int getEndParamId(int lane, int voice) override {
    // End might be stored as length (start + length = end)
    // Or as direct end value
    // This implementation assumes it's calculated or not stored separately
    
    // If your Monsoon stores end directly:
    // return laneBase + voiceOffset + someOffset;
    
    // If using length instead, calculate end = start + length
    // For now, return -1 (not directly stored)
    return -1;
  }
  
  int getRotationParamId(int lane, int voice) override {
    // Rotation stored in ROT parameter
    
    int laneBase = getLaneBase(lane, true);  // true = get ROT base
    if (laneBase < 0) return -1;
    
    int voiceIndex = voice - 1;
    int voiceOffset = voiceIndex * VOICE_STRIDE;
    
    // ROT is at position +2 in stride
    int paramId = laneBase + voiceOffset + 2;
    
    return paramId;
  }
  
  // ===== SYNC HELPERS =====
  
  void syncEditorToModule(const SandsVisualEditorV2::VoiceState& state,
                         int voice, int lane) override {
    if (!module) return;
    
    // Sync 16 step values
    for (int step = 0; step < 16; ++step) {
      int paramId = getStepParamId(lane, voice, step);
      if (paramId >= 0 && paramId < (int)module->params.size()) {
        float value = state.values[step][lane];
        module->params[paramId].setValue(value);
      }
    }
    
    // Sync rotation
    int rotParamId = getRotationParamId(lane, voice);
    if (rotParamId >= 0 && rotParamId < (int)module->params.size()) {
      module->params[rotParamId].setValue(state.rotation[lane]);
    }
    
    // Sync start (if available)
    int startParamId = getStartParamId(lane, voice);
    if (startParamId >= 0 && startParamId < (int)module->params.size()) {
      module->params[startParamId].setValue(state.startStep[lane]);
    }
  }
  
  void syncModuleToEditor(SandsVisualEditorV2::VoiceState& state,
                         int voice, int lane) override {
    if (!module) return;
    
    // Sync 16 step values
    for (int step = 0; step < 16; ++step) {
      int paramId = getStepParamId(lane, voice, step);
      if (paramId >= 0 && paramId < (int)module->params.size()) {
        state.values[step][lane] = module->params[paramId].getValue();
      }
    }
    
    // Sync rotation
    int rotParamId = getRotationParamId(lane, voice);
    if (rotParamId >= 0 && rotParamId < (int)module->params.size()) {
      state.rotation[lane] = (int)module->params[rotParamId].getValue();
    }
    
    // Sync start
    int startParamId = getStartParamId(lane, voice);
    if (startParamId >= 0 && startParamId < (int)module->params.size()) {
      state.startStep[lane] = (int)module->params[startParamId].getValue();
    }
    
    // For end, calculate as start + 16 (or however your design works)
    state.endStep[lane] = state.startStep[lane] + 16;
    if (state.endStep[lane] > 16) state.endStep[lane] = 16;
  }
};

/**
 * StraitsEastSandsPanel
 * 
 * Visual editor panel for Straits East (voices 2-8).
 * Customized for Monsoon parameter layout.
 */

struct StraitsEastSandsPanel : StraitsVisualEditorPanel {
  static constexpr int NUM_VOICES_EAST = 7;  // Voices 2-8
  
  StraitsEastSandsPanel(rack::Module* m) : StraitsVisualEditorPanel(m) {
    // Override parameter manager with Monsoon-specific implementation
    delete paramManager;
    paramManager = new MonsoonParameterManager(m);
  }
};

/**
 * StraitsWestSandsPanel
 * 
 * Visual editor panel for Straits West (voices 9-16).
 * Same as East but maps to different voice numbers.
 */

struct StraitsWestSandsPanel : StraitsVisualEditorPanel {
  static constexpr int NUM_VOICES_WEST = 8;  // Voices 9-16
  
  StraitsWestSandsPanel(rack::Module* m) : StraitsVisualEditorPanel(m) {
    delete paramManager;
    paramManager = new MonsoonParameterManager(m);
  }
};

/**
 * StraitsEastSandsWidget
 * 
 * Complete module widget for Straits East.
 * Integrates visual editor with Monsoon.
 */

struct StraitsEastSandsWidget : rack::ModuleWidget {
  StraitsEastSands* module;
  StraitsEastSandsPanel* visualPanel;
  
  StraitsEastSandsWidget(StraitsEastSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, 
      "res/panels/StraitsEastSands_Visual_24HP.svg")));
    
    // Create visual editor panel
    visualPanel = new StraitsEastSandsPanel(module);
    visualPanel->box.pos = rack::Vec(10, 60);
    visualPanel->box.size = rack::Vec(220, 220);
    addChild(visualPanel);
    
    // (Optional: Add jacks for 14 CV inputs + attenuverters if Interchange)
    // addChild(createWidget<PJ301MPort>(Vec(20, 100)));
    // etc.
  }
  
  void step() override {
    ModuleWidget::step();
    
    if (module && visualPanel) {
      // Sync playback position to visual editor
      // This calls module->getCurrentStep() which you would implement
      // For now, you can set it manually from module logic:
      
      // Example: if module has currentStep member
      // visualPanel->setCurrentPlayStep(module->currentStep);
    }
  }
};

/**
 * StraitsWestSandsWidget
 * 
 * Complete module widget for Straits West (8 voices instead of 7).
 */

struct StraitsWestSandsWidget : rack::ModuleWidget {
  StraitsWestSands* module;
  StraitsWestSandsPanel* visualPanel;
  
  StraitsWestSandsWidget(StraitsWestSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, 
      "res/panels/StraitsWestSands_Visual_24HP.svg")));
    
    // Create visual editor panel
    visualPanel = new StraitsWestSandsPanel(module);
    visualPanel->box.pos = rack::Vec(10, 60);
    visualPanel->box.size = rack::Vec(220, 220);
    addChild(visualPanel);
  }
  
  void step() override {
    ModuleWidget::step();
    if (module && visualPanel) {
      // Sync playback position
    }
  }
};

}  // namespace redDot

/**
 * ============================================================================
 * INTEGRATION CHECKLIST FOR YOUR PROJECT
 * ============================================================================
 * 
 * 1. VERIFY PARAMETER IDs
 *    □ Check your Monsoon.hpp for actual parameter IDs
 *    □ Update POLY_*_BASE constants above
 *    □ Verify VOICE_STRIDE (usually 3)
 *    □ Test with first voice to confirm mapping
 * 
 * 2. IMPLEMENT MODULE CLASS
 *    □ Add getCurrentStep() method to Monsoon/Straits modules
 *    □ Implement dataToJson() / dataFromJson() for persistence
 *    □ Wire up visual panel in module widget
 * 
 * 3. REGISTER MODELS
 *    In plugin.cpp:
 *    
 *    Model* modelStraitsEastSands = createModel<StraitsEastSands, 
 *                                                  StraitsEastSandsWidget>(
 *      "StraitsEastSands"
 *    );
 *    
 *    Model* modelStraitsWestSands = createModel<StraitsWestSands,
 *                                                  StraitsWestSandsWidget>(
 *      "StraitsWestSands"
 *    );
 * 
 * 4. TEST PARAMETER SYNC
 *    □ Load plugin in VCV Rack
 *    □ Manually change Monsoon parameter (use Rack UI)
 *    □ Editor should update (sync module→editor)
 *    □ Change editor value (click/drag bar)
 *    □ Monsoon parameter should update (sync editor→module)
 *    □ Test all 5 lanes
 *    □ Test all 7 voices (East) or 8 (West)
 * 
 * 5. TEST VOICE SWITCHING
 *    □ Click V2 tab
 *    □ Edit some values
 *    □ Click V3 tab
 *    □ V2 state should be saved
 *    □ V3 state should load
 *    □ Click back to V2
 *    □ Previous edits should be there (with undo/redo)
 * 
 * 6. TEST PLAYBACK SYNC
 *    □ Play Monsoon
 *    □ Active step indicator should follow playback
 *    □ Indicator should animate (glow pulse)
 *    □ Stop playback
 *    □ Indicator should disappear
 * 
 * 7. TEST PERSISTENCE
 *    □ Make changes in editor
 *    □ Save patch
 *    □ Reload patch
 *    □ All editor state should restore exactly
 * 
 * 8. PERFORMANCE
 *    □ No audio glitches when editing
 *    □ Tab switching is instant (< 1ms)
 *    □ Playback doesn't stutter
 *    □ CPU usage stays low during interaction
 * 
 * ============================================================================
 */
