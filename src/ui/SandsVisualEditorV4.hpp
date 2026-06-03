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
  bool showUndoDebug = false;   // hide internal undo-count readout by default
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
    // Layout derives ALL geometry from the owner's box.size so draw() and
    // hit-testing always use the SAME coordinate space as the actual widget
    // bounds — no fixed-pixel assumptions, no scale/DPI mismatch.
    rack::Vec boxSize = rack::Vec(550, 250);  // updated each frame/event from owner
    int laneCount = 3;

    float topPadding = 18.f;   // space above lanes (label/control row)
    float botPadding = 8.f;    // space below lanes
    float padding    = 6.f;    // left/right inset for the step grid

    float laneAreaH() const { return std::max(1.f, boxSize.y - topPadding - botPadding); }
    float laneHeightF() const { return laneAreaH() / std::max(1, laneCount); }
    float gridW() const { return std::max(1.f, boxSize.x - 2.f * padding); }
    float stepWidthF() const { return gridW() / (float)STEP_COUNT; }
    float handleWidthF() const { return std::min(stepWidthF() * 0.8f, laneHeightF() * 0.5f); }

    // Back-compat accessors used throughout the file:
    float laneHeight  = 30.f;  // kept as fields for any external refs (unused internally now)
    float stepWidth   = 30.f;
    float handleWidth = 12.f;

    float getLaneY(int lane) const { return topPadding + lane * laneHeightF(); }
    float getStepX(int step) const { return padding + step * stepWidthF(); }
    float getStepCenterX(int step) const { return getStepX(step) + stepWidthF() / 2.f; }
    float getLaneCenterY(int lane) const { return getLaneY(lane) + laneHeightF() / 2.f; }

    rack::Rect getLaneRect(int lane) const {
      return rack::Rect(padding, getLaneY(lane), gridW(), laneHeightF());
    }
    rack::Rect getStepRect(int lane, int step) const {
      return rack::Rect(getStepX(step), getLaneY(lane), stepWidthF(), laneHeightF());
    }
  } layout;

  // Keep layout.boxSize / laneCount in sync with the actual widget before any
  // draw or pointer-event geometry is computed.
  void syncLayout() {
    layout.boxSize  = box.size;
    layout.laneCount = laneCount;
  }
  
  SandsVisualEditorV4(Mode m = POLY) : mode(m) {
    setMode(m);
    box.size = rack::Vec(550, 250);
    resetState();
    initializePresets();
  }
  
  void setMode(Mode m) {
    mode = m;
    laneCount = (mode == MONO) ? 6 : 3;
    // NOTE: box.size is owned by the module that creates this editor (it sets
    // box.size = mm2px(ED_W, ED_H)). The layout derives lane height from
    // box.size.y / laneCount, so we must NOT force a hardcoded height here —
    // doing so previously made poly lanes ~30px tall regardless of the module's
    // intended (shorter, mono-like) editor height.
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
    syncLayout();
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
    
    // Undo-count readout is internal debug state; hidden by default.
    if (showUndoDebug) {
        std::string undoText = "U:" + std::to_string(undoHistory.size());
        nvgText(vg, x + 50, y, undoText.c_str(), nullptr);
    }
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
  // Zones within a lane (top-to-bottom):
  //   top ~45%  : START handle (at offset cell) / END handle (at end cell) circles
  //   bottom ~30%: ROTATION strip — drag anywhere along it to set rotation
  //   middle    : bar value (probability) drag
  // Separating rotation into its own strip means it never collides with the
  // start/end handles and is grabbable even when rotation == 0.
  DragState::Type hitTestHandle(int lane, float x, float y) const {
    if (lane < 0 || lane >= laneCount) return DragState::NONE;
    const ProbabilityLane& L = currentState.lanes[lane];
    const float r = layout.handleWidthF() * 0.8f;  // generous hit radius
    rack::Rect laneR = layout.getLaneRect(lane);
    const float handleCy = laneR.pos.y + layout.handleWidthF() * 0.7f;  // handles sit near top

    // START handle — bracket "[" at bar `offset`, near lane top
    int startBar = L.offset % STEP_COUNT;
    float sx = layout.getStepCenterX(startBar);
    if (std::hypot(x - sx, y - handleCy) <= r) return DragState::START;

    // END handle — bracket "]" at bar `(offset + length - 1) % 16`, near lane top
    int endBar = (L.offset + L.length - 1) % STEP_COUNT;
    float ex = layout.getStepCenterX(endBar);
    if (std::hypot(x - ex, y - handleCy) <= r) return DragState::END;

    // ROTATION strip — bottom ~30% of the lane, but only WITHIN the start–end
    // window (rotation is a phase inside the window, so it's only grabbable
    // there). The window spans physical bars [offset .. offset+length-1] and may
    // wrap around the 16-step grid.
    float rotTop = laneR.pos.y + laneR.size.y * 0.70f;
    if (y >= rotTop && y <= laneR.pos.y + laneR.size.y) {
      int startBar = L.offset % STEP_COUNT;
      int endBar   = (L.offset + L.length - 1) % STEP_COUNT;
      float halfStep = layout.stepWidthF() * 0.5f;
      float sxL = layout.getStepCenterX(startBar) - halfStep;
      float exR = layout.getStepCenterX(endBar)   + halfStep;
      bool inWindow;
      if (endBar >= startBar) {
        inWindow = (x >= sxL && x <= exR);                 // contiguous
      } else {
        // window wraps: [start .. right edge] OR [left edge .. end]
        float leftEdge  = layout.getStepCenterX(0) - halfStep;
        float rightEdge = layout.getStepCenterX(STEP_COUNT - 1) + halfStep;
        inWindow = (x >= sxL && x <= rightEdge) || (x >= leftEdge && x <= exR);
      }
      if (inWindow) return DragState::ROTATION;
    }

    return DragState::NONE;
  }

  // Draw a vertical square bracket. dir=+1 → "[" (opens right, for START),
  // dir=-1 → "]" (opens left, for END). Centred at (cx, cy), spanning height h.
  void drawBracket(NVGcontext* vg, float cx, float cy, float h, float tongue,
                   int dir, NVGcolor col, float w) {
    float top = cy - h * 0.5f, bot = cy + h * 0.5f;
    float spineX = cx - dir * (tongue * 0.5f);   // vertical spine offset to one side
    float tipX   = cx + dir * (tongue * 0.5f);   // tongues reach to the other side
    nvgBeginPath(vg);
    nvgMoveTo(vg, tipX, top);
    nvgLineTo(vg, spineX, top);     // top tongue
    nvgLineTo(vg, spineX, bot);     // spine
    nvgLineTo(vg, tipX, bot);       // bottom tongue
    nvgStrokeColor(vg, col);
    nvgStrokeWidth(vg, w);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStroke(vg);
  }

  void drawHandles(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect laneR = layout.getLaneRect(lane);
    const float cy = laneR.pos.y + layout.handleWidthF() * 0.7f;  // near lane top
    const float r  = layout.handleWidthF() / 2.f;
    const float brH = r * 2.2f;           // bracket height
    const float tongue = r * 0.9f;        // bracket tongue length

    int startBar = L.offset % STEP_COUNT;
    int endBar   = (L.offset + L.length - 1) % STEP_COUNT;
    float sx = layout.getStepCenterX(startBar);
    float ex = layout.getStepCenterX(endBar);

    // Connector line first (so brackets sit on top).
    nvgBeginPath(vg);
    if (endBar >= startBar) {
      nvgMoveTo(vg, sx, cy);
      nvgLineTo(vg, ex, cy);
    } else {
      float rightEdge = layout.getStepCenterX(STEP_COUNT - 1) + layout.stepWidthF() / 2.f;
      float leftEdge  = layout.getStepCenterX(0) - layout.stepWidthF() / 2.f;
      nvgMoveTo(vg, sx, cy); nvgLineTo(vg, rightEdge, cy);
      nvgMoveTo(vg, leftEdge, cy); nvgLineTo(vg, ex, cy);
    }
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.3f));
    nvgStrokeWidth(vg, 1.f);
    nvgStroke(vg);

    // START: left square bracket "[" (bright). END: right square bracket "]".
    drawBracket(vg, sx, cy, brH, tongue, +1, nvgRGBAf(1.f, 1.f, 1.f, 0.9f), 2.f);
    drawBracket(vg, ex, cy, brH, tongue, -1, nvgRGBAf(1.f, 1.f, 1.f, 0.7f), 2.f);
  }
  
  void drawRotationIndicator(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect laneR = layout.getLaneRect(lane);
    float stripTop = laneR.pos.y + laneR.size.y * 0.70f;
    float stripH   = laneR.size.y * 0.30f;

    // Faint track ONLY across the start–end window (matches the grabbable
    // region in hitTestHandle). Drawn per in-window cell so it wraps correctly.
    int startBar = L.offset % STEP_COUNT;
    int lenC = std::max(1, std::min(L.length, STEP_COUNT));
    NVGcolor track = colors.rotation; track.a = 0.10f;
    for (int k = 0; k < lenC; ++k) {
      int bar = (startBar + k) % STEP_COUNT;
      rack::Rect c = layout.getStepRect(lane, bar);
      nvgBeginPath(vg);
      nvgRect(vg, c.pos.x, stripTop, c.size.x, stripH);
      nvgFillColor(vg, track);
      nvgFill(vg);
    }

    // Rotation marker sits WITHIN the start-end window: physical bar =
    // (offset + rotation) % 16. Rotation is a phase offset inside the window,
    // so it always lands between the start "[" and end "]".
    int rotBar = (L.offset + (L.rotation % std::max(1, L.length))) % STEP_COUNT;
    rack::Rect cell = layout.getStepRect(lane, rotBar);
    float cx = cell.pos.x + cell.size.x * 0.5f;
    float cyc = stripTop + stripH * 0.5f;
    float h = stripH * 0.7f;
    float halfW = (cell.size.x - 2.f) * 0.42f;

    // Distinct from the square start/end brackets: a "phase" marker drawn as a
    // pair of inward-pointing chevrons ›‹ around the rotation block, plus a thin
    // baseline highlight of the block. Brighter when rotation > 0.
    NVGcolor mk = colors.rotation; mk.a = (L.rotation > 0) ? 0.9f : 0.4f;

    // block baseline tint
    nvgBeginPath(vg);
    nvgRect(vg, cell.pos.x + 1.f, stripTop, cell.size.x - 2.f, stripH);
    NVGcolor blockTint = colors.rotation; blockTint.a = (L.rotation > 0) ? 0.30f : 0.15f;
    nvgFillColor(vg, blockTint);
    nvgFill(vg);

    // left chevron "›" pointing right toward centre
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - halfW - 2.f, cyc - h * 0.5f);
    nvgLineTo(vg, cx - halfW + 2.f, cyc);
    nvgLineTo(vg, cx - halfW - 2.f, cyc + h * 0.5f);
    // right chevron "‹" pointing left toward centre
    nvgMoveTo(vg, cx + halfW + 2.f, cyc - h * 0.5f);
    nvgLineTo(vg, cx + halfW - 2.f, cyc);
    nvgLineTo(vg, cx + halfW + 2.f, cyc + h * 0.5f);
    nvgStrokeColor(vg, mk);
    nvgStrokeWidth(vg, 1.8f);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStroke(vg);
  }
  
  // Draw a per-lane playhead highlight.
  // Each lane uses its own lanePlayStep[l] so LENGTH/OFFSET/ROTATION
  // differences between lanes are reflected correctly.
  void drawLanePlayheads(NVGcontext* vg) {
    for (int l = 0; l < laneCount; ++l) {
      int step = lanePlayStep[l];
      if (step < 0 || step >= STEP_COUNT) continue;

      rack::Rect rect = layout.getStepRect(l, step);

      // Highlight the WHOLE active-step block: lighten the entire cell column
      // so the current step (where the probability roll happens) is obvious.
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.18f * activeStepAlpha));
      nvgFill(vg);

      // Re-draw this step's probability bar brighter on top of the highlight
      float prob = currentState.lanes[l].probabilities[step];
      float barH = prob * rect.size.y;
      NVGcolor c = getLaneColor(l);
      c.a = 0.55f + 0.45f * activeStepAlpha;   // boosted vs the static 1.0 bar
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x + 1, rect.pos.y + (rect.size.y - barH), rect.size.x - 2, barH);
      nvgFillColor(vg, c);
      nvgFill(vg);

      // Bright top-edge tick
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x + 1, rect.pos.y, rect.size.x - 2, 2.f);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.9f * activeStepAlpha));
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
    syncLayout();
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
    syncLayout();
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
        // Drag changes length: end lands on the dragged step.
        // length = forward distance from offset to step, inclusive.
        // Dragging left of start shrinks toward 1 (no surprise full-wrap to 16).
        {
          int fwd = (step - L.offset + STEP_COUNT) % STEP_COUNT;  // 0..15
          int newLen = fwd + 1;                                   // inclusive → 1..16
          L.length = rack::math::clamp(newLen, 1, STEP_COUNT);
        }
        break;

      case DragState::ROTATION: {
        // Rotation strip drag: the marker lives WITHIN the window at physical
        // bar (offset + rotation) % 16. Convert the physical step under the
        // cursor back to a window-relative rotation in [0, length-1].
        int rel = ((step - L.offset) % STEP_COUNT + STEP_COUNT) % STEP_COUNT;
        L.rotation = rack::math::clamp(rel, 0, std::max(0, L.length - 1));
        break;
      }

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
    int step = (int)((x - layout.padding) / layout.stepWidthF());
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
      ? 0.6f + 0.3f * sinf(rack::system::getTime() * 5.f)
      : 0.5f;   // baseline so the current-step block stays visible when stopped
  }
};

}
