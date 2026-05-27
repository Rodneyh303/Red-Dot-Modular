#pragma once
#include <rack.hpp>

namespace redDot {

/**
 * SpreadControlWidget
 * 
 * Compact spread control for visual editor lanes.
 * Can be displayed alongside visual editor to show/adjust spread per lane.
 * 
 * Usage:
 *   auto spreadCtrl = new SpreadControlWidget();
 *   spreadCtrl->setLabel("Spread");
 *   spreadCtrl->setValue(0.5);
 *   addChild(spreadCtrl);
 */

struct SpreadControlWidget : rack::Widget {
  float value = 0.0f;  // 0.0-1.0
  
  struct Style {
    NVGcolor backgroundColor = rack::color::hex("#141416");
    NVGcolor trackColor = rack::color::hex("#333333");
    NVGcolor fillColor = rack::color::hex("#26a69a");
    NVGcolor handleColor = rack::color::hex("#d4af37");
  } style;
  
  std::string label = "Spread";
  
  SpreadControlWidget() {
    box.size = rack::Vec(60, 50);
  }
  
  void setValue(float v) {
    value = rack::math::clamp(v, 0.0f, 1.0f);
  }
  
  float getValue() const { return value; }
  
  void setLabel(const std::string& s) { label = s; }
  
  void draw(const rack::DrawArgs& args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, style.backgroundColor);
    nvgFill(args.vg);
    
    // Label
    nvgFontSize(args.vg, 9.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(args.vg, rack::color::hex("#888888"));
    nvgText(args.vg, box.size.x / 2.f, 2.f, label.c_str(), nullptr);
    
    // Track
    float trackY = 18.f;
    float trackHeight = 4.f;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 5.f, trackY, box.size.x - 10.f, trackHeight);
    nvgFillColor(args.vg, style.trackColor);
    nvgFill(args.vg);
    
    // Fill (spread amount)
    float fillWidth = (box.size.x - 10.f) * value;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 5.f, trackY, fillWidth, trackHeight);
    nvgFillColor(args.vg, style.fillColor);
    nvgFill(args.vg);
    
    // Handle
    float handleX = 5.f + fillWidth - 2.f;
    float handleY = trackY + trackHeight / 2.f;
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, handleX, handleY, 3.f);
    nvgFillColor(args.vg, style.handleColor);
    nvgFill(args.vg);
    
    // Value text
    nvgFontSize(args.vg, 8.f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, rack::color::hex("#cccccc"));
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", value);
    nvgText(args.vg, box.size.x / 2.f, box.size.y - 8.f, buf, nullptr);
    
    // Border
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(args.vg, rack::color::hex("#2a2a2a"));
    nvgStrokeWidth(args.vg, 0.5f);
    nvgStroke(args.vg);
  }
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      setValueFromMouse(e.pos.x);
      e.consume(this);
    }
  }
  
  void onDragMove(const rack::event::DragMove& e) override {
    setValueFromMouse(e.pos.x);
  }
  
  void setValueFromMouse(float mouseX) {
    float normalized = (mouseX - 5.f) / (box.size.x - 10.f);
    setValue(normalized);
  }
};

/**
 * SpreadControlPanel
 * 
 * Panel showing spread controls for all lanes in visual editor.
 * Displays 3 (poly) or 6 (mono) spread controls side by side.
 * 
 * Usage:
 *   auto spreadPanel = new SpreadControlPanel(6);  // 6 lanes for mono
 *   addChild(spreadPanel);
 *   
 *   // Get spread values
 *   float restSpread = spreadPanel->getSpread(0);
 *   float melodySpread = spreadPanel->getSpread(1);
 *   // ...
 */

struct SpreadControlPanel : rack::Widget {
  std::vector<SpreadControlWidget*> controls;
  int numLanes = 6;
  
  static constexpr const char* MONO_LANE_NAMES[] = {
    "Rest", "Melody", "Octave", "Legato", "Accent", "Variation"
  };
  
  static constexpr const char* POLY_LANE_NAMES[] = {
    "Rest", "Melody", "Octave"
  };
  
  SpreadControlPanel(int nLanes = 6) : numLanes(nLanes) {
    box.size = rack::Vec(70 * nLanes, 60);
    
    const char** names = (nLanes == 3) ? POLY_LANE_NAMES : MONO_LANE_NAMES;
    
    for (int i = 0; i < nLanes; ++i) {
      auto ctrl = new SpreadControlWidget();
      ctrl->box.pos = rack::Vec(70 * i + 5, 5);
      ctrl->setLabel(names[i]);
      addChild(ctrl);
      controls.push_back(ctrl);
    }
  }
  
  void setSpread(int lane, float value) {
    if (lane >= 0 && lane < (int)controls.size()) {
      controls[lane]->setValue(value);
    }
  }
  
  float getSpread(int lane) const {
    if (lane >= 0 && lane < (int)controls.size()) {
      return controls[lane]->getValue();
    }
    return 0.0f;
  }
  
  void setAllSpreads(const std::vector<float>& spreads) {
    for (size_t i = 0; i < spreads.size() && i < controls.size(); ++i) {
      controls[i]->setValue(spreads[i]);
    }
  }
  
  std::vector<float> getAllSpreads() const {
    std::vector<float> spreads;
    for (auto* ctrl : controls) {
      spreads.push_back(ctrl->getValue());
    }
    return spreads;
  }
};

/**
 * Integration Example: Visual Editor with Spread Controls
 * 
 * Shows how to combine visual editor + spread controls + parameter manager.
 * 
 * struct MyExpanderWidget : rack::ModuleWidget {
 *   SandsVisualEditorV4* visualEditor;
 *   SpreadControlPanel* spreadPanel;
 *   ParameterManager* paramManager;
 *   
 *   MyExpanderWidget(MyExpander* mod) {
 *     // Create visual editor
 *     visualEditor = new SandsVisualEditorV4(Mode::MONO);
 *     visualEditor->box.pos = Vec(10, 60);
 *     addChild(visualEditor);
 *     
 *     // Create spread controls
 *     spreadPanel = new SpreadControlPanel(6);
 *     spreadPanel->box.pos = Vec(10, 310);  // Below editor
 *     addChild(spreadPanel);
 *     
 *     // Create manager
 *     paramManager = new MonoSandsParameterManager(patternEngine);
 *   }
 *   
 *   void step() override {
 *     // Get spread values from UI
 *     for (int l = 0; l < 6; ++l) {
 *       float spread = spreadPanel->getSpread(l);
 *       paramManager->setSpread(l, spread);  // If manager supports it
 *     }
 *     
 *     // Sync editor ↔ PatternEngine
 *     paramManager->syncPatternEngineToEditor(visualEditor->currentState);
 *   }
 * };
 */

}  // namespace redDot
