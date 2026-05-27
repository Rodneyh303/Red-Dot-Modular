#pragma once
#include <rack.hpp>
#include <array>
#include <deque>
#include <cstring>

namespace redDot {

/**
 * SandsVisualEditorV3
 * 
 * Configurable visual editor for both Mono and Poly Sands:
 * 
 * MONO SANDS (MonsoonSandsExpander):
 *   6 lanes: REST, MELODY, OCTAVE, LEGATO, ACCENT, VARIATION
 *   Single voice (voice 1 only)
 *   Mode: MONO = 6 lanes, all per-voice control
 * 
 * POLY SANDS (Straits East/West):
 *   3 lanes: REST, MELODY, OCTAVE
 *   Multiple voices (7 or 8)
 *   Mode: POLY = 3 lanes per-voice, other lanes global
 *   (VARIATION, LEGATO, ACCENT controlled globally, not per-voice)
 */

struct SandsVisualEditorV3 : rack::Widget {
  static constexpr int STEP_COUNT = 16;
  
  // Configuration modes
  enum Mode {
    MONO,  // 6 lanes: Rest, Melody, Octave, Legato, Accent, Variation
    POLY   // 3 lanes: Rest, Melody, Octave (only)
  };
  
  // Lane indices (mode-dependent)
  enum Lane {
    REST = 0,
    MELODY = 1,
    OCTAVE = 2,
    LEGATO = 3,      // MONO only
    ACCENT = 4,      // MONO only
    VARIATION = 5    // MONO only
  };
  
  struct Colors {
    NVGcolor rest       = rack::color::hex("#505050");      // Gray
    NVGcolor melody     = rack::color::hex("#d4af37");      // Gold
    NVGcolor octave     = rack::color::hex("#b8860b");      // Dark gold
    NVGcolor legato     = rack::color::hex("#26a69a");      // Teal
    NVGcolor accent     = rack::color::hex("#ff9500");      // Orange
    NVGcolor variation  = rack::color::hex("#ff6b6b");      // Red
    NVGcolor rotation   = rack::color::hex("#26a69a");      // Teal
    NVGcolor handle     = rack::color::hex("#888888");      // Gray
    NVGcolor background = rack::color::hex("#141416");      // Dark
    NVGcolor border     = rack::color::hex("#2a2a2a");      // Darker
    NVGcolor active     = rack::color::hex("#cc2222");      // Red (active step)
  };
  
  // Per-voice data (flexible based on mode)
  struct VoiceState {
    // Values: [step][lane]
    // Mono: 6 lanes, Poly: 3 lanes
    std::array<std::array<float, 6>, STEP_COUNT> values;
    
    // Per-lane parameters
    std::array<int, 6> startStep;
    std::array<int, 6> endStep;
    std::array<int, 6> rotation;
    
    bool operator==(const VoiceState& other) const {
      for (int s = 0; s < STEP_COUNT; ++s) {
        for (int l = 0; l < 6; ++l) {
          if (values[s][l] != other.values[s][l]) return false;
        }
      }
      for (int l = 0; l < 6; ++l) {
        if (startStep[l] != other.startStep[l] || 
            endStep[l] != other.endStep[l] ||
            rotation[l] != other.rotation[l]) return false;
      }
      return true;
    }
  };
  
  // Configuration
  Mode mode = POLY;
  int laneCount = 3;  // 3 for POLY, 6 for MONO
  
  VoiceState currentState;
  VoiceState clipboard;
  Colors colors;
  
  // Undo/redo
  std::deque<VoiceState> undoHistory;
  std::deque<VoiceState> redoHistory;
  static constexpr int UNDO_HISTORY_SIZE = 50;
  
  // Presets
  struct PresetSeed {
    char name[32];
    VoiceState state;
  };
  static constexpr int PRESET_BANK_SIZE = 8;
  std::array<PresetSeed, PRESET_BANK_SIZE> presetBank;
  int selectedPreset = 0;
  
  // Interaction
  struct DragState {
    bool isDragging = false;
    bool isDraggingBar = false;
    bool isDraggingLeftHandle = false;
    bool isDraggingRightHandle = false;
    bool isDraggingRotation = false;
    int dragLane = 0;
    int dragStep = 0;
  } dragState;
  
  // Keyboard/UI
  struct KeyboardState {
    int selectedLane = 0;
    bool showPresetPanel = false;
  } kbState;
  
  // Animation
  int currentPlayStep = -1;
  float activeStepAlpha = 0.f;
  
  // Layout (adaptive)
  struct Layout {
    float laneHeight = 30.f;
    float stepWidth = 15.f;
    float handleWidth = 12.f;
    float padding = 40.f;
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
  
  // Constructor
  SandsVisualEditorV3(Mode m = POLY) : mode(m) {
    setMode(m);
    box.size = rack::Vec(500, 250);
    
    // Initialize state
    resetState();
    initializePresets();
  }
  
  // ===== MODE MANAGEMENT =====
  
  void setMode(Mode m) {
    mode = m;
    laneCount = (mode == MONO) ? 6 : 3;
    
    // Adjust box height based on lane count
    box.size.y = 35.f + (laneCount * 30.f) + 40.f;
    
    resetState();
  }
  
  const char* getLaneName(int lane) const {
    static const char* monoNames[] = {"REST", "MELODY", "OCTAVE", "LEGATO", "ACCENT", "VARIATION"};
    static const char* polyNames[] = {"REST", "MELODY", "OCTAVE"};
    
    if (mode == MONO) {
      return (lane >= 0 && lane < 6) ? monoNames[lane] : "?";
    } else {
      return (lane >= 0 && lane < 3) ? polyNames[lane] : "?";
    }
  }
  
  NVGcolor getLaneColor(int lane) const {
    switch (lane) {
      case REST:      return colors.rest;
      case MELODY:    return colors.melody;
      case OCTAVE:    return colors.octave;
      case LEGATO:    return colors.legato;
      case ACCENT:    return colors.accent;
      case VARIATION: return colors.variation;
      default:        return colors.rest;
    }
  }
  
  // ===== STATE MANAGEMENT =====
  
  void resetState() {
    for (int s = 0; s < STEP_COUNT; ++s) {
      for (int l = 0; l < laneCount; ++l) {
        currentState.values[s][l] = 0.5f;
      }
    }
    
    for (int l = 0; l < laneCount; ++l) {
      currentState.startStep[l] = 0;
      currentState.endStep[l] = STEP_COUNT;
      currentState.rotation[l] = 0;
    }
    
    undoHistory.clear();
    redoHistory.clear();
  }
  
  void saveToHistory() {
    if (undoHistory.empty() || !(undoHistory.back() == currentState)) {
      undoHistory.push_back(currentState);
      if (undoHistory.size() > UNDO_HISTORY_SIZE) {
        undoHistory.pop_front();
      }
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
  
  // ===== PRESETS =====
  
  void initializePresets() {
    // Preset 1: Even
    snprintf(presetBank[0].name, sizeof(presetBank[0].name), "Even");
    for (int s = 0; s < STEP_COUNT; ++s) {
      for (int l = 0; l < laneCount; ++l) {
        presetBank[0].state.values[s][l] = 0.5f;
      }
    }
    
    // Preset 2: Euclidean
    snprintf(presetBank[1].name, sizeof(presetBank[1].name), "Euclidean");
    for (int s = 0; s < STEP_COUNT; ++s) {
      presetBank[1].state.values[s][REST] = (s % 2 == 0) ? 1.f : 0.f;
      for (int l = 1; l < laneCount; ++l) {
        presetBank[1].state.values[s][l] = 0.5f;
      }
    }
    
    // Preset 3: Ramp
    snprintf(presetBank[2].name, sizeof(presetBank[2].name), "Ramp");
    for (int s = 0; s < STEP_COUNT; ++s) {
      float val = s / (float)(STEP_COUNT - 1);
      for (int l = 0; l < laneCount; ++l) {
        presetBank[2].state.values[s][l] = val;
      }
    }
    
    // Preset 4: Sine
    snprintf(presetBank[3].name, sizeof(presetBank[3].name), "Sine");
    for (int s = 0; s < STEP_COUNT; ++s) {
      float angle = (s / (float)STEP_COUNT) * 2.f * M_PI;
      float val = 0.5f + 0.5f * sinf(angle);
      for (int l = 0; l < laneCount; ++l) {
        presetBank[3].state.values[s][l] = val;
      }
    }
    
    // Presets 5-7: Custom
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
  
  void randomizeLane(int lane) {
    saveToHistory();
    for (int s = 0; s < STEP_COUNT; ++s) {
      currentState.values[s][lane] = rack::random::uniform();
    }
  }
  
  void randomizeAll() {
    saveToHistory();
    for (int s = 0; s < STEP_COUNT; ++s) {
      for (int l = 0; l < laneCount; ++l) {
        currentState.values[s][l] = rack::random::uniform();
      }
    }
  }
  
  // ===== DRAWING =====
  
  void draw(const rack::DrawArgs& args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, colors.background);
    nvgFill(args.vg);
    
    // Draw control bar
    drawControlBar(args.vg);
    
    // Draw each lane
    for (int lane = 0; lane < laneCount; ++lane) {
      drawLaneLabel(args.vg, lane);
      drawLaneBars(args.vg, lane);
      drawLengthHandles(args.vg, lane);
      drawRotationBlock(args.vg, lane);
    }
    
    // Active step indicator
    if (currentPlayStep >= 0 && currentPlayStep < STEP_COUNT) {
      drawActiveStepIndicator(args.vg);
    }
    
    // Preset panel
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
    float x = 10.f;
    
    // Mode label
    nvgFontSize(vg, 9.f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, rack::color::hex("#888888"));
    
    const char* modeStr = (mode == MONO) ? "MONO" : "POLY";
    nvgText(vg, x, y, modeStr, nullptr);
    
    // Undo/Redo
    std::string undoText = "U:" + std::to_string(undoHistory.size());
    std::string redoText = "R:" + std::to_string(redoHistory.size());
    nvgText(vg, x + 50, y, undoText.c_str(), nullptr);
    nvgText(vg, x + 100, y, redoText.c_str(), nullptr);
    
    // Lane indicator
    std::string laneText = "Lane:" + std::string(getLaneName(kbState.selectedLane));
    nvgFillColor(vg, getLaneColor(kbState.selectedLane));
    nvgText(vg, x + 150, y, laneText.c_str(), nullptr);
  }
  
  void drawLaneLabel(NVGcontext* vg, int lane) {
    float y = layout.getLaneCenterY(lane);
    nvgFontSize(vg, 10.f);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    
    NVGcolor color = (lane == kbState.selectedLane) ?
                     getLaneColor(lane) : 
                     rack::color::hex("#666666");
    
    nvgFillColor(vg, color);
    nvgText(vg, layout.padding - 8, y, getLaneName(lane), nullptr);
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
    
    // Preset buttons
    float btnW = 70.f;
    float btnH = 20.f;
    float btnSpaceX = 78.f;
    float btnSpaceY = 28.f;
    
    for (int p = 0; p < PRESET_BANK_SIZE; ++p) {
      int row = p / 4;
      int col = p % 4;
      float bx = panelX + 8.f + col * btnSpaceX;
      float by = panelY + 22.f + row * btnSpaceY;
      
      bool isSelected = (p == selectedPreset);
      nvgBeginPath(vg);
      nvgRect(vg, bx, by, btnW, btnH);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#26a69a") : 
                   rack::color::hex("#333333"));
      nvgFill(vg);
      
      nvgBeginPath(vg);
      nvgRect(vg, bx, by, btnW, btnH);
      nvgStrokeColor(vg, colors.border);
      nvgStrokeWidth(vg, 0.5f);
      nvgStroke(vg);
      
      nvgFontSize(vg, 8.f);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, isSelected ? 
                   rack::color::hex("#ffffff") : 
                   rack::color::hex("#888888"));
      nvgText(vg, bx + btnW / 2.f, by + btnH / 2.f, 
              presetBank[p].name, nullptr);
    }
  }
  
  // ===== KEYBOARD INPUT =====
  
  void onKey(const rack::event::Key& e) override {
    if (e.action == GLFW_PRESS) {
      if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL && e.key == GLFW_KEY_Z) {
        undo();
        e.consume(this);
        return;
      }
      
      if (((e.mods & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT) && 
           e.key == GLFW_KEY_Z) ||
          ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL && e.key == GLFW_KEY_Y)) {
        redo();
        e.consume(this);
        return;
      }
      
      // Lane selection (1-6 for mono, 1-3 for poly)
      if (e.key >= GLFW_KEY_1 && e.key <= GLFW_KEY_6) {
        int lane = e.key - GLFW_KEY_1;
        if (lane < laneCount) {
          kbState.selectedLane = lane;
          e.consume(this);
          return;
        }
      }
      
      if (e.key == GLFW_KEY_R && (e.mods & RACK_MOD_MASK) == 0) {
        randomizeLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
      
      if (e.key == GLFW_KEY_R && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
        randomizeAll();
        e.consume(this);
        return;
      }
      
      if (e.key == GLFW_KEY_P && (e.mods & RACK_MOD_MASK) == 0) {
        kbState.showPresetPanel = !kbState.showPresetPanel;
        e.consume(this);
        return;
      }
      
      if (e.key == GLFW_KEY_C && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
        copyLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
      
      if (e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
        pasteLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
      
      if (e.key == GLFW_KEY_V && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
        reverseLane(kbState.selectedLane);
        e.consume(this);
        return;
      }
    }
  }
  
  // ===== MOUSE INTERACTION =====
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      int lane = getLaneAtY(e.pos.y);
      int step = getStepAtX(e.pos.x);
      
      if (lane >= 0 && lane < laneCount && step >= 0 && step < STEP_COUNT) {
        int start = currentState.startStep[lane];
        int end = currentState.endStep[lane];
        int rotationStep = (start + currentState.rotation[lane]) % STEP_COUNT;
        
        if (step == rotationStep && isInRotationBlock(e.pos, lane, step)) {
          dragState.isDraggingRotation = true;
          dragState.dragLane = lane;
          dragState.dragStep = step;
          dragState.isDragging = true;
          e.consume(this);
          return;
        }
        
        if (isInLengthHandle(e.pos, lane, true)) {
          dragState.isDraggingLeftHandle = true;
          dragState.dragLane = lane;
          dragState.isDragging = true;
          e.consume(this);
          return;
        }
        
        if (isInLengthHandle(e.pos, lane, false)) {
          dragState.isDraggingRightHandle = true;
          dragState.dragLane = lane;
          dragState.isDragging = true;
          e.consume(this);
          return;
        }
        
        dragState.isDraggingBar = true;
        dragState.dragLane = lane;
        dragState.dragStep = step;
        dragState.isDragging = true;
        
        if (step >= start && step < end) {
          setBarValue(lane, step, e.pos.y);
        }
        
        e.consume(this);
      }
    } else if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (dragState.isDragging) {
        saveToHistory();
      }
      dragState.isDragging = false;
      dragState.isDraggingBar = false;
      dragState.isDraggingLeftHandle = false;
      dragState.isDraggingRightHandle = false;
      dragState.isDraggingRotation = false;
    }
  }
  
  void onDragMove(const rack::event::DragMove& e) override {
    if (!dragState.isDragging) return;
    
    int lane = dragState.dragLane;
    
    if (dragState.isDraggingBar) {
      int step = getStepAtX(e.pos.x);
      if (step >= 0 && step < STEP_COUNT) {
        int start = currentState.startStep[lane];
        int end = currentState.endStep[lane];
        if (step >= start && step < end) {
          setBarValue(lane, step, e.pos.y);
        }
      }
    } else if (dragState.isDraggingRotation) {
      int step = getStepAtX(e.pos.x);
      int start = currentState.startStep[lane];
      if (step >= 0 && step < STEP_COUNT) {
        int newRotation = (step - start + STEP_COUNT) % STEP_COUNT;
        currentState.rotation[lane] = newRotation;
      }
    } else if (dragState.isDraggingLeftHandle) {
      int step = getStepAtX(e.pos.x);
      if (step >= 0 && step < STEP_COUNT) {
        int end = currentState.endStep[lane];
        if (step < end) {
          currentState.startStep[lane] = step;
        }
      }
    } else if (dragState.isDraggingRightHandle) {
      int step = getStepAtX(e.pos.x);
      if (step >= 0 && step <= STEP_COUNT) {
        int start = currentState.startStep[lane];
        if (step > start) {
          currentState.endStep[lane] = step;
        }
      }
    }
  }
  
  // ===== HELPERS =====
  
  int getLaneAtY(float y) const {
    for (int lane = 0; lane < laneCount; ++lane) {
      rack::Rect rect = layout.getLaneRect(lane);
      if (y >= rect.pos.y && y < rect.pos.y + rect.size.y) {
        return lane;
      }
    }
    return -1;
  }
  
  int getStepAtX(float x) const {
    if (x < layout.padding) return -1;
    int step = (int)((x - layout.padding) / layout.stepWidth);
    if (step < 0 || step >= STEP_COUNT) return -1;
    return step;
  }
  
  bool isInLengthHandle(rack::Vec pos, int lane, bool isLeft) const {
    int start = currentState.startStep[lane];
    int end = currentState.endStep[lane];
    float targetX = isLeft ? layout.getStepCenterX(start) : layout.getStepCenterX(end - 1);
    float targetY = layout.getLaneCenterY(lane);
    
    float radius = layout.handleWidth / 2.f + 4.f;
    float dist = rack::math::distance(pos, rack::Vec(targetX, targetY));
    return dist < radius;
  }
  
  bool isInRotationBlock(rack::Vec pos, int lane, int step) const {
    rack::Rect rect = layout.getStepRect(lane, step);
    return rect.contains(pos);
  }
  
  void setBarValue(int lane, int step, float mouseY) {
    rack::Rect rect = layout.getStepRect(lane, step);
    float relY = mouseY - rect.pos.y;
    float value = 1.f - (relY / rect.size.y);
    value = rack::math::clamp(value, 0.f, 1.f);
    currentState.values[step][lane] = value;
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

}  // namespace redDot
