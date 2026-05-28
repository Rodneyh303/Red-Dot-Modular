#pragma once
#include <rack.hpp>

namespace redDot {

/**
 * TabButton
 * 
 * A simple tab button for voice selection in poly voice editors.
 * Shows voice number (V2, V3, etc.) with highlight when selected.
 * 
 * Usage:
 *   auto tab = new TabButton();
 *   tab->label = "V2";
 *   tab->voiceIdx = 0;
 *   tab->box.pos = Vec(10, 50);
 *   tab->box.size = Vec(50, 25);
 *   tab->onPressed = [this](int idx) { onVoiceTabPressed(idx); };
 *   addChild(tab);
 */

struct TabButton : rack::OpaqueWidget {
  std::string label = "V?";
  int voiceIdx = 0;
  bool isSelected = false;
  
  // Colors
  NVGcolor colorBackground = rack::color::hex("#2c2c2c");      // Dark gray
  NVGcolor colorSelected = rack::color::hex("#26a69a");         // Teal
  NVGcolor colorText = rack::color::hex("#ffffff");             // White
  NVGcolor colorBorder = rack::color::hex("#444444");           // Border
  
  std::function<void(int)> onPressed;
  
  void draw(const rack::DrawArgs& args) override {
    NVGcontext* vg = args.vg;
    
    // Background
    NVGcolor bgColor = isSelected ? colorSelected : colorBackground;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);
    
    // Border
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(vg, colorBorder);
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);
    
    // Text label
    nvgFontSize(vg, 10.f);
    nvgFontFace(vg, "sans-bold");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, colorText);
    nvgText(vg, box.size.x / 2.f, box.size.y / 2.f, label.c_str(), nullptr);
  }
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (onPressed) {
        onPressed(voiceIdx);
      }
      e.consume(this);
    }
  }
  
  // Optional: highlight effect on hover
  void onEnter(const rack::event::Enter& e) override {
    if (!isSelected) {
      colorBackground = rack::color::hex("#3c3c3c");  // Lighter on hover
    }
  }
  
  void onLeave(const rack::event::Leave& e) override {
    if (!isSelected) {
      colorBackground = rack::color::hex("#2c2c2c");  // Back to normal
    }
  }
};

/**
 * TabButtonGroup
 * 
 * Container for multiple tab buttons.
 * Manages selection state and spacing.
 */

struct TabButtonGroup : rack::Widget {
  std::vector<TabButton*> tabs;
  int selectedIdx = 0;
  
  float tabWidth  = 50.f;
  float tabHeight = 25.f;
  float spacing   =  2.f;
  
  TabButtonGroup(int numTabs, int startVoiceNumber = 2,
                 float tWidth = 50.f, float tHeight = 25.f, float tSpacing = 2.f) {
    tabWidth  = tWidth;
    tabHeight = tHeight;
    spacing   = tSpacing;

    box.size.y = tabHeight;
    box.size.x = numTabs * (tabWidth + spacing) - spacing;
    
    for (int i = 0; i < numTabs; ++i) {
      auto tab = new TabButton();
      tab->voiceIdx = i;
      tab->label = "V" + std::to_string(startVoiceNumber + i);
      tab->box.pos = Vec((tabWidth + spacing) * i, 0);
      tab->box.size = Vec(tabWidth, tabHeight);
      
      tab->onPressed = [this](int voiceIdx) {
        selectTab(voiceIdx);
      };
      
      addChild(tab);
      tabs.push_back(tab);
    }
    
    // Select first tab
    selectTab(0);
  }
  
  void selectTab(int idx) {
    if (idx < 0 || idx >= (int)tabs.size()) return;
    
    // Deselect previous
    if (selectedIdx >= 0 && selectedIdx < (int)tabs.size()) {
      tabs[selectedIdx]->isSelected = false;
    }
    
    // Select new
    selectedIdx = idx;
    tabs[selectedIdx]->isSelected = true;
  }
  
  int getSelectedTab() const {
    return selectedIdx;
  }
};

}  // namespace redDot
