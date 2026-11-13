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
#include <string>

#include "ui/SandsRangeGestureCore.hpp"
#include "engines/SequencerEngine.hpp"   // (bug 17) the voiceRead_ ring-fold pin
#include "VoiceResolver.hpp"

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

// slurp a text file (the delegation pins read the live sources; run_all runs
// from the repo root — the D grep-in-test precedent in test_range_strip_gesture)
static bool slurpFile(const char* path, std::string& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

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

    // ── GATE 7 (bug 9): a wrapped window is pickable — press in the body, move ──
    {
        // The 32-cell grid, cell w = 10 px, grid [0, 320). A window dragged past
        // step 32: offset 30, len 3 → cells 30, 31, 0 (the front arc 300..320,
        // the back arc 0..10). The front arc's MIDDLE (step 31) is the body —
        // the old lane test's open-ended END zone (`x >= endZoneL`, with the
        // zone's left edge at 10 − edgeW = 0) swallowed it: every press in the
        // window classified END and the window could never be MOVED again.
        const float w = 10.f, gridL = 0.f, gridR = 320.f;
        const float edgeW = Core::edgeZoneW(w);
        bool ok = Core::classifyWindow(315.f, 300.f, 10.f, edgeW, gridL, gridR, 30.f, w) == Core::Z_WINDOW;
        ok = ok && Core::classifyWindow(305.f, 300.f, 10.f, edgeW, gridL, gridR, 30.f, w) == Core::Z_START;
        ok = ok && Core::classifyWindow(3.f,   300.f, 10.f, edgeW, gridL, gridR, 30.f, w) == Core::Z_END;
        // the doc's full sequence: drag past 32, release, press in the body, move.
        WidgetModel m;
        m.lanes[1][0] = 4; m.lanes[1][1] = 28; m.lanes[1][2] = 1;
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 1, 30));   // grab the body
        m.dispatch(evMove(2));                                    // slide across the seam (+4)
        ok = ok && m.lanes[1][1] == 0 && m.lanes[1][0] == 4;      // (28+4) mod 32
        m.dispatch(evRelease());
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 1, 2));     // pick it up again
        m.dispatch(evMove(5));
        ok = ok && m.lanes[1][1] == 3;
        // the wrapped STATE itself: author (30,3), press at the front arc's
        // middle (step 31, classified Z_WINDOW above), move — it moves.
        WidgetModel m2;
        m2.lanes[1][0] = 3; m2.lanes[1][1] = 30; m2.lanes[1][2] = 0;
        m2.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 1, 31));
        m2.dispatch(evMove(0));                                   // one cell left
        ok = ok && m2.lanes[1][1] == 31 && m2.lanes[1][0] == 3;
        m2.dispatch(evRelease());
        ok = ok && m2.lorCommitLane == 1;
        check(ok, "7 (bug 9): a wrapped window's body is grabbable — drag past 32, "
                  "release, press in the body and move; the window moves (no stuck pickup)");
    }

    // ── GATE 8 (bug 10): a length-1 window — the bar moves, the edges resize ────
    {
        const float w = 10.f, gridL = 0.f, gridR = 320.f;
        const float edgeW = Core::edgeZoneW(w);
        // the classifier (Rodney's ruling): the CELL is the body (MOVE); the
        // edges are the bands one cell OUTSIDE the cell.
        bool ok = Core::classifyWindow(85.f, 80.f, 90.f, edgeW, gridL, gridR, 10.f, w) == Core::Z_WINDOW;
        ok = ok && Core::classifyWindow(75.f, 80.f, 90.f, edgeW, gridL, gridR, 10.f, w) == Core::Z_START;
        ok = ok && Core::classifyWindow(95.f, 80.f, 90.f, edgeW, gridL, gridR, 10.f, w) == Core::Z_END;
        ok = ok && Core::classifyWindow(60.f, 80.f, 90.f, edgeW, gridL, gridR, 10.f, w) == Core::Z_NONE;
        // press on the BAR: the window MOVES, the length stays 1 (accent movable).
        WidgetModel m;
        m.lanes[3][0] = 1; m.lanes[3][1] = 8; m.lanes[3][2] = 0;
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 3, 8));
        m.dispatch(evMove(10));
        ok = ok && m.lanes[3][1] == 10 && m.lanes[3][0] == 1;     // moved, still 1 wide
        m.dispatch(evRelease());
        ok = ok && m.lorCommitLane == 3;
        // press on the EDGE (the outside band): the EDGE moves (a resize), not
        // the body — the offset moves and the length is no longer 1.
        WidgetModel m2;
        m2.lanes[3][0] = 1; m2.lanes[3][1] = 8; m2.lanes[3][2] = 0;
        m2.dispatch(evPress(Core::Z_START, S::O_LANE, 3, 8));
        m2.dispatch(evMove(9));                                   // the front edge +1
        ok = ok && m2.lanes[3][1] == 9;                           // the edge moved
        ok = ok && m2.lanes[3][0] != 1;                           // a resize, not a slide
        check(ok, "8 (bug 10): a length-1 window's bar moves it (the cell is the "
                  "body); the outside bands grab the edges (Rodney's ruling)");
    }

    // ── GATE 9 (bug 11): the strip's window highlight — the ONE contains law ────
    {
        // The voice strip and the probability lanes draw their window tint from
        // the SAME set (windowContains); pinned against an independent oracle
        // (wrapMod(bar − off) < len) for EVERY (off, len, bar) on the ring —
        // contiguous, crossing, wrapping, full-ring and length-1.
        bool ok = true;
        for (int off = 0; off < 32; ++off)
            for (int len = 1; len <= 32; ++len)
                for (int bar = 0; bar < 32; ++bar)
                    ok = ok && Core::windowContains(off, len, bar, 32)
                                   == (Core::wrapMod(bar - off, 32) < len);
        // the named cases: a wrapped window lights its two arcs; a 1-wide lights
        // its single cell; the full ring lights everything.
        ok = ok &&  Core::windowContains(30, 4, 31, 32) && Core::windowContains(30, 4, 1, 32)
                 && !Core::windowContains(30, 4, 5, 32);
        ok = ok &&  Core::windowContains(8, 1, 8, 32) && !Core::windowContains(8, 1, 9, 32);
        ok = ok &&  Core::windowContains(7, 32, 0, 32) && Core::windowContains(7, 32, 31, 32);
        // the DELEGATION pin (the D grep-in-test precedent): both consumers call
        // the ONE law — the editor's rangeBarInWindow and the lane core's
        // barInWindow have no parallel copy to drift.
        std::string ed, lane;
        bool got = slurpFile("src/ui/SandsVisualEditorV4.hpp", ed)
                && slurpFile("src/ui/SandsLaneWindowCore.hpp", lane);
        ok = ok && got;
        ok = ok && ed.find("SandsRangeGestureCore::windowContains") != std::string::npos;
        ok = ok && lane.find("SandsRangeGestureCore::windowContains") != std::string::npos;
        ok = ok && ed.find("kStripTintA") != std::string::npos;   // the strip's window tint (the contrast-checked constant)
        check(ok, "9 (bug 11): the strip's window tint draws from the ONE contains "
                  "law (the same set as the lanes, always, layered under the mask)");
    }

    // ── GATE 10 (bug 12): the rotation chevron's extent is one step width ──────
    {
        // chevronGeom: the pair spans [cx − halfW − arm, cx + halfW + arm], so
        // 2*(halfW + arm) == stepW EXACTLY — the old fixed 3 px arm overran the
        // cell at 32 columns (extent 2*(0.42*(stepW−2)+3) > stepW). Pinned at
        // the 32-column width and the neighbouring grid widths.
        bool ok = true;
        const float widths[] = { 10.625f, 21.25f, 5.3125f, 42.5f, 8.f, 20.f };
        for (float w : widths) {
            const auto g = Core::chevronGeom(w);
            ok = ok && g.arm > 0.f && g.halfW > 0.f;
            ok = ok && (2.f * (g.halfW + g.arm) - w) < 1e-4f && (w - 2.f * (g.halfW + g.arm)) < 1e-4f;
        }
        // the old form OVERSHOT the cell at 32 columns — the regression guard.
        const float w32 = 10.625f;
        const float oldExtent = 2.f * ((w32 - 2.f) * 0.42f + 3.f);
        ok = ok && oldExtent > w32;                       // the bug, for the record
        ok = ok && 2.f * (Core::chevronGeom(w32).halfW + Core::chevronGeom(w32).arm) <= w32;
        // the DELEGATION pin: drawRotationIndicator uses the core's geometry.
        std::string ed;
        ok = ok && slurpFile("src/ui/SandsVisualEditorV4.hpp", ed);
        ok = ok && ed.find("chevronGeom") != std::string::npos;
        check(ok, "10 (bug 12): the rotation chevron's x-extent is exactly one "
                  "step width (sized to the grid, not a fixed arm)");
    }

    // ── GATE 11 (bug 13): the raw index — dragging TOWARD the grid edge works ──
    {
        // stepIndexAt is UNBOUNDED (floor): a cursor past the grid's left edge
        // reads −1 (== the LAST step, one to the left on the ring), past the
        // right edge reads ring/ring+1... The drag laws wrap mod the ring, so
        // the RAW index gives continuous dragging across both edges — the old
        // off-grid rejection stalled the drag ("only drags away from the edge").
        const float w = 10.f, gridL = 0.f;
        bool ok = Core::stepIndexAt(-0.5f * w, gridL, w) == -1;    // half a cell out
        ok = ok && Core::stepIndexAt(-1.9f * w, gridL, w) == -2;   // nearly two cells out
        ok = ok && Core::stepIndexAt(0.5f * w, gridL, w) == 0;     // in-grid unchanged
        ok = ok && Core::stepIndexAt(31.5f * w, gridL, w) == 31;
        ok = ok && Core::stepIndexAt(32.5f * w, gridL, w) == 32;   // past the right edge
        // the doc's case: a wrapped window with ONE step on one side (31,0,1);
        // press the edge-step body (step 0) and move TOWARD the edge (raw −1):
        // the window moves toward the edge (the start slides 31 -> 30).
        WidgetModel m;
        m.lanes[1][0] = 3; m.lanes[1][1] = 31; m.lanes[1][2] = 0;
        m.dispatch(evPress(Core::Z_WINDOW, S::O_LANE, 1, 0));
        m.dispatch(evMove(-1));
        ok = ok && m.lanes[1][1] == 30 && m.lanes[1][0] == 3;      // dragged BACK across the seam
        m.dispatch(evMove(-2));
        ok = ok && m.lanes[1][1] == 29;
        // past the right edge: raw 32 ≡ 0 (mod the ring) — no snap, no jump.
        WidgetModel m2;
        m2.strip[0] = 4; m2.strip[1] = 5; m2.strip[2] = 0;
        m2.dispatch(evPress(Core::Z_WINDOW, S::O_STRIP, 0, 6));
        m2.dispatch(evMove(32));
        ok = ok && m2.strip[1] == 31;                              // (5 + (32−6)) mod 32
        m2.dispatch(evMove(33));
        ok = ok && m2.strip[1] == 0;                               // wraps cleanly
        check(ok, "11 (bug 13): the raw cell index — a drag toward the grid edge "
                  "crosses the seam continuously; the wrapped one-step-side window "
                  "drags back");
    }

    // ── GATE 12 (bug 14): the 1-wide outside bands stay inside the grid ─────────
    {
        // The 32-cell grid, cell w = 10 px, grid [0, 320). A 1-wide window at
        // step 0: the START band would draw at [−10, 0) — past the grid; the
        // clamp collapses it to nothing inside. At the last step: the END band
        // clamps. Mid-grid: unchanged. The clamped bands are ALWAYS inside
        // [gridL, gridR] and never negative-width.
        const float gridL = 0.f, gridR = 320.f, edgeW = 10.f;
        bool ok = true;
        auto clamp = [&](float x, float w2, float& ox, float& ow2) {
            ox = x; ow2 = w2;
            Core::clampBand(ox, ow2, gridL, gridR);
        };
        auto inside = [&](float x, float w2)->bool {
            return x >= gridL - 1e-4f && x + w2 <= gridR + 1e-4f && w2 >= 0.f;
        };
        // step 0's START band [−10, 0): fully outside — collapses to zero width.
        float x, w2;
        clamp(-edgeW, edgeW, x, w2);
        ok = ok && inside(x, w2) && w2 == 0.f && x == gridL;
        // step 0's END band [10, 20): inside — unchanged.
        clamp(10.f, edgeW, x, w2);
        ok = ok && inside(x, w2) && x == 10.f && w2 == edgeW;
        // the last step's END band [320, 330): fully outside — collapses.
        clamp(320.f, edgeW, x, w2);
        ok = ok && inside(x, w2) && w2 == 0.f && x == gridR;
        // the last step's START band [300, 310): inside — unchanged.
        clamp(300.f, edgeW, x, w2);
        ok = ok && inside(x, w2) && x == 300.f && w2 == edgeW;
        // a HALF-outside band keeps its inside part: [−5, 5) -> [0, 5).
        clamp(-5.f, edgeW, x, w2);
        ok = ok && inside(x, w2) && x == 0.f && w2 == 5.f;
        // every band on the ring clamps inside (the exhaustive guard).
        for (float bx = -2.f * edgeW; bx <= gridR + 2.f * edgeW; bx += 3.f) {
            clamp(bx, edgeW, x, w2);
            ok = ok && inside(x, w2);
        }
        check(ok, "12 (bug 14): the 1-wide outside handle bands clamp into the "
                  "grid rect (step 1 and step 32 never overdraw the panel)");
    }

    // ── GATE 13 (bug 15): the strip's window figure meets the contrast minimum ──
    {
        // The strip's background is the editor's recess, #101216 (display dark
        // always — pinned to setTheme's value). The window figure (the tint
        // layered under the lit fill) must READ: the WCAG relative-luminance
        // ratio of the blended figure vs the background clears the pinned
        // minimum; the tint alone (the painted-mask mode's figure) clears its
        // own lower bar. The old values (lit 0.14 / tint 0.05) measured ~1.7:1
        // — grey on grey, "the grab range is invisible".
        const float bg[3] = { 0x10 / 255.f, 0x12 / 255.f, 0x16 / 255.f };
        const float tint = Core::kStripTintA, lit = Core::kStripLitA;
        const float eff = 1.f - (1.f - tint) * (1.f - lit);   // the stacked figure
        float fig[3];
        for (int c = 0; c < 3; ++c)
            fig[c] = Core::blendChannel(bg[c], 1.f, eff);
        const float ratioFig = Core::contrastRatio(fig[0], fig[1], fig[2], bg[0], bg[1], bg[2]);
        float tinted[3];
        for (int c = 0; c < 3; ++c)
            tinted[c] = Core::blendChannel(bg[c], 1.f, tint);
        const float ratioTint = Core::contrastRatio(tinted[0], tinted[1], tinted[2], bg[0], bg[1], bg[2]);
        bool ok = ratioFig >= Core::kStripMinContrast;
        ok = ok && ratioTint >= Core::kStripMinTintContrast;
        // the regression record: the OLD figure (lit 0.14 + tint 0.05) FAILED.
        const float oldEff = 1.f - (1.f - 0.05f) * (1.f - 0.14f);
        float oldFig[3];
        for (int c = 0; c < 3; ++c)
            oldFig[c] = Core::blendChannel(bg[c], 1.f, oldEff);
        const float oldRatio = Core::contrastRatio(oldFig[0], oldFig[1], oldFig[2], bg[0], bg[1], bg[2]);
        ok = ok && oldRatio < Core::kStripMinContrast;   // the bug, for the record
        // the constants are the ones the strip draws with (the delegation pin).
        std::string ed;
        ok = ok && slurpFile("src/ui/SandsVisualEditorV4.hpp", ed);
        ok = ok && ed.find("kStripLitA") != std::string::npos
                 && ed.find("kStripTintA") != std::string::npos
                 && ed.find("kStripDimA") != std::string::npos;
        std::printf("    [bug 15] figure %.2f:1 (min %.1f), tint %.2f:1 (min %.1f), old %.2f:1\n",
                    ratioFig, Core::kStripMinContrast, ratioTint, Core::kStripMinTintContrast, oldRatio);
        check(ok, "13 (bug 15): the voice strip's window figure meets the minimum "
                  "contrast ratio against the recess (the constants are pinned)");
    }

    // ── GATE 14 (bugs 17+18): the SHARED lane playhead — in-window, always ──────
    {
        // lanePlayheadBar (the ONE law; calcPlayhead delegates): for EVERY
        // (off, len, rot, tick) the drawn playhead is INSIDE [off, off+len) —
        // the doc's test. The VAR scan exception is retired (no caller anywhere).
        bool ok = true;
        for (int off = 0; off < 32; ++off)
            for (int len = 1; len <= 32; ++len)
                for (int rot = 0; rot < 32; ++rot)
                    for (int tick = -3; tick < 35; ++tick) {
                        const int bar = Core::lanePlayheadBar(tick, len, off, rot, 32);
                        ok = ok && bar >= 0 && bar < 32;
                        ok = ok && Core::windowContains(off, len, bar, 32);   // in-window
                    }
        // the law's formula: off + ((tick + rot) mod len), wrapped on the ring.
        ok = ok && Core::lanePlayheadBar(7, 8, 20, 3, 32) == ((20 + ((7 + 3) % 8)) % 32);
        ok = ok && Core::lanePlayheadBar(7, 4, 30, 1, 32) == ((30 + ((7 + 1) % 4)) % 32);   // a wrapped window
        // the delegation + the retirement pins (the D grep-in-test precedent):
        // calcPlayhead delegates to the core law; NO caller draws the scan.
        std::string veh, east, macro;
        ok = ok && slurpFile("src/ui/VisualExpanderHelpers.hpp", veh)
               && slurpFile("src/StraitsEastSandsVisual.cpp", east)
               && slurpFile("src/StraitsSandsMacroVisual.cpp", macro);
        ok = ok && veh.find("lanePlayheadBar") != std::string::npos;          // the delegation
        ok = ok && veh.find("variationLanePlayhead") == std::string::npos;    // the helpers retired
        ok = ok && east.find("variationLanePlayhead") == std::string::npos;   // no scan callers
        ok = ok && macro.find("variationLanePlayhead") == std::string::npos;
        check(ok, "14 (bugs 17+18): the shared lane playhead is in-window for every "
                  "(off, len, rot, tick); the VAR scan exception is retired everywhere");
    }

    // ── GATE 15 (bug 17): the voice strip's read cell — the RING fold ───────────
    {
        // The engine's voiceRead_ (the strip's rp) must land where the walk put
        // it — including the IDENTITY voice under a bar-2 GLOBAL window. The old
        // % 16 fold (the 16-era leftover) put every read in bar 1, so the strip's
        // playhead left the window ("Voice (GLOBAL) lane playhead outside its
        // range"). The ring fold keeps it; for every ≤16 configuration the value
        // is < 16 either way (the goldens' bit-identity).
        SequencerEngine eng;
        eng.reset();
        // the GLOBAL window placed in bar 2: length 16, offset 16. V1 (slot 1 —
        // the always-live mono voice; inactive poly slots park at 0 by design).
        eng.updateWindow(16.f, 0.f, false, 16.f, 0.f, false);
        bool ok = true;
        for (int i = 0; i < 24; ++i) {
            eng.advancePlayhead(1, nullptr);
            const int rp = eng.voiceReadPosition(dotModular::VoiceResolver::slot(1));   // V1 (an identity voice)
            ok = ok && rp >= 16 && rp < 32;                          // in bar 2 — no bar-1 fold
        }
        // the default global window (16/0): the identity read stays in [0, 16).
        eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        for (int i = 0; i < 24; ++i) {
            eng.advancePlayhead(1, nullptr);
            const int rp = eng.voiceReadPosition(dotModular::VoiceResolver::slot(1));
            ok = ok && rp >= 0 && rp < 16;
        }
        check(ok, "15 (bug 17): the identity voice's read cell folds on the RING — "
                  "bar 2 stays bar 2 (the strip's playhead inside its range)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
