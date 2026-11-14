#pragma once
// SandsRangeGestureCore — the voice-range strip's GESTURE MATH, extracted rack-free
// from SandsVisualEditorV4 (the SandsLaneWindowCore precedent) so the interaction
// gates of CC_FIX_VOICE_RANGE_STRIP_INTERACTION run in the battery (the widget header
// cannot compile headless — the shim carries no widget types; the plugin-TU smoke
// compiles the widget on both SDK legs and pins the wiring).
//
// THE THREE FAULTS THIS CORE FIXES (Rodney's doc):
//   A. ENGAGEMENT JUMP — the edge drags assigned the ABSOLUTE cell under the cursor
//      at engagement, so the edge jumped by the cells crossed before engagement.
//      FIX: every edge move is deltaSteps = (the cell under the cursor) − pressCell
//      applied to the PRESS window (grabOff, grabLen); delta 0 reproduces the press
//      edge exactly (no jump at engagement).
//   B. EDGE TARGET TOO SMALL — the START/END grab zones were sub-cell, so most
//      presses on an edge fell into the WINDOW body and moved the whole window.
//      FIX: the zones are at least max(1 cell, 8 px) wide on EACH side of the edge
//      boundary; the WINDOW body takes the rest and never captures a press within
//      one cell of an edge. The drawn edge ribbons use the SAME constant.
//   C. ROTATION NOT IN THE WINDOW — the strip's rotation drag wrote the ABSOLUTE
//      step unclamped, so the chevron (drawn at offset+rotation) could leave the
//      window. FIX: the LANES' rule — rel = (step − offset) mod ring, clamped to
//      [0, len−1]; the same clamp applies on every write of voice R (the drag; the
//      CV fold and the push already clamp — the engine's authoring invariant).
//
// UI only: no engine, store, geometry or rotation-convention change (the doc's
// DO-NOT list — stepWidthF/getStepX/getStepAtX/the 32-column draw/the Peak preset
// are untouched).
#include <algorithm>
#include <array>
#include <cmath>

namespace redDot {
struct SandsRangeGestureCore {
    // ── (A) the edge drags: delta-relative from the PRESS window ──────────────────
    static int wrapMod(int x, int ring) { return ((x % ring) + ring) % ring; }
    // START: the FRONT edge moves by delta; the END edge stays at its press position.
    static int startOff(int delta, int grabOff, int ring) {
        return wrapMod(grabOff + delta, ring);
    }
    static int startLen(int delta, int grabOff, int grabLen, int ring) {
        const int grabEnd = grabOff + std::max(1, grabLen) - 1;   // the press END edge
        return std::clamp(wrapMod(grabEnd - startOff(delta, grabOff, ring), ring) + 1, 1, ring);
    }
    // END: the BACK edge moves by delta; the START stays at grabOff.
    static int endLen(int delta, int grabOff, int grabLen, int ring) {
        const int grabEnd = grabOff + std::max(1, grabLen) - 1;
        return std::clamp(wrapMod(grabEnd + delta - grabOff, ring) + 1, 1, ring);
    }
    // WINDOW (the unchanged slide law, extracted so the gate can pin it): the window
    // slides by delta, length and rotation preserved.
    static int windowOff(int delta, int grabOff, int ring) {
        return wrapMod(grabOff + delta, ring);
    }

    // ── (C) rotation: the LANES' rule — window-relative, clamped in-window ────────
    static int clampRot(int r, int len) {
        return std::clamp(r, 0, std::max(0, len - 1));
    }
    static int rotationAt(int step, int off, int len, int ring) {
        return clampRot(wrapMod(step - off, ring), len);
    }

    // ── (B) the edge zones: >= max(1 cell, 8 px) EACH side of the edge boundary ───
    static float edgeZoneW(float stepW) { return std::max(stepW, 8.f); }
    // Z_ROTATION: the bottom band's zone — classifyBand never returns it (it
    // classifies the top band); the DRAG REDUCER's zone domain includes it.
    enum Zone { Z_NONE = 0, Z_START, Z_END, Z_WINDOW, Z_ROTATION };
    // The window's BODY extent (px): between the bounds (contiguous) or the two outer
    // arcs (a window wrapped past the grid's right edge). gridL/gridR = the step
    // grid's x extents. Mirrors the old inWindowX cell extents exactly.
    static bool inBody(float x, float startBoundX, float endBoundX, float gridL, float gridR) {
        if (endBoundX > startBoundX) return (x > startBoundX && x < endBoundX);
        return (x > startBoundX && x <= gridR) || (x >= gridL && x < endBoundX);   // wrapped
    }
    // ── (B + bugs 9/10, LOR_EDITING_BUG_LIST) THE WINDOW-RELATIVE CLASSIFIER ────
    // ONE press-classification law for the strip band AND the probability lanes.
    // The press maps to its WINDOW-RELATIVE clock distance from the start boundary
    // (through the wrap), then classifies:
    //   START:  [startBound − edgeW, startBound + edgeW] — a BOUNDED band on EACH
    //           side of the boundary (one cell out + one cell in — the B law);
    //   END:    [endBound − edgeW,   endBound + edgeW]   — the same at the back;
    //   WINDOW: the rest of the window                    — the body (MOVE);
    //   NONE:   beyond both bands                          — the paint arm / no grab.
    // THE BODY GUARANTEE (bug 9): edgeW is capped at span/3, so the middle third of
    // the window ALWAYS remains a body — the old lane test's open-ended
    // `x >= endZoneL` let a WRAPPED window's END zone swallow its whole front arc
    // (the "dragged past step 32, cannot be picked up to move again" report).
    // THE LENGTH-1 RULE (bug 10, Rodney): a 1-wide window's cell IS the body; its
    // edges are the bands one cell OUTSIDE the cell — so a length-1 window
    // (accent) can always be picked up and moved.
    static Zone classifyWindow(float x, float startBoundX, float endBoundX,
                               float edgeW, float gridL, float gridR,
                               float spanPx, float stepW) {
        if (spanPx <= stepW + 1e-3f) {
            // LENGTH 1: the cell is the body; the edges live OUTSIDE it.
            if (x >= startBoundX - edgeW && x <  startBoundX) return Z_START;
            if (x >  endBoundX   && x <= endBoundX + edgeW)   return Z_END;
            if (x >= startBoundX && x <= endBoundX)           return Z_WINDOW;
            return Z_NONE;
        }
        const float ew = std::min(edgeW, spanPx / 3.f);
        // the clock distance from the start boundary, through the wrap
        float dist = -1.f;
        if (endBoundX > startBoundX) {
            if (x >= startBoundX && x <= endBoundX) dist = x - startBoundX;
        } else if (x >= startBoundX && x <= gridR) {
            dist = x - startBoundX;                            // the front arc
        } else if (x >= gridL && x <= endBoundX) {
            dist = (gridR - startBoundX) + (x - gridL);        // the wrapped-back arc
        }
        if (dist < 0.f) {
            // outside the window: only the OUTWARD halves of the bands grab
            if (x >= startBoundX - ew && x <  startBoundX) return Z_START;
            if (x >  endBoundX   && x <= endBoundX + ew)   return Z_END;
            return Z_NONE;
        }
        if (dist <= ew)          return Z_START;
        if (dist >= spanPx - ew) return Z_END;
        return Z_WINDOW;
    }

    // ── (bug 11, LOR_EDITING_BUG_LIST) THE ONE WINDOW-CONTAINS LAW ──────────────
    // Whether a physical bar is inside the (off, len) window on the ring — the
    // ONE set both the probability lanes' faint range tint and the voice strip's
    // window highlight draw from ("the voice strip shows the window the
    // probability lanes show", the same offset and length). Contiguous, wrapped
    // or full-ring. SandsLaneWindowCore::barInWindow and the editor's
    // rangeBarInWindow both delegate here — no parallel copy can drift.
    static bool windowContains(int off, int len, int bar, int ring) {
        const int s = wrapMod(off, ring);
        const int l = std::clamp(len, 1, ring);
        const int e = (s + l - 1) % ring;
        const int b = wrapMod(bar, ring);
        return (e >= s) ? (b >= s && b <= e) : (b >= s || b <= e);
    }

    // ── (bugs 17+18, LOR_EDITING_BUG_LIST) THE SHARED LANE-PLAYHEAD LAW ─────────
    // ONE law for EVERY lane's drawn playhead (the VAR lane included — the doc's
    // ruling: "No lane draws the global scan when it has a window"): the window
    // walk's read position, physical bar = off + ((tick + rot) mod len), wrapped
    // on the ring — BY CONSTRUCTION inside [off, off+len). VisualExpanderHelpers'
    // calcPlayhead delegates here (the windowContains precedent: ONE law, every
    // consumer; the old VAR special-case drew the engine's SCAN instead).
    static int lanePlayheadBar(int tick, int len, int off, int rot, int ring) {
        const int l = std::clamp(len, 1, ring);
        const int o = wrapMod(off, ring);
        const int r = wrapMod(rot, ring);
        return wrapMod(o + wrapMod(tick + r, l), ring);
    }

    // ── (bug 12, LOR_EDITING_BUG_LIST) THE CHEVRON GEOMETRY ────────────────────
    // The rotation marker's chevron pair, sized to the GRID: the pair's total
    // x-extent is EXACTLY one step width (the old fixed 3 px arm overran the
    // cell at 32 columns — "chevrons wider than the probability bars"). The
    // chevrons ›‹ point inward around the cell's centre; arm = the arm depth.
    struct ChevronGeom { float halfW, arm; };
    static ChevronGeom chevronGeom(float stepW) {
        // the pair spans [cx − halfW − arm, cx + halfW + arm] → 2*(halfW+arm) == stepW
        const float arm = stepW * 0.20f;
        const float halfW = stepW * 0.5f - arm;
        return ChevronGeom{ halfW, arm };
    }

    // ── (bug 13, LOR_EDITING_BUG_LIST) THE RAW CELL INDEX ───────────────────────
    // The drag's cell extraction, UNBOUNDED: a cursor past the grid's edge keeps
    // counting cells (−1, −2 … left of step 0; ring, ring+1 … right of the last
    // step). Every drag law wraps mod the ring internally, so feeding the RAW
    // index gives CONTINUOUS dragging across both edges — the pre-fix extraction
    // returned −1 off-grid and the drag STALLED ("grabbing from the step at the
    // edge only drags away from the edge": a wrapped range with one step left on
    // one side could not be dragged back). Presses/paint still use the bounded
    // in-grid cell (a press must be ON a real cell).
    static int stepIndexAt(float x, float gridL, float stepW) {
        return (int)std::floor((x - gridL) / stepW);
    }

    // ── (bug 14, LOR_EDITING_BUG_LIST) THE GRID-CLAMPED BAND ────────────────────
    // An edge/handle band clamped INTO the grid rect: a 1-wide window's OUTSIDE
    // bands (and any edge ribbon) never overdraw the panel past the grid — the
    // band that would draw past the edge is clipped to the grid (a fully outside
    // band collapses to zero width).
    static void clampBand(float& x, float& w, float gridL, float gridR) {
        const float x2 = std::clamp(x + w, gridL, gridR);
        x = std::clamp(x, gridL, gridR);
        w = x2 - x;
    }

    // ── (bug 15, LOR_EDITING_BUG_LIST) THE STRIP'S CONTRAST-CHECKED FIGURE ──────
    // The voice strip's window fills, raised until the figure READS: the old
    // values (lit 0.14 / tint 0.05 white over the #101216 recess ≈ 1.7:1) were
    // grey on grey ("the grab range is invisible"). The constants clear the
    // pinned minimum (kStripMinContrast) against the strip's background —
    // computed with the WCAG relative-luminance ratio below. Hue-NEUTRAL by
    // design (the voice frame spans all lane hues; the lanes keep their own
    // 0.05 tint — their windows read via the coloured bars and the dimming).
    static constexpr float kStripTintA = 0.14f;   // the window tint (the mask-mode figure)
    static constexpr float kStripLitA  = 0.24f;   // the in-window presence fill
    static constexpr float kStripDimA  = 0.045f;  // the out-of-window recess
    static constexpr float kStripMinContrast      = 2.5f;   // the window figure vs the background
    static constexpr float kStripMinTintContrast  = 1.4f;   // the tint alone (over a muted mask cell)
    // the sRGB relative luminance + the WCAG contrast ratio, and the alpha blend
    static float srgbLuminance(float r, float g, float b) {
        auto lin = [](float c) { return (c <= 0.04045f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
        return 0.2126f * lin(r) + 0.7152f * lin(g) + 0.0722f * lin(b);
    }
    static float contrastRatio(float r1, float g1, float b1, float r2, float g2, float b2) {
        const float L1 = srgbLuminance(r1, g1, b1), L2 = srgbLuminance(r2, g2, b2);
        return (std::max(L1, L2) + 0.05f) / (std::min(L1, L2) + 0.05f);
    }
    static float blendChannel(float under, float over, float alpha) {
        return under + alpha * (over - under);
    }

    // ── THE DRAG REDUCER (LOR_EDITING_BUG_LIST.md §PLAN 1 — the ROOT ISSUE) ──────
    // The press/move/release SEQUENCING extracted from the widget's GLFW handlers so
    // the LOR drag law is testable headless (the doc: "It can only be exercised in
    // Rack, which is why each fix has been checked by eye"). The widget ONLY
    // translates GLFW events into these calls — press(step, zone) at the ENGAGED
    // press (the strip arms first and engages on the first cell crossing; a lane
    // engages at the GLFW press), move(step) per crossed cell, release() — and
    // copies the window back out. Every law the reducer applies is one of the pure
    // functions above (A: the press-anchored edge deltas; C: the rotation clamp;
    // the slide law). The paint/arm machinery (pressArmed/paintPress/mask) is NOT
    // LOR state — it stays widget-side.
    struct LorDragState {
        enum Owner { O_NONE = 0, O_STRIP, O_LANE };
        Owner  owner     = O_NONE;
        Zone   zone      = Z_NONE;
        int    laneIdx   = 0;              // the LANE owner's lane (else unused)
        int    pressStep = 0;              // the engaged press cell (the deltas' origin)
        int    grabOff   = 0;              // the window's offset at press
        int    grabLen   = 1;              // the window's length at press
        std::array<int, 3> window = {1, 0, 0};   // the CURRENT LOR {len, off, rot} (P-B)
        std::array<int, 3> before = {1, 0, 0};   // the LOR at press (the commit report)
        bool   active    = false;          // a drag is engaged
        bool   changed() const {
            return window[0] != before[0] || window[1] != before[1] || window[2] != before[2];
        }
    };
    struct LorDragEvent {
        enum Type { PRESS, MOVE, RELEASE };
        Type  type    = RELEASE;
        Zone  zone    = Z_NONE;            // PRESS only: the press classification
        LorDragState::Owner owner = LorDragState::O_NONE;   // PRESS only: which surface owns the drag
        int   laneIdx = 0;                 // PRESS only: the LANE owner's lane
        int   step    = 0;                 // PRESS/MOVE: the cell under the cursor
        std::array<int, 3> win = {1, 0, 0};      // PRESS only: the window the press grabbed (P-B)
    };
    // Pure: (state, event, ring) -> state. MOVE on an inactive drag and PRESS with
    // Z_NONE are no-ops (the translation layer only arms what it classified).
    // RELEASE clears the engagement but KEEPS window/before — the host reads the
    // before/after pair for its commit callback after the reduce.
    static LorDragState lorDragReduce(LorDragState s, const LorDragEvent& e, int ring) {
        switch (e.type) {
        case LorDragEvent::PRESS:
            if (e.zone == Z_NONE) { s.active = false; s.owner = LorDragState::O_NONE; s.zone = Z_NONE; return s; }
            s.owner = e.owner; s.zone = e.zone; s.laneIdx = e.laneIdx;
            s.pressStep = e.step;
            s.grabOff = e.win[1];
            s.grabLen = std::max(1, e.win[0]);
            for (int i = 0; i < 3; ++i) { s.window[i] = e.win[i]; s.before[i] = e.win[i]; }
            s.active = true;
            return s;
        case LorDragEvent::MOVE:
            if (!s.active) return s;
            switch (s.zone) {
            case Z_WINDOW:
                // the unchanged slide law: the window follows the cells crossed
                // since the press; length and rotation preserved.
                s.window[1] = windowOff(e.step - s.pressStep, s.grabOff, ring);
                break;
            case Z_START:
                // (A) the front edge moves by the press-anchored delta; the END
                // edge stays at its press position; (C) R clamps into the resize.
                s.window[1] = startOff(e.step - s.pressStep, s.grabOff, ring);
                s.window[0] = startLen(e.step - s.pressStep, s.grabOff, s.grabLen, ring);
                s.window[2] = clampRot(s.window[2], s.window[0]);
                break;
            case Z_END:
                // (A) the back edge moves by the press-anchored delta; the START
                // stays at grabOff; (C) R clamps into the resize.
                s.window[0] = endLen(e.step - s.pressStep, s.grabOff, s.grabLen, ring);
                s.window[2] = clampRot(s.window[2], s.window[0]);
                break;
            case Z_ROTATION:
                // (C) the lanes' rule: window-relative, clamped in-window.
                s.window[2] = rotationAt(e.step, s.window[1], s.window[0], ring);
                break;
            default: break;
            }
            return s;
        case LorDragEvent::RELEASE:
            s.active = false; s.owner = LorDragState::O_NONE; s.zone = Z_NONE;
            return s;
        }
        return s;
    }
};
} // namespace redDot
