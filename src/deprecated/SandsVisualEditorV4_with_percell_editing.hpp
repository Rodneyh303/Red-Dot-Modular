// ============================================================================
// DEPRECATED / ARCHIVED — NOT COMPILED.
// This is the SandsVisualEditorV4 as it was BEFORE per-cell probability editing
// was removed. It retains the DragState::BAR path (drag a step's bar up/down to
// set that step's probability directly). Per-cell editing was never part of the
// dot.modular plan; the live editor manipulates only the window (length/offset/
// rotation) + LOR/spread modulation. Kept here purely for possible reuse in a
// different project. Do NOT include this from compiled code.
// ============================================================================
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
    NVGcolor background = nvgRGB(0x10, 0x12, 0x16);  // matches Monsoon "slot" recess (#101216)
    NVGcolor border     = nvgRGB(0x2a, 0x2f, 0x37);  // matches Monsoon "slotline" (#2a2f37)
    NVGcolor active     = nvgRGB(0xcc, 0x22, 0x22);
    NVGcolor inactiveText = nvgRGB(0x6a, 0x6e, 0x76);  // dim hint text for inert state
  };

  // Theme: swap the recess/border (and handle) to match the host panel theme.
  // Lane colours stay constant (they read identically on both themes); only the
  // editor's background well + border + neutral handle change.
  bool lightTheme = false;
  void setTheme(bool light) {
    lightTheme = light;
    if (light) {
      colors.background = nvgRGB(0xd8, 0xda, 0xde);  // Monsoon light "slot"
      colors.border     = nvgRGB(0xc0, 0xc4, 0xca);  // Monsoon light "slotline"
      colors.handle     = nvgRGB(0x70, 0x76, 0x80);
      colors.rest       = nvgRGB(0x9a, 0x9a, 0x9a);  // lighten neutral rest for light bg
    } else {
      colors.background = nvgRGB(0x10, 0x12, 0x16);
      colors.border     = nvgRGB(0x2a, 0x2f, 0x37);
      colors.handle     = nvgRGB(0x88, 0x88, 0x88);
      colors.rest       = nvgRGB(0x50, 0x50, 0x50);
    }
  }
  
  // Per-lane probability distribution
  struct ProbabilityLane {
    std::array<float, STEP_COUNT> probabilities;  // 16 values
    int length = STEP_COUNT;    // How many steps active  (EDIT value)
    int offset = 0;             // Window start            (EDIT value)
    int rotation = 0;           // Rotation within window  (EDIT value)

    // Display-only L/O/R: what the user SEES. With no CV these equal the edit
    // values; when L/O/R CV is patched, the owning module overwrites them each
    // frame with the engine's CV-applied values via setDisplayLOR(), so the
    // window + markers track modulation. Editing/drag use the EDIT values
    // (editStartBar/editEndBar), so display is non-destructive.
    int dispLength   = STEP_COUNT;
    int dispOffset   = 0;
    int dispRotation = 0;
    void setDisplayLOR(int len, int off, int rot) {
      dispLength   = std::max(1, std::min(len, STEP_COUNT));
      dispOffset   = ((off % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
      dispRotation = ((rot % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
    }
    void syncDisplayToEdit() { setDisplayLOR(length, offset, rotation); }

    // Get effective probability at sequencer step (EDIT values — playback index)
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

    // ── Single source of truth for "which physical bars are in the window" ──
    // Reads the DISPLAY L/O/R so the visible window reflects CV modulation; with
    // no CV these equal the edit values (kept in sync via syncDisplayToEdit()).
    int startBar() const { return ((dispOffset % STEP_COUNT) + STEP_COUNT) % STEP_COUNT; }
    int endBar()   const { return (startBar() + std::max(1, std::min(dispLength, STEP_COUNT)) - 1) % STEP_COUNT; }
    bool barInWindow(int bar) const {
      int s = startBar(), e = endBar();
      bar = ((bar % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
      return (e >= s) ? (bar >= s && bar <= e)        // contiguous
                      : (bar >= s || bar <= e);       // wrapped
    }
    // window-relative index of a physical bar (0 = start cell), or -1 if outside
    int windowIndexOf(int bar) const {
      if (!barInWindow(bar)) return -1;
      return (((bar - startBar()) % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
    }
    // EDIT-value window edges — for hit-testing and drag resize, which must act on
    // the user's actual (un-modulated) window, not the CV-display window.
    int editStartBar() const { return ((offset % STEP_COUNT) + STEP_COUNT) % STEP_COUNT; }
    int editEndBar()   const { return (editStartBar() + std::max(1, std::min(length, STEP_COUNT)) - 1) % STEP_COUNT; }
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

  // INERT: when true the editor shows only a hint (set by the poly visuals when
  // the Straits East CV expander is absent — without it there's no poly voice
  // count, so no lanes to draw). Mono never sets this.
  bool inert = false;
  const char* inertMessage = nullptr;
  
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
    enum Type { NONE, BAR, START, END, ROTATION, WINDOW };
    Type  type         = NONE;
    int   dragLane     = 0;
    int   dragStep     = 0;
    math::Vec dragPos  = {};  // accumulated absolute position (Rack2: DragMove has delta not pos)
    int   grabStep     = 0;   // step under cursor at press (for relative WINDOW move)
    int   grabOffset   = 0;   // lane.offset at press (window slides relative to this)
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

  // Reset all playhead positions. Call when the source (Monsoon) is disconnected
  // or stops, so the highlight doesn't freeze on a stale step. drawLanePlayheads
  // skips lanes with step < 0, and step() drops activeStepAlpha to baseline once
  // no lane is active, so the editor reads as idle rather than stuck.
  void clearPlaySteps() {
    for (int l = 0; l < 6; ++l) lanePlayStep[l] = -1;
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
      currentState.lanes[l].syncDisplayToEdit();
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

    // INERT state: the poly visuals (East/Macro) have no data to show until the
    // Straits East CV expander is attached (it defines the poly voice count, so
    // there are no poly lanes to draw without it). Rather than display frozen
    // bars that look broken, show nothing but a clear hint. The owning module
    // sets `inert` each frame from cachedPolyVoiceExpander == nullptr.
    if (inert) {
      const char* msg = inertMessage ? inertMessage : "Attach Straits East";
      nvgFontSize(args.vg, 13.f);
      nvgFillColor(args.vg, colors.inactiveText);
      nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, msg, nullptr);
      nvgBeginPath(args.vg);
      nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
      nvgStrokeColor(args.vg, colors.border);
      nvgStrokeWidth(args.vg, 1.f);
      nvgStroke(args.vg);
      return;
    }

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
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect rect  = layout.getStepRect(lane, step);
    float prob       = L.probabilities[step];
    float barHeight  = prob * rect.size.y;
    // Offset-aware + wrap-correct (shared helper, matches the band & hit-test).
    bool  isInWindow = L.barInWindow(step);
    float dimAlpha   = isInWindow ? 1.f : 0.22f;

    // Background
    nvgBeginPath(vg);
    nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgFillColor(vg, colors.background);
    nvgFill(vg);

    // In-window cells get a faint tint behind the bar so the active RANGE reads
    // as a continuous block (the primary visual indicator now that brackets are
    // gone — the lit run *is* the offset→length window).
    if (isInWindow) {
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.05f));
      nvgFill(vg);
    }

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

    // ── Range markers on the cell itself (replaces the [ ] brackets) ──────────
    // Start cell: a bright bar down the LEFT edge (the offset marker — "range
    // begins here"). End cell: a matching bar down the RIGHT edge. These ride on
    // the cells, so even a 1-wide window shows both edges on the same cell
    // unambiguously (the old brackets collided and got stuck in that case).
    NVGcolor edge = getLaneColor(lane); edge.a = 0.95f;
    if (step == L.startBar()) {
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, 2.5f, rect.size.y);
      nvgFillColor(vg, edge);
      nvgFill(vg);
    }
    if (step == L.endBar()) {
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x + rect.size.x - 2.5f, rect.pos.y, 2.5f, rect.size.y);
      nvgFillColor(vg, edge);
      nvgFill(vg);
    }
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
    rack::Rect laneR = layout.getLaneRect(lane);
    int startBar = L.offset % STEP_COUNT;
    int endBar   = (L.offset + L.length - 1) % STEP_COUNT;
    float halfStep = layout.stepWidthF() * 0.5f;

    auto inWindowX = [&](float px)->bool {
      float sxL = layout.getStepCenterX(startBar) - halfStep;
      float exR = layout.getStepCenterX(endBar)   + halfStep;
      if (endBar >= startBar) return (px >= sxL && px <= exR);          // contiguous
      float leftEdge  = layout.getStepCenterX(0) - halfStep;            // wrapped
      float rightEdge = layout.getStepCenterX(STEP_COUNT - 1) + halfStep;
      return (px >= sxL && px <= rightEdge) || (px >= leftEdge && px <= exR);
    };

    // TOP band (~top 16% of the lane): the window control strip.
    //   • left edge of the START cell   → drag to set OFFSET  (resize from front)
    //   • right edge of the END cell     → drag to set LENGTH (resize from back)
    //   • anywhere else in the range     → MOVE the whole window
    // The edges are quarter-cell zones that ride ON the cells, so even a 1-wide
    // window exposes a distinct left-quarter (start) and right-quarter (end) on
    // the same cell — the old floating brackets collided there and got stuck.
    float bandBot = laneR.pos.y + laneR.size.y * 0.16f;
    if (y >= laneR.pos.y && y <= bandBot && inWindowX(x)) {
      float edgeW = layout.stepWidthF() * 0.30f;
      rack::Rect cs = layout.getStepRect(lane, L.editStartBar());
      rack::Rect ce = layout.getStepRect(lane, L.editEndBar());
      if (x <= cs.pos.x + edgeW)                       return DragState::START; // front edge
      if (x >= ce.pos.x + ce.size.x - edgeW)           return DragState::END;   // back edge
      return DragState::WINDOW;                                                 // body → move
    }

    // BOTTOM ~30%: ROTATION strip, but only within the start–end window.
    float rotTop = laneR.pos.y + laneR.size.y * 0.70f;
    if (y >= rotTop && y <= laneR.pos.y + laneR.size.y && inWindowX(x))
      return DragState::ROTATION;

    return DragState::NONE;
  }

  // Top "move strip": a thin band along the lane top across the in-window cells.
  // It's the grab-to-move affordance (drag it to slide the whole window). The
  // range extent itself is now shown by the per-cell shading + the start/end edge
  // bars drawn in drawStep, so this strip only needs to read as "grab here".
  void drawHandles(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect laneR = layout.getLaneRect(lane);
    float bandTop = laneR.pos.y;
    float bandH   = laneR.size.y * 0.16f;
    int lenC = std::max(1, std::min(L.length, STEP_COUNT));
    NVGcolor band = nvgRGBAf(1.f, 1.f, 1.f, 0.12f);
    for (int k = 0; k < lenC; ++k) {
      int bar = (L.startBar() + k) % STEP_COUNT;
      rack::Rect c = layout.getStepRect(lane, bar);
      nvgBeginPath(vg);
      nvgRect(vg, c.pos.x, bandTop, c.size.x, bandH);
      nvgFillColor(vg, band);
      nvgFill(vg);
    }
  }
  
  void drawRotationIndicator(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect laneR = layout.getLaneRect(lane);
    float stripTop = laneR.pos.y + laneR.size.y * 0.70f;
    float stripH   = laneR.size.y * 0.30f;

    // Faint track ONLY across the start–end window. Uses the DISPLAY window so it
    // tracks CV modulation in step with the window band (which uses startBar()).
    int startBar = L.dispOffset % STEP_COUNT;
    int lenC = std::max(1, std::min(L.dispLength, STEP_COUNT));
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
    // (dispOffset + dispRotation) % 16. Uses DISPLAY values so the chevron moves
    // with the modulated window.
    int rotBar = (L.dispOffset + (L.dispRotation % std::max(1, L.dispLength))) % STEP_COUNT;
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
    if (inert) return;   // no interaction until poly source (Straits East) is attached
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
          // For a WINDOW move, remember where the grab started so the window
          // slides relative to the cursor (grab the middle, the whole range
          // follows) rather than snapping its start to the pointer.
          dragState.grabStep   = getStepAtX(e.pos.x);
          dragState.grabOffset = currentState.lanes[lane].offset;
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

      case DragState::WINDOW: {
        // Move the whole window: slide offset by how far the cursor has moved in
        // steps since the grab. Length and rotation are preserved. Offset wraps
        // around the 16-step grid (mod), so the window can cross the boundary.
        int deltaSteps = step - dragState.grabStep;
        int newOff = ((dragState.grabOffset + deltaSteps) % STEP_COUNT + STEP_COUNT) % STEP_COUNT;
        L.offset = newOff;
        break;
      }

      case DragState::START: {
        // Resize from the FRONT: the dragged step becomes the new start, the END
        // bar stays put, length adjusts. Keeps the back edge anchored (intuitive
        // when you grab the front edge). Wrap-safe via mod arithmetic.
        int endBar = L.editEndBar();
        int newLen = ((endBar - step) % STEP_COUNT + STEP_COUNT) % STEP_COUNT + 1; // 1..16
        L.offset  = ((step % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
        L.length  = rack::math::clamp(newLen, 1, STEP_COUNT);
        L.rotation = rack::math::clamp(L.rotation, 0, std::max(0, L.length - 1));
        break;
      }

      case DragState::END: {
        // Resize from the BACK: the dragged step becomes the new end, START stays
        // put, length = inclusive forward distance start→step. Dragging back past
        // the start clamps to 1 (no surprise full-wrap to 16).
        int fwd = ((step - L.editStartBar()) % STEP_COUNT + STEP_COUNT) % STEP_COUNT; // 0..15
        L.length = rack::math::clamp(fwd + 1, 1, STEP_COUNT);
        L.rotation = rack::math::clamp(L.rotation, 0, std::max(0, L.length - 1));
        break;
      }

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
    // Reflect the dragged edit values in the display immediately so the window
    // follows the hand; next process frame re-applies any CV on top.
    if (dragState.isDragging && dragState.dragLane >= 0 && dragState.dragLane < laneCount)
      currentState.lanes[dragState.dragLane].syncDisplayToEdit();
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
