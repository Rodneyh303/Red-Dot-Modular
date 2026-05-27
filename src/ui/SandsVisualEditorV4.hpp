#pragma once
#include <rack.hpp>
#include <array>
#include <deque>
#include <cstring>

namespace redDot {

/**
 * SandsVisualEditorV4 (Corrected)
 * 
 * Visual replacement for length/offset/rotation knobs.
 * 
 * Data: 16 probability values per lane (0.0-1.0)
 * Indexing: Use length/offset/rotation to select which 16 values get used
 * 
 * Mono: 6 lanes × 16 steps = 96 probability values
 * Poly: 3 lanes × 16 steps per-voice
 * 
 * Visual editor shows the 16 bars for probability editing.
 * Length/offset/rotation are just parameters for sequencer indexing.
 */

struct SandsVisualEditorV4 : rack::Widget {
  static constexpr int STEP_COUNT = 16;  // 16 probability values per lane
  
  enum Mode {
    MONO,
    POLY
  };
  
  enum Lane {
    REST = 0,
    MELODY = 1,
    OCTAVE = 2,
    LEGATO = 3,
    ACCENT = 4,
    VARIATION = 5
  };
  
  struct Colors {
    NVGcolor rest       = rack::color::hex("#505050");
    NVGcolor melody     = rack::color::hex("#d4af37");
    NVGcolor octave     = rack::color::hex("#b8860b");
    NVGcolor legato     = rack::color::hex("#26a69a");
    NVGcolor accent     = rack::color::hex("#ff9500");
    NVGcolor variation  = rack::color::hex("#ff6b6b");
    NVGcolor rotation   = rack::color::hex("#26a69a");
    NVGcolor handle     = rack::color::hex("#888888");
    NVGcolor background = rack::color::hex("#141416");
    NVGcolor border     = rack::color::hex("#2a2a2a");
    NVGcolor active     = rack::color::hex("#cc2222");
  };
  
  // Per-lane probability distribution
  struct ProbabilityLane {
    std::array<float, STEP_COUNT> probabilities;  // 16 values
    int length = STEP_COUNT;    // How many steps active
    int offset = 0;             // Window start
    int rotation = 0;           // Rotation within window
    
    // Get effective probability at sequencer step
    float getEffectiveProb(int step) const {
      if (step < 0 || step >= length) return 0.f;
      int idx = (offset + step + rotation) % STEP_COUNT;
      return probabilities[idx];
    }
    
    void setProbability(int step, float value) {
      if (step >= 0 && step < STEP_COUNT) {
        probabilities[step] = rack::math::clamp(value, 0.f, 1.f);
      }
    }
  };
  
  struct VoiceState {
    std::array<ProbabilityLane, 6> lanes;
    
    bool operator==(const VoiceState& other) const {
      for (int l = 0; l < 6; ++l) {
        for (int s = 0; s < STEP_COUNT; ++s) {
          if (lanes[l].probabilities[s] != other.lanes[l].probabilities[s]) return false;
        }
        if (lanes[l].length != other.lanes[l].length ||
            lanes[l].offset != other.lanes[l].offset ||
            lanes[l].rotation != other.lanes[l].rotation) return false;
      }
      return true;
    }
  };
  
  Mode mode = POLY;
  int laneCount = 3;
  
  VoiceState currentState;
  VoiceState clipboard;
  Colors colors;
  
  std::deque<VoiceState> undoHistory;
  std::deque<VoiceState> redoHistory;
  static constexpr int UNDO_HISTORY_SIZE = 50;
  
  struct PresetSeed {
    char name[32];
    VoiceState state;
  };
  static constexpr int PRESET_BANK_SIZE = 8;
  std::array<PresetSeed, PRESET_BANK_SIZE> presetBank;
  int selectedPreset = 0;
  
  struct DragState {
    bool isDragging = false;
    bool isDraggingBar = false;
    int dragLane = 0;
    int dragStep = 0;
  } dragState;
  
  struct KeyboardState {
    int selectedLane = 0;
    bool showPresetPanel = false;
  } kbState;
  
  int currentPlayStep = -1;
  float activeStepAlpha = 0.f;
  
  struct Layout {
    float laneHeight = 30.f;
    float stepWidth = 30.f;
    float handleWidth = 12.f;
    float padding = 40.f;
    float topPadding = 35.f;
    
    float getLaneY(int lane) const { return topPadding + lane * laneHeight; }
    float getStepX(int step) const { return padding + step * stepWidth; }
    float getStepCenterX(int step) const { return getStepX(step) + stepWidth / 2.f; }
    float getLaneCenterY(int lane) const { return getLaneY(lane) + laneHeight / 2.f; }
    
    rack::Rect getLaneRect(int lane) const {
      return rack::Rect(0, getLaneY(lane), 550, laneHeight);
    }
    
    rack::Rect getStepRect(int lane, int step) const {
      return rack::Rect(getStepX(step), getLaneY(lane), stepWidth, laneHeight);
    }
  } layout;
  
  SandsVisualEditorV4(Mode m = POLY) : mode(m) {
    setMode(m);
    box.size = rack::Vec(550, 250);
    resetState();
    initializePresets();
  }
  
  void setMode(Mode m) {
    mode = m;
    laneCount = (mode == MONO) ? 6 : 3;
    box.size.y = 35.f + (laneCount * 30.f) + 40.f;
  }
  
  void resetState() {
    for (int l = 0; l < 6; ++l) {
      for (int s = 0; s < STEP_COUNT; ++s) {
        currentState.lanes[l].probabilities[s] = 0.5f;
      }
      currentState.lanes[l].length = STEP_COUNT;
      currentState.lanes[l].offset = 0;
      currentState.lanes[l].rotation = 0;
    }
  }
  
  void saveToHistory() {
    if (undoHistory.empty() || !(undoHistory.back() == currentState)) {
      undoHistory.push_back(currentState);
      if (undoHistory.size() > UNDO_HISTORY_SIZE) undoHistory.pop_front();
      redoHistory.clear();
    }
  }
  
  void undo() {
    if (!undoHistory.empty()) {
      redoHistory.push_back(currentState);
      currentState = undoHistory.back();
      undoHistory.pop_back();
    }
  }
  
  void redo() {
    if (!redoHistory.empty()) {
      undoHistory.push_back(currentState);
      currentState = redoHistory.back();
      redoHistory.pop_back();
    }
  }
  
  void randomizeLane(int lane) {
    saveToHistory();
    for (int s = 0; s < STEP_COUNT; ++s) {
      currentState.lanes[lane].probabilities[s] = rack::random::uniform();
    }
  }
  
  void randomizeAll() {
    saveToHistory();
    for (int l = 0; l < laneCount; ++l) {
      for (int s = 0; s < STEP_COUNT; ++s) {
        currentState.lanes[l].probabilities[s] = rack::random::uniform();
      }
    }
  }
  
  void reverseLane(int lane) {
    saveToHistory();
    std::array<float, STEP_COUNT> temp = currentState.lanes[lane].probabilities;
    for (int s = 0; s < STEP_COUNT; ++s) {
      currentState.lanes[lane].probabilities[s] = temp[STEP_COUNT - 1 - s];
    }
  }
  
  void copyLane(int lane) {
    clipboard = currentState;
  }
  
  void pasteLane(int lane) {
    saveToHistory();
    currentState.lanes[lane] = clipboard.lanes[lane];
  }
  
  void initializePresets() {
    snprintf(presetBank[0].name, sizeof(presetBank[0].name), "Flat");
    for (int l = 0; l < laneCount; ++l) {
      for (int s = 0; s < STEP_COUNT; ++s) {
        presetBank[0].state.lanes[l].probabilities[s] = 0.5f;
      }
    }
    
    snprintf(presetBank[1].name, sizeof(presetBank[1].name), "Peak");
    for (int l = 0; l < laneCount; ++l) {
      for (int s = 0; s < STEP_COUNT; ++s) {
        float center = STEP_COUNT / 2.f;
        float dist = fabsf(s - center) / center;
        presetBank[1].state.lanes[l].probabilities[s] = 1.f - dist;
      }
    }
    
    snprintf(presetBank[2].name, sizeof(presetBank[2].name), "Ramp");
    for (int l = 0; l < laneCount; ++l) {
      for (int s = 0; s < STEP_COUNT; ++s) {
        presetBank[2].state.lanes[l].probabilities[s] = s / (float)STEP_COUNT;
      }
    }
    
    snprintf(presetBank[3].name, sizeof(presetBank[3].name), "Random");
    for (int l = 0; l < laneCount; ++l) {
      for (int s = 0; s < STEP_COUNT; ++s) {
        presetBank[3].state.lanes[l].probabilities[s] = rack::random::uniform();
      }
    }
    
    for (int p = 4; p < PRESET_BANK_SIZE; ++p) {
      snprintf(presetBank[p].name, sizeof(presetBank[p].name), "Custom%d", p - 3);
      presetBank[p].state = currentState;
    }
  }
  
  void loadPreset(int idx) {
    if (idx >= 0 && idx < PRESET_BANK_SIZE) {
      saveToHistory();
      currentState = presetBank[idx].state;
      selectedPreset = idx;
    }
  }
  
  void draw(const rack::DrawArgs& args) override {
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, colors.background);
    nvgFill(args.vg);
    
    drawControlBar(args.vg);
    
    for (int lane = 0; lane < laneCount; ++lane) {
      drawLaneLabel(args.vg, lane);
      for (int step = 0; step < STEP_COUNT; ++step) {
        drawStep(args.vg, lane, step);
      }
      drawHandles(args.vg, lane);
      drawRotationIndicator(args.vg, lane);
    }
    
    if (currentPlayStep >= 0) drawActiveStepIndicator(args.vg);
    if (kbState.showPresetPanel) drawPresetPanel(args.vg);
    
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(args.vg, colors.border);
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);
  }
  
  void drawControlBar(NVGcontext* vg) {
    float y = 8.f;
    float x = 10.f;
    
    nvgFontSize(vg, 9.f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, rack::color::hex("#888888"));
    
    const char* modeStr = (mode == MONO) ? "MONO" : "POLY";
    nvgText(vg, x, y, modeStr, nullptr);
    
    std::string undoText = "U:" + std::to_string(undoHistory.size());
    nvgText(vg, x + 50, y, undoText.c_str(), nullptr);
  }
  
  void drawLaneLabel(NVGcontext* vg, int lane) {
    static const char* monoNames[] = {"REST", "MELODY", "OCTAVE", "LEGATO", "ACCENT", "VARIATION"};
    static const char* polyNames[] = {"REST", "MELODY", "OCTAVE"};
    
    const char* name = (mode == MONO) ? monoNames[lane] : polyNames[lane];
    float y = layout.getLaneCenterY(lane);
    
    nvgFontSize(vg, 10.f);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, (lane == kbState.selectedLane) ? getLaneColor(lane) : rack::color::hex("#666666"));
    nvgText(vg, layout.padding - 8, y, name, nullptr);
  }
  
  void drawStep(NVGcontext* vg, int lane, int step) {
    rack::Rect rect = layout.getStepRect(lane, step);
    float prob = currentState.lanes[lane].probabilities[step];
    float barHeight = prob * rect.size.y;
    
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgFillColor(vg, colors.background);
    nvgFill(vg);
    
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x + 1, rect.pos.y + (rect.size.y - barHeight), rect.size.x - 2, barHeight);
    nvgFillColor(vg, getLaneColor(lane));
    nvgFill(vg);
    
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgStrokeColor(vg, rack::color::hex("#333333"));
    nvgStrokeWidth(vg, 0.5f);
    nvgStroke(vg);
  }
  
  void drawHandles(NVGcontext* vg, int lane) {
    const ProbabilityLane& probLane = currentState.lanes[lane];
    
    float leftX = layout.getStepCenterX(0);
    float rightX = layout.getStepCenterX(probLane.length - 1);
    float centerY = layout.getLaneCenterY(lane);
    
    nvgBeginPath(vg);
    nvgCircle(vg, leftX, centerY, layout.handleWidth / 2.f);
    nvgFillColor(vg, colors.handle);
    nvgFill(vg);
    
    nvgBeginPath(vg);
    nvgCircle(vg, rightX, centerY, layout.handleWidth / 2.f);
    nvgFillColor(vg, colors.handle);
    nvgFill(vg);
  }
  
  void drawRotationIndicator(NVGcontext* vg, int lane) {
    const ProbabilityLane& probLane = currentState.lanes[lane];
    
    if (probLane.rotation >= 0 && probLane.rotation < probLane.length) {
      rack::Rect rect = layout.getStepRect(lane, probLane.rotation);
      
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgFillColor(vg, colors.rotation);
      nvgFill(vg);
      
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgStrokeColor(vg, rack::color::hex("#35c7b8"));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
    }
  }
  
  void drawActiveStepIndicator(NVGcontext* vg) {
    float x = layout.getStepCenterX(currentPlayStep);
    float y1 = layout.topPadding - 5.f;
    float y2 = layout.getLaneY(laneCount) + layout.laneHeight + 5.f;
    
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y1);
    nvgLineTo(vg, x, y2);
    nvgStrokeColor(vg, colors.active);
    nvgStrokeWidth(vg, 2.f);
    nvgGlobalAlpha(vg, activeStepAlpha);
    nvgStroke(vg);
    nvgGlobalAlpha(vg, 1.f);
  }
  
  void drawPresetPanel(NVGcontext* vg) {
    float panelX = layout.padding;
    float panelY = 70.f;
    float panelW = 300.f;
    float panelH = 140.f;
    
    nvgBeginPath(vg);
    nvgRect(vg, panelX, panelY, panelW, panelH);
    nvgFillColor(vg, rack::color::hex("#1a1a1a"));
    nvgFill(vg);
    
    nvgFontSize(vg, 11.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, rack::color::hex("#d4af37"));
    nvgText(vg, panelX + panelW / 2.f, panelY + 5, "PRESET SEEDS", nullptr);
    
    float btnW = 70.f;
    float btnH = 20.f;
    
    for (int p = 0; p < PRESET_BANK_SIZE; ++p) {
      int row = p / 4;
      int col = p % 4;
      float bx = panelX + 8.f + col * 78.f;
      float by = panelY + 22.f + row * 28.f;
      
      bool isSelected = (p == selectedPreset);
      nvgBeginPath(vg);
      nvgRect(vg, bx, by, btnW, btnH);
      nvgFillColor(vg, isSelected ? rack::color::hex("#26a69a") : rack::color::hex("#333333"));
      nvgFill(vg);
      
      nvgFontSize(vg, 8.f);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, rack::color::hex("#888888"));
      nvgText(vg, bx + btnW / 2.f, by + btnH / 2.f, presetBank[p].name, nullptr);
    }
  }
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      int lane = getLaneAtY(e.pos.y);
      int step = getStepAtX(e.pos.x);
      
      if (lane >= 0 && lane < laneCount && step >= 0 && step < STEP_COUNT) {
        dragState.isDraggingBar = true;
        dragState.dragLane = lane;
        dragState.dragStep = step;
        dragState.isDragging = true;
        setBarValue(lane, step, e.pos.y);
        e.consume(this);
      }
    } else if (e.action == GLFW_RELEASE) {
      if (dragState.isDragging) saveToHistory();
      dragState.isDragging = false;
      dragState.isDraggingBar = false;
    }
  }
  
  void onDragMove(const rack::event::DragMove& e) override {
    if (!dragState.isDragging || !dragState.isDraggingBar) return;
    int step = getStepAtX(e.pos.x);
    if (step >= 0 && step < STEP_COUNT) setBarValue(dragState.dragLane, step, e.pos.y);
  }
  
  void onKey(const rack::event::Key& e) override {
    if (e.action != GLFW_PRESS) return;
    
    if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL && e.key == GLFW_KEY_Z) {
      undo();
      e.consume(this);
      return;
    }
    
    if (e.key >= GLFW_KEY_1 && e.key <= GLFW_KEY_6) {
      int lane = e.key - GLFW_KEY_1;
      if (lane < laneCount) {
        kbState.selectedLane = lane;
        e.consume(this);
        return;
      }
    }
    
    if (e.key == GLFW_KEY_R) {
      if ((e.mods & RACK_MOD_MASK) == 0) randomizeLane(kbState.selectedLane);
      else if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) randomizeAll();
      e.consume(this);
      return;
    }
    
    if (e.key == GLFW_KEY_P) {
      kbState.showPresetPanel = !kbState.showPresetPanel;
      e.consume(this);
      return;
    }
    
    if (e.key == GLFW_KEY_C && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
      copyLane(kbState.selectedLane);
      e.consume(this);
      return;
    }
    
    if (e.key == GLFW_KEY_V) {
      if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) pasteLane(kbState.selectedLane);
      else if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) reverseLane(kbState.selectedLane);
      e.consume(this);
      return;
    }
  }
  
  NVGcolor getLaneColor(int lane) const {
    switch (lane) {
      case REST: return colors.rest;
      case MELODY: return colors.melody;
      case OCTAVE: return colors.octave;
      case LEGATO: return colors.legato;
      case ACCENT: return colors.accent;
      case VARIATION: return colors.variation;
      default: return colors.rest;
    }
  }
  
  int getLaneAtY(float y) const {
    for (int lane = 0; lane < laneCount; ++lane) {
      rack::Rect rect = layout.getLaneRect(lane);
      if (y >= rect.pos.y && y < rect.pos.y + rect.size.y) return lane;
    }
    return -1;
  }
  
  int getStepAtX(float x) const {
    if (x < layout.padding) return -1;
    int step = (int)((x - layout.padding) / layout.stepWidth);
    return (step >= 0 && step < STEP_COUNT) ? step : -1;
  }
  
  void setBarValue(int lane, int step, float mouseY) {
    rack::Rect rect = layout.getStepRect(lane, step);
    float relY = mouseY - rect.pos.y;
    float value = 1.f - (relY / rect.size.y);
    value = rack::math::clamp(value, 0.f, 1.f);
    currentState.lanes[lane].setProbability(step, value);
  }
  
  void step() override {
    Widget::step();
    if (currentPlayStep >= 0) {
      activeStepAlpha = 0.3f + 0.4f * sinf(rack::system::getTime() * 6.f);
    } else {
      activeStepAlpha = 0.f;
    }
  }
};

}
