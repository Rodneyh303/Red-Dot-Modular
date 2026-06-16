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
  bool isDisabled = false;   // greyed out: voice index >= active poly count
  
  // Colors
  NVGcolor colorBackground = nvgRGB(0x2c, 0x2c, 0x2c);      // Dark gray
  NVGcolor colorSelected = nvgRGB(0x26, 0xa6, 0x9a);         // Teal
  NVGcolor colorText = nvgRGB(0xff, 0xff, 0xff);             // White
  NVGcolor colorBorder = nvgRGB(0x44, 0x44, 0x44);           // Border
  NVGcolor colorDisabled = nvgRGB(0x1a, 0x1a, 0x1a);         // Dim (inactive)
  NVGcolor colorTextDim  = nvgRGB(0x55, 0x55, 0x55);         // Dim text
  
  std::function<void(int)> onPressed;
  
  void draw(const widget::Widget::DrawArgs& args) override {
    NVGcontext* vg = args.vg;
    
    // Background
    NVGcolor bgColor = isDisabled ? colorDisabled
                                  : (isSelected ? colorSelected : colorBackground);
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
    
    // Text label. Must use a LOADED font handle (nvgFontFaceId), not a face-name
    // string — "sans-bold" isn't registered with this nvg context, so nvgFontFace
    // silently rendered nothing (the tab numbers didn't appear).
    std::shared_ptr<rack::window::Font> font =
        APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
    if (!font) font = APP->window->uiFont;
    if (font) {
        nvgFontFaceId(vg, font->handle);
        nvgFontSize(vg, 10.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, isDisabled ? colorTextDim : colorText);
        nvgText(vg, box.size.x / 2.f, box.size.y / 2.f, label.c_str(), nullptr);
    }
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
      colorBackground = nvgRGB(0x3c, 0x3c, 0x3c);  // Lighter on hover
    }
  }
  
  void onLeave(const rack::event::Leave& e) override {
    if (!isSelected) {
      colorBackground = nvgRGB(0x2c, 0x2c, 0x2c);  // Back to normal
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
  
  float tabWidth = 50.f;
  float tabHeight = 25.f;
  float spacing = 2.f;
  
  TabButtonGroup(int numTabs, int startVoiceNumber = 2) {
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

  // Multi-row layout: tabs wrap across `numRows`, sized to fill `totalWidthPx`
  // and `totalHeightPx`. Use for large voice counts (e.g. 15) where a single
  // row would be too cramped. Build with TabButtonGroup(numTabs, start, rows, w, h).
  TabButtonGroup(int numTabs, int startVoiceNumber, int numRows,
                 float totalWidthPx, float totalHeightPx) {
    if (numRows < 1) numRows = 1;
    int perRow = (numTabs + numRows - 1) / numRows;   // ceil
    float tw = (totalWidthPx - spacing * (perRow - 1)) / perRow;
    float th = (totalHeightPx - spacing * (numRows - 1)) / numRows;
    tabWidth = tw; tabHeight = th;
    box.size.x = totalWidthPx;
    box.size.y = totalHeightPx;

    for (int i = 0; i < numTabs; ++i) {
      int row = i / perRow;
      int col = i % perRow;
      auto tab = new TabButton();
      tab->voiceIdx = i;
      tab->label = "V" + std::to_string(startVoiceNumber + i);
      tab->box.pos  = Vec(col * (tw + spacing), row * (th + spacing));
      tab->box.size = Vec(tw, th);
      tab->onPressed = [this](int voiceIdx) { selectTab(voiceIdx); };
      addChild(tab);
      tabs.push_back(tab);
    }
    selectTab(0);
  }
  
  void selectTab(int idx) {
    if (idx < 0 || idx >= (int)tabs.size()) return;
    if (tabs[idx]->isDisabled) return;   // can't select a greyed-out voice
    
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

  // Grey out tabs for voices beyond the active poly count. Disabled tabs are
  // dimmed and (in the widget's selection handling) should not be selectable.
  // activeCount = number of usable voices (e.g. numPolyVoices).
  void setActiveCount(int activeCount) {
    for (int i = 0; i < (int)tabs.size(); ++i)
      tabs[i]->isDisabled = (i >= activeCount);
    // If the current selection is now disabled, fall back to the last active.
    if (selectedIdx >= activeCount && activeCount > 0)
      selectTab(activeCount - 1);
  }
};

}  // namespace redDot
