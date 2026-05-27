#pragma once
#include "../plugin.hpp"
#include "SandsVisualEditor.hpp"

namespace redDot {

/**
 * StraitsEastVisualWidget
 * 
 * Example implementation of SandsVisualEditor for Straits East expander.
 * Shows visual DNA editor with 7 voice tabs.
 * 
 * Integration:
 *   • Each tab corresponds to one voice (2-8)
 *   • Editor reads/writes to Monsoon DNA parameters
 *   • Real-time sync with playback
 */

struct StraitsEastVisualWidget : rack::Widget {
  static constexpr int NUM_VOICES = 7;  // Voices 2-8
  static constexpr int TAB_WIDTH = 60.f;
  
  std::array<redDot::SandsVisualEditor*, NUM_VOICES> editors;
  int selectedVoice = 0;
  
  StraitsEastVisualWidget() {
    box.size = rack::Vec(550, 300);
    
    // Create editor for each voice
    for (int v = 0; v < NUM_VOICES; ++v) {
      auto editor = new redDot::SandsVisualEditor();
      editor->box.pos = rack::Vec(0, 30);
      editor->box.size = rack::Vec(box.size.x, box.size.y - 40);
      addChild(editor);
      editors[v] = editor;
    }
  }
  
  void draw(const rack::DrawArgs& args) override {
    // Draw tabs
    drawTabs(args.vg);
    
    // Draw selected editor
    editors[selectedVoice]->visible = true;
    Widget::draw(args);
  }
  
  void drawTabs(NVGcontext* vg) {
    float tabX = 10.f;
    
    for (int v = 0; v < NUM_VOICES; ++v) {
      bool isSelected = (v == selectedVoice);
      float x = tabX + v * (TAB_WIDTH + 2.f);
      
      // Tab background
      nvgBeginPath(vg);
      nvgRect(vg, x, 5, TAB_WIDTH, 20);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#26a69a") : 
                   rack::color::hex("#333333"));
      nvgFill(vg);
      
      // Tab border
      nvgBeginPath(vg);
      nvgRect(vg, x, 5, TAB_WIDTH, 20);
      nvgStrokeColor(vg, rack::color::hex("#666666"));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
      
      // Tab label
      nvgFontSize(vg, 10.f);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, rack::color::hex("#ffffff"));
      char label[8];
      snprintf(label, sizeof(label), "V%d", v + 2);  // Voices 2-8
      nvgText(vg, x + TAB_WIDTH / 2.f, 15, label, nullptr);
    }
  }
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      // Check if clicked on tab
      float tabX = 10.f;
      for (int v = 0; v < NUM_VOICES; ++v) {
        float x = tabX + v * (TAB_WIDTH + 2.f);
        rack::Rect tabRect(x, 5, TAB_WIDTH, 20);
        if (tabRect.contains(e.pos)) {
          selectedVoice = v;
          e.consume(this);
          return;
        }
      }
    }
    
    Widget::onButton(e);
  }
  
  // Update editor from Monsoon parameters
  void syncFromModule(StraitsEastSands* module) {
    if (!module) return;
    
    auto editor = editors[selectedVoice];
    int voice = selectedVoice + 2;  // Voices 2-8
    
    // Sync DNA data from module
    // This would read from module->params and populate editor->voiceData
    // (Implementation depends on PatternEngine structure)
  }
  
  // Write editor changes to Monsoon parameters
  void syncToModule(StraitsEastSands* module) {
    if (!module) return;
    
    auto editor = editors[selectedVoice];
    int voice = selectedVoice + 2;  // Voices 2-8
    
    // Write DNA data to module
    // This would update module->params from editor->voiceData
    // (Implementation depends on PatternEngine structure)
  }
};

}  // namespace redDot
