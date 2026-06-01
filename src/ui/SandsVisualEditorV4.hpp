#pragma once
#include <rack.hpp>
#include <array>
#include <deque>
#include <cstring>

namespace redDot {

using namespace rack;  // widget::Widget::DrawArgs, event::*, math::*
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

struct SandsVisualEditorV4 : rack::TransparentWidget {
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
    NVGcolor rest       = nvgRGB(0x50, 0x50, 0x50);
    NVGcolor melody     = nvgRGB(0xd4, 0xaf, 0x37);
    NVGcolor octave     = nvgRGB(0xb8, 0x86, 0x0b);
    NVGcolor legato     = nvgRGB(0x26, 0xa6, 0x9a);
    NVGcolor accent     = nvgRGB(0xff, 0x95, 0x00);
    NVGcolor variation  = nvgRGB(0xff, 0x6b, 0x6b);
    NVGcolor rotation   = nvgRGB(0x26, 0xa6, 0x9a);
    NVGcolor handle     = nvgRGB(0x88, 0x88, 0x88);
    NVGcolor background = nvgRGB(0x14, 0x14, 0x16);
    NVGcolor border     = nvgRGB(0x2a, 0x2a, 0x2a);
    NVGcolor active     = nvgRGB(0xcc, 0x22, 0x22);
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
    enum Type { NONE, BAR, START, END, ROTATION };
    Type  type         = NONE;
    int   dragLane     = 0;
    int   dragStep     = 0;
    math::Vec dragPos  = {};  // accumulated absolute position (Rack2: DragMove has delta not pos)
    // backward-compat aliases used elsewhere in the file
    bool  isDragging     = false;
    bool  isDraggingBar  = false;
  } dragState;
  
  struct KeyboardState {
    int selectedLane = 0;
    bool showPresetPanel = false;
  } kbState;
  
  // Per-lane playhead — each lane can be at a different step
  // due to independent LENGTH / OFFSET / ROTATION per lane.
  // Set via setLanePlayStep(lane, step) from the widget each frame.
  // step = -1 means sequencer not running (no indicator drawn).
  int lanePlayStep[6] = {-1,-1,-1,-1,-1,-1};
  float activeStepAlpha = 0.f;

  // Convenience: set all active lanes to the same global step
  // (used when L/O/R is not yet available)
  void setGlobalPlayStep(int step) {
    for (int l = 0; l < 6; ++l) lanePlayStep[l] = step;
  }

  void setLanePlayStep(int lane, int step) {
    if (lane >= 0 && lane < 6) lanePlayStep[lane] = step;
  }
  
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
  
  void draw(const widget::Widget::DrawArgs& args) override {
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
    
    if (kbState.showPresetPanel) drawPresetPanel(args.vg);
    drawLanePlayheads(args.vg);
    
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
    nvgFillColor(vg, nvgRGB(0x88, 0x88, 0x88));
    
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
    nvgFillColor(vg, (lane == kbState.selectedLane) ? getLaneColor(lane) : nvgRGB(0x66, 0x66, 0x66));
    nvgText(vg, layout.padding - 8, y, name, nullptr);
  }
  
  void drawStep(NVGcontext* vg, int lane, int step) {
    rack::Rect rect  = layout.getStepRect(lane, step);
    float prob       = currentState.lanes[lane].probabilities[step];
    float barHeight  = prob * rect.size.y;
    bool  isInWindow = (step < currentState.lanes[lane].length);
    float dimAlpha   = isInWindow ? 1.f : 0.22f;

    // Background
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgFillColor(vg, colors.background);
    nvgFill(vg);

    // Probability bar — dimmed if outside window
    NVGcolor barCol = getLaneColor(lane);
    barCol.a *= dimAlpha;
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x + 1, rect.pos.y + (rect.size.y - barHeight), rect.size.x - 2, barHeight);
    nvgFillColor(vg, barCol);
    nvgFill(vg);

    // Border — slightly dim outside window
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgStrokeColor(vg, nvgRGBAf(0.2f, 0.2f, 0.2f, dimAlpha));
    nvgStrokeWidth(vg, 0.5f);
    nvgStroke(vg);
  }
  
  // ── Handle hit testing ──────────────────────────────────────────────────────
  // Returns START, END, ROTATION, or NONE.
  // Checked BEFORE bar hit-test so handles have priority.
  DragState::Type hitTestHandle(int lane, float x, float y) const {
    if (lane < 0 || lane >= laneCount) return DragState::NONE;
    const ProbabilityLane& L = currentState.lanes[lane];
    const float r = layout.handleWidth * 0.7f;  // slightly generous hit radius
    const float cy = layout.getLaneCenterY(lane);

    // Start handle — drawn at bar `offset`
    int startBar = L.offset % STEP_COUNT;
    float sx = layout.getStepCenterX(startBar);
    if (std::hypot(x - sx, y - cy) <= r) return DragState::START;

    // End handle — drawn at bar `(offset + length - 1) % 16`
    int endBar = (L.offset + L.length - 1) % STEP_COUNT;
    float ex = layout.getStepCenterX(endBar);
    if (std::hypot(x - ex, y - cy) <= r) return DragState::END;

    // Rotation indicator — full step rect at bar `rotation`
    rack::Rect rr = layout.getStepRect(lane, L.rotation % STEP_COUNT);
    if (x >= rr.pos.x && x <= rr.pos.x + rr.size.x &&
        y >= rr.pos.y && y <= rr.pos.y + rr.size.y)
      return DragState::ROTATION;

    return DragState::NONE;
  }

  void drawHandles(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    const float cy = layout.getLaneCenterY(lane);
    const float r  = layout.handleWidth / 2.f;

    int startBar = L.offset % STEP_COUNT;
    int endBar   = (L.offset + L.length - 1) % STEP_COUNT;

    // Start handle: white circle
    nvgBeginPath(vg);
    nvgCircle(vg, layout.getStepCenterX(startBar), cy, r);
    nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.85f));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, layout.getStepCenterX(startBar), cy, r);
    nvgStrokeColor(vg, nvgRGBAf(0.f, 0.f, 0.f, 0.5f));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);

    // End handle: outlined circle (hollow centre so it's distinct from start)
    nvgBeginPath(vg);
    nvgCircle(vg, layout.getStepCenterX(endBar), cy, r);
    nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.35f));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, layout.getStepCenterX(endBar), cy, r);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.85f));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);

    // Connector line between start and end (wraps if needed)
    float sx = layout.getStepCenterX(startBar);
    float ex = layout.getStepCenterX(endBar);
    nvgBeginPath(vg);
    if (endBar >= startBar) {
      nvgMoveTo(vg, sx, cy);
      nvgLineTo(vg, ex, cy);
    } else {
      // Wraps — draw to right edge and from left edge
      float rightEdge = layout.getStepCenterX(STEP_COUNT - 1) + layout.stepWidth / 2.f;
      float leftEdge  = layout.getStepCenterX(0) - layout.stepWidth / 2.f;
      nvgMoveTo(vg, sx, cy); nvgLineTo(vg, rightEdge, cy);
      nvgMoveTo(vg, leftEdge, cy); nvgLineTo(vg, ex, cy);
    }
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.3f));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);
  }
  
  void drawRotationIndicator(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    if (L.rotation <= 0) return;  // rotation=0 means no shift, nothing to indicate

    int rotBar = L.rotation % STEP_COUNT;
    rack::Rect rect = layout.getStepRect(lane, rotBar);

    // Teal left-edge stripe to mark rotation point
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, 2.f, rect.size.y);
    nvgFillColor(vg, colors.rotation);
    nvgFill(vg);
  }
  
  // Draw a per-lane playhead highlight.
  // Each lane uses its own lanePlayStep[l] so LENGTH/OFFSET/ROTATION
  // differences between lanes are reflected correctly.
  void drawLanePlayheads(NVGcontext* vg) {
    for (int l = 0; l < laneCount; ++l) {
      int step = lanePlayStep[l];
      if (step < 0 || step >= STEP_COUNT) continue;

      rack::Rect rect = layout.getStepRect(l, step);

      // Bright top-edge tick
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x + 1, rect.pos.y, rect.size.x - 2, 2.f);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.85f * activeStepAlpha));
      nvgFill(vg);

      // Subtle full-column tint
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.12f * activeStepAlpha));
      nvgFill(vg);
    }
  }
  
  void drawPresetPanel(NVGcontext* vg) {
    float panelX = layout.padding;
    float panelY = 70.f;
    float panelW = 300.f;
    float panelH = 140.f;
    
    nvgBeginPath(vg);
    nvgRect(vg, panelX, panelY, panelW, panelH);
    nvgFillColor(vg, nvgRGB(0x1a, 0x1a, 0x1a));
    nvgFill(vg);
    
    nvgFontSize(vg, 11.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(0xd4, 0xaf, 0x37));
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
      nvgFillColor(vg, isSelected ? nvgRGB(0x26, 0xa6, 0x9a) : nvgRGB(0x33, 0x33, 0x33));
      nvgFill(vg);
      
      nvgFontSize(vg, 8.f);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, nvgRGB(0x88, 0x88, 0x88));
      nvgText(vg, bx + btnW / 2.f, by + btnH / 2.f, presetBank[p].name, nullptr);
    }
  }
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      int lane = getLaneAtY(e.pos.y);
      int step = getStepAtX(e.pos.x);

      if (lane >= 0 && lane < laneCount) {
        // Hit-test handles first — they have priority over bar dragging
        DragState::Type handleHit = hitTestHandle(lane, e.pos.x, e.pos.y);
        if (handleHit != DragState::NONE) {
          dragState.type       = handleHit;
          dragState.dragLane   = lane;
          dragState.isDragging = true;
          dragState.isDraggingBar = false;
          dragState.dragPos    = e.pos;   // capture start pos
          e.consume(this);
          return;
        }

        // Fall through to bar drag
        if (step >= 0 && step < STEP_COUNT) {
          dragState.type          = DragState::BAR;
          dragState.isDraggingBar = true;
          dragState.isDragging    = true;
          dragState.dragLane      = lane;
          dragState.dragStep      = step;
          dragState.dragPos       = e.pos;   // capture start pos
          setBarValue(lane, step, e.pos.y);
          e.consume(this);
        }
      }
    } else if (e.action == GLFW_RELEASE) {
      if (dragState.isDragging) saveToHistory();
      dragState.isDragging    = false;
      dragState.isDraggingBar = false;
      dragState.type          = DragState::NONE;
    }
  }
  
  void onDragMove(const rack::event::DragMove& e) override {
    if (!dragState.isDragging) return;
    // Rack 2: DragMove gives delta, not absolute pos. Accumulate.
    dragState.dragPos = dragState.dragPos.plus(e.mouseDelta);
    int lane = dragState.dragLane;
    int step = getStepAtX(dragState.dragPos.x);
    if (step < 0 || step >= STEP_COUNT) return;

    ProbabilityLane& L = currentState.lanes[lane];

    switch (dragState.type) {
      case DragState::BAR:
        setBarValue(lane, step, dragState.dragPos.y);
        break;

      case DragState::START:
        // Drag changes offset. Length stays the same — window slides.
        L.offset = step;
        break;

      case DragState::END:
        // Drag changes length. Offset stays fixed.
        // length = distance from offset to end step (wrapping OK, min 1, max 16)
        {
          int newLen = (step - L.offset + STEP_COUNT) % STEP_COUNT;
          if (newLen == 0) newLen = STEP_COUNT;  // full wrap = 16 steps
          L.length = rack::math::clamp(newLen, 1, STEP_COUNT);
        }
        break;

      case DragState::ROTATION:
        // Drag sets rotation directly (0–length-1, clamped)
        L.rotation = rack::math::clamp(step, 0, STEP_COUNT - 1);
        break;

      default: break;
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
    // Alpha pulses when any lane has an active playhead
    bool anyActive = false;
    for (int l = 0; l < laneCount; ++l) {
      if (lanePlayStep[l] >= 0) { anyActive = true; break; }
    }
    activeStepAlpha = anyActive
      ? 0.4f + 0.35f * sinf(rack::system::getTime() * 5.f)
      : 0.f;
  }
};

}
