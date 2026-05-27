#pragma once
#include <rack.hpp>
#include "SandsVisualEditorV2.hpp"

namespace redDot {

/**
 * SandsParameterManager
 * 
 * Handles bidirectional sync between visual editor and Monsoon module.
 * Maps SandsVisualEditor data ↔ Monsoon.hpp parameter IDs
 * 
 * Parameter structure (example for Straits East, voices 2-8):
 *   POLY_DNA_VOICE_X_LEN, _OFF, _ROT     (Rest lane)
 *   POLY_MELODY_VOICE_X_LEN, _OFF, _ROT  (Melody lane)
 *   POLY_OCTAVE_VOICE_X_LEN, _OFF, _ROT  (Octave lane)
 *   POLY_LEGATO_VOICE_X_LEN, _OFF, _ROT  (Legato lane - if available)
 * 
 * Note: Variation lane may be stored differently or as flags
 */

struct SandsParameterManager {
  // Reference to module for parameter access
  rack::Module* module = nullptr;
  
  // Parameter ID offsets (customize per module)
  struct ParameterMap {
    int restLenBase = 0;           // POLY_DNA_VOICE_1_LEN
    int restOffBase = 0;           // POLY_DNA_VOICE_1_OFF
    int restRotBase = 0;           // POLY_DNA_VOICE_1_ROT
    
    int melodyLenBase = 0;         // POLY_MELODY_VOICE_1_LEN
    int melodyOffBase = 0;         // POLY_MELODY_VOICE_1_OFF
    int melodyRotBase = 0;         // POLY_MELODY_VOICE_1_ROT
    
    int octaveLenBase = 0;         // POLY_OCTAVE_VOICE_1_LEN
    int octaveOffBase = 0;         // POLY_OCTAVE_VOICE_1_OFF
    int octaveRotBase = 0;         // POLY_OCTAVE_VOICE_1_ROT
    
    int legato = 0;                // POLY_LEGATO_VOICE_1 (if flags)
    int variation = 0;             // POLY_VARIATION_VOICE_1 (if flags)
    
    int voiceStride = 3;           // Parameter stride per voice (usually 3: len, off, rot)
  } paramMap;
  
  SandsParameterManager(rack::Module* m) : module(m) {}
  
  // ===== PARAMETER SYNC (Editor → Module) =====
  
  void syncEditorToModule(const SandsVisualEditorV2::VoiceState& state, 
                          int voiceNumber, int lane) {
    if (!module) return;
    
    // Calculate parameter IDs based on voice and lane
    int voiceOffset = voiceNumber * paramMap.voiceStride;
    
    // Sync 16 step values
    for (int step = 0; step < 16; ++step) {
      int paramId = getStepParamId(lane, voiceNumber, step);
      if (paramId >= 0) {
        float value = state.values[step][lane];
        module->params[paramId].setValue(value);
      }
    }
    
    // Sync start/end (or length)
    int startParamId = getStartParamId(lane, voiceNumber);
    int endParamId = getEndParamId(lane, voiceNumber);
    
    if (startParamId >= 0) {
      module->params[startParamId].setValue(state.startStep[lane]);
    }
    if (endParamId >= 0) {
      module->params[endParamId].setValue(state.endStep[lane]);
    }
    
    // Sync rotation
    int rotParamId = getRotationParamId(lane, voiceNumber);
    if (rotParamId >= 0) {
      module->params[rotParamId].setValue(state.rotation[lane]);
    }
  }
  
  void syncAllLanesToModule(const SandsVisualEditorV2::VoiceState& state, 
                            int voiceNumber) {
    for (int lane = 0; lane < 5; ++lane) {
      syncEditorToModule(state, voiceNumber, lane);
    }
  }
  
  // ===== PARAMETER SYNC (Module → Editor) =====
  
  void syncModuleToEditor(SandsVisualEditorV2::VoiceState& state,
                         int voiceNumber, int lane) {
    if (!module) return;
    
    // Sync 16 step values
    for (int step = 0; step < 16; ++step) {
      int paramId = getStepParamId(lane, voiceNumber, step);
      if (paramId >= 0) {
        state.values[step][lane] = module->params[paramId].getValue();
      }
    }
    
    // Sync start/end
    int startParamId = getStartParamId(lane, voiceNumber);
    int endParamId = getEndParamId(lane, voiceNumber);
    
    if (startParamId >= 0) {
      state.startStep[lane] = (int)module->params[startParamId].getValue();
    }
    if (endParamId >= 0) {
      state.endStep[lane] = (int)module->params[endParamId].getValue();
    }
    
    // Sync rotation
    int rotParamId = getRotationParamId(lane, voiceNumber);
    if (rotParamId >= 0) {
      state.rotation[lane] = (int)module->params[rotParamId].getValue();
    }
  }
  
  void syncAllLanesFromModule(SandsVisualEditorV2::VoiceState& state,
                              int voiceNumber) {
    for (int lane = 0; lane < 5; ++lane) {
      syncModuleToEditor(state, voiceNumber, lane);
    }
  }
  
  // ===== PARAMETER ID HELPERS =====
  
  int getStepParamId(int lane, int voice, int step) {
    // Override this per module based on Monsoon.hpp parameter layout
    // Example: POLY_DNA_VOICE_1_LEN + (voice-2)*3 + step
    // Return -1 if not available
    return -1;
  }
  
  int getStartParamId(int lane, int voice) {
    // Return parameter ID for start/offset
    // Return -1 if not available
    return -1;
  }
  
  int getEndParamId(int lane, int voice) {
    // Return parameter ID for end/length
    // Return -1 if not available
    return -1;
  }
  
  int getRotationParamId(int lane, int voice) {
    // Return parameter ID for rotation
    // Return -1 if not available
    return -1;
  }
};

/**
 * StraitsVisualEditorPanel
 * 
 * Complete visual editor with multi-voice tabs and module integration.
 * - 7 tabs (voices 2-8 for East, 9-16 for West)
 * - Real-time bidirectional parameter sync
 * - State persistence (save/load)
 * - Playback sync animation
 */

struct StraitsVisualEditorPanel : rack::Widget {
  static constexpr int NUM_VOICES = 7;  // Voices 2-8 (East) or 9-15 (West)
  static constexpr int TAB_WIDTH = 50.f;
  static constexpr int TAB_HEIGHT = 22.f;
  
  // Module reference
  rack::Module* module = nullptr;
  
  // Visual editors (one per voice)
  std::array<SandsVisualEditorV2*, NUM_VOICES> editors;
  
  // Parameter manager
  SandsParameterManager paramManager;
  
  // State
  int selectedVoice = 0;
  int currentPlayStep = -1;
  float lastSyncTime = 0.f;
  static constexpr float SYNC_INTERVAL = 0.016f;  // ~60Hz
  
  // Persistence
  json_t* savedState = nullptr;
  
  StraitsVisualEditorPanel(rack::Module* m) : module(m), paramManager(m) {
    box.size = rack::Vec(600, 300);
    
    // Create editor for each voice
    for (int v = 0; v < NUM_VOICES; ++v) {
      auto editor = new SandsVisualEditorV2();
      editor->box.pos = rack::Vec(10, 40);
      editor->box.size = rack::Vec(box.size.x - 20, box.size.y - 60);
      addChild(editor);
      editors[v] = editor;
      
      // Load initial state from module
      paramManager.syncAllLanesFromModule(editor->currentState, v);
    }
  }
  
  ~StraitsVisualEditorPanel() {
    if (savedState) json_decref(savedState);
  }
  
  // ===== DRAWING =====
  
  void draw(const rack::DrawArgs& args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, rack::color::hex("#141416"));
    nvgFill(args.vg);
    
    // Draw tabs
    drawTabs(args.vg);
    
    // Hide all editors, show selected
    for (int v = 0; v < NUM_VOICES; ++v) {
      editors[v]->visible = (v == selectedVoice);
    }
    
    // Delegate to widget drawing
    Widget::draw(args);
  }
  
  void drawTabs(NVGcontext* vg) {
    float startX = 10.f;
    float y = 5.f;
    
    for (int v = 0; v < NUM_VOICES; ++v) {
      bool isSelected = (v == selectedVoice);
      float x = startX + v * (TAB_WIDTH + 3.f);
      
      // Tab background
      nvgBeginPath(vg);
      nvgRect(vg, x, y, TAB_WIDTH, TAB_HEIGHT);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#26a69a") : 
                   rack::color::hex("#333333"));
      nvgFill(vg);
      
      // Tab border
      nvgBeginPath(vg);
      nvgRect(vg, x, y, TAB_WIDTH, TAB_HEIGHT);
      nvgStrokeColor(vg, rack::color::hex("#666666"));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
      
      // Tab label
      nvgFontSize(vg, 10.f);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#ffffff") : 
                   rack::color::hex("#888888"));
      
      char label[8];
      snprintf(label, sizeof(label), "V%d", v + 2);  // Voices 2-8
      nvgText(vg, x + TAB_WIDTH / 2.f, y + TAB_HEIGHT / 2.f, label, nullptr);
    }
  }
  
  // ===== INTERACTION =====
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      // Check if clicked on tab
      float startX = 10.f;
      for (int v = 0; v < NUM_VOICES; ++v) {
        float x = startX + v * (TAB_WIDTH + 3.f);
        rack::Rect tabRect(x, 5, TAB_WIDTH, TAB_HEIGHT);
        if (tabRect.contains(e.pos)) {
          selectVoice(v);
          e.consume(this);
          return;
        }
      }
    }
    
    Widget::onButton(e);
  }
  
  // ===== VOICE MANAGEMENT =====
  
  void selectVoice(int voiceNum) {
    if (voiceNum >= 0 && voiceNum < NUM_VOICES) {
      // Save current voice state before switching
      if (selectedVoice != voiceNum) {
        syncEditorToModule(selectedVoice);
      }
      
      selectedVoice = voiceNum;
      
      // Load selected voice state
      syncModuleToEditor(selectedVoice);
    }
  }
  
  // ===== SYNC: EDITOR → MODULE =====
  
  void syncEditorToModule(int voiceNum) {
    auto editor = editors[voiceNum];
    paramManager.syncAllLanesToModule(editor->currentState, voiceNum);
  }
  
  void syncAllEditorsToModule() {
    for (int v = 0; v < NUM_VOICES; ++v) {
      syncEditorToModule(v);
    }
  }
  
  // ===== SYNC: MODULE → EDITOR =====
  
  void syncModuleToEditor(int voiceNum) {
    auto editor = editors[voiceNum];
    paramManager.syncAllLanesFromModule(editor->currentState, voiceNum);
  }
  
  void syncAllEditorsFromModule() {
    for (int v = 0; v < NUM_VOICES; ++v) {
      syncModuleToEditor(v);
    }
  }
  
  // ===== PLAYBACK SYNC =====
  
  void setCurrentPlayStep(int step) {
    currentPlayStep = step;
    editors[selectedVoice]->setCurrentPlayStep(step);
  }
  
  // ===== STATE PERSISTENCE =====
  
  json_t* saveState() {
    json_t* root = json_object();
    
    // Save all voice states
    json_t* voices = json_array();
    for (int v = 0; v < NUM_VOICES; ++v) {
      json_t* voiceState = serializeVoiceState(editors[v]->currentState);
      json_array_append(voices, voiceState);
      json_decref(voiceState);
    }
    json_object_set_new(root, "voices", voices);
    
    // Save selected voice
    json_object_set_new(root, "selectedVoice", json_integer(selectedVoice));
    
    return root;
  }
  
  void loadState(json_t* root) {
    if (!root) return;
    
    // Load all voice states
    json_t* voices = json_object_get(root, "voices");
    if (json_is_array(voices)) {
      for (int v = 0; v < NUM_VOICES && v < json_array_size(voices); ++v) {
        json_t* voiceState = json_array_get(voices, v);
        if (voiceState) {
          deserializeVoiceState(voiceState, editors[v]->currentState);
        }
      }
    }
    
    // Load selected voice
    json_t* sel = json_object_get(root, "selectedVoice");
    if (json_is_integer(sel)) {
      selectVoice(json_integer_value(sel));
    }
  }
  
  // ===== SERIALIZATION HELPERS =====
  
  json_t* serializeVoiceState(const SandsVisualEditorV2::VoiceState& state) {
    json_t* obj = json_object();
    
    // Serialize step values (16×5 = 80 values)
    json_t* values = json_array();
    for (int s = 0; s < 16; ++s) {
      for (int l = 0; l < 5; ++l) {
        json_array_append_new(values, json_real(state.values[s][l]));
      }
    }
    json_object_set_new(obj, "values", values);
    
    // Serialize handles and rotation
    json_t* startSteps = json_array();
    json_t* endSteps = json_array();
    json_t* rotations = json_array();
    
    for (int l = 0; l < 5; ++l) {
      json_array_append_new(startSteps, json_integer(state.startStep[l]));
      json_array_append_new(endSteps, json_integer(state.endStep[l]));
      json_array_append_new(rotations, json_integer(state.rotation[l]));
    }
    
    json_object_set_new(obj, "startSteps", startSteps);
    json_object_set_new(obj, "endSteps", endSteps);
    json_object_set_new(obj, "rotations", rotations);
    
    return obj;
  }
  
  void deserializeVoiceState(json_t* obj, SandsVisualEditorV2::VoiceState& state) {
    // Deserialize step values
    json_t* values = json_object_get(obj, "values");
    if (json_is_array(values)) {
      int idx = 0;
      for (int s = 0; s < 16 && idx < json_array_size(values); ++s) {
        for (int l = 0; l < 5 && idx < json_array_size(values); ++l) {
          json_t* val = json_array_get(values, idx++);
          if (json_is_real(val) || json_is_integer(val)) {
            state.values[s][l] = json_number_value(val);
          }
        }
      }
    }
    
    // Deserialize handles and rotation
    json_t* startSteps = json_object_get(obj, "startSteps");
    json_t* endSteps = json_object_get(obj, "endSteps");
    json_t* rotations = json_object_get(obj, "rotations");
    
    if (json_is_array(startSteps) && json_is_array(endSteps) && json_is_array(rotations)) {
      for (int l = 0; l < 5 && l < json_array_size(startSteps); ++l) {
        state.startStep[l] = json_integer_value(json_array_get(startSteps, l));
        state.endStep[l] = json_integer_value(json_array_get(endSteps, l));
        state.rotation[l] = json_integer_value(json_array_get(rotations, l));
      }
    }
  }
  
  // ===== MODULE INTEGRATION =====
  
  void step() override {
    Widget::step();
    
    // Periodic sync from module (not every frame, but regularly)
    float now = rack::system::getTime();
    if (now - lastSyncTime > SYNC_INTERVAL) {
      // Only sync selected voice (others sync on tab switch)
      syncModuleToEditor(selectedVoice);
      lastSyncTime = now;
      
      // Sync editor changes back to module
      syncEditorToModule(selectedVoice);
    }
    
    // Update playback sync for selected voice
    if (module) {
      // This would call module->getCurrentStep() if available
      // setCurrentPlayStep(module->getCurrentStep());
    }
  }
};

}  // namespace redDot
