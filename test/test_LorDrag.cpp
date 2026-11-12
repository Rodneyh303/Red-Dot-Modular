// test_LorDrag.cpp — LOR_EDITING_BUG_LIST.md §PLAN 1+3: THE DRAG REDUCER and its
// headless gate suite. The doc's ROOT ISSUE: "The press, drag and release logic
// lives inside the GLFW event handlers of the editor widget. It can only be
// exercised in Rack, which is why each fix has been checked by eye. Extract it."
// The extraction is SandsRangeGestureCore::lorDragReduce — a pure
// (state, event, ring) -> state machine whose every law is one of the core's
// pinned functions (A the press-anchored edge deltas, C the rotation clamp, the
// slide law). This suite scripts press/move/release sequences and asserts the
// state after EACH event (the doc's §PLAN 3):
//
//   1. EDGE DRAG one cell at a time moves the edge one cell at a time, with no
//      jump at engagement (delta 0 == the press window) — START and END, a lane
//      and the strip.
//   2. PAST 16 AND PAST 31: no snap at 16, no wrap anomaly — the slide and the
//      edge drags cross the old 16 boundary and the ring seam continuously;
//      offsets 16..31 apply (the F-domain at the reducer level).
//   3. ONE OWNER PER DRAG: a press in a lane body never changes the voice window
//      and never fires onRangeEdit; a press in the strip band never changes a
//      lane (ADDITION E's law, now by construction: the owner is set at press).
//   4. THE ZONE RIDE: a press classified START/END/WINDOW/ROTATION engages that
//      gesture (the reducer is zone-faithful; the classifier itself is pinned by
//      test_range_strip_gesture's B gates).
//   5. ROTATION stays in [0, length-1] for EVERY length and drag position, and
//      re-clamps into the resized window after a START/END resize.
//   6. THE BODY DRAG moves the window with the grab offset held (the slide law),
//      length and rotation preserved, wrapping at the ring.
//
// The WIDGET MODEL below mirrors the widget's translation layer (the two stores
// + the copy-back + the callbacks + the setRangeState push guard), so the owner
// gates are behavioural, not just structural. The widget's own GLFW translation
// (the arm/paint machinery, the pointer accumulation) is pinned by the plugin-TU
// smoke compiling the widget on both SDK legs.
//
// Build (run_all.sh): test_LorDrag|

#include <cstdio>

#include "ui/SandsRangeGestureCore.hpp"

using Core = redDot::SandsRangeGestureCore;
using S    = redDot::SandsRangeGestureCore::LorDragState;
using E    = redDot::SandsRangeGestureCore::LorDragEvent;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── event builders (the widget's translation vocabulary) ──────────────────────
static E evPress(Core::Zone z, S::Owner owner, int laneIdx, int step) {
    E e; e.type = E::PRESS; e.zone = z; e.owner = owner; e.laneIdx = laneIdx; e.step = step;
    e.win[0] = 1; e.win[1] = 0; e.win[2] = 0;   // the model supplies the real window
    return e;
}
static E evMove(int step)  { E e; e.type = E::MOVE;   e.step = step; return e; }
static E evRelease()       { E e; e.type = E::RELEASE; return e; }

// ── the widget model: the two stores + the translation's copy-back ────────────
// Mirrors SandsVisualEditorV4's translation layer: the strip's lor[] and the
// lanes' LOR are separate stores; the reducer's window is copied back to the
// OWNER's store per move; the strip reports per-move (onRangeEdit) and the lanes
// commit-only (onLorCommit) — exactly the widget's callback shapes.
struct WidgetModel {
    static constexpr int RING = 32;
    static constexpr int LANES = 7;
    int strip[3] = {16, 0, 0};        // the voice-range strip's display row {L,O,R}
    int lanes[LANES][3];              // the probability lanes' {L,O,R}
    int  rangeEdits = 0;              // onRangeEdit fired (per strip move)
    int  rangeCommits = 0;            // onRangeCommit fired (strip release, changed)
    int  lorCommitLane = -1;          // onLorCommit's lane (-1 = never fired)
    int  commitBefore[3] = {0, 0, 0};
    int  commitAfter[3]  = {0, 0, 0};
    S    drag;

    WidgetModel() { for (int l = 0; l < LANES; ++l) { lanes[l][0] = 16; lanes[l][1] = 0; lanes[l][2] = 0; } }

    // setRangeState's guard: the host's per-frame push must not fight a live drag.
    bool stripPushBlocked() const { return drag.active && drag.owner == S::O_STRIP; }

    void dispatch(const E& e) {
        if (e.type == E::PRESS) {
            if (e.zone == Core::Z_NONE) return;   // the translation never arms NONE
            E pe = e;
            if (pe.owner == S::O_STRIP) {
                pe.win[0] = strip[0]; pe.win[1] = strip[1]; pe.win[2] = strip[2];
            } else if (pe.laneIdx >= 0 && pe.laneIdx < LANES) {
                pe.win[0] = lanes[pe.laneIdx][0]; pe.win[1] = lanes[pe.laneIdx][1]; pe.win[2] = lanes[pe.laneIdx][2];
            } else {
                return;
            }
            drag = Core::lorDragReduce(drag, pe, RING);
            return;
        }
        if (e.type == E::MOVE) {
            if (!drag.active) return;
            drag = Core::lorDragReduce(drag, e, RING);
            if (drag.owner == S::O_STRIP) {
                strip[0] = drag.window[0]; strip[1] = drag.window[1]; strip[2] = drag.window[2];
                ++rangeEdits;                                   // onRangeEdit(lor...)
            } else if (drag.laneIdx >= 0 && drag.laneIdx < LANES) {
                lanes[drag.laneIdx][0] = drag.window[0];         // no per-move lane callback
                lanes[drag.laneIdx][1] = drag.window[1];         // (the commit-only model)
                lanes[drag.laneIdx][2] = drag.window[2];
            }
            return;
        }
        // RELEASE: report the before/after pair (the commit), then clear.
        if (drag.active && drag.changed()) {
            for (int i = 0; i < 3; ++i) { commitBefore[i] = drag.before[i]; commitAfter[i] = drag.window[i]; }
            if (drag.owner == S::O_STRIP) ++rangeCommits;
            else                          lorCommitLane = drag.laneIdx;
        }
        drag = Core::lorDragReduce(drag, evRelease(), RING);
    }
};

int main() {
    // ── GATE 1: the edge drags — one cell at a time, no jump at engagement ─────
    {
        // A lane's START edge: window {8,4} (len 4 at off 8), press on the START
        // cell; a sequence of one-cell moves right. The FIRST engaged move (delta
        // 0, the press cell) must reproduce the press window EXACTLY.
        WidgetModel m;
        m.lanes[2][0] = 4; m.lanes[2][1] = 8; m.lanes[2][2] = 1;
        m.dispatch(evPress(Core::Z_START, S::O_LANE, 2, 8));
        bool ok = m.drag.active && m.drag.owner == S::O_LANE && m.drag.zone == Core::Z_START;
        m.dispatch(evMove(8));   // delta 0 — the press window verbatim, NO JUMP
        ok = ok && m.lanes[2][0] == 4 && m.lanes[2][1] == 8 && m.lanes[2][2] == 1;
        for (int delta = 1; delta <= 6; ++delta) {
            m.dispatch(evMove(8 + delta));
            // the front edge = press + delta (wrapping); the END edge stays at 11;
            // the length adjusts; R clamps into the resized window.
            const int off = ((8 + delta) % 32 + 32) % 32;
            const int len = ((11 - off) % 32 + 32) % 32 + 1;
            ok = ok && m.lanes[2][1] == off && m.lanes[2][0] == len;
            ok = ok && m.lanes[2][2] >= 0 && m.lanes[2][2] <= len - 1;
        }
        // The END edge: fresh drag, the back edge moves one cell at a time, the
        // START stays at grabOff, delta 0 reproduces the press length.
        WidgetModel m2;
        m2.lanes[0][0] = 4; m2.lanes[0][1] = 8; m2.lanes[0][2] = 2;
        m2.dispatch(evPress(Core::Z_END, S::O_LANE, 0, 11));
        m2.dispatch(evMove(11));
        ok = ok && m2.lanes[0][0] == 4 && m2.lanes[0][1] == 8;
        for (int delta = 1; delta <= 5; ++delta) {
            m2.dispatch(evMove(11 + delta));
            ok = ok && m2.lanes[0][0] == 4 + delta && m2.lanes[0][1] == 8;
        }
        // The STRIP's edges run the same law (one state machine, both owners).
        WidgetModel m3;
        m3.strip[0] = 6; m3.strip[1] = 20; m3.strip[2] = 3;
        m3.dispatch(evPress(Core::Z_START, S::O_STRIP, 0, 20));
        m3.dispatch(evMove(20));
        ok = ok && m3.strip[0] == 6 && m3.strip[1] == 20;
        for (int delta = 1; delta <= 4; ++delta) {
            m3.dispatch(evMove(20 + delta));
            ok = ok && m3.strip[1] == 20 + delta;
        }
        check(ok, "1: the edge drags are one-cell-at-a-time with no jump at engagement "
                  "(START + END, a lane and the strip)");
    }

    // ── GATE 2: past 16 and past 31 — no snap at 16, no wrap anomaly ───────────
    {
        // The body slide crossing the old 16 boundary (the pre-F snap line) and
        // the ring seam: the offset follows wrapMod continuously; len/rot hold.
        WidgetModel m;
        m.lanes[1][0] = 5; m.lanes[1][1] = 14; m.lanes[1][2] = 2;
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 1, 14));
        bool ok = true;
        for (int delta = 0; delta <= 20; ++delta) {
            m.dispatch(evMove(14 + delta));
            const int off = ((14 + delta) % 32 + 32) % 32;
            ok = ok && m.lanes[1][1] == off;      // 14,15,16,...,31,0,1,2 — continuous
            ok = ok && m.lanes[1][0] == 5 && m.lanes[1][2] == 2;
        }
        // Offsets 16..31 APPLY (the F-domain): the slide parks the window in the
        // upper half and a fresh press there picks the window up again.
        m.dispatch(evRelease());
        ok = ok && m.lanes[1][1] == 2;            // ((14+20) mod 32)
        m.lanes[1][1] = 22;                        // author an upper-half start directly
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 1, 24));
        m.dispatch(evMove(26));
        ok = ok && m.lanes[1][1] == 24 && m.lanes[1][0] == 5;   // it moved, no snap to 15
        // The START edge dragged past the seam wraps the front edge continuously
        // (the length follows, no blow-up past the ring).
        WidgetModel m2;
        m2.strip[0] = 3; m2.strip[1] = 30; m2.strip[2] = 0;
        m2.dispatch(evPress(Core::Z_START, S::O_STRIP, 0, 30));
        for (int step = 30; step <= 33; ++step) {
            m2.dispatch(evMove(step % 32));
            const int off = step % 32;
            const int len = (((30 + 3 - 1) - off) % 32 + 32) % 32 + 1;
            ok = ok && m2.strip[1] == off && m2.strip[0] == len && m2.strip[0] <= 32;
        }
        check(ok, "2: no snap at 16, no wrap anomaly past 31 — the slide and the edges "
                  "cross both boundaries continuously; offsets 16..31 apply");
    }

    // ── GATE 3: one owner per drag — the lane/strip separation (ADDITION E) ────
    {
        // A press in a LANE body: the lane's window moves, the STRIP's window and
        // onRangeEdit never fire.
        WidgetModel m;
        m.strip[0] = 7; m.strip[1] = 3; m.strip[2] = 1;
        m.lanes[4][0] = 6; m.lanes[4][1] = 10; m.lanes[4][2] = 2;
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 4, 12));
        m.dispatch(evMove(15));
        bool ok = m.lanes[4][1] == 13 && m.lanes[4][0] == 6;
        ok = ok && m.strip[0] == 7 && m.strip[1] == 3 && m.strip[2] == 1;   // untouched
        ok = ok && m.rangeEdits == 0;                                        // no onRangeEdit
        ok = ok && m.lorCommitLane == -1;                                    // not yet released
        m.dispatch(evRelease());
        ok = ok && m.lorCommitLane == 4 && m.rangeCommits == 0;              // the LANE commit fired
        ok = ok && m.commitBefore[1] == 10 && m.commitAfter[1] == 13;        // the before/after pair
        // A press on the STRIP band: the strip's window moves, no lane changes,
        // the per-frame push is blocked for the drag's duration.
        WidgetModel m2;
        m2.lanes[3][0] = 9; m2.lanes[3][1] = 5; m2.lanes[3][2] = 4;
        m2.dispatch(evPress(Core::Z_WINDOW, S::O_STRIP, 0, 4));
        ok = ok && m2.stripPushBlocked();                                    // setRangeState's guard
        m2.dispatch(evMove(9));
        ok = ok && m2.strip[1] == 5 && m2.strip[0] == 16;
        ok = ok && m2.lanes[3][0] == 9 && m2.lanes[3][1] == 5 && m2.lanes[3][2] == 4;  // untouched
        ok = ok && m2.rangeEdits == 1;                                       // onRangeEdit fired once
        ok = ok && m2.lorCommitLane == -1;
        m2.dispatch(evRelease());
        ok = ok && m2.rangeCommits == 1 && m2.lorCommitLane == -1;           // the STRIP commit only
        // A drag on one lane never touches ANOTHER lane.
        WidgetModel m3;
        m3.lanes[5][0] = 4; m3.lanes[5][1] = 2; m3.lanes[5][2] = 0;
        m3.lanes[6][0] = 8; m3.lanes[6][1] = 12; m3.lanes[6][2] = 1;
        m3.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 5, 3));
        m3.dispatch(evMove(6));
        ok = ok && m3.lanes[5][1] == 5;
        ok = ok && m3.lanes[6][0] == 8 && m3.lanes[6][1] == 12 && m3.lanes[6][2] == 1;
        check(ok, "3: one owner per drag — a lane press never changes the voice window "
                  "(and vice versa); the push guard holds; the commits fire on the right surface");
    }

    // ── GATE 4: the zone rides the press — the reducer is zone-faithful ────────
    {
        bool ok = true;
        const Core::Zone zones[] = { Core::Z_START, Core::Z_END, Core::Z_WINDOW, Core::Z_ROTATION };
        for (Core::Zone z : zones) {
            WidgetModel m;
            m.dispatch(evPress(z, S::O_LANE, 0, 5));
            ok = ok && m.drag.active && m.drag.zone == z;
            m.dispatch(evRelease());
            ok = ok && !m.drag.active;                     // released cleanly
        }
        // A NONE press never engages (the translation never arms it).
        WidgetModel m;
        m.dispatch(evPress(Core::Z_NONE, S::O_LANE, 0, 5));
        m.dispatch(evMove(9));
        ok = ok && !m.drag.active && m.lanes[0][1] == 0;   // nothing happened
        // MOVE on an inactive drag is a no-op.
        WidgetModel m2;
        m2.dispatch(evMove(9));
        ok = ok && !m2.drag.active;
        check(ok, "4: the press's zone rides the drag; NONE never engages; "
                  "moves on an inactive drag are no-ops");
    }

    // ── GATE 5: rotation stays in [0, length-1] for every length and drag ──────
    {
        bool ok = true;
        for (int len = 1; len <= 32; ++len) {
            for (int off = 0; off < 32; off += 5) {
                WidgetModel m;
                m.lanes[0][0] = len; m.lanes[0][1] = off; m.lanes[0][2] = 0;
                m.dispatch(evPress(Core::Z_ROTATION, S::O_LANE, 0, off));
                for (int step = 0; step < 32; ++step) {
                    m.dispatch(evMove(step));
                    ok = ok && m.lanes[0][2] >= 0 && m.lanes[0][2] <= len - 1;
                    ok = ok && m.lanes[0][0] == len && m.lanes[0][1] == off;  // R only
                }
            }
        }
        // A resize re-clamps R into the resized window (the START shrink).
        WidgetModel m;
        m.lanes[2][0] = 8; m.lanes[2][1] = 4; m.lanes[2][2] = 7;
        m.dispatch(evPress(Core::Z_START, S::O_LANE, 2, 4));
        m.dispatch(evMove(9));                              // shrink 8 -> 3
        ok = ok && m.lanes[2][0] == 3 && m.lanes[2][2] <= 2;
        // The strip's rotation runs the same law.
        WidgetModel m2;
        m2.strip[0] = 5; m2.strip[1] = 30; m2.strip[2] = 0;
        m2.dispatch(evPress(Core::Z_ROTATION, S::O_STRIP, 0, 30));
        for (int step = 0; step < 32; ++step) {
            m2.dispatch(evMove(step));
            ok = ok && m2.strip[2] >= 0 && m2.strip[2] <= 4;
        }
        check(ok, "5: rotation in [0, length-1] for every length, offset and drag — "
                  "and re-clamped into a resized window (the lanes' rule, every write)");
    }

    // ── GATE 6: the body drag moves the window with the grab offset held ───────
    {
        WidgetModel m;
        m.strip[0] = 9; m.strip[1] = 7; m.strip[2] = 3;
        m.dispatch(evPress(Core::Z_WINDOW, S::O_STRIP, 0, 10));   // grab mid-window
        bool ok = true;
        for (int delta = -35; delta <= 35; ++delta) {
            m.dispatch(evMove(10 + delta));
            const int off = ((7 + delta) % 32 + 32) % 32;         // press off + delta, wrapped
            ok = ok && m.strip[1] == off;
            ok = ok && m.strip[0] == 9 && m.strip[2] == 3;        // len/rot preserved
        }
        // Backward past the seam (the "past step 1" case): continuous.
        ok = ok && m.strip[1] == ((7 + 35) % 32 + 32) % 32;
        m.dispatch(evRelease());
        ok = ok && m.rangeCommits == 1 && m.commitAfter[1] == m.strip[1];
        check(ok, "6: the body drag slides the window with the grab offset held, "
                  "length and rotation preserved, wrapping both directions");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
