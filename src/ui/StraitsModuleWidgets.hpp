#pragma once
#include "../plugin.hpp"
#include "StraitsVisualEditorPanel.hpp"

namespace redDot {

/**
 * StraitsEastSandsWidget
 * 
 * Complete module widget for Straits East (voices 2-8, 24HP visual editor).
 * Integrates visual DNA editor with 7-voice tab system.
 * 
 * Module: StraitsEastSands
 * Editor: 7 tabs (voices 2-8)
 * Parameters: Monsoon.hpp POLY_* parameters
 */

struct StraitsEastSandsWidget : rack::ModuleWidget {
  StraitsEastSands* module;
  StraitsVisualEditorPanel* visualPanel;
  
  StraitsEastSandsWidget(StraitsEastSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/StraitsEastSands_Visual_24HP.svg")));
    
    // Create visual editor panel
    visualPanel = new StraitsVisualEditorPanel(module);
    visualPanel->box.pos = rack::Vec(10, 60);
    addChild(visualPanel);
  }
  
  void step() override {
    ModuleWidget::step();
    
    if (module && visualPanel) {
      // Sync playback position to visual editor
      if (module->getCurrentStep) {
        visualPanel->setCurrentPlayStep(module->getCurrentStep());
      }
      
      // Bidirectional sync happens in visualPanel->step()
    }
  }
};

/**
 * StraitsWestSandsWidget
 * 
 * Complete module widget for Straits West (voices 9-16, 24HP visual editor).
 * Identical to East except for voice numbering (9-16 instead of 2-8).
 * 
 * Module: StraitsWestSands
 * Editor: 8 tabs (voices 9-16)
 * Parameters: Monsoon.hpp POLY_* parameters
 */

struct StraitsWestSandsWidget : rack::ModuleWidget {
  StraitsWestSands* module;
  StraitsVisualEditorPanelWest* visualPanel;
  
  StraitsWestSandsWidget(StraitsWestSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/StraitsWestSands_Visual_24HP.svg")));
    
    // Create visual editor panel
    visualPanel = new StraitsVisualEditorPanelWest(module);
    visualPanel->box.pos = rack::Vec(10, 60);
    addChild(visualPanel);
  }
  
  void step() override {
    ModuleWidget::step();
    
    if (module && visualPanel) {
      // Sync playback position to visual editor
      if (module->getCurrentStep) {
        visualPanel->setCurrentPlayStep(module->getCurrentStep());
      }
    }
  }
};

}  // namespace redDot

/**
 * ============================================================================
 * CUSTOM PARAMETER MAPPING TEMPLATE
 * ============================================================================
 * 
 * To use StraitsVisualEditorPanel with your module, create a custom
 * ParameterManager that maps editor lanes/steps to your parameter IDs.
 * 
 * Example implementation for Monsoon.hpp:
 * 
 * struct MonsoonParameterManager : SandsParameterManager {
 *   
 *   // Override to map step values
 *   int getStepParamId(int lane, int voice, int step) override {
 *     // Lane: 0=Rest, 1=Melody, 2=Octave, 3=Legato, 4=Variation
 *     // Voice: 2-8 (Straits East) or 9-16 (Straits West)
 *     // Step: 0-15
 *     
 *     int laneOffsets[] = {0, 48, 96, 144, 192};  // Per lane offset
 *     int baseId = POLY_DNA_VOICE_1_LEN;
 *     int laneId = baseId + laneOffsets[lane];
 *     int voiceOffset = (voice - 1) * 3;  // Stride of 3 per voice
 *     
 *     return laneId + voiceOffset + step;  // Final param ID
 *   }
 *   
 *   int getRotationParamId(int lane, int voice) override {
 *     // POLY_DNA_VOICE_X_ROT (or similar)
 *     int laneOffsets[] = {POLY_DNA_OFFSET, POLY_MELODY_OFFSET, ...};
 *     int baseId = POLY_DNA_VOICE_1_ROT;
 *     int voiceOffset = (voice - 1) * 3;
 *     
 *     return baseId + voiceOffset;
 *   }
 *   
 *   int getStartParamId(int lane, int voice) override {
 *     // If using separate start parameter (not all modules have this)
 *     // Otherwise, can calculate from rotation offset
 *     return -1;  // Not available
 *   }
 *   
 *   int getEndParamId(int lane, int voice) override {
 *     // If using separate length parameter
 *     return -1;  // Not available
 *   }
 * };
 * 
 * Then use in widget:
 * 
 * struct MonsoonSandsWidget : rack::ModuleWidget {
 *   void setup() {
 *     auto panel = new StraitsVisualEditorPanel(module);
 *     panel->paramManager = new MonsoonParameterManager(module);
 *     addChild(panel);
 *   }
 * };
 * 
 * ============================================================================
 */
