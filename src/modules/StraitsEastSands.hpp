#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../ui/TabButton.hpp"
#include "../managers/PolyVoiceSandsParameterManager.hpp"

namespace redDot {

/**
 * StraitsEastSands Module
 * 
 * Poly voice DNA editor for Straits East (voices 2-8, 7 total)
 * Expandable right of Straits (East) 24HP poly expander
 * 
 * Features:
 *   - 7 voice tabs for selecting which voice to edit
 *   - Visual editor with 3 lanes (REST, MELODY, OCTAVE)
 *   - Per-voice spread controls (21 knobs: 7 voices × 3 lanes)
 *   - Interpolation target selector (AVERAGE_POLY or MONO_DRAW)
 *   - Real-time visual feedback of interpolated probabilities
 * 
 * Voice Mapping:
 *   Tab V2 → voice index 0 → Monsoon voice 2
 *   Tab V3 → voice index 1 → Monsoon voice 3
 *   ...
 *   Tab V8 → voice index 6 → Monsoon voice 8
 * 
 * Data Flow:
 *   User clicks V3 tab
 *     → syncEditorToPatternEngine(1, ...)  [save V2 state]
 *     → selectedVoice = 1
 *     → syncPatternEngineToEditor(1, ...)  [load V3 state]
 *     → Visual editor shows V3 probabilities
 */

struct StraitsEastSands : rack::Module {
  enum ParamIds {
    // Spread controls: 7 voices × 3 lanes = 21 params
    SPREAD_V0_R,  // Voice 2, REST lane
    SPREAD_V0_M,  // Voice 2, MELODY lane
    SPREAD_V0_O,  // Voice 2, OCTAVE lane
    
    SPREAD_V1_R,  // Voice 3, REST lane
    SPREAD_V1_M,
    SPREAD_V1_O,
    
    SPREAD_V2_R,  // Voice 4
    SPREAD_V2_M,
    SPREAD_V2_O,
    
    SPREAD_V3_R,  // Voice 5
    SPREAD_V3_M,
    SPREAD_V3_O,
    
    SPREAD_V4_R,  // Voice 6
    SPREAD_V4_M,
    SPREAD_V4_O,
    
    SPREAD_V5_R,  // Voice 7
    SPREAD_V5_M,
    SPREAD_V5_O,
    
    SPREAD_V6_R,  // Voice 8
    SPREAD_V6_M,
    SPREAD_V6_O,
    
    INTERP_TARGET,  // Toggle: AVERAGE_POLY (0) vs MONO_DRAW (1)
    
    NUM_PARAMS
  };
  
  enum InputIds {
    NUM_INPUTS
  };
  
  enum OutputIds {
    NUM_OUTPUTS
  };
  
  enum LightIds {
    NUM_LIGHTS
  };
  
  StraitsEastSands() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    
    // Configure spread parameters
    for (int v = 0; v < 7; ++v) {
      for (int l = 0; l < 3; ++l) {
        int paramId = SPREAD_V0_R + v * 3 + l;
        configParam(paramId, 0.f, 1.f, 0.f, 
                   rack::string::f("Voice %d Spread %s", v+2, 
                     l == 0 ? "REST" : (l == 1 ? "MELODY" : "OCTAVE")));
      }
    }
    
    // Interpolation target selector
    configParam(INTERP_TARGET, 0.f, 1.f, 0.f, "Interpolation Target");
  }
  
  void process(const ProcessArgs& args) override {
    // DNA processing would go here if needed
    // For now, just syncing visual editor in widget
  }
};

struct StraitsEastSandsWidget : rack::ModuleWidget {
  StraitsEastSands* module = nullptr;
  
  // Visual editor (single widget, switches between voices)
  SandsVisualEditorV4* visualEditor = nullptr;
  
  // Voice tabs (V2-V8)
  TabButtonGroup* tabGroup = nullptr;
  
  // Current selected voice
  int selectedVoice = 0;
  
  // Parameter manager
  PolyVoiceSandsParameterManager* paramMgr = nullptr;
  
  StraitsEastSandsWidget(StraitsEastSands* mod) : module(mod) {
    setModule(module);
    setPanel(rack::createPanel(rack::asset::plugin(pluginInstance, "res/StraitsEastSands_24HP.svg")));
    
    // Get references to pattern engine and sequencer from parent Monsoon
    PatternEngine* patternEngine = nullptr;
    SequencerEngine* sequencerEngine = nullptr;
    
    if (module->rightExpander.module) {
      // Assuming parent is Monsoon module
      auto monsoon = dynamic_cast<redDot::Monsoon*>(module->rightExpander.module);
      if (monsoon) {
        patternEngine = &monsoon->patternEngine;
        sequencerEngine = &monsoon->sequencerEngine;
      }
    }
    
    // Create parameter manager
    paramMgr = new PolyVoiceSandsParameterManager(
      patternEngine,
      sequencerEngine,
      7,  // 7 voices for East
      0   // startVoiceIdx
    );
    
    // ===== TAB BUTTONS =====
    // Create voice tabs (V2-V8)
    tabGroup = new TabButtonGroup(7, 2);  // 7 tabs, starting at V2
    tabGroup->box.pos = Vec(10, 40);
    addChild(tabGroup);
    
    // ===== VISUAL EDITOR =====
    // Single visual editor that switches voices
    visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::Mode::POLY);
    visualEditor->box.pos = Vec(10, 80);
    visualEditor->box.size = Vec(180, 150);
    addChild(visualEditor);
    
    // ===== SPREAD CONTROLS =====
    // Spread sliders for selected voice (3 lanes)
    for (int l = 0; l < 3; ++l) {
      auto slider = createParam<rack::Slider>(
        Vec(20 + l * 60, 250),
        module,
        StraitsEastSands::SPREAD_V0_R + l
      );
      addParam(slider);
    }
    
    // ===== INTERPOLATION TARGET TOGGLE =====
    addParam(createParam<rack::CKSS>(
      Vec(50, 300),
      module,
      StraitsEastSands::INTERP_TARGET
    ));
    
    // ===== LABEL DISPLAY =====
    // Show current voice label
    // (Would need custom widget for this)
  }
  
  void step() override {
    if (!module || !paramMgr) return;
    
    // Get selected voice from tab group
    int newSelected = tabGroup->getSelectedTab();
    if (newSelected != selectedVoice) {
      onVoiceTabChanged(newSelected);
    }
    
    // Update spread parameters from knobs
    if (module) {
      for (int v = 0; v < 7; ++v) {
        for (int l = 0; l < 3; ++l) {
          int paramId = StraitsEastSands::SPREAD_V0_R + v * 3 + l;
          float spread = module->params[paramId].getValue();
          paramMgr->setSpread(v, l, spread);
        }
      }
      
      // Update interpolation target
      bool useMono = module->params[StraitsEastSands::INTERP_TARGET].getValue() > 0.5f;
      auto target = useMono ? 
        SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY;
      paramMgr->setInterpolationTarget(target);
    }
    
    // Sync current selected voice to visual editor
    // This shows interpolated values
    paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
    
    ModuleWidget::step();
  }
  
  /**
   * Handle voice tab change
   * 
   * Saves current voice state and loads new voice state
   */
  void onVoiceTabChanged(int newVoiceIdx) {
    if (!paramMgr || !visualEditor) return;
    
    // Save current voice's edits back to PatternEngine
    paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
    
    // Switch to new voice
    selectedVoice = newVoiceIdx;
    
    // Load new voice's state from PatternEngine
    paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
    
    // Update spread knob values to match new voice
    if (module) {
      for (int l = 0; l < 3; ++l) {
        int paramId = StraitsEastSands::SPREAD_V0_R + selectedVoice * 3 + l;
        float currentSpread = paramMgr->getSpread(selectedVoice, l);
        module->params[paramId].setValue(currentSpread);
      }
    }
  }
};

}  // namespace redDot
