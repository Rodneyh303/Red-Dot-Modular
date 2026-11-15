#pragma once
// SandsLaneWindowCore — the lane window's DISPLAY MATH, extracted rack-free from
// SandsVisualEditorV4::ProbabilityLane (STEPS_32 Stage 3). The DIMMING RULE's gate needs
// the grid's in-window cell set testable headless ("the displayed in-window cell set equals
// the engine window set, per bar, including a window that crosses the bar boundary and one
// that wraps past 32") — the LanternRecordCore precedent: the widget's state math moves to
// a header the headless battery compiles (rack-free engine types via the test shim — no
// rack UI/widget/GLFW surface). VERBATIM move: every member and method body is the widget
// struct's, with the phrase constant named once — the phrase domain IS the engine's usable
// cap (dotModular::kPatternLengthCap, 32 since the 2b flip), and the widget's STEP_COUNT
// aliases it (the column-count parameterisation: the 64-step display later is a constant
// change, the plan's "design the grid to be column-count-parameterised rather than
// hardcoded 16").
//
// The struct itself holds ints only; the engine header supply is the phrase constant.

#include <array>
#include <algorithm>

#include "../dsp/engines/PatternEngine.hpp"   // dotModular::kPatternLengthCap — the phrase domain (the STEPS_32 constant authority)

namespace redDot {

struct SandsLaneWindowCore {
    int length = dotModular::kPatternLengthCap;    // How many steps active  (EDIT value)
    int offset = 0;             // Window start            (EDIT value)
    int rotation = 0;           // Rotation within window  (EDIT value)

    // Display-only L/O/R: what the user SEES. With no CV these equal the edit
    // values; when L/O/R CV is patched, the owning module overwrites them each
    // frame with the engine's CV-applied values via setDisplayLOR(), so the
    // window + markers track modulation. Editing/drag use the EDIT values
    // (editStartBar/editEndBar), so display is non-destructive.
    int dispLength   = dotModular::kPatternLengthCap;
    int dispOffset   = 0;
    int dispRotation = 0;
    void setDisplayLOR(int len, int off, int rot) {
      dispLength   = std::max(1, std::min(len, dotModular::kPatternLengthCap));
      dispOffset   = ((off % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap;
      dispRotation = ((rot % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap;
    }
    void syncDisplayToEdit() { setDisplayLOR(length, offset, rotation); }

    // ── Single source of truth for "which physical bars are in the window" ──
    // Reads the DISPLAY L/O/R so the visible window reflects CV modulation; with
    // no CV these equal the edit values (kept in sync via syncDisplayToEdit()).
    //
    // STEPS_32 Stage 3 — THE DIMMING RULE (the plan, Rodney-agreed): the grid's
    // dimming is the SECOND display channel, independent of the bar indicator's
    // disc — "which CELLS are inside the window". The window is defined on the
    // phrase (the 32-step ring): cells outside it are dimmed, cells inside are
    // lit, and a window that crosses the bar line into bar 2 (or wraps past 32
    // back to 1) lights the in-window cells in BOTH bars — each 16-cell half
    // shows the intersection of the window with its own cells. The pin
    // (test_sands_grid_windows) checks this set against the engine's own window
    // closed form, SequencerEngine::windowMaskBits(len, off), for a battery of
    // windows including the crossing and the wrap. Dimming is display only —
    // playback and the engine window are unchanged; this math READS the window,
    // it never recomputes it.
    int startBar() const { return ((dispOffset % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap; }
    int endBar()   const { return (startBar() + std::max(1, std::min(dispLength, dotModular::kPatternLengthCap)) - 1) % dotModular::kPatternLengthCap; }
    bool barInWindow(int bar) const {
      int s = startBar(), e = endBar();
      bar = ((bar % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap;
      return (e >= s) ? (bar >= s && bar <= e)        // contiguous
                      : (bar >= s || bar <= e);       // wrapped
    }
    // window-relative index of a physical bar (0 = start cell), or -1 if outside
    int windowIndexOf(int bar) const {
      if (!barInWindow(bar)) return -1;
      return (((bar - startBar()) % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap;
    }
    // EDIT-value window edges — for hit-testing and drag resize, which must act on
    // the user's actual (un-modulated) window, not the CV-display window.
    int editStartBar() const { return ((offset % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap; }
    int editEndBar()   const { return (editStartBar() + std::max(1, std::min(length, dotModular::kPatternLengthCap)) - 1) % dotModular::kPatternLengthCap; }
    // Edit-window membership (mirrors barInWindow but on the EDIT L/O = the live handle position,
    // not the CV/committed display). Used under lock to render the live window as the solid one.
    bool editBarInWindow(int bar) const {
      int s = editStartBar(), e = editEndBar();
      bar = ((bar % dotModular::kPatternLengthCap) + dotModular::kPatternLengthCap) % dotModular::kPatternLengthCap;
      return (e >= s) ? (bar >= s && bar <= e) : (bar >= s || bar <= e);
    }
    // True when the live edit window differs from the committed/display window -- i.e. the user has
    // moved LOR (under lock, this is the divergence that triggers the ghost echo).
    bool lorDiverged() const {
      return editStartBar() != startBar()
             || dispLength != std::max(1, std::min(length, dotModular::kPatternLengthCap));
    }
};

} // namespace redDot
