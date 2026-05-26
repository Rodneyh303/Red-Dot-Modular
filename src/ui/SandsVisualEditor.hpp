#pragma once
#include <rack.hpp>
#include <array>

namespace redDot {

/**
 * SandsVisualEditor
 * 
 * Interactive visual sequencer for DNA parameters:
 * - 5 probability lanes (Rest, Melody, Octave, Legato, Variation)
 * - 16-step bar chart interface
 * - Length handles (start/end points)
 * - Rotation blocks (per-lane rotation offset indicator)
 * 
 * Design:
 *   • Click bar to set probability (height = 0-100%)
 *   • Drag to paint pattern across steps
 *   • Drag length handles to adjust range
 *   • Drag rotation block to rotate pattern
 */

struct SandsVisualEditor : rack::Widget {
  static constexpr int STEP_COUNT = 16;
  static constexpr int LANE_COUNT = 5;
  
  // Lane types
  enum Lane {
    REST = 0,
    MELODY = 1,
    OCTAVE = 2,
    LEGATO = 3,
    VARIATION = 4
  };
  
  // Colors
  struct Colors {
    NVGcolor rest       = rack::color::hex("#505050");      // Gray
    NVGcolor melody     = rack::color::hex("#d4af37");      // Gold
    NVGcolor octave     = rack::color::hex("#b8860b");      // Dark gold
    NVGcolor legato     = rack::color::hex("#26a69a");      // Teal
    NVGcolor variation  = rack::color::hex("#ff6b6b");      // Red
    NVGcolor rotation   = rack::color::hex("#26a69a");      // Teal (always)
    NVGcolor handle     = rack::color::hex("#888888");      // Gray
    NVGcolor background = rack::color::hex("#141416");      // Dark
    NVGcolor border     = rack::color::hex("#2a2a2a");      // Darker
  };
  
  // Per-voice data
  struct VoiceData {
    std::array<float, LANE_COUNT> values[STEP_COUNT];    // Step values [step][lane]
    std::array<int, LANE_COUNT> startStep;                // Start offset per lane
    std::array<int, LANE_COUNT> endStep;                  // End offset per lane (or length)
    std::array<int, LANE_COUNT> rotation;                 // Rotation offset per lane
    int selectedLane = 0;
  };
  
  VoiceData voiceData;
  Colors colors;
  
  // Interaction state
  struct DragState {
    bool isDragging = false;
    bool isDraggingBar = false;
    bool isDraggingLeftHandle = false;
    bool isDraggingRightHandle = false;
    bool isDraggingRotation = false;
    
    int dragLane = 0;
    int dragStep = 0;
    int dragStartRotation = 0;
    float dragStartValue = 0.f;
  } dragState;
  
  // Layout
  struct Layout {
    float laneHeight = 30.f;
    float stepWidth = 15.f;
    float handleWidth = 12.f;
    float padding = 8.f;
    float topPadding = 20.f;
    
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
  
  SandsVisualEditor() {
    box.size = rack::Vec(500, 200);
    
    // Initialize data
    for (int v = 0; v < STEP_COUNT; ++v) {
      for (int l = 0; l < LANE_COUNT; ++l) {
        voiceData.values[v][l] = 0.5f;
      }
    }
    
    for (int l = 0; l < LANE_COUNT; ++l) {
      voiceData.startStep[l] = 0;
      voiceData.endStep[l] = STEP_COUNT;
      voiceData.rotation[l] = 0;
    }
  }
  
  // Get lane color
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
  
  // Get lane label
  const char* getLaneLabel(int lane) const {
    switch (lane) {
      case REST:      return "REST";
      case MELODY:    return "MELODY";
      case OCTAVE:    return "OCTAVE";
      case LEGATO:    return "LEGATO";
      case VARIATION: return "VARIATION";
      default:        return "?";
    }
  }
  
  // ===== DRAWING =====
  
  void draw(const rack::DrawArgs& args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, colors.background);
    nvgFill(args.vg);
    
    // Draw each lane
    for (int lane = 0; lane < LANE_COUNT; ++lane) {
      drawLaneLabel(args.vg, lane);
      drawLaneBars(args.vg, lane);
      drawLengthHandles(args.vg, lane);
      drawRotationBlock(args.vg, lane);
    }
    
    // Border
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgStrokeColor(args.vg, colors.border);
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);
  }
  
  void drawLaneLabel(NVGcontext* vg, int lane) {
    float y = layout.getLaneCenterY(lane);
    nvgFontSize(vg, 10.f);
    nvgFontFaceId(vg, 0);  // Default font
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, rack::color::hex("#888888"));
    nvgText(vg, layout.padding - 4, y, getLaneLabel(lane), nullptr);
  }
  
  void drawLaneBars(NVGcontext* vg, int lane) {
    int start = voiceData.startStep[lane];
    int end = voiceData.endStep[lane];
    
    for (int step = 0; step < STEP_COUNT; ++step) {
      drawProbabilityBar(vg, lane, step, step >= start && step < end);
    }
  }
  
  void drawProbabilityBar(NVGcontext* vg, int lane, int step, bool isActive) {
    rack::Rect rect = layout.getStepRect(lane, step);
    float value = voiceData.values[step][lane];
    float barHeight = value * rect.size.y;
    
    NVGcolor color = getLaneColor(lane);
    
    if (!isActive) {
      // Dim bars outside range
      color.a = 0.3f;
    }
    
    // Background rect
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgFillColor(vg, colors.background);
    nvgFill(vg);
    
    // Value bar (drawn from bottom)
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
    int start = voiceData.startStep[lane];
    int end = voiceData.endStep[lane];
    
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
    
    // Left handle indicator (◄)
    nvgFontSize(vg, 8.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, colors.background);
    nvgText(vg, startX, centerY, "◄", nullptr);
    
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
    
    // Right handle indicator (►)
    nvgFillColor(vg, colors.background);
    nvgText(vg, endX, centerY, "►", nullptr);
  }
  
  void drawRotationBlock(NVGcontext* vg, int lane) {
    int start = voiceData.startStep[lane];
    int end = voiceData.endStep[lane];
    int rotation = voiceData.rotation[lane];
    
    int rotationStep = (start + rotation) % STEP_COUNT;
    
    if (rotationStep >= start && rotationStep < end) {
      rack::Rect rect = layout.getStepRect(lane, rotationStep);
      
      // Rotation block (filled with teal)
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgFillColor(vg, colors.rotation);
      nvgFill(vg);
      
      // Border (brighter)
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgStrokeColor(vg, rack::color::hex("#35c7b8"));
      nvgStrokeWidth(vg, 1.f);
      nvgStroke(vg);
    }
  }
  
  // ===== INTERACTION =====
  
  void onHover(const rack::event::Hover& e) override {
    Widget::onHover(e);
  }
  
  void onButton(const rack::event::Button& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      rack::Vec mousePos = e.pos;
      
      // Check which lane and step
      int lane = getLaneAtY(mousePos.y);
      int step = getStepAtX(mousePos.x);
      
      if (lane >= 0 && lane < LANE_COUNT && step >= 0 && step < STEP_COUNT) {
        int start = voiceData.startStep[lane];
        int end = voiceData.endStep[lane];
        int rotationStep = (start + voiceData.rotation[lane]) % STEP_COUNT;
        
        // Check if clicked on rotation block
        if (step == rotationStep && isInRotationBlock(mousePos, lane, step)) {
          dragState.isDraggingRotation = true;
          dragState.dragLane = lane;
          dragState.dragStep = step;
          dragState.dragStartRotation = voiceData.rotation[lane];
          dragState.isDragging = true;
          e.consume(this);
          return;
        }
        
        // Check if clicked on length handle
        if (isInLengthHandle(mousePos, lane, true)) {  // Left handle
          dragState.isDraggingLeftHandle = true;
          dragState.dragLane = lane;
          dragState.isDragging = true;
          e.consume(this);
          return;
        }
        
        if (isInLengthHandle(mousePos, lane, false)) {  // Right handle
          dragState.isDraggingRightHandle = true;
          dragState.dragLane = lane;
          dragState.isDragging = true;
          e.consume(this);
          return;
        }
        
        // Otherwise, set probability bar value
        dragState.isDraggingBar = true;
        dragState.dragLane = lane;
        dragState.dragStep = step;
        dragState.dragStartValue = voiceData.values[step][lane];
        dragState.isDragging = true;
        
        // Set value based on click height
        if (step >= start && step < end) {
          setBarValue(lane, step, mousePos.y);
        }
        
        e.consume(this);
      }
    } else if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      dragState.isDragging = false;
      dragState.isDraggingBar = false;
      dragState.isDraggingLeftHandle = false;
      dragState.isDraggingRightHandle = false;
      dragState.isDraggingRotation = false;
    }
  }
  
  void onDragMove(const rack::event::DragMove& e) override {
    if (!dragState.isDragging) return;
    
    rack::Vec mousePos = e.pos;
    int lane = dragState.dragLane;
    
    if (dragState.isDraggingBar) {
      int step = getStepAtX(mousePos.x);
      if (step >= 0 && step < STEP_COUNT) {
        int start = voiceData.startStep[lane];
        int end = voiceData.endStep[lane];
        if (step >= start && step < end) {
          setBarValue(lane, step, mousePos.y);
        }
      }
    } else if (dragState.isDraggingRotation) {
      int step = getStepAtX(mousePos.x);
      int start = voiceData.startStep[lane];
      if (step >= 0 && step < STEP_COUNT) {
        int newRotation = (step - start + STEP_COUNT) % STEP_COUNT;
        voiceData.rotation[lane] = newRotation;
      }
    } else if (dragState.isDraggingLeftHandle) {
      int step = getStepAtX(mousePos.x);
      if (step >= 0 && step < STEP_COUNT) {
        int end = voiceData.endStep[lane];
        if (step < end) {
          voiceData.startStep[lane] = step;
        }
      }
    } else if (dragState.isDraggingRightHandle) {
      int step = getStepAtX(mousePos.x);
      if (step >= 0 && step <= STEP_COUNT) {
        int start = voiceData.startStep[lane];
        if (step > start) {
          voiceData.endStep[lane] = step;
        }
      }
    }
  }
  
  // ===== HELPERS =====
  
  int getLaneAtY(float y) const {
    for (int lane = 0; lane < LANE_COUNT; ++lane) {
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
    int start = voiceData.startStep[lane];
    int end = voiceData.endStep[lane];
    float targetX = isLeft ? layout.getStepCenterX(start) : layout.getStepCenterX(end - 1);
    float targetY = layout.getLaneCenterY(lane);
    
    float radius = layout.handleWidth / 2.f + 4.f;  // Slightly larger hit area
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
    voiceData.values[step][lane] = value;
  }
  
  // Randomize lane
  void randomizeLane(int lane) {
    for (int step = 0; step < STEP_COUNT; ++step) {
      voiceData.values[step][lane] = rack::random::uniform();
    }
  }
  
  // Reset lane
  void resetLane(int lane) {
    for (int step = 0; step < STEP_COUNT; ++step) {
      voiceData.values[step][lane] = 0.5f;
    }
    voiceData.startStep[lane] = 0;
    voiceData.endStep[lane] = STEP_COUNT;
    voiceData.rotation[lane] = 0;
  }
};

}  // namespace redDot
