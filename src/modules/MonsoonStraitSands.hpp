#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../managers/PolySandsParameterManager.hpp"

namespace redDot {

/**
 * MonsoonStraitSands Expander
 * 
 * Global/Macro DNA editor for all poly voices (voices 2-16)
 * Expandable right of Monsoon Straits (Poly) expanders
 * 
 * Features:
 *   - Single visual editor showing representative voice (voice 2)
 *   - 3 spread controls (one per lane, applied globally to all voices)
 *   - Interpolation target selector (AVERAGE_POLY or MONO_DRAW)
 *   - All 15 poly voices interpolate toward same target
 * 
 * Voice Configuration:
 *   All voices 2-16 (15 total)
 *   East (2-8, 7 voices) + West (9-16, 8 voices)
 *   Single editor shows voice 2 as representative
 *   Same spread applied to all voices
 * 
 * Data Flow:
 *   User adjusts SPREAD_REST knob to 0.5
 *     → All 15 voices' REST lane interpolate toward their collective average
 *     → Visual editor shows voice 2's interpolated result
 *     → Each voice interpolates based on its own draw + same spread value
 */

struct MonsoonStraitSands : rack::Module {
  enum ParamIds {
    // Spread controls: 3 lanes (applied to all voices)
    SPREAD_REST,    // Applied to all 15 voices for REST lane
    SPREAD_MELODY,  // Applied to all 15 voices for MELODY lane
    SPREAD_OCTAVE,  // Applied to all 15 voices for OCTAVE lane
    
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
  
  MonsoonStraitSands() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    
    // Spread parameters (global)
    configParam(SPREAD_REST, 0.f, 1.f, 0.f, "Spread REST (All Voices)");
    configParam(SPREAD_MELODY, 0.f, 1.f, 0.f, "Spread MELODY (All Voices)");
    configParam(SPREAD_OCTAVE, 0.f, 1.f, 0.f, "Spread OCTAVE (All Voices)");
    
    // Interpolation target selector
    configParam(INTERP_TARGET, 0.f, 1.f, 0.f, "Interpolation Target");
  }
  
  void process(const ProcessArgs& args) override {
    // DNA processing would go here if needed
    // For now, just syncing visual editor in widget
  }
};

struct MonsoonStraitSandsWidget : rack::ModuleWidget {
  MonsoonStraitSands* module = nullptr;
  
  // Visual editor (shows representative voice 2)
  SandsVisualEditorV4* visualEditor = nullptr;
  
  // Parameter manager for global poly
  PolySandsParameterManager* paramMgr = nullptr;
  
  MonsoonStraitSandsWidget(MonsoonStraitSands* mod) : module(mod) {
    setModule(module);
    setPanel(rack::createPanel(rack::asset::plugin(pluginInstance, "res/MonsoonStraitSands_24HP.svg")));
    
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
    // 7 voices for the 15 total (though it spans both East and West)
    paramMgr = new PolySandsParameterManager(
      patternEngine,
      sequencerEngine,
      module,
      7  // Representative count (uses active voices dynamically)
    );
    
    // ===== TITLE LABEL =====
    // Add decorative label
    // (Would need custom widget for nice text display)
    
    // ===== VISUAL EDITOR =====
    // Single visual editor showing voice 2 (representative of all)
    visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::Mode::POLY);
    visualEditor->box.pos = Vec(10, 40);
    visualEditor->box.size = Vec(200, 150);
    addChild(visualEditor);
    
    // ===== SPREAD CONTROL SLIDERS =====
    // 3 knobs: one per lane, applied to all voices
    float xStart = 15;
    float yStart = 210;
    float spacing = 70;
    
    // REST spread
    auto sliderRest = createParam<rack::Slider>(
      Vec(xStart, yStart),
      module,
      MonsoonStraitSands::SPREAD_REST
    );
    addParam(sliderRest);
    
    // MELODY spread
    auto sliderMelody = createParam<rack::Slider>(
      Vec(xStart + spacing, yStart),
      module,
      MonsoonStraitSands::SPREAD_MELODY
    );
    addParam(sliderMelody);
    
    // OCTAVE spread
    auto sliderOctave = createParam<rack::Slider>(
      Vec(xStart + spacing * 2, yStart),
      module,
      MonsoonStraitSands::SPREAD_OCTAVE
    );
    addParam(sliderOctave);
    
    // ===== INTERPOLATION TARGET TOGGLE =====
    addParam(createParam<rack::CKSS>(
      Vec(90, 270),
      module,
      MonsoonStraitSands::INTERP_TARGET
    ));
  }
  
  void step() override {
    if (!module || !paramMgr) return;
    
    // Update spread parameters from knobs
    // MacroSpreadManager applies same spread to all voices
    if (module) {
      float spreadRest = module->params[MonsoonStraitSands::SPREAD_REST].getValue();
      float spreadMelody = module->params[MonsoonStraitSands::SPREAD_MELODY].getValue();
      float spreadOctave = module->params[MonsoonStraitSands::SPREAD_OCTAVE].getValue();
      
      // Set spreads (applies to all voices)
      paramMgr->spreadMgr.setSpread(0, spreadRest);   // REST
      paramMgr->spreadMgr.setSpread(1, spreadMelody); // MELODY
      paramMgr->spreadMgr.setSpread(2, spreadOctave); // OCTAVE
      
      // Update interpolation target
      bool useMono = module->params[MonsoonStraitSands::INTERP_TARGET].getValue() > 0.5f;
      auto target = useMono ? 
        SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY;
      paramMgr->spreadMgr.setInterpolationTarget(target);
    }
    
    // Sync to visual editor
    // Shows interpolated values for voice 2 (representative)
    paramMgr->syncPatternEngineToEditor(visualEditor->currentState);
    
    ModuleWidget::step();
  }
};

}  // namespace redDot
