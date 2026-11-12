#pragma once
#include <rack.hpp>
#include <GLFW/glfw3.h>   // glfwSetCursor / glfwCreateStandardCursor (cursor cues)
#include <array>
#include <deque>
#include <cstring>
#include "../dsp/LaneMapping.hpp"   // canonical editor lane order — the Lane enum below is guarded against it
#include "../dsp/engines/PatternEngine.hpp"   // dotModular::kMaskLenMask — the STEPS_32 Stage 2a mask-constant authority
#include "SandsLaneWindowCore.hpp"   // STEPS_32 Stage 3: the lane window's display math (rack-free, verbatim) — ProbabilityLane's base
#include "SandsRangeGestureCore.hpp" // CC_FIX_VOICE_RANGE_STRIP_INTERACTION: the strip's gesture math (rack-free — the same precedent)

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
 // STEPS_32 Stage 3 — the column-count parameterisation (the plan: "design the grid to be
 // column-count-parameterised rather than hardcoded 16"): the phrase domain IS the engine's
 // usable cap (32 since the 2b flip) via SandsLaneWindowCore's authority. The grid now draws
 // ALL phrase columns — the lane window math (inherited from SandsLaneWindowCore) runs on the
 // same domain, so a 32-step lane pattern is fully visible and authorable (the horizontal
 // cells halve — the plan's accepted cost, "the first 64-step blocker": a later 64 cap is a
 // CONSTANT change from here, not a sweep). Every loop and array below derives from this
 // constant; the hosts' push loops iterate it, so bars/pushes widen with it automatically.
 inline static constexpr int STEP_COUNT = dotModular::kPatternLengthCap;
 static_assert(STEP_COUNT == 32, "STEPS_32 Stage 3: the Sands grid draws the 32-step phrase (the 2b cap)");
 // Max editor lanes = EAST_LANES (MEL/OCT/QMIX/REST/ACC/VAR/LEG). Backing arrays are sized to
 // this; MONO shows 7, POLY 5. Was 6 pre-QMIX — the East step() writing lane index 6 into a
 // 6-element array was the QMIX add-crash (out-of-bounds write corrupting adjacent memory).
 inline static constexpr int MAX_LANES = 7;
  
  enum Mode {
    MONO,
    POLY
  };
  
  // QMIX-widened EDITOR lane order (== the strand order, drawLaneLabel, and every other
  // Sands site — identity). QMIX inserted at index 2, shifting REST→3, ACCENT→4, VAR→5,
  // LEG→6. WAS the pre-QMIX 6-lane order (MEL0 OCT1 REST2 ACC3 VAR4 LEG5) — that stale enum made
  // syncPatternEngineToEditor write each lane's probabilities to the wrong row (REST spread showed
  // on the QMIX row, ACCENT on the REST row). These symbolic names index VoiceState::lanes[] rows.
  enum Lane {
    MELODY = 0,
    OCTAVE = 1,
    QMIX = 2,
    REST = 3,
    ACCENT = 4,
    VARIATION = 5,
    LEGATO = 6
  };
  // GUARD: this editor-lane enum is a SECOND source of truth for the editor lane order — that's
  // why dsp/LaneMapping.hpp couldn't catch the stale pre-QMIX values that misrouted spread display
  // (REST bars on the QMIX row). Tie it to the canonical LaneMapping order so any future drift on
  // EITHER side trips at compile time. Editor lane == strand index (identity).
  static_assert(QMIX == dotModular::QMIX_EDITOR_LANE, "editor QMIX lane must be dotModular::QMIX_EDITOR_LANE (2)");
  static_assert(MELODY == 0 && OCTAVE == 1 && QMIX == 2 && REST == 3
                && ACCENT == 4 && VARIATION == 5 && LEGATO == 6,
                "SandsVisualEditorV4::Lane must equal the QMIX editor order (dsp/LaneMapping.hpp)");
  // (int) casts: Lane and dotModular::EngineStrand are distinct enums; comparing them directly
  // trips -Wenum-compare. We deliberately assert their integer values coincide (editor lane ==
  // strand, identity), so compare as ints.
  static_assert((int)MELODY == (int)dotModular::STRAND_MELODY && (int)OCTAVE == (int)dotModular::STRAND_OCTAVE
                && (int)QMIX == (int)dotModular::STRAND_QMIX && (int)REST == (int)dotModular::STRAND_RHYTHM
                && (int)ACCENT == (int)dotModular::STRAND_ACCENT && (int)VARIATION == (int)dotModular::STRAND_VARIATION
                && (int)LEGATO == (int)dotModular::STRAND_LEGATO,
                "editor lane == engine strand (identity); keep both in step");
  
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

  // Theme: the lane display stays DARK on both themes. Only its BEZEL follows the host.
  //
  // The old comment here claimed "lane colours read identically on both themes". They do
  // not. The lane hues are high-luminance (yellow, orange, teal) and were tuned against a
  // dark well; on the light "slot" (#d8dade) melody yellow falls from 7.9:1 contrast to
  // 1.7:1 -- under the 3:1 floor for graphical objects, i.e. genuinely unreadable. Every
  // lane loses 60-80%.
  //
  // Worse than dim, it is WRONG: the bars encode probability in alpha, so on a light ground
  // a low-probability step washes toward the background and reads as ABSENT rather than
  // unlikely. The display lies about the data.
  //
  // So we theme the PANEL, not the DISPLAY -- as hardware does: light panel, dark screen,
  // and nobody expects an LCD to invert. This also keeps ONE lane colour table with one
  // meaning per hue, instead of two that can drift apart.
  //
  // (The Lantern is NOT evidence for this. It has no light theme because none was written
  //  yet -- absence is not a decision. When it gets one, the same rule applies: theme its
  //  panel, leave its LCD dark.)
  //
  // The border still follows the host: a light bezel around a dark well reads as a recessed
  // screen on the light panel rather than a hole punched in it.
  bool lightTheme = false;
  void setTheme(bool light) {
    lightTheme = light;
    // Display: dark always. These are the values the lane colours were designed against.
    colors.background = nvgRGB(0x10, 0x12, 0x16);
    colors.handle     = nvgRGB(0x88, 0x88, 0x88);
    colors.rest       = nvgRGB(0x50, 0x50, 0x50);
    // Bezel: follows the host panel, so the well reads as inset into it.
    colors.border     = light ? nvgRGB(0xc0, 0xc4, 0xca)   // Monsoon light "slotline"
                              : nvgRGB(0x2a, 0x2f, 0x37);
  }

  // Per-lane probability distribution
  // STEPS_32 Stage 3: the L/O/R FIELDS and the window/display MATH live in
  // SandsLaneWindowCore (moved VERBATIM — rack-free, the dimming-rule pin's testable
  // surface); this struct adds the per-cell probabilities and the two accessors that
  // read/write them.
  struct ProbabilityLane : SandsLaneWindowCore {
    std::array<float, STEP_COUNT> probabilities;  // one value per phrase cell (32 since the 2b flip)

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
  };
  
  struct VoiceState {
    std::array<ProbabilityLane, MAX_LANES> lanes;   // 7 lanes (QMIX-widened; was 6)
    
    bool operator==(const VoiceState& other) const {
      for (int l = 0; l < MAX_LANES; ++l) {
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
  // readOnly: render data normally but block all editing/drag. Used for East/Macro
  // tab 1 when Sands Mono is attached — the lane data belongs to Mono and is shown
  // read-only (edit it on Sands Mono). Unlike inert, no hint message, data still drawn.
  bool readOnly = false;
  // Per-lane edit lock (P4 / G5): host returns true for a lane whose values are
  // owned elsewhere (e.g. delegated to Macro) — that lane is shown but its handles
  // and bar can't be edited, while other lanes stay editable. editorLane index.
  // EDIT-PERMISSION: is editing this lane BLOCKED (delegated to Macro / owned elsewhere / read-only
  // tab)? This is NOT lock-mode -- it gates drag/hover on inoperable lanes. For lock-mode use
  // lockActiveFn below. (These were conflated once; keep them distinct.)
  std::function<bool(int editorLane)> laneEditBlockedFn;
  bool laneEditBlocked(int editorLane) const { return laneEditBlockedFn && laneEditBlockedFn(editorLane); }
  // TRUE lock-mode state (engine.locked), distinct from laneEditBlockedFn (which is edit-PERMISSION and
  // is deliberately false on Macro). The ghost echo needs actual lock, not edit-permission, because
  // the display window also diverges from edit under normal CV modulation when UNLOCKED -- so
  // divergence alone can't distinguish "locked" from "CV modulating". Host wires to engine.locked.
  std::function<bool()> lockActiveFn;
  bool lockActive() const { return lockActiveFn && lockActiveFn(); }
  // Ghost echo for this lane: lock-mode active AND the live edit window has diverged from the
  // committed/display window. Under lock the display is frozen (LOR LATCH) while edit tracks the
  // handle, so divergence here = a genuine moved-under-lock echo (not CV modulation).
  bool lorGhost(int editorLane) const { return lockActive() && currentState.lanes[editorLane].lorDiverged(); }
  // Optional right-click callback: host registers this to open a context menu
  // when a lane row is right-clicked. Called with (lane, pos); return true to consume.
  std::function<bool(int lane, rack::math::Vec pos)> onLaneRightClick;

  // Optional LOR-commit callback: fired on drag RELEASE with the affected editor lane and the
  // length/offset/rotation triple BEFORE and AFTER the drag. The host pushes a Rack history
  // action against the STORE (getLorBase/setLorBase) so the drag is Ctrl+Z-undoable. The
  // editor stays decoupled from the module/store -- it only reports the change.
  std::function<void(int lane, const int before[3], const int after[3])> onLorCommit;
  // (the drag's before/after pair lives in lorDrag — the reducer keeps it for the commit)

  VoiceState currentState;
  VoiceState clipboard;
  Colors colors;
  
  std::deque<VoiceState> undoHistory;
  bool showUndoDebug = false;   // hide internal undo-count readout by default
  // The Sands expanders zero the editor's internal top/bottom padding so lanes
  // fill the box evenly and align with the painted lanes + kit-bound jacks. The
  // MONO/POLY control-bar text is drawn at a fixed top offset and would then land
  // on lane 0, so it can be suppressed (the panel art can carry POLY/MONO).
  // Lane labels (MELODY/OCTAVE/…) are NOT suppressible — the panel does not draw
  // them (see dotmod_design.py: "No <text> for control labels"); the editor is
  // their only source and they track the lane centres.
  bool showControlBar = true;
  std::deque<VoiceState> redoHistory;
  static constexpr int UNDO_HISTORY_SIZE = 50;
  // (ADDITION D — CC_FIX_VOICE_RANGE_STRIP_INTERACTION) the vestigial PRESET BANK is
  // RETIRED — the seed struct, the bank array + its size constant, the selected
  // index, the seeding, the load path, the panel draw and the never-set flag that
  // guarded it are all deleted: nothing in the live editor, the East/Macro visuals
  // or the plugin registration ever called the apply path (the deprecated copy
  // stays out of the build).
  
  struct DragState {
    enum Type { NONE, START, END, ROTATION, WINDOW };
    // (LOR_EDITING_BUG_LIST §PLAN 1 — the drag reducer) the LOR drag STATE lives in
    // the reducer (lorDrag below); this keeps the Type enum (the zone vocabulary
    // the hit tests, hover and cursor cues share) and the LANE drag's pointer
    // accumulator.
    math::Vec dragPos  = {};  // accumulated absolute position (Rack2: DragMove has delta not pos)
  } dragState;
  // THE DRAG REDUCER's state — the ONE LOR drag state machine (LOR_EDITING_BUG_LIST
  // §PLAN 1, the ROOT ISSUE fix). The owner is set at the ENGAGED press (the strip
  // band is hit-tested FIRST — a press there starts a STRIP drag and returns;
  // anything below starts a LANE drag — ADDITION E's law). A LANE drag never calls
  // onRangeEdit and never writes rangeStrip.lor; a STRIP drag never writes lanes[l].
  // The widget is the TRANSLATION layer: GLFW press/move/release ->
  // SandsRangeGestureCore::lorDragReduce calls + the window copy-back (the laws are
  // the core's pure functions; the paint/arm machinery stays here — it is the paint
  // feature, not the LOR drag).
  redDot::SandsRangeGestureCore::LorDragState lorDrag;
  // DragState::Type -> the reducer's Zone (the translation vocabulary).
  static redDot::SandsRangeGestureCore::Zone coreZoneOf(DragState::Type t) {
    switch (t) {
      case DragState::START:    return redDot::SandsRangeGestureCore::Z_START;
      case DragState::END:      return redDot::SandsRangeGestureCore::Z_END;
      case DragState::ROTATION: return redDot::SandsRangeGestureCore::Z_ROTATION;
      case DragState::WINDOW:   return redDot::SandsRangeGestureCore::Z_WINDOW;
      default:                  return redDot::SandsRangeGestureCore::Z_NONE;
    }
  }
  bool stripDragLive() const {
    return lorDrag.active && lorDrag.owner == redDot::SandsRangeGestureCore::LorDragState::O_STRIP;
  }
  bool laneDragLive() const {
    return lorDrag.active && lorDrag.owner == redDot::SandsRangeGestureCore::LorDragState::O_LANE;
  }

  // ── UNIFIED ADDRESSING Stage 3: the VOICE-RANGE strip ────────────────────────
  // A thin CONSTANT-block lane reserved ABOVE lane 0 (the frame the probability lanes
  // nest within). Spec (CC_PROMPT_UNIFIED_ADDRESSING_SPEC.md Stage 3): flat lit cell =
  // in-range step, dim = out of range — deliberately NOT a probability-magnitude bar,
  // this lane shows WHERE the voice reads. It animates with the playhead (the engine's
  // per-voice read cell glows brighter; a leading-edge bar shows the walk direction,
  // the lanePlayDir idiom) and it is ALSO the drag-authoring surface for the voice-LOR
  // rows: the same START/END/MOVE/ROTATION gestures as a probability lane, reported to
  // the host (which owns the store) via onRangeEdit (live, per move) and onRangeCommit
  // (release, the onLorCommit idiom one level up — voice instead of lane).
  // The host owns ALL the data: it pushes the effective row via setRangeState() each
  // frame and reports gestures back through the callbacks. With layout.rangeLaneH == 0
  // the strip is fully absent (geometry, draw, hit-test) — pre-Stage-3 hosts untouched.
  struct RangeStrip {
    bool  enabled = false;
    int   lor[3]  = {16, 0, 0};   // DISPLAY window (the effective row, engine-resolved)
    int   readPos = -1;           // the voice frame's read cell (0..15), -1 = idle
    int   dir     = 1;            // the walk sign (+1/-1) for the leading-edge cue
    bool  blocked = false;        // follow-Macro / read-only — render, no gestures
    const char* label = "";       // host caption (drawn like the lane labels)
    // THE UNIFIED GESTURE MODEL (the follow-up fix): a press is PENDING (pressArmed)
    // until the cursor crosses into a NEW step cell — released unmoved it is a CLICK
    // (the paint toggle of the press cell, ANY cell: full-window strips are
    // paintable); crossed, it engages the gesture the press zone picked (an LOR
    // window gesture in-window; the paint stroke out-of-window). The old press-time
    // split made paint unreachable on a full window (every press was an LOR zone).
    bool  pressArmed = false;
    int   pressCell = -1;                       // the step cell at press (the click target / threshold)
    DragState::Type pressZone = DragState::NONE; // hitTestRange at press (the gesture to engage)
    bool  paintPress = false;     // a paint stroke is ENGAGED (drag-paint)
    int   paintTarget = -1;       // the stroke's uniform target (latched by the first cell)
    unsigned paintApplied = 0u;   // cells already toggled this stroke (no double-toggle on wiggle)
    // (§PLAN 1) the LOR drag state (the zone, the grab values, the before/after
    // pair) lives in lorDrag — the reducer; this keeps the strip's POINTER
    // accumulator (the arm, the paint stroke and the drag all accumulate here).
    math::Vec dragPos  = {};      // accumulated absolute position (DragMove gives delta)
  } rangeStrip;
  // Live authoring callbacks (host-wired; the editor stays store-decoupled).
  std::function<void(int L, int O, int R)> onRangeEdit;                        // per move
  std::function<void(const int before[3], const int after[3])> onRangeCommit;  // on release
  std::function<bool(rack::math::Vec pos)> onRangeRightClick;                  // host menu; true = consumed
  // ── UNIFIED ADDRESSING Stage 4: the OUTPUT MASK on the strip ────────────────
  // When the row's effective mask kind is PAINTED the strip becomes the paint
  // surface (the spec: lit = play, dim = mute) and a CLICK (press->release with no
  // window drag) toggles the cell under the cursor through onMaskToggle — the host
  // writes the store (one Rack history action per toggle). LOR drags still author
  // the window (press->move = window gesture; press->release = paint). The host
  // pushes the EFFECTIVE pattern (delegation-resolved) each frame; edits are
  // blocked while the row follows Macro (the delegation discipline).
  // STEPS_32 Stage 2a: the plane is the DECIDED uint64 integer representation,
  // ring-masked. The display cells are STEP_COUNT (32) wide — the FULL phrase
  // grid — and the mask plane holds the ring-wide pattern with the same domain
  // (the old "visible 16" note was stale; the Stage-2b flip widened the host push,
  // and the grid draws every column).
  bool     maskActive  = false;
  uint64_t maskPattern = dotModular::kMaskLenMask;
  std::function<void(int cell, bool play)> onMaskToggle;
  // The stroke's end (release) — the host pushes ONE Rack history action for the
  // whole stroke (click or drag); onMaskToggle applied the cells live.
  std::function<void()> onMaskStrokeEnd;
  void setRangeMask(bool active, uint64_t pattern) {
    maskActive  = active;
    maskPattern = pattern & dotModular::kMaskLenMask;
  }
  bool maskCellPlays(int cell) const {
    return (cell >= 0 && cell < STEP_COUNT) && ((maskPattern >> cell) & 1ULL) != 0ULL;
  }
  // Per-frame display push (host). While a drag is live the window L/O/R stays the
  // DRAG's local values (the store push lags one control block; overwriting would
  // fight the hand) — readPos/dir/blocked still update.
  void setRangeState(int L, int O, int R, int readPos, int dir, bool blocked, const char* label) {
    rangeStrip.enabled = true;
    if (!stripDragLive()) {   // (§PLAN 1) the reducer's engagement guards the window push
      rangeStrip.lor[0] = std::max(1, std::min(L, STEP_COUNT));
      rangeStrip.lor[1] = ((O % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
      rangeStrip.lor[2] = ((R % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
    }
    rangeStrip.readPos = readPos;
    rangeStrip.dir     = (dir < 0) ? -1 : 1;
    rangeStrip.blocked = blocked;
    if (label) rangeStrip.label = label;
  }
  // Strip window membership (mirrors ProbabilityLane::barInWindow on the display row).
  bool rangeBarInWindow(int bar) const {
    const int s = rangeStrip.lor[1];
    const int e = (s + std::max(1, std::min(rangeStrip.lor[0], STEP_COUNT)) - 1) % STEP_COUNT;
    bar = ((bar % STEP_COUNT) + STEP_COUNT) % STEP_COUNT;
    return (e >= s) ? (bar >= s && bar <= e) : (bar >= s || bar <= e);
  }
  

  // Hover tracking for discoverability: which lane + which window zone the cursor
  // is over (reuses DragState::Type). draw() highlights the hovered zone.
  int hoverLane = -1;
  DragState::Type hoverZone = DragState::NONE;

  // Cursor cues. API (confirmed): glfwSetCursor(APP->window->win, cursor) with
  // cursor from glfwCreateStandardCursor(...). Cached (created once) to avoid
  // per-frame allocation; destroyed in the dtor; reset to nullptr on leave so the
  // cursor never sticks outside the editor.
  GLFWcursor* curResize = nullptr;   // START/END edges → horizontal resize
  GLFWcursor* curHand   = nullptr;   // WINDOW body / ROTATION → hand (movable)
  void applyCursor(DragState::Type z) {
    if (!curResize) curResize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    if (!curHand)   curHand   = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    GLFWcursor* c = nullptr;
    if (z == DragState::START || z == DragState::END)            c = curResize;
    else if (z == DragState::WINDOW || z == DragState::ROTATION) c = curHand;
    glfwSetCursor(APP->window->win, c);   // nullptr → default arrow
  }
  
  struct KeyboardState {
    int selectedLane = 0;
    // (the preset-panel flag RETIRED — ADDITION D; it was never set true, the panel was dead)
  } kbState;
  
  // Per-lane playhead — each lane can be at a different step
  // due to independent LENGTH / OFFSET / ROTATION per lane.
  // Set via setLanePlayStep(lane, step) from the widget each frame.
  // step = -1 means sequencer not running (no indicator drawn).
  int lanePlayStep[MAX_LANES] = {-1,-1,-1,-1,-1,-1,-1};   // 7 lanes (QMIX-widened)
  float activeStepAlpha = 0.f;
  int lanePlayDir[MAX_LANES] = {1,1,1,1,1,1,1};   // per-lane trail direction (+1 fwd / -1 rev); Mode E and per-lane
  // Set all lanes at once (Mode E global cue). Per-lane callers use setLanePlayDir.
  void setPlayDir(int d) { int s = (d < 0) ? -1 : +1; for (int l = 0; l < MAX_LANES; ++l) lanePlayDir[l] = s; }
  void setLanePlayDir(int lane, int d) { if (lane >= 0 && lane < MAX_LANES) lanePlayDir[lane] = (d < 0) ? -1 : +1; }

  // Convenience: set all active lanes to the same global step
  // (used when L/O/R is not yet available)
  void setGlobalPlayStep(int step) {
    for (int l = 0; l < MAX_LANES; ++l) lanePlayStep[l] = step;
  }

  void setLanePlayStep(int lane, int step) {
    if (lane >= 0 && lane < MAX_LANES) lanePlayStep[lane] = step;
  }

  // Reset all playhead positions. Call when the source (Monsoon) is disconnected
  // or stops, so the highlight doesn't freeze on a stale step. drawLanePlayheads
  // skips lanes with step < 0, and step() drops activeStepAlpha to baseline once
  // no lane is active, so the editor reads as idle rather than stuck.
  void clearPlaySteps() {
    for (int l = 0; l < MAX_LANES; ++l) lanePlayStep[l] = -1;
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
    // UNIFIED ADDRESSING Stage 3: the voice-range strip reserved ABOVE lane 0
    // (px; 0 = absent). The host grows the editor BOX by this height so the
    // probability lanes keep their full height (no shrinking — the spec's rule):
    // laneAreaH() subtracts the strip, and lane 0 starts at topPadding + rangeLaneH.
    float rangeLaneH = 0.f;

    float laneAreaH() const { return std::max(1.f, boxSize.y - topPadding - botPadding - rangeLaneH); }
    float laneHeightF() const { return laneAreaH() / std::max(1, laneCount); }
    float gridW() const { return std::max(1.f, boxSize.x - 2.f * padding); }
    float stepWidthF() const { return gridW() / (float)STEP_COUNT; }
    float handleWidthF() const { return std::min(stepWidthF() * 0.8f, laneHeightF() * 0.5f); }

    // Back-compat accessors used throughout the file:
    float laneHeight  = 30.f;  // kept as fields for any external refs (unused internally now)
    float stepWidth   = 30.f;
    float handleWidth = 12.f;

    float getLaneY(int lane) const { return topPadding + rangeLaneH + lane * laneHeightF(); }
    float getStepX(int step) const { return padding + step * stepWidthF(); }
    float getStepCenterX(int step) const { return getStepX(step) + stepWidthF() / 2.f; }
    float getLaneCenterY(int lane) const { return getLaneY(lane) + laneHeightF() / 2.f; }

    rack::Rect getLaneRect(int lane) const {
      return rack::Rect(padding, getLaneY(lane), gridW(), laneHeightF());
    }
    rack::Rect getStepRect(int lane, int step) const {
      return rack::Rect(getStepX(step), getLaneY(lane), stepWidthF(), laneHeightF());
    }
    // The voice-range strip (Stage 3): spans the grid width at the top of the box.
    rack::Rect getRangeLaneRect() const {
      return rack::Rect(padding, topPadding, gridW(), rangeLaneH);
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
  }

  ~SandsVisualEditorV4() {
    if (curResize) glfwDestroyCursor(curResize);
    if (curHand)   glfwDestroyCursor(curHand);
  }
  
  // Explicit lane-count override. 0 = derive from mode (the original behaviour, unchanged).
  // Set to 6 by East when it shows the extra VARIATION/LEGATO lanes.
  int laneCountOverride = 0;
  void setLaneCount(int n) { laneCountOverride = n; laneCount = n; }

  void setMode(Mode m) {
    mode = m;
    // Phase 2: POLY mode now shows 5 lanes (added q-mix), MONO shows 7 (was 6).
    laneCount = laneCountOverride ? laneCountOverride : ((mode == MONO) ? 7 : 5);
    // NOTE: box.size is owned by the module that creates this editor (it sets
    // box.size = mm2px(ED_W, ED_H)). The layout derives lane height from
    // box.size.y / laneCount, so we must NOT force a hardcoded height here —
    // doing so previously made poly lanes ~30px tall regardless of the module's
    // intended (shorter, mono-like) editor height.
  }
  
  void resetState() {
    for (int l = 0; l < MAX_LANES; ++l) {
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
  
  // (the preset seeding + the preset load path RETIRED — ADDITION D, with the bank)
  
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
    // sets inert each frame from cachedPolyVoiceExpander == nullptr.
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

    if (showControlBar) drawControlBar(args.vg);

    drawRangeStrip(args.vg);

    for (int lane = 0; lane < laneCount; ++lane) {
      drawLaneLabel(args.vg, lane);
      for (int step = 0; step < STEP_COUNT; ++step) {
        drawStep(args.vg, lane, step);
      }
      drawHandles(args.vg, lane);
      drawRotationIndicator(args.vg, lane);
    }
    
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
  
  // ── UNIFIED ADDRESSING Stage 3: the voice-range strip ────────────────────────
  // A thin constant-block lane above lane 0. EVERY cell is the same height (flat
  // fill — the range, not a probability magnitude); lit = in the effective window,
  // dim = out. The read cell glows brighter and carries the leading-edge bar (the
  // walk direction, lanePlayDir idiom) — the strip animates with the voice's read
  // walk. Start/end edge bars mark the window like drawStep's range markers.
  void drawRangeStrip(NVGcontext* vg) {
    if (layout.rangeLaneH <= 0.f || !rangeStrip.enabled) return;
    const rack::Rect r = layout.getRangeLaneRect();

    // Silver/white — the voice FRAME is hue-neutral (a voice spans all seven lane
    // hues); luminance carries the state, exactly like the rotation chevrons.
    const NVGcolor lit  = nvgRGBAf(1.f, 1.f, 1.f, 0.14f);
    const NVGcolor dim  = nvgRGBAf(1.f, 1.f, 1.f, 0.045f);
    const NVGcolor edge = nvgRGBAf(1.f, 1.f, 1.f, rangeStrip.blocked ? 0.45f : 0.85f);

    for (int step = 0; step < STEP_COUNT; ++step) {
      rack::Rect cell = rack::Rect(layout.getStepX(step), r.pos.y, layout.stepWidthF(), r.size.y);
      // Stage 4: when the effective mask is PAINTED the strip shows the PRESENCE
      // figure (lit = the mask plays this step, dim = mute — the paint surface);
      // otherwise it shows the range window (lit = in-window).
      const bool inWin = maskActive ? maskCellPlays(step) : rangeBarInWindow(step);
      nvgBeginPath(vg);
      nvgRect(vg, cell.pos.x, cell.pos.y, cell.size.x, cell.size.y);
      nvgFillColor(vg, inWin ? lit : dim);
      nvgFill(vg);
      // window edge markers (the drawStep idiom, on the strip's own row). (B) The
      // grab ribbons are drawn at the SAME width as the hit zones (edgeZoneW — the
      // doc's "draw the handles and grab zones from the same constants"), so what
      // reads grabbable IS grabbable: a crisp bar at the boundary + a faint ribbon
      // one cell in (and the zone extends one cell OUT too — hitTestRange).
      const int s = rangeStrip.lor[1];
      const int e = (s + std::max(1, std::min(rangeStrip.lor[0], STEP_COUNT)) - 1) % STEP_COUNT;
      const float edgeW = redDot::SandsRangeGestureCore::edgeZoneW(layout.stepWidthF());
      // (bug 10) a 1-WIDE window's cell IS the body; its edge RIBBONS are the
      // OUTSIDE bands (matching the hit zones). The crisp 2.5px boundary bars
      // stay ON the cell's own edges either way (the drawStep idiom: both edges
      // show on the same cell unambiguously).
      const bool oneWide = (s == e);
      if (step == s) {
        nvgBeginPath(vg); nvgRect(vg, oneWide ? cell.pos.x - edgeW : cell.pos.x, cell.pos.y, edgeW, cell.size.y);
        NVGcolor ribbon = edge; ribbon.a *= 0.28f;
        nvgFillColor(vg, ribbon); nvgFill(vg);
        nvgBeginPath(vg); nvgRect(vg, cell.pos.x, cell.pos.y, 2.5f, cell.size.y);
        nvgFillColor(vg, edge); nvgFill(vg);
      }
      if (step == e) {
        nvgBeginPath(vg); nvgRect(vg, oneWide ? cell.pos.x + cell.size.x : cell.pos.x + cell.size.x - edgeW, cell.pos.y, edgeW, cell.size.y);
        NVGcolor ribbon = edge; ribbon.a *= 0.28f;
        nvgFillColor(vg, ribbon); nvgFill(vg);
        nvgBeginPath(vg); nvgRect(vg, cell.pos.x + cell.size.x - 2.5f, cell.pos.y, 2.5f, cell.size.y);
        nvgFillColor(vg, edge); nvgFill(vg);
      }
    }

    // The read cell: the voice's current buffer cell, glowing + leading-edge bar.
    if (rangeStrip.readPos >= 0 && rangeStrip.readPos < STEP_COUNT) {
      rack::Rect cell = rack::Rect(layout.getStepX(rangeStrip.readPos), r.pos.y,
                                   layout.stepWidthF(), r.size.y);
      nvgBeginPath(vg);
      nvgRect(vg, cell.pos.x, cell.pos.y, cell.size.x, cell.size.y);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.38f * activeStepAlpha));
      nvgFill(vg);
      // bright top-edge tick (the drawLanePlayheads idiom)
      nvgBeginPath(vg);
      nvgRect(vg, cell.pos.x + 1, cell.pos.y, cell.size.x - 2, 2.f);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.9f * activeStepAlpha));
      nvgFill(vg);
      // directional leading-edge bar on the side the walk is travelling TOWARD
      const float ex = (rangeStrip.dir < 0) ? cell.pos.x + 0.5f
                                            : cell.pos.x + cell.size.x - 2.5f;
      nvgBeginPath(vg);
      nvgRect(vg, ex, cell.pos.y, 2.f, cell.size.y);
      nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.8f * activeStepAlpha));
      nvgFill(vg);
    }

    // (C) VOICE-LOR PARITY: the ROTATION chevron at the LANES' placement — physical
    // bar = (offset + rotation) % STEP_COUNT, exactly drawRotationIndicator's law
    // (R is window-relative and clamped to [0, len−1] by the drag, so the chevron
    // is ALWAYS inside the window; the old absolute-R write let it leave — picture
    // 4). Hue-neutral silver (the strip's palette — the voice frame spans all
    // seven lane hues). Sits over the ROTATION gesture band (the strip's lower
    // 35%, hitTestRange's zone) so it reads as the grab target it is.
    {
      const int rotBar = (rangeStrip.lor[1]
                          + (rangeStrip.lor[2] % std::max(1, rangeStrip.lor[0]))) % STEP_COUNT;
      rack::Rect cell = rack::Rect(layout.getStepX(rotBar), r.pos.y,
                                   layout.stepWidthF(), r.size.y);
      const float cx = cell.pos.x + cell.size.x * 0.5f;
      const float cyc = r.pos.y + r.size.y * 0.5f;
      const float h = r.size.y * 0.9f;
      const float d = 2.6f;
      const float halfW = (cell.size.x - 2.f) * 0.42f;
      const float on = (rangeStrip.lor[2] > 0) ? 1.f : 0.55f;

      // block baseline tint — DARKEN the rotation cell (the lane marker's law)
      nvgBeginPath(vg);
      nvgRect(vg, cell.pos.x + 1.f, r.pos.y, cell.size.x - 2.f, r.size.y);
      NVGcolor blockTint = nvgRGB(0x00, 0x00, 0x00);
      blockTint.a = (rangeStrip.lor[2] > 0) ? 0.32f : 0.16f;
      nvgFillColor(vg, blockTint);
      nvgFill(vg);

      // chevrons ›‹ — the path built once, stroked twice (dark halo under, white core over)
      auto chevrons = [&]() {
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - halfW - d, cyc - h * 0.5f);
        nvgLineTo(vg, cx - halfW + d, cyc);
        nvgLineTo(vg, cx - halfW - d, cyc + h * 0.5f);
        nvgMoveTo(vg, cx + halfW + d, cyc - h * 0.5f);
        nvgLineTo(vg, cx + halfW - d, cyc);
        nvgLineTo(vg, cx + halfW + d, cyc + h * 0.5f);
      };
      nvgLineCap(vg, NVG_ROUND);
      nvgLineJoin(vg, NVG_ROUND);
      chevrons();
      { NVGcolor halo = nvgRGB(0x0a, 0x0c, 0x10); halo.a = 0.60f * on;
        nvgStrokeColor(vg, halo); nvgStrokeWidth(vg, 3.0f); nvgStroke(vg); }
      chevrons();
      { NVGcolor core = nvgRGB(0xf2, 0xf2, 0xf6); core.a = 0.95f * on;
        nvgStrokeColor(vg, core); nvgStrokeWidth(vg, 1.4f); nvgStroke(vg); }
    }

    // Separator under the strip (the live editor shows strip + lanes as 8 rows).
    nvgBeginPath(vg);
    nvgRect(vg, 0, r.pos.y + r.size.y - 0.5f, box.size.x, 0.75f);
    nvgFillColor(vg, colors.border);
    nvgFill(vg);

    // Caption (right-aligned like the lane labels; the host sets the text).
    if (rangeStrip.label && rangeStrip.label[0]) {
      nvgFontSize(vg, 8.f);
      nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, rangeStrip.blocked ? nvgRGB(0x66, 0x66, 0x66)
                                          : nvgRGBAf(1.f, 1.f, 1.f, 0.8f));
      nvgText(vg, layout.padding - 8, r.pos.y + r.size.y * 0.5f, rangeStrip.label, nullptr);
    }
  }
  
  void drawLaneLabel(NVGcontext* vg, int lane) {
    // Long lane names from the canonical table (unified order; length-asserted in
    // LaneMapping). Was a local literal — the exact per-file-copy drift class.
    const char* name = (lane >= 0 && lane < MAX_LANES) ? dotModular::SandsLaneNames::EDITOR_LONG[lane] : "";
    // PURELY VERTICAL fix: X / right-align / left-of-grid position unchanged (never the problem).
    // Was at the lane CENTRE (getLaneCenterY) which is the same height as the mod/spread knobs to
    // the left, so the label sliced through them. Move Y UP to sit ABOVE the knob row — near the
    // lane's top edge, nudged up a few px into the gap above the row.
    float y = layout.getLaneY(lane) + 4.f;

    nvgFontSize(vg, 9.f);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, (lane == kbState.selectedLane) ? getLaneColor(lane) : nvgRGB(0x66, 0x66, 0x66));
    nvgText(vg, layout.padding - 8, y, name, nullptr);
  }
  
  void drawStep(NVGcontext* vg, int lane, int step) {
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect rect  = layout.getStepRect(lane, step);
    float prob       = L.probabilities[step];
    float barHeight  = prob * rect.size.y;
    // Under lock with a moved LOR, the SOLID window follows the LIVE edit (the handle the user is
    // moving); the committed/display window becomes a ghost echo (drawn below). Otherwise the solid
    // window is the display window (so CV modulation shows normally when unlocked).
    const bool ghost = lorGhost(lane);
    bool  isInWindow = ghost ? L.editBarInWindow(step) : L.barInWindow(step);
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
    const int solidStart = ghost ? L.editStartBar() : L.startBar();
    const int solidEnd   = ghost ? L.editEndBar()   : L.endBar();
    if (step == solidStart) {
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x, rect.pos.y, 2.5f, rect.size.y);
      nvgFillColor(vg, edge);
      nvgFill(vg);
    }
    if (step == solidEnd) {
      nvgBeginPath(vg);
      nvgRect(vg, rect.pos.x + rect.size.x - 2.5f, rect.pos.y, 2.5f, rect.size.y);
      nvgFillColor(vg, edge);
      nvgFill(vg);
    }

    // Ghost echo: under lock with a moved LOR, mark the COMMITTED (still-playing) window as a faint
    // trace so the user sees where the playhead is held while the live (solid) window shows where it
    // will land at unlock. Committed window = barInWindow/startBar/endBar (display L/O, frozen).
    if (ghost) {
      if (L.barInWindow(step)) {
        nvgBeginPath(vg);
        nvgRect(vg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
        nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.05f));
        nvgFill(vg);
      }
      NVGcolor gedge = getLaneColor(lane); gedge.a = 0.30f;   // faint = echo
      if (step == L.startBar()) {
        nvgBeginPath(vg);
        nvgRect(vg, rect.pos.x, rect.pos.y, 2.5f, rect.size.y);
        nvgFillColor(vg, gedge); nvgFill(vg);
      }
      if (step == L.endBar()) {
        nvgBeginPath(vg);
        nvgRect(vg, rect.pos.x + rect.size.x - 2.5f, rect.pos.y, 2.5f, rect.size.y);
        nvgFillColor(vg, gedge); nvgFill(vg);
      }
    }
  }
  
  // ── Handle hit testing ──────────────────────────────────────────────────────
  // Zones within a lane (top-to-bottom):
  //   top ~45%  : START handle (at offset cell) / END handle (at end cell) circles
  //   bottom ~30%: ROTATION strip — drag anywhere along it to set rotation
  //   middle    : bar value (probability) drag
  // Separating rotation into its own strip means it never collides with the
  // start/end handles and is grabbable even when rotation == 0.
  // Shared START/END edge-zone width (px) for a lane's window. Generous (a full
  // cell, min ~4mm) so it's grabbable, but capped at 1/3 of the window span so a
  // middle-third MOVE zone ALWAYS exists — even for a 2-cell window. Used by both
  // hitTestHandle and drawHandles so the drawn ribbons match the grab zones.
  // (bug 10) a 1-WIDE window has no in-window middle to preserve — its edges are
  // the OUTSIDE bands (Rodney's ruling), so the cap does not apply at length 1.
  float windowEdgeW(const ProbabilityLane& L) const {
    const int lenC = std::max(1, std::min(L.length, STEP_COUNT));
    const float want = std::max(layout.stepWidthF(), mm2px(4.f));
    if (lenC <= 1) return want;
    const float spanPx = lenC * layout.stepWidthF();
    return std::min(want, spanPx / 3.f);
  }

  DragState::Type hitTestHandle(int lane, float x, float y) const {
    if (lane < 0 || lane >= laneCount) return DragState::NONE;
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect laneR = layout.getLaneRect(lane);
    const int startBar = L.editStartBar();
    const int endBar   = L.editEndBar();
    float halfStep = layout.stepWidthF() * 0.5f;

    // The ROTATION band's extent (the window's INCLUSIVE cell run — unchanged
    // from the old test: the whole start–end span, wrapped or contiguous).
    auto inWindowX = [&](float px)->bool {
      float sxL = layout.getStepCenterX(startBar) - halfStep;
      float exR = layout.getStepCenterX(endBar)   + halfStep;
      if (endBar >= startBar) return (px >= sxL && px <= exR);          // contiguous
      float leftEdge  = layout.getStepCenterX(0) - halfStep;            // wrapped
      float rightEdge = layout.getStepCenterX(STEP_COUNT - 1) + halfStep;
      return (px >= sxL && px <= rightEdge) || (px >= leftEdge && px <= exR);
    };

    // (bugs 9+10, LOR_EDITING_BUG_LIST) the ONE window-relative classifier — the
    // same law the strip band uses (classifyWindow): bounded edge bands on EACH
    // side of each boundary, wrap-correct, the body ALWAYS non-empty (edgeW
    // capped at span/3), and a LENGTH-1 window's cell IS the body with the edges
    // one cell OUTSIDE it (Rodney's ruling — accent, length 1, is movable).
    // The old inline test was open-ended (`x >= endZoneL`): a WRAPPED window's
    // END zone swallowed its whole front arc — the "dragged past step 32, cannot
    // be picked up to move again" report (bug 9); and the old 1-wide half-split
    // left a length-1 window with no body at all (bug 10).
    //   TOP ~65% of the lane = the window band: START / END / MOVE;
    //   BOTTOM ~35% = ROTATION (the inclusive window span, unchanged).
    if (y >= laneR.pos.y && y <= laneR.pos.y + laneR.size.y) {
      const bool topBand = (y <= laneR.pos.y + laneR.size.y * 0.65f);
      if (!topBand)
        return inWindowX(x) ? DragState::ROTATION : DragState::NONE;
      const float startBoundX = layout.getStepX(startBar);
      const float endBoundX   = layout.getStepX(endBar) + layout.stepWidthF();
      const float gridL = layout.padding;
      const float gridR = layout.padding + layout.gridW();
      const float stepW = layout.stepWidthF();
      const int   lenC  = std::max(1, std::min(L.length, STEP_COUNT));
      switch (redDot::SandsRangeGestureCore::classifyWindow(
                 x, startBoundX, endBoundX, windowEdgeW(L), gridL, gridR,
                 lenC * stepW, stepW)) {
        case redDot::SandsRangeGestureCore::Z_START:  return DragState::START;
        case redDot::SandsRangeGestureCore::Z_END:    return DragState::END;
        case redDot::SandsRangeGestureCore::Z_WINDOW: return DragState::WINDOW;
        default:                                      return DragState::NONE;
      }
    }

    return DragState::NONE;
  }

  // ── Stage 3: the voice-range strip hit test. Same zone shape as a probability
  // lane (top 65% = window strip: START / END / MOVE; bottom 35% = ROTATION),
  // over the strip's display window, restricted to the strip rect. NONE when the
  // strip is absent (rangeLaneH == 0), disabled, or blocked (follow-Macro).
  DragState::Type hitTestRange(float x, float y) const {
    if (layout.rangeLaneH <= 0.f || !rangeStrip.enabled) return DragState::NONE;
    const rack::Rect r = layout.getRangeLaneRect();
    // (E) the boundary row (y == the band's bottom) belongs to the BAND — the same
    // seam rule as the press branch's y-gate (<=), so a seam press classifies a zone
    // instead of arming paint.
    if (y < r.pos.y || y > r.pos.y + r.size.y) return DragState::NONE;
    const int startBar = rangeStrip.lor[1];
    const int endBar   = (startBar + std::max(1, std::min(rangeStrip.lor[0], STEP_COUNT)) - 1) % STEP_COUNT;
    // (B + bugs 9/10) THE WINDOW-RELATIVE CLASSIFIER (classifyWindow): the
    // START/END zones are bounded bands edgeZoneW (>= max(1 cell, 8 px)) on EACH
    // side of the edge boundary — one cell outside the window AND one cell in —
    // so a press within one cell of an edge grabs the EDGE, never the window
    // body; the body takes the rest and is ALWAYS non-empty (span/3 cap; a
    // length-1 window's cell IS the body, its edges one cell OUTSIDE — Rodney's
    // ruling). The math lives in SandsRangeGestureCore (rack-free, the testable
    // surface); the drawn ribbons use the same constant (drawRangeStrip).
    const float startBoundX = layout.getStepX(startBar);
    const float endBoundX   = layout.getStepX(endBar) + layout.stepWidthF();
    const float edgeW = redDot::SandsRangeGestureCore::edgeZoneW(layout.stepWidthF());
    const float gridL = layout.padding;
    const float gridR = layout.padding + layout.gridW();
    const float stepW = layout.stepWidthF();
    const float spanPx = std::max(1, std::min(rangeStrip.lor[0], STEP_COUNT)) * stepW;
    float bandBot = r.pos.y + r.size.y * 0.65f;
    if (y <= bandBot) {
      switch (redDot::SandsRangeGestureCore::classifyWindow(
                  x, startBoundX, endBoundX, edgeW, gridL, gridR, spanPx, stepW)) {
        case redDot::SandsRangeGestureCore::Z_START:  return DragState::START;
        case redDot::SandsRangeGestureCore::Z_END:    return DragState::END;
        case redDot::SandsRangeGestureCore::Z_WINDOW: return DragState::WINDOW;
        default: return DragState::NONE;
      }
    }
    // The ROTATION band stays strictly in-window (R is window-relative).
    return redDot::SandsRangeGestureCore::inBody(x, startBoundX, endBoundX, gridL, gridR)
               ? DragState::ROTATION : DragState::NONE;
  }

  // Top "move strip": a thin band along the lane top across the in-window cells.
  // It's the grab-to-move affordance (drag it to slide the whole window). The
  // range extent itself is now shown by the per-cell shading + the start/end edge
  // bars drawn in drawStep, so this strip only needs to read as "grab here".
  void drawHandles(NVGcontext* vg, int lane) {
    const ProbabilityLane& L = currentState.lanes[lane];
    rack::Rect laneR = layout.getLaneRect(lane);
    float bandTop = laneR.pos.y;
    float bandH   = laneR.size.y * 0.65f;       // matches the hit-zone band
    int lenC = std::max(1, std::min(L.length, STEP_COUNT));

    // Window body fill across the EDIT range (matches inWindowX / the MOVE hit
    // zone). Iterate from editStartBar so the fill = the grabbable body.
    // Under lock+diverged (ghost), the live edit window is the SOLID one -> stronger body/edge.
    const bool ghostHandles = lorGhost(lane);
    NVGcolor body = nvgRGBAf(1.f, 1.f, 1.f, ghostHandles ? 0.20f : 0.10f);
    for (int k = 0; k < lenC; ++k) {
      int bar = (L.editStartBar() + k) % STEP_COUNT;
      rack::Rect c = layout.getStepRect(lane, bar);
      nvgBeginPath(vg);
      nvgRect(vg, c.pos.x, bandTop, c.size.x, bandH);
      nvgFillColor(vg, body);
      nvgFill(vg);
    }

    // START / END edge ribbons — drawn at the actual grab-zone width so the user
    // can SEE where to grab (was an invisible 30%-cell zone before).
    // Edge ribbons use the EDIT bars (editStartBar/editEndBar) so the drawn grab
    // affordance lands exactly where hitTestHandle grabs — even while CV is
    // modulating the DISPLAYED window (which the probability bars/playhead show).
    float edgeW = windowEdgeW(L);
    rack::Rect cs = layout.getStepRect(lane, L.editStartBar());
    rack::Rect ce = layout.getStepRect(lane, L.editEndBar());
    bool oneWide = (L.editStartBar() == L.editEndBar());
    NVGcolor edge = getLaneColor(lane); edge.a = ghostHandles ? 0.95f : 0.55f;

    auto fillRect = [&](float x, float w, NVGcolor col) {
      nvgBeginPath(vg); nvgRect(vg, x, bandTop, w, bandH);
      nvgFillColor(vg, col); nvgFill(vg);
    };
    if (oneWide) {
      // (bug 10, Rodney's ruling) a 1-wide window's cell IS the body (MOVE); the
      // edge ribbons are the OUTSIDE bands — matching the hit zones exactly.
      fillRect(cs.pos.x - edgeW, edgeW, edge);        // START = the left outside band
      fillRect(cs.pos.x + cs.size.x, edgeW, edge);    // END   = the right outside band
    } else {
      fillRect(cs.pos.x, edgeW, edge);                            // START ribbon
      fillRect(ce.pos.x + ce.size.x - edgeW, edgeW, edge);        // END ribbon
    }

    // Hover highlight: brighten whichever zone the cursor is in.
    if (hoverLane == lane && hoverZone != DragState::NONE) {
      NVGcolor hi = getLaneColor(lane); hi.a = 0.32f;
      if (hoverZone == DragState::START) {
        fillRect(oneWide ? cs.pos.x - edgeW : cs.pos.x, edgeW, hi);
      } else if (hoverZone == DragState::END) {
        if (oneWide) fillRect(cs.pos.x + cs.size.x, edgeW, hi);
        else         fillRect(ce.pos.x + ce.size.x - edgeW, edgeW, hi);
      } else if (hoverZone == DragState::WINDOW) {
        for (int k = 0; k < lenC; ++k) {
          int bar = (L.editStartBar() + k) % STEP_COUNT;
          rack::Rect c = layout.getStepRect(lane, bar);
          fillRect(c.pos.x, c.size.x, hi);
        }
      }
      // ROTATION hover is highlighted by drawRotationIndicator already.
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
    NVGcolor track = nvgRGB(0x00, 0x00, 0x00); track.a = 0.07f;   // faint neutral darkening (was teal)
    for (int k = 0; k < lenC; ++k) {
      int bar = (startBar + k) % STEP_COUNT;
      rack::Rect c = layout.getStepRect(lane, bar);
      nvgBeginPath(vg);
      nvgRect(vg, c.pos.x, stripTop, c.size.x, stripH);
      nvgFillColor(vg, track);
      nvgFill(vg);
    }

    // Rotation marker sits WITHIN the start-end window: physical bar =
    // (dispOffset + dispRotation) % STEP_COUNT. Uses DISPLAY values so the chevron moves
    // with the modulated window.
    int rotBar = (L.dispOffset + (L.dispRotation % std::max(1, L.dispLength))) % STEP_COUNT;
    rack::Rect cell = layout.getStepRect(lane, rotBar);
    float cx = cell.pos.x + cell.size.x * 0.5f;
    float cyc = stripTop + stripH * 0.5f;
    float h = stripH * 0.9f;          // chevron height (~0.27 of lane height, up from ~0.21)
    float d = 3.f;                    // chevron arm depth (horizontal), up from 2
    float halfW = (cell.size.x - 2.f) * 0.42f;

    // Phase marker, hue-INDEPENDENT so it reads on every lane — including legato, whose hue
    // IS colors.rotation (teal), where the old fixed-teal marker vanished. Any single hue
    // collides with the lane sharing it, so use luminance contrast instead: darken the cell
    // (works over any colour) and draw the chevrons as a white core with a dark halo (legible
    // over bright OR dark fills). Brighter when rotation > 0.
    const float on = (L.rotation > 0) ? 1.f : 0.55f;

    // block baseline tint — DARKEN the rotation cell (universal across lane hues)
    nvgBeginPath(vg);
    nvgRect(vg, cell.pos.x + 1.f, stripTop, cell.size.x - 2.f, stripH);
    NVGcolor blockTint = nvgRGB(0x00, 0x00, 0x00); blockTint.a = (L.rotation > 0) ? 0.32f : 0.16f;
    nvgFillColor(vg, blockTint);
    nvgFill(vg);

    // chevrons ›‹ — path built once, stroked twice (dark halo under, white core over)
    auto chevrons = [&]() {
      nvgBeginPath(vg);
      nvgMoveTo(vg, cx - halfW - d, cyc - h * 0.5f);   // left "›" toward centre
      nvgLineTo(vg, cx - halfW + d, cyc);
      nvgLineTo(vg, cx - halfW - d, cyc + h * 0.5f);
      nvgMoveTo(vg, cx + halfW + d, cyc - h * 0.5f);   // right "‹" toward centre
      nvgLineTo(vg, cx + halfW - d, cyc);
      nvgLineTo(vg, cx + halfW + d, cyc + h * 0.5f);
    };
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    chevrons();                                          // halo
    { NVGcolor halo = nvgRGB(0x0a, 0x0c, 0x10); halo.a = 0.60f * on;
      nvgStrokeColor(vg, halo); nvgStrokeWidth(vg, 3.4f); nvgStroke(vg); }
    chevrons();                                          // white core
    { NVGcolor core = nvgRGB(0xf2, 0xf2, 0xf6); core.a = 0.95f * on;
      nvgStrokeColor(vg, core); nvgStrokeWidth(vg, 1.6f); nvgStroke(vg); }
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

      // Directional leading-edge marker: a bright vertical bar on the side the
      // playhead is travelling TOWARD (right edge when forward, left when reverse).
      // Uses THIS lane's direction (lanePlayDir[l]) so a reversed / pendulum lane reads
      // backward at a glance, independent of the others and of global Mode E.
      {
        float ex = (lanePlayDir[l] < 0) ? rect.pos.x + 0.5f
                                        : rect.pos.x + rect.size.x - 2.5f;
        nvgBeginPath(vg);
        nvgRect(vg, ex, rect.pos.y, 2.f, rect.size.y);
        nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.8f * activeStepAlpha));
        nvgFill(vg);
      }
    }
  }
  
  // (the preset panel's draw RETIRED — ADDITION D, with the bank; it was guarded by
  // a flag that was never set true — dead draw code)
  
  void onHover(const rack::event::Hover& e) override {
    Widget::onHover(e);
    if (inert) { hoverLane = -1; hoverZone = DragState::NONE; return; }
    syncLayout();
    // Stage 3: the strip gets the hover first (it sits above lane 0).
    DragState::Type rz = hitTestRange(e.pos.x, e.pos.y);
    if (rz != DragState::NONE) {
      hoverLane = -1; hoverZone = DragState::NONE;
      applyCursor(rangeStrip.blocked ? DragState::NONE : rz);
      e.consume(this);
      return;
    }
    if (readOnly) { hoverLane = -1; hoverZone = DragState::NONE; return; }
    int lane = getLaneAtY(e.pos.y);
    if (lane >= 0 && laneEditBlocked(lane)) { hoverLane = -1; hoverZone = DragState::NONE; return; }
    DragState::Type z = (lane >= 0 && lane < laneCount)
                        ? hitTestHandle(lane, e.pos.x, e.pos.y) : DragState::NONE;
    hoverLane = (z == DragState::NONE) ? -1 : lane;
    hoverZone = z;
    applyCursor(z);
    e.consume(this);
  }

  void onLeave(const rack::event::Leave& e) override {
    Widget::onLeave(e);
    hoverLane = -1;
    hoverZone = DragState::NONE;
    glfwSetCursor(APP->window->win, nullptr);   // CRITICAL: reset or it sticks globally
  }

  void onButton(const rack::event::Button& e) override {
    syncLayout();
    if (inert) return;   // no edit when inert (no poly src); readOnly blocks lanes, not the strip's menu
    // ── Stage 3: the voice-range strip (above lane 0) ──
    if (layout.rangeLaneH > 0.f && rangeStrip.enabled
        && e.pos.y >= layout.topPadding
        && e.pos.y <= layout.topPadding + layout.rangeLaneH) {
      // (E) the BAND owns its bottom boundary: a press exactly on the strip/lane
      // seam goes to the STRIP (the doc's rule — the band is hit-tested first).
      if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
        // THE UNIFIED GESTURE MODEL: ONE arm for every press — the press is PENDING
        // (pressArmed) carrying its cell + zone; onDragMove engages the gesture when
        // the cursor crosses into a new cell, and a release without a crossing is a
        // CLICK (the paint toggle of the press cell — in-window included, so a
        // full-window strip is paintable; the old press-time split could never arm
        // paint there because every press hit an LOR zone).
        if (!rangeStrip.blocked && !readOnly) {
          rangeStrip.pressArmed   = true;
          rangeStrip.pressCell    = getStepAtX(e.pos.x);
          rangeStrip.pressZone    = hitTestRange(e.pos.x, e.pos.y);
          rangeStrip.paintTarget  = -1;
          rangeStrip.paintApplied = 0u;
          rangeStrip.dragPos      = e.pos;
          e.consume(this);
          return;
        }
        e.consume(this);   // a press on a blocked strip stays the strip's
        return;
      }
      if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (onRangeRightClick && onRangeRightClick(e.pos)) { e.consume(this); return; }
        return;   // the strip consumes its own right-clicks either way
      }
      if (e.action == GLFW_RELEASE && rangeStrip.pressArmed) {
        // A pure CLICK (no cell crossed): toggle the press cell — ANY cell, in-window
        // or not. The host transitions the row to Painted on the first toggle
        // (seeded from the current effective mask); one stroke, one history action.
        rangeStrip.pressArmed = false;
        if (onMaskToggle && rangeStrip.pressCell >= 0 && rangeStrip.pressCell < STEP_COUNT) {
          if (rangeStrip.paintTarget < 0)
            rangeStrip.paintTarget = maskCellPlays(rangeStrip.pressCell) ? 0 : 1;
          onMaskToggle(rangeStrip.pressCell, rangeStrip.paintTarget != 0);
        }
        if (onMaskStrokeEnd) onMaskStrokeEnd();
        e.consume(this);
        return;
      }
      if (e.action == GLFW_RELEASE && stripDragLive()) {
        // (§PLAN 1) the translation: RELEASE through the reducer, then report the
        // before/after pair it kept for exactly this (the host's Rack-history
        // commit; window == the strip's lor, kept in sync by the moves).
        const int before[3] = { lorDrag.before[0], lorDrag.before[1], lorDrag.before[2] };
        const int after[3]  = { lorDrag.window[0], lorDrag.window[1], lorDrag.window[2] };
        const bool lorChanged = (after[0] != before[0] || after[1] != before[1] || after[2] != before[2]);
        redDot::SandsRangeGestureCore::LorDragEvent rel;
        rel.type = redDot::SandsRangeGestureCore::LorDragEvent::RELEASE;
        lorDrag  = redDot::SandsRangeGestureCore::lorDragReduce(lorDrag, rel, STEP_COUNT);
        if (lorChanged && onRangeCommit)
          onRangeCommit(before, after);
        e.consume(this);
        return;
      }
      if (e.action == GLFW_RELEASE && rangeStrip.paintPress) {
        rangeStrip.paintPress = false;
        // The drag-paint stroke ends — the host pushes ONE history action for the
        // whole stroke (the click case is handled by the pressArmed branch above).
        if (onMaskStrokeEnd) onMaskStrokeEnd();
        e.consume(this);
        return;
      }
      return;   // any other event over the strip: not ours to bubble into the lanes
    }
    if (readOnly) return;   // no edit when inert (no poly src) or readOnly (mono owns tab 1)
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
      int lane = getLaneAtY(e.pos.y);
      //int step = getStepAtX(e.pos.x);

      if (lane >= 0 && lane < laneCount) {
        if (laneEditBlocked(lane)) { e.consume(this); return; }  // P4: delegated lane — inoperable
        // Hit-test handles first — they have priority over bar dragging
        DragState::Type handleHit = hitTestHandle(lane, e.pos.x, e.pos.y);
        if (handleHit != DragState::NONE) {
          dragState.dragPos = e.pos;   // the pointer accumulator (the reducer owns the LOR state)
          // (§PLAN 1) the translation: the ENGAGED press through the reducer — it
          // captures the press cell, the press window (the WINDOW slide's anchor
          // and the edge drags' delta base, ADDITION E's law) and the before/
          // after pair for the release's commit report.
          redDot::SandsRangeGestureCore::LorDragEvent prs;
          prs.type = redDot::SandsRangeGestureCore::LorDragEvent::PRESS;
          prs.zone = coreZoneOf(handleHit);
          prs.owner = redDot::SandsRangeGestureCore::LorDragState::O_LANE;
          prs.laneIdx = lane;
          prs.step = getStepAtX(e.pos.x);
          prs.win[0] = currentState.lanes[lane].length;
          prs.win[1] = currentState.lanes[lane].offset;
          prs.win[2] = currentState.lanes[lane].rotation;
          lorDrag = redDot::SandsRangeGestureCore::lorDragReduce(lorDrag, prs, STEP_COUNT);
          e.consume(this);
          return;
        }

        // Per-cell probability editing was removed (never part of the plan): the
        // editor manipulates only the window (length/offset/rotation) via the
        // handles above. A click that isn't on a handle does nothing here, so we
        // don't steal it from the window gestures. (Archived editor with per-cell
        // editing: src/deprecated/SandsVisualEditorV4_with_percell_editing.hpp.)
      }
    } else if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      // Right-click: if host registered a lane right-click callback, invoke it.
      int lane = getLaneAtY(e.pos.y);
      if (lane >= 0 && lane < laneCount && onLaneRightClick) {
        if (onLaneRightClick(lane, e.pos)) {
          e.consume(this);
          return;
        }
      }
    } else if (e.action == GLFW_RELEASE) {
      if (laneDragLive()) {
        saveToHistory();
        // Report the LOR change to the host for Rack-history undo — the reducer's
        // before/after pair (§PLAN 1: the release keeps them for exactly this).
        const int lane = lorDrag.laneIdx;
        if (onLorCommit && lane >= 0 && lane < laneCount) {
          const int after[3] = { lorDrag.window[0], lorDrag.window[1], lorDrag.window[2] };
          if (after[0] != lorDrag.before[0] || after[1] != lorDrag.before[1]
              || after[2] != lorDrag.before[2]) {
            onLorCommit(lane, lorDrag.before.data(), after);
          }
        }
      }
      redDot::SandsRangeGestureCore::LorDragEvent rel;
      rel.type = redDot::SandsRangeGestureCore::LorDragEvent::RELEASE;
      lorDrag  = redDot::SandsRangeGestureCore::lorDragReduce(lorDrag, rel, STEP_COUNT);
    }
  }
  
  void onDragMove(const rack::event::DragMove& e) override {
    syncLayout();
    // ── THE PENDING PRESS (the unified gesture model): the cursor crossing into a
    // NEW cell engages the gesture the press zone picked — an LOR window gesture
    // in-window, the paint stroke out-of-window. Sub-cell jitter never engages
    // (a sloppy click stays a click); a press outside the step cells never engages.
    if (rangeStrip.pressArmed) {
      rangeStrip.dragPos = rangeStrip.dragPos.plus(e.mouseDelta);
      const int step = getStepAtX(rangeStrip.dragPos.x);
      if (rangeStrip.pressCell < 0 || step < 0 || step >= STEP_COUNT || step == rangeStrip.pressCell)
        return;   // still pending
      rangeStrip.pressArmed = false;
      if (rangeStrip.pressZone != DragState::NONE) {
        // (§PLAN 1) the translation: the crossing ENGAGES the gesture the press
        // zone picked — the reducer's PRESS at the PRESS cell with the press
        // window (A: the edge drags' delta base; the commit's before/after pair).
        redDot::SandsRangeGestureCore::LorDragEvent prs;
        prs.type = redDot::SandsRangeGestureCore::LorDragEvent::PRESS;
        prs.zone = coreZoneOf(rangeStrip.pressZone);
        prs.owner = redDot::SandsRangeGestureCore::LorDragState::O_STRIP;
        prs.laneIdx = 0;
        prs.step = rangeStrip.pressCell;
        prs.win[0] = rangeStrip.lor[0];
        prs.win[1] = rangeStrip.lor[1];
        prs.win[2] = rangeStrip.lor[2];
        lorDrag = redDot::SandsRangeGestureCore::lorDragReduce(lorDrag, prs, STEP_COUNT);
      } else {
        rangeStrip.paintPress = true;   // out-of-window: the drag-paint stroke
      }
      // fall through — the engaged branch (dragging / paintPress) applies this move
    }
    // ── Stage 4: the paint-stroke drag — every NEW cell crossed toggles (the first
    // cell latches the stroke's uniform target, like painting a rhythm); the local
    // paintApplied mask stops a cursor wiggle from double-toggling a cell before the
    // host's next pattern push lands.
    if (rangeStrip.paintPress) {
      rangeStrip.dragPos = rangeStrip.dragPos.plus(e.mouseDelta);
      const int step = getStepAtX(rangeStrip.dragPos.x);
      if (step >= 0 && step < STEP_COUNT && onMaskToggle
          && !((rangeStrip.paintApplied >> step) & 1u)) {
        rangeStrip.paintApplied |= (1u << step);
        if (rangeStrip.paintTarget < 0)
          rangeStrip.paintTarget = maskCellPlays(step) ? 0 : 1;
        onMaskToggle(step, rangeStrip.paintTarget != 0);
      }
      return;
    }
    // ── Stage 3: the voice-range strip drag (the lane gestures, one level up) ──
    if (stripDragLive()) {
      rangeStrip.dragPos = rangeStrip.dragPos.plus(e.mouseDelta);
      int step = getStepAtX(rangeStrip.dragPos.x);
      if (step < 0 || step >= STEP_COUNT) return;
      // (§PLAN 1) the translation: MOVE through the reducer, then copy the window
      // back — the reducer applies the A/C laws (the press-anchored edge deltas,
      // the slide law, the rotation clamp; the laws live in the core, pinned by
      // the headless suite); the widget only reports.
      redDot::SandsRangeGestureCore::LorDragEvent mv;
      mv.type = redDot::SandsRangeGestureCore::LorDragEvent::MOVE;
      mv.step = step;
      lorDrag = redDot::SandsRangeGestureCore::lorDragReduce(lorDrag, mv, STEP_COUNT);
      int* lor = rangeStrip.lor;
      lor[0] = lorDrag.window[0];
      lor[1] = lorDrag.window[1];
      lor[2] = lorDrag.window[2];
      if (onRangeEdit) onRangeEdit(lor[0], lor[1], lor[2]);
      return;
    }
    if (!laneDragLive()) return;
    // Rack 2: DragMove gives delta, not absolute pos. Accumulate.
    dragState.dragPos = dragState.dragPos.plus(e.mouseDelta);
    const int lane = lorDrag.laneIdx;
    int step = getStepAtX(dragState.dragPos.x);
    if (step < 0 || step >= STEP_COUNT) return;
    if (lane < 0 || lane >= laneCount) return;

    // (§PLAN 1) the translation: MOVE through the reducer, then copy the window
    // back to the lane. The reducer applies the SAME laws the strip uses (one
    // state machine, both owners — ADDITION E's separation holds by construction:
    // the owner was set at press and a LANE drag's window never reaches the strip
    // or onRangeEdit). The old lane branch's ROTATION case inlined the same
    // expression the strip's used; the reducer spells each law once.
    redDot::SandsRangeGestureCore::LorDragEvent mv;
    mv.type = redDot::SandsRangeGestureCore::LorDragEvent::MOVE;
    mv.step = step;
    lorDrag = redDot::SandsRangeGestureCore::lorDragReduce(lorDrag, mv, STEP_COUNT);
    ProbabilityLane& L = currentState.lanes[lane];
    L.length   = lorDrag.window[0];
    L.offset   = lorDrag.window[1];
    L.rotation = lorDrag.window[2];
    // Reflect the dragged edit values in the display immediately so the window
    // follows the hand; next process frame re-applies any CV on top.
    // BUT under lock, do NOT sync: the display must stay frozen at the committed (pre-lock)
    // position while the edit tracks the hand -- that divergence is what drives the ghost echo
    // (live=solid, committed=ghost). Syncing here would collapse the divergence and hide the ghost.
    if (!lockActive())
      L.syncDisplayToEdit();
  }
  
  
  NVGcolor getLaneColor(int lane) const {
    switch (lane) {
      case REST: return colors.rest;
      case MELODY: return colors.melody;
      case OCTAVE: return colors.octave;
      case QMIX: return colors.octave;   // QMIX shares the octave-family hue for now (own colour TBD)
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
