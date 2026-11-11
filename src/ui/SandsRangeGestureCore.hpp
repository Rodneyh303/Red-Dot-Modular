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
    enum Zone { Z_NONE = 0, Z_START, Z_END, Z_WINDOW };
    // The window's BODY extent (px): between the bounds (contiguous) or the two outer
    // arcs (a window wrapped past the grid's right edge). gridL/gridR = the step
    // grid's x extents. Mirrors the old inWindowX cell extents exactly.
    static bool inBody(float x, float startBoundX, float endBoundX, float gridL, float gridR) {
        if (endBoundX > startBoundX) return (x > startBoundX && x < endBoundX);
        return (x > startBoundX && x <= gridR) || (x >= gridL && x < endBoundX);   // wrapped
    }
    // The top band's classification. The edge zones extend edgeZoneW on EACH side of
    // the edge boundary (one cell outside + one cell inside the window); a press
    // there grabs the EDGE, never the body. Tiny windows whose zones overlap split
    // at the window midpoint (the 1-wide half-split rule, generalized).
    static Zone classifyBand(float x, float startBoundX, float endBoundX,
                             float edgeW, float gridL, float gridR) {
        const bool sHit = (x >= startBoundX - edgeW && x <= startBoundX + edgeW);
        const bool eHit = (x >= endBoundX   - edgeW && x <= endBoundX   + edgeW);
        if (sHit && eHit)
            return (x <= (startBoundX + endBoundX) * 0.5f) ? Z_START : Z_END;
        if (sHit) return Z_START;
        if (eHit) return Z_END;
        return inBody(x, startBoundX, endBoundX, gridL, gridR) ? Z_WINDOW : Z_NONE;
    }
};
} // namespace redDot
