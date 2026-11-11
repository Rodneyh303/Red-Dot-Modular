// test_range_strip_gesture.cpp — CC_FIX_VOICE_RANGE_STRIP_INTERACTION: the
// voice-range strip's THREE interaction faults, fixed and pinned.
//
// THE TESTABLE SURFACE: the strip's gesture math lives in SandsRangeGestureCore
// (rack-free — the SandsLaneWindowCore precedent; the widget header cannot compile
// headless, the shim carries no widget types — the plugin-TU smoke compiles the
// widget on both SDK legs and pins the wiring). The gates below are the doc's:
//
//   A. ENGAGEMENT JUMP — the edge drags are delta-relative from the PRESS window:
//      delta 0 (the first engaged move, the press cell itself) reproduces the press
//      edge EXACTLY; each further cell moves the edge one cell in the drag
//      direction; the OTHER edge never moves.
//   B. EDGE TARGET — a press within one cell (>= max(1 cell, 8 px)) of an edge
//      boundary grabs that EDGE, never the window body; the middle moves the
//      window; beyond the zones (out-of-window) is NONE (the paint arm).
//   C. ROTATION IN THE WINDOW — the lanes' rule (rel = (step − off) mod 32,
//      clamped [0, len−1]): for EVERY (off, len, step) the rotation is in range and
//      the chevron bar (off + rot) mod 32 lands INSIDE the window.
//   + the WINDOW slide law unchanged (the "behave as before" gate), and the strip's
//     rotation rule IS the lanes' rule (one core function, both consumers).
//
// East and Macro share ONE editor (SandsVisualEditorV4) — one code path, no
// per-host divergence to pin. No engine, store, geometry or rotation-convention
// change (the doc's DO-NOT); the suite pins pure gesture math.
//
// Build (run_all.sh): test_range_strip_gesture|

#include <cstdio>
#include <cstdint>

#include "ui/SandsRangeGestureCore.hpp"

using redDot::SandsRangeGestureCore;
static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

int main() {
    const int RING = 32;   // STEP_COUNT == kPatternLengthCap (the widget's own assert)

    // ── GATE A: the edge drags — no jump at engagement, one cell at a time ──────
    {
        // A press on the START edge of a 4-wide window at offset 8, then a sequence
        // of one-cell moves right (the doc's synthetic press + moves).
        const int grabOff = 8, grabLen = 4;
        bool ok = true;
        // delta 0 — the FIRST move after engagement (the press cell): the press
        // window verbatim, NO JUMP (the fault-A law).
        ok = ok && SandsRangeGestureCore::startOff(0, grabOff, RING) == grabOff;
        ok = ok && SandsRangeGestureCore::startLen(0, grabOff, grabLen, RING) == grabLen;
        // one cell at a time: the front edge follows the cursor cell-for-cell, the
        // END edge stays at its press position (12), the length adjusts.
        for (int delta = 0; delta <= 6; ++delta) {
            const int off = SandsRangeGestureCore::startOff(delta, grabOff, RING);
            const int len = SandsRangeGestureCore::startLen(delta, grabOff, grabLen, RING);
            ok = ok && off == ((grabOff + delta) % RING + RING) % RING;          // the edge = press + delta
            ok = ok && ((off + len - 1) % RING + RING) % RING
                        == ((grabOff + grabLen - 1) % RING + RING) % RING;        // the other edge stays
            ok = ok && len >= 1 && len <= RING;
        }
        // the same law mirrored for the END drag: the back edge moves by delta, the
        // START stays at grabOff.
        for (int delta = 0; delta <= 6; ++delta) {
            const int len = SandsRangeGestureCore::endLen(delta, grabOff, grabLen, RING);
            ok = ok && ((grabOff + len - 1) % RING + RING) % RING
                        == ((grabOff + grabLen - 1 + delta) % RING + RING) % RING;
            ok = ok && len >= 1 && len <= RING;
        }
        // a full-circle drag wraps to the press window (the ring absorbs it).
        ok = ok && SandsRangeGestureCore::startLen(RING, grabOff, grabLen, RING) == grabLen;
        ok = ok && SandsRangeGestureCore::endLen(RING, grabOff, grabLen, RING) == grabLen;
        // the exhaustive no-jump law: delta 0 reproduces the press window for EVERY
        // press window on the ring.
        for (int off = 0; off < RING; ++off)
            for (int len = 1; len <= RING; ++len) {
                ok = ok && SandsRangeGestureCore::startLen(0, off, len, RING) == len;
                ok = ok && SandsRangeGestureCore::endLen(0, off, len, RING) == len;
            }
        check(ok, "A: edge drags are delta-relative — no jump at engagement, one cell "
                  "at a time, the other edge stays, delta 0 == the press window (exhaustive)");
    }

    // ── GATE B: the edge target — one cell each side of the edge boundary ───────
    {
        // A 4-wide window at offset 8 on a 32-cell grid: cell w = 10 px, grid
        // [0, 320). startBound = x(8) = 80; endBound = x(11) + 10 = 120.
        const float w = 10.f, gridL = 0.f, gridR = 320.f;
        const float sb = 80.f, eb = 120.f;
        const float edgeW = SandsRangeGestureCore::edgeZoneW(w);
        check(edgeW >= w && edgeW >= 8.f, "B: the zone width is >= max(1 cell, 8 px)");
        bool ok = true;
        // within one cell of an edge (either side): the EDGE, never the body.
        for (float d = -edgeW * 0.99f; d <= edgeW * 0.99f; d += edgeW / 8.f) {
            ok = ok && SandsRangeGestureCore::classifyBand(sb + d, sb, eb, edgeW, gridL, gridR)
                           == SandsRangeGestureCore::Z_START;
            ok = ok && SandsRangeGestureCore::classifyBand(eb + d, sb, eb, edgeW, gridL, gridR)
                           == SandsRangeGestureCore::Z_END;
        }
        // the middle of the window moves the window.
        ok = ok && SandsRangeGestureCore::classifyBand(100.f, sb, eb, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_WINDOW;
        // beyond the zones (out-of-window) is NONE — the paint arm keeps its presses.
        ok = ok && SandsRangeGestureCore::classifyBand(0.f,  sb, eb, edgeW, gridL, gridR) == SandsRangeGestureCore::Z_NONE;
        ok = ok && SandsRangeGestureCore::classifyBand(65.f, sb, eb, edgeW, gridL, gridR) == SandsRangeGestureCore::Z_NONE;
        ok = ok && SandsRangeGestureCore::classifyBand(200.f, sb, eb, edgeW, gridL, gridR) == SandsRangeGestureCore::Z_NONE;
        // a tiny (1-wide) window: the zones overlap — split at the midpoint, no body.
        const float sb1 = 80.f, eb1 = 90.f;   // window [80, 90), one cell
        ok = ok && SandsRangeGestureCore::classifyBand(75.f, sb1, eb1, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_START;
        ok = ok && SandsRangeGestureCore::classifyBand(95.f, sb1, eb1, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_END;
        // a WRAPPED window (offset 30, len 4 → cells 30,31,0,1): the body is the two
        // outer arcs; the edge zones sit at x(30)=300 and x(1)+10=20.
        const float sbw = 300.f, ebw = 20.f;
        ok = ok && SandsRangeGestureCore::classifyBand(305.f, sbw, ebw, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_START;   // the start boundary zone
        ok = ok && SandsRangeGestureCore::classifyBand(15.f,  sbw, ebw, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_END;     // the end boundary zone
        ok = ok && SandsRangeGestureCore::classifyBand(318.f, sbw, ebw, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_WINDOW;  // the right arc
        ok = ok && SandsRangeGestureCore::classifyBand(5.f,   sbw, ebw, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_WINDOW;  // the left arc
        ok = ok && SandsRangeGestureCore::classifyBand(150.f, sbw, ebw, edgeW, gridL, gridR)
                       == SandsRangeGestureCore::Z_NONE;    // the gap between the arcs
        // the ROTATION band's body test (inBody) is the same extent set.
        ok = ok &&  SandsRangeGestureCore::inBody(318.f, sbw, ebw, gridL, gridR)
                 && !SandsRangeGestureCore::inBody(150.f, sbw, ebw, gridL, gridR);
        check(ok, "B: a press within one cell of an edge grabs the edge; the middle "
                  "moves the window; beyond is NONE (paint); tiny + wrapped windows");
    }

    // ── GATE C: rotation — the lanes' rule, always in the window ────────────────
    {
        bool ok = true;
        // for EVERY (off, len, step): the rotation is in [0, len−1] and the chevron
        // bar (off + rot) mod 32 is INSIDE the window [off, off+len).
        for (int off = 0; off < RING; ++off)
            for (int len = 1; len <= RING; ++len)
                for (int step = 0; step < RING; ++step) {
                    const int r = SandsRangeGestureCore::rotationAt(step, off, len, RING);
                    ok = ok && r >= 0 && r <= len - 1;
                    const int chev = ((off + r) % RING + RING) % RING;
                    const int rel  = ((chev - off) % RING + RING) % RING;
                    ok = ok && rel < len;   // the chevron is in-window
                }
        // the clamp on every write: a resize clamps R into the new window.
        ok = ok && SandsRangeGestureCore::clampRot(7, 4) == 3;
        ok = ok && SandsRangeGestureCore::clampRot(2, 4) == 2;
        ok = ok && SandsRangeGestureCore::clampRot(0, 1) == 0;
        // the lanes' rule IS the strip's rule (one function): rel = (step − off)
        // mod 32 clamped — e.g. drag to the window's own start ⇒ R = 0; drag one
        // past the end ⇒ clamped to len−1.
        ok = ok && SandsRangeGestureCore::rotationAt(8, 8, 4, RING) == 0;
        ok = ok && SandsRangeGestureCore::rotationAt(12, 8, 4, RING) == 3;   // the last in-window step
        ok = ok && SandsRangeGestureCore::rotationAt(13, 8, 4, RING) == 3;   // clamped
        ok = ok && SandsRangeGestureCore::rotationAt(7, 8, 4, RING) == 3;  // (7-8) mod 32 = 31 → clamped to len-1
        check(ok, "C: rotation is window-relative and clamped — the chevron is always "
                  "inside [off, off+len) for every (off, len, step) on the ring");
    }

    // ── GATE: the window slide law unchanged (behave as before) ─────────────────
    {
        bool ok = true;
        for (int off = 0; off < RING; ++off)
            for (int delta = -40; delta <= 40; ++delta)
                ok = ok && SandsRangeGestureCore::windowOff(delta, off, RING)
                            == ((off + delta) % RING + RING) % RING;   // the old inline expression
        check(ok, "the WINDOW slide law is the old expression verbatim (behave as before)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
