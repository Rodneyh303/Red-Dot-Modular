#pragma once
#include "../plugin.hpp"

namespace redDot {

/**
 * StraitsEastSands Module
 * 
 * Complete module implementation for Straits East visual editor integration.
 * This is an expander for Monsoon that provides visual DNA editing.
 * 
 * Features:
 *  - 7 voices (2-8, with voice 1 on Monsoon)
 *  - Per-voice DNA parameter access
 *  - Playback position tracking
 *  - State persistence (save/load)
 */

struct StraitsEastSands : rack::Module {
  
  enum ParamId {
    // (This is just a stub - your actual module will have many more params)
    PARAMS_LEN
  };
  
  enum InputId {
    INPUTS_LEN
  };
  
  enum OutputId {
    OUTPUTS_LEN
  };
  
  enum LightId {
    LIGHTS_LEN
  };
  
  // Reference to parent Monsoon module
  rack::Module* monsoonModule = nullptr;
  
  // Current playback step (synced from Monsoon)
  int currentStep = -1;
  
  StraitsEastSands() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    
    // This module has no direct parameters - all control happens
    // through the visual editor and parameter binding to Monsoon
  }
  
  void process(const ProcessArgs& args) override {
    // This module is purely a visual editor UI
    // No audio processing needed
    // All parameter updates flow through the visual editor
  }
  
  // ===== MONSOON INTEGRATION =====
  
  void setMonsoonModule(rack::Module* monsoon) {
    monsoonModule = monsoon;
  }
  
  rack::Module* getMonsoonModule() {
    return monsoonModule;
  }
  
  // Get current playback step from Monsoon
  int getCurrentStep() {
    if (!monsoonModule) return -1;
    
    // You would call methods on Monsoon to get current step
    // This is a placeholder - implement based on your Monsoon design
    // Example:
    // return static_cast<Monsoon*>(monsoonModule)->playbackStep;
    
    return currentStep;
  }
  
  void setCurrentStep(int step) {
    currentStep = step;
  }
  
  // ===== STATE PERSISTENCE =====
  
  json_t* dataToJson() override {
    json_t* root = json_object();
    
    // Store reference to connected Monsoon module (if any)
    if (monsoonModule) {
      // Find module ID
      for (size_t i = 0; i < APP->engine->getModules().size(); ++i) {
        if (APP->engine->getModules()[i] == monsoonModule) {
          json_object_set_new(root, "monsoonModuleId", json_integer(i));
          break;
        }
      }
    }
    
    return root;
  }
  
  void dataFromJson(json_t* root) override {
    // Restore Monsoon module reference
    json_t* monsoonId = json_object_get(root, "monsoonModuleId");
    if (json_is_integer(monsoonId)) {
      size_t id = json_integer_value(monsoonId);
      auto modules = APP->engine->getModules();
      if (id < modules.size()) {
        monsoonModule = modules[id];
      }
    }
  }
};

/**
 * StraitsWestSands Module
 * 
 * Identical to StraitsEastSands but for voices 9-16.
 * Can be used alongside East for full 16-voice control.
 */

struct StraitsWestSands : rack::Module {
  
  enum ParamId {
    PARAMS_LEN
  };
  
  enum InputId {
    INPUTS_LEN
  };
  
  enum OutputId {
    OUTPUTS_LEN
  };
  
  enum LightId {
    LIGHTS_LEN
  };
  
  rack::Module* monsoonModule = nullptr;
  int currentStep = -1;
  
  StraitsWestSands() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
  }
  
  void process(const ProcessArgs& args) override {
    // Pure visual editor, no audio processing
  }
  
  void setMonsoonModule(rack::Module* monsoon) {
    monsoonModule = monsoon;
  }
  
  int getCurrentStep() {
    if (!monsoonModule) return -1;
    return currentStep;
  }
  
  void setCurrentStep(int step) {
    currentStep = step;
  }
  
  json_t* dataToJson() override {
    json_t* root = json_object();
    if (monsoonModule) {
      for (size_t i = 0; i < APP->engine->getModules().size(); ++i) {
        if (APP->engine->getModules()[i] == monsoonModule) {
          json_object_set_new(root, "monsoonModuleId", json_integer(i));
          break;
        }
      }
    }
    return root;
  }
  
  void dataFromJson(json_t* root) override {
    json_t* monsoonId = json_object_get(root, "monsoonModuleId");
    if (json_is_integer(monsoonId)) {
      size_t id = json_integer_value(monsoonId);
      auto modules = APP->engine->getModules();
      if (id < modules.size()) {
        monsoonModule = modules[id];
      }
    }
  }
};

/**
 * Optional: Extended Monsoon with visual editor support
 * 
 * If you want to add visual editor directly to Monsoon (instead of
 * as a separate expander), you could enhance Monsoon like this:
 */

struct MonsoonWithVisualEditor : rack::Module {
  // ... existing Monsoon code ...
  
  // New: Add reference to visual editor UI
  StraitsVisualEditorPanel* visualEditor = nullptr;
  
  void setVisualEditor(StraitsVisualEditorPanel* editor) {
    visualEditor = editor;
  }
  
  // Supply current playback step to editor
  void updatePlaybackSync() {
    if (visualEditor) {
      // Assuming your Monsoon has a playback position
      // visualEditor->setCurrentPlayStep(currentPlaybackStep);
    }
  }
  
  // Called by editor when user changes parameters
  void onEditorChange() {
    // Any module-level logic needed when visual editor updates
  }
};

}  // namespace redDot

/**
 * ============================================================================
 * MODULE WIDGET SETUP
 * ============================================================================
 * 
 * In your plugin.cpp or modules file, add:
 * 
 * #include "ui/MonsoonSandsIntegration.hpp"
 * 
 * Then in the Model creation:
 * 
 *   Model* modelStraitsEastSands = createModel<
 *     redDot::StraitsEastSands, 
 *     redDot::StraitsEastSandsWidget
 *   >("StraitsEastSands");
 *   
 *   Model* modelStraitsWestSands = createModel<
 *     redDot::StraitsWestSands,
 *     redDot::StraitsWestSandsWidget
 *   >("StraitsWestSands");
 * 
 * And add to the plugin model list:
 *   plugin->addModel(modelStraitsEastSands);
 *   plugin->addModel(modelStraitsWestSands);
 * 
 * ============================================================================
 */

/**
 * ============================================================================
 * MONSOON EXPANDER SETUP (OPTIONAL)
 * ============================================================================
 * 
 * If your Monsoon doesn't already support expanders, add this to Monsoon.cpp:
 * 
 * struct MonsoonModule : Module {
 *   // ... existing code ...
 *   
 *   int expanderInput = -1;   // No input expander
 *   int expanderOutput = -1;  // Output to Straits
 *   
 *   void process(ProcessArgs& args) {
 *     // ... normal processing ...
 *     
 *     // Tell expanders about current playback step
 *     if (rightExpander.module) {
 *       auto* straitsModule = dynamic_cast<StraitsEastSands*>(rightExpander.module);
 *       if (straitsModule) {
 *         straitsModule->setCurrentStep(currentPlaybackStep);
 *       }
 *     }
 *   }
 * };
 * 
 * Note: If Monsoon already has expander support, just make sure it calls:
 *   straitsModule->setCurrentStep(currentStep);
 * 
 * ============================================================================
 */

/**
 * ============================================================================
 * CREATING PANEL SVGS
 * ============================================================================
 * 
 * For visual editor modules, create simple 24HP panels:
 * 
 * 1. StraitsEastSands_Visual_24HP.svg
 *    - 24HP = 230.4mm width
 *    - Height: 380mm (standard Rack height)
 *    - Leave space for visual editor (roughly 220×220)
 *    - Optional: Decorative elements, module name
 * 
 * 2. StraitsWestSands_Visual_24HP.svg
 *    - Same dimensions
 *    - Could say "West" instead of "East"
 * 
 * Example minimal SVG:
 * 
 *   <svg viewBox="0 0 230 380" xmlns="http://www.w3.org/2000/svg">
 *     <!-- Background -->
 *     <rect width="230" height="380" fill="#141416"/>
 *     
 *     <!-- Title -->
 *     <text x="115" y="20" text-anchor="middle" font-size="12" fill="#d4af37">
 *       STRAITS EAST
 *     </text>
 *     
 *     <!-- Decorative border -->
 *     <rect x="0" y="0" width="230" height="380" 
 *           fill="none" stroke="#333333" stroke-width="2"/>
 *   </svg>
 * 
 * ============================================================================
 */
