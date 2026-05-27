#pragma once
#include <rack.hpp>
#include <array>
#include <deque>
#include <cstring>

namespace redDot {

/**
 * SandsVisualEditorV2
 * 
 * Phase 2 enhancements to SandsVisualEditor:
 * - Keyboard shortcuts (arrow keys, number input, Ctrl+Z undo)
 * - Undo/redo with history
 * - Preset seeds (randomize + bank of patterns)
 * - Active step animation (playback sync)
 * - Copy/paste between lanes
 */

struct SandsVisualEditorV2 : rack::Widget {
  static constexpr int STEP_COUNT = 16;
  static constexpr int LANE_COUNT = 5;
  static constexpr int UNDO_HISTORY_SIZE = 50;
  static constexpr int PRESET_BANK_SIZE = 8;
  
  enum Lane {
    REST = 0,
    MELODY = 1,
    OCTAVE = 2,
    LEGATO = 3,
    VARIATION = 4
  };
  
  struct Colors {
    NVGcolor rest       = rack::color::hex("#505050");
    NVGcolor melody     = rack::color::hex("#d4af37");
    NVGcolor octave     = rack::color::hex("#b8860b");
    NVGcolor legato     = rack::color::hex("#26a69a");
    NVGcolor variation  = rack::color::hex("#ff6b6b");
    NVGcolor rotation   = rack::color::hex("#26a69a");
    NVGcolor handle     = rack::color::hex("#888888");
    NVGcolor background = rack::color::hex("#141416");
    NVGcolor border     = rack::color::hex("#2a2a2a");
    NVGcolor active     = rack::color::hex("#cc2222");  // Active step line
  };
  
  // Complete voice state (for undo/redo)
  struct VoiceState {
    std::array<float, LANE_COUNT> values[STEP_COUNT];
    std::array<int, LANE_COUNT> startStep;
    std::array<int, LANE_COUNT> endStep;
    std::array<int, LANE_COUNT> rotation;
    
    bool operator==(const VoiceState& other) const {
      for (int s = 0; s < STEP_COUNT; ++s) {
        for (int l = 0; l < LANE_COUNT; ++l) {
          if (values[s][l] != other.values[s][l]) return false;
        }
      }
      for (int l = 0; l < LANE_COUNT; ++l) {
        if (startStep[l] != other.startStep[l] || 
            endStep[l] != other.endStep[l] ||
            rotation[l] != other.rotation[l]) return false;
      }
      return true;
    }
  };
  
  // Preset seed pattern
  struct PresetSeed {
    char name[32];
    VoiceState state;
  };
  
  VoiceState currentState;
  VoiceState clipboard;  // For copy/paste
  Colors colors;
  
  // Undo/redo history
  std::deque<VoiceState> undoHistory;
  std::deque<VoiceState> redoHistory;
  
  // Preset bank
  std::array<PresetSeed, PRESET_BANK_SIZE> presetBank;
  int selectedPreset = 0;
  
  // Interaction state
  struct DragState {
    bool isDragging = false;
    bool isDraggingBar = false;
    bool isDraggingLeftHandle = false;
    bool isDraggingRightHandle = false;
    bool isDraggingRotation = false;
    
    int dragLane = 0;
    int dragStep = 0;
  } dragState;
  
  // Keyboard/UI state
  struct KeyboardState {
    bool isSelectingRange = false;
    int selectionStart = 0;
    int selectionEnd = 0;
    int selectedLane = 0;
    bool showPresetPanel = false;
    bool showNumberInput = false;
    char numberInputBuffer[8];
    int numberInputLen = 0;
  } kbState;
  
  // Animation
  int currentPlayStep = -1;
  float activeStepAlpha = 0.f;
  
  // Layout
  struct Layout {
    float laneHeight = 30.f;
    float stepWidth = 15.f;
    float handleWidth = 12.f;
    float padding = 40.f;  // Larger for controls
    float topPadding = 35.f;
    
    float getLaneY(int lane) const { return topPadding + lane * laneHeight; }
    float getStepX(int step) const { return padding + step * stepWidth; }
    float getStepCenterX(int step) const { return getStepX(step) + stepWidth / 2.f; }
    float getLaneCenterY(int lane) const { return getLaneY(lane) + laneHeight / 2.f; }
    
    rack::Rect getLaneRect(int lane) const {
      return rack::Rect(0, getLaneY(lane), 500, laneHeight);
    }
    
    rack::Rect getStepRect(int lane, int step) const {
      return rack::Rect(getStepX(step), getLaneY(lane), stepWidth, laneHeight);
    }
  } layout;
  
  SandsVisualEditorV2() {
    box.size = rack::Vec(500, 250);
    
    // Initialize state
    resetState();
    
    // Initialize preset bank with default patterns
    initializePresets();
  }
  
  // ===== STATE MANAGEMENT =====
  
  void resetState() {
    for (int v = 0; v < STEP_COUNT; ++v) {
      for (int l = 0; l < LANE_COUNT; ++l) {
        currentState.values[v][l] = 0.5f;
      }
    }
    
    for (int l = 0; l < LANE_COUNT; ++l) {
      currentState.startStep[l] = 0;
      currentState.endStep[l] = STEP_COUNT;
      currentState.rotation[l] = 0;
    }
    
    undoHistory.clear();
    redoHistory.clear();
  }
  
  void saveToHistory() {
    // Only save if state changed
    if (undoHistory.empty() || !(undoHistory.back() == currentState)) {
      undoHistory.push_back(currentState);
      if (undoHistory.size() > UNDO_HISTORY_SIZE) {
        undoHistory.pop_front();
      }
      redoHistory.clear();  // Clear redo on new action
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
  
  // ===== PRESETS =====
  
  void initializePresets() {
    // Preset 1: Even distribution
    snprintf(presetBank[0].name, sizeof(presetBank[0].name), "Even");
    for (int s = 0; s < STEP_COUNT; ++s) {
      for (int l = 0; l < LANE_COUNT; ++l) {
        presetBank[0].state.values[s][l] = 0.5f;
      }
    }
    
    // Preset 2: Euclidean rhythm (Rest)
    snprintf(presetBank[1].name, sizeof(presetBank[1].name), "Euclidean");
    for (int s = 0; s < STEP_COUNT; ++s) {
      presetBank[1].state.values[s][REST] = (s % 2 == 0) ? 1.f : 0.f;
      for (int l = 1; l < LANE_COUNT; ++l) {
        presetBank[1].state.values[s][l] = 0.5f;
      }
    }
    
    // Preset 3: Ramp up
    snprintf(presetBank[2].name, sizeof(presetBank[2].name), "Ramp");
    for (int s = 0; s < STEP_COUNT; ++s) {
      float val = s / (float)(STEP_COUNT - 1);
      for (int l = 0; l < LANE_COUNT; ++l) {
        presetBank[2].state.values[s][l] = val;
      }
    }
    
    // Preset 4: Sine wave
    snprintf(presetBank[3].name, sizeof(presetBank[3].name), "Sine");
    for (int s = 0; s < STEP_COUNT; ++s) {
      float angle = (s / (float)STEP_COUNT) * 2.f * M_PI;
      float val = 0.5f + 0.5f * sinf(angle);
      for (int l = 0; l < LANE_COUNT; ++l) {
        presetBank[3].state.values[s][l] = val;
      }
    }
    
    // Presets 5-7: User custom (empty)
    for (int p = 4; p < PRESET_BANK_SIZE; ++p) {
      snprintf(presetBank[p].name, sizeof(presetBank[p].name), "Custom%d", p - 3);
      presetBank[p].state = currentState;
    }
  }
  
  void loadPreset(int presetIdx) {
    if (presetIdx >= 0 && presetIdx < PRESET_BANK_SIZE) {
      saveToHistory();
      currentState = presetBank[presetIdx].state;
      selectedPreset = presetIdx;
    }
  }
  
  void savePreset(int presetIdx) {
    if (presetIdx >= 0 && presetIdx < PRESET_BANK_SIZE) {
      presetBank[presetIdx].state = currentState;
    }
  }
  
  void randomizeAll() {
    saveToHistory();
    for (int s = 0; s < STEP_COUNT; ++s) {
      for (int l = 0; l < LANE_COUNT; ++l) {
        currentState.values[s][l] = rack::random::uniform();
      }
    }
  }
  
  void randomizeLane(int lane) {
    saveToHistory();
    for (int s = 0; s < STEP_COUNT; ++s) {
      currentState.values[s][lane] = rack::random::uniform();
    }
  }
  
  // ===== COPY/PASTE =====
  
  void copyLane(int lane) {
    clipboard = currentState;
  }
  
  void pasteLane(int lane) {
    saveToHistory();
    for (int s = 0; s < STEP_COUNT; ++s) {
      currentState.values[s][lane] = clipboard.values[s][lane];
    }
  }
  
  void reverseLane(int lane) {
    saveToHistory();
    std::array<float, STEP_COUNT> temp;
    for (int s = 0; s < STEP_COUNT; ++s) {
      temp[s] = currentState.values[s][lane];
    }
    for (int s = 0; s < STEP_COUNT; ++s) {
      currentState.values[s][lane] = temp[STEP_COUNT - 1 - s];
    }
  }
  
  // ===== DRAWING =====
  
  void draw(const rack::DrawArgs& args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, colors.background);
    nvgFill(args.vg);
    
    // Draw control bar at top
    drawControlBar(args.vg);
    
    // Draw each lane
    for (int lane = 0; lane < LANE_COUNT; ++lane) {
      drawLaneLabel(args.vg, lane);
      drawLaneBars(args.vg, lane);
      drawLengthHandles(args.vg, lane);
      drawRotationBlock(args.vg, lane);
    }
    
    // Draw active step indicator (playback sync)
    if (currentPlayStep >= 0 && currentPlayStep < STEP_COUNT) {
      drawActiveStepIndicator(args.vg);
    }
    
    // Draw preset panel if visible
    if (kbState.showPresetPanel) {
      drawPresetPanel(args.vg);
    }
    
    // Border
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(args.vg, colors.border);
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);
  }
  
  void drawControlBar(NVGcontext* vg) {
    float y = 8.f;
    float spacing = 60.f;
    float x = 10.f;
    
    // Undo/Redo indicators
    nvgFontSize(vg, 9.f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    
    std::string undoText = "U:" + std::to_string(undoHistory.size());
    nvgFillColor(vg, undoHistory.empty() ? 
                 rack::color::hex("#444444") : rack::color::hex("#888888"));
    nvgText(vg, x, y, undoText.c_str(), nullptr);
    
    std::string redoText = "R:" + std::to_string(redoHistory.size());
    nvgFillColor(vg, redoHistory.empty() ? 
                 rack::color::hex("#444444") : rack::color::hex("#888888"));
    nvgText(vg, x + spacing, y, redoText.c_str(), nullptr);
    
    // Selected lane indicator
    std::string laneText = "Lane:" + getLaneName(kbState.selectedLane);
    nvgFillColor(vg, colors.background);
    nvgFillColor(vg, getLaneColor(kbState.selectedLane));
    nvgText(vg, x + spacing * 2, y, laneText.c_str(), nullptr);
    
    // Preset indicator
    std::string presetText = "Preset:" + std::string(presetBank[selectedPreset].name);
    nvgFillColor(vg, rack::color::hex("#888888"));
    nvgText(vg, x + spacing * 3.5, y, presetText.c_str(), nullptr);
  }
  
  void drawPresetPanel(NVGcontext* vg) {
    float panelX = layout.padding;
    float panelY = 70.f;
    float panelW = 300.f;
    float panelH = 140.f;
    
    // Panel background
    nvgBeginPath(vg);
    nvgRect(vg, panelX, panelY, panelW, panelH);
    nvgFillColor(vg, rack::color::hex("#1a1a1a"));
    nvgFill(vg);
    
    // Panel border
    nvgBeginPath(vg);
    nvgRect(vg, panelX, panelY, panelW, panelH);
    nvgStrokeColor(vg, colors.border);
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);
    
    // Title
    nvgFontSize(vg, 11.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, rack::color::hex("#d4af37"));
    nvgText(vg, panelX + panelW / 2.f, panelY + 5, "PRESET SEEDS", nullptr);
    
    // Preset buttons (2 rows × 4)
    float btnW = 70.f;
    float btnH = 20.f;
    float btnSpaceX = 78.f;
    float btnSpaceY = 28.f;
    
    for (int p = 0; p < PRESET_BANK_SIZE; ++p) {
      int row = p / 4;
      int col = p % 4;
      float bx = panelX + 8.f + col * btnSpaceX;
      float by = panelY + 22.f + row * btnSpaceY;
      
      // Button background
      bool isSelected = (p == selectedPreset);
      nvgBeginPath(vg);
      nvgRect(vg, bx, by, btnW, btnH);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#26a69a") : 
                   rack::color::hex("#333333"));
      nvgFill(vg);
      
      // Button border
      nvgBeginPath(vg);
      nvgRect(vg, bx, by, btnW, btnH);
      nvgStrokeColor(vg, colors.border);
      nvgStrokeWidth(vg, 0.5f);
      nvgStroke(vg);
      
      // Button label
      nvgFontSize(vg, 8.f);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#ffffff") : 
                   rack::color::hex("#888888"));
      nvgText(vg, bx + btnW / 2.f, by + btnH / 2.f, 
              presetBank[p].name, nullptr);
    }
  }
  
  void drawActiveStepIndicator(NVGcontext* vg) {
    float x = layout.getStepCenterX(currentPlayStep);
    float y1 = layout.topPadding - 5.f;
    float y2 = layout.getLaneY(LANE_COUNT) + layout.laneHeight + 5.f;
    
    // Animated glow
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y1);
    nvgLineTo(vg, x, y2);
    nvgStrokeColor(vg, colors.active);
    nvgStrokeWidth(vg, 2.f);
    nvgGlobalAlpha(vg, activeStepAlpha);
    nvgStroke(vg);
    nvgGlobalAlpha(vg, 1.f);
  }
  
  void drawLaneLabel(NVGcontext* vg, int lane) {
    float y = layout.getLaneCenterY(lane);
    nvgFontSize(vg, 10.f);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    
    NVGcolor color = (lane == kbState.selectedLane) ?
                     getLaneColor(lane) : 
                     rack::color::hex("#666666");
    
    nvgFillColor(vg, color);
    nvgText(vg, layout.padding - 8, y, getLaneName(lane).c_str(), nullptr);
  }
  
  void drawLaneBars(NVGcontext* vg, int lane) {
    int start = currentState.startStep[lane];
    int end = currentState.endStep[lane];
    
    for (int step = 0; step < STEP_COUNT; ++step) {
      drawProbabilityBar(vg, lane, step, step >= start && step < end);
    }
  }
  
  void drawProbabilityBar(NVGcontext* vg, int lane, int step, bool isActive) {
    rack::Rect rect = layout.getStepRect(lane, step);
    float value = currentState.values[step][lane];
    float barHeight = value * rect.size.y;
    
    NVGcolor color = getLaneColor(lane);
    if (!isActive) {
      color.a = 0.3f;
    }
    
    // Background
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgFillColor(vg, colors.background);
    nvgFill(vg);
    
    // Value bar
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x + 1, rect.pos.y + (rect.size.y - barHeight), 
            rect.size.x - 2, barHeight);
    nvgFillColor(vg, color);
    nvgFill(vg);
    
    // Border
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgStrokeColor(vg, rack::color::hex("#333333"));
    nvgStrokeWidth(vg, 0.5f);
    nvgStroke(vg);
  }
  
  void drawLengthHandles(NVGcontext* vg, int lane) {
    int start = currentState.startStep[lane];
    int end = currentState.endStep[lane];
    
    float startX = layout.getStepCenterX(start);
    float endX = layout.getStepCenterX(end - 1);
    float centerY = layout.getLaneCenterY(lane);
    
    // Left handle
    nvgBeginPath(vg);
    nvgCircle(vg, startX, centerY, layout.handleWidth / 2.f);
    nvgFillColor(vg, colors.handle);
    nvgFill(vg);
    
    nvgBeginPath(vg);
    nvgCircle(vg, startX, centerY, layout.handleWidth / 2.f);
    nvgStrokeColor(vg, colors.border);
    nvgStrokeWidth(vg, 0.5f);
    nvgStroke(vg);
    
    // Right handle
    nvgBeginPath(vg);
    nvgCircle(vg, endX, centerY, layout.handleWidth / 2.f);
    nvgFillColor(vg, colors.handle);
    nvgFill(vg);
    
    nvgBeginPath(vg);
    nvgCircle(vg, endX, centerY, layout.handleWidth / 2.f);
    nvgStrokeColor(vg, colors.border);
    nvgStrokeWidth(vg, 0.5f);
    nvgStroke(vg);
  }
  
  void drawRotationBlock(NVGcontext* vg, int lane) {
    int start = currentState.startStep[lane];
    int end = currentState.endStep[lane];
    int rotation = currentState.rotation[lane];
    
    int rotationStep = (start + rotation) % STEP_COUNT;
    
    if (rotationStep >= start && rotationStep < end) {
      rack::Rect rect = layout.getStepRect(lane, rotationStep);
      
      // Rotation block
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgFillColor(vg, colors.rotation);
      nvgFill(vg);
      
      // Border
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgStrokeColor(vg, rack::color::hex("#35c7b8"));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
    }
  }
  
  // ===== KEYBOARD INPUT =====
  
  void onKey(const rack::event::Key& e) override {
    if (e.action == GLFW_PRESS) {
      // Undo: Ctrl+Z
      if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL && e.key == GLFW_KEY_Z) {
        undo();
        e.consume(this);
        return;
      }
      
      // Redo: Ctrl+Shift+Z or Ctrl+Y
      if (((e.mods & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT) && 
           e.key == GLFW_KEY_Z) ||
          ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL && e.key == GLFW_KEY_Y)) {
        redo();
        e.consume(this);
        return;
      }
      
      // Select lane: 1-5
      if (e.key >= GLFW_KEY_1 && e.key <= GLFW_KEY_5) {
        kbState.selectedLane = e.key - GLFW_KEY_1;
        e.consume(this);
        return;
      }
      
      // Arrow keys: Navigate selected lane
      if (e.key == GLFW_KEY_LEFT || e.key == GLFW_KEY_RIGHT) {
        int direction = (e.key == GLFW_KEY_RIGHT) ? 1 : -1;
        // Could navigate through steps or adjust values
        e.consume(this);
        return;
      }
      
      // R: Randomize selected lane
      if (e.key == GLFW_KEY_R && (e.mods & RACK_MOD_MASK) == 0) {
        randomizeLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
      
      // Shift+R: Randomize all
      if (e.key == GLFW_KEY_R && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
        randomizeAll();
        e.consume(this);
        return;
      }
      
      // P: Toggle preset panel
      if (e.key == GLFW_KEY_P && (e.mods & RACK_MOD_MASK) == 0) {
        kbState.showPresetPanel = !kbState.showPresetPanel;
        e.consume(this);
        return;
      }
      
      // C: Copy lane
      if (e.key == GLFW_KEY_C && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
        copyLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
      
      // V: Paste lane
      if (e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
        pasteLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
      
      // Shift+V: Reverse lane
      if (e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
        reverseLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
    }
  }
  
  // ===== HELPERS =====
  
  NVGcolor getLaneColor(int lane) const {
    switch (lane) {
      case REST:      return colors.rest;
      case MELODY:    return colors.melody;
      case OCTAVE:    return colors.octave;
      case LEGATO:    return colors.legato;
      case VARIATION: return colors.variation;
      default:        return colors.rest;
    }
  }
  
  std::string getLaneName(int lane) const {
    static const std::array<const char*, LANE_COUNT> names = {
      "REST", "MELODY", "OCTAVE", "LEGATO", "VARIATION"
    };
    if (lane >= 0 && lane < LANE_COUNT) return names[lane];
    return "?";
  }
  
  void step() override {
    Widget::step();
    
    // Animate active step indicator
    if (currentPlayStep >= 0) {
      activeStepAlpha = 0.3f + 0.4f * sinf(rack::system::getTime() * 6.f);
    } else {
      activeStepAlpha = 0.f;
    }
  }
};

}  // namespace redDot
