// STEPS_32 STAGE 3 — the Sands probability grid's DIMMING RULE pins (the plan's
// "## DIMMING RULE — probability grid" gate). The Lantern block landed Stage 3's first
// display block and the ring suite (test_ring_bar_indicator) the second; this suite lands
// the grid's dimming law:
//
//   Two independent display channels, never merged — the DISC (the BAR LEDs) reports which
//   BAR is active (engine position); the GRID DIMMING reports which CELLS are inside the
//   window. Rules 1-5 of the plan block, as pins:
//   * R1 the <=16 face: for EVERY (off, len) pair with off+len <= 16 (the GOLDEN LAW's
//     reproduction set), the grid's in-window set over the phrase equals the engine's —
//     the dimming of the first 16 cells is the legacy set (the engine closed form IS the
//     legacy math at those windows).
//   * R2/R3 bar 1 and bar 2: cells outside the window are dimmed, inside are lit — on the
//     32-column grid each 16-cell half shows the intersection of the window with its own
//     cells (the set is asserted PER BAR against the engine set's intersection).
//   * R4 the crossing and the wrap: a window that crosses the bar line into bar 2 and one
//     that wraps past 32 back to 1 light the in-window cells in BOTH bars — the pin
//     battery includes both classes (and the engine oracle is the RING's own closed form,
//     so a wrap window's two arcs compare exactly).
//   * R5 display only: the dimming math READS the display L/O/R and never recomputes it —
//     setDisplayLOR moves the visible window WITHOUT touching the edit values, the edit
//     edges stay put, and rotation never moves the window (it rides the pattern indexing,
//     not the window edges).
//
// THE TESTABLE SURFACE: the grid's window math was extracted VERBATIM into
// SandsLaneWindowCore (rack-free — the LanternRecordCore precedent) and
// SandsVisualEditorV4::ProbabilityLane now inherits it; the widget's own static_assert
// pins STEP_COUNT == kPatternLengthCap == 32 (compiled by the battery's plugin-TU smoke on
// both SDK legs, since the widget header itself cannot compile headless — the shim carries
// no widget types). The ORACLE is the engine's own window closed form,
// SequencerEngine::windowMaskBits(len, off) — the same function setWindow builds the live
// windowMask from: "the displayed in-window cell set equals the engine window set" reads
// against the engine's own builder, not a re-implementation.
//
// Build (run_all.sh): test_sands_grid_windows|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <string>

#include "ui/SandsLaneWindowCore.hpp"
#include "SequencerEngine.hpp"

namespace dm = dotModular;
using redDot::SandsLaneWindowCore;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// the grid's in-window set over the whole phrase, from the core's own math
static std::string setStr(const SandsLaneWindowCore& lane, int base, int count) {
    std::string s;
    for (int c = 0; c < count; ++c)
        if (lane.barInWindow(base + c)) s += '1'; else s += '0';
    return s;
}

static bool setEqualsEngine(const SandsLaneWindowCore& lane, uint64_t engineMask,
                            int base, int count) {
    for (int c = 0; c < count; ++c) {
        const bool engine = ((engineMask >> ((base + c) & (dm::kMaskRing - 1))) & 1ULL) != 0ULL;
        if (lane.barInWindow(base + c) != engine) return false;
    }
    return true;
}

int main() {
    std::printf("test_sands_grid_windows — the Sands grid's dimming rule (Stage 3, the plan's gate)\n");

    // the parameterisation: the core's phrase domain is the engine cap (the widget's
    // STEP_COUNT aliases it — the widget-side static_assert is compiled by the smoke)
    static_assert(dm::kPatternLengthCap == 32, "the phrase domain is the 2b cap");

    // ── 1. R1 — the <=16 face: EVERY reproduction-set window, phrase-wide equality ──
    {
        bool allOK = true; const char* bad = "";
        for (int off = 0; off < 16 && allOK; ++off) {
            for (int len = 1; off + len <= 16; ++len) {
                SandsLaneWindowCore lane;
                lane.setDisplayLOR(len, off, 0);
                if (!setEqualsEngine(lane, SequencerEngine::windowMaskBits(len, off), 0, dm::kPatternLengthCap)) {
                    allOK = false; bad = " (first miss)";
                    std::printf("    window (%d,%d): grid %s\n", len, off, setStr(lane, 0, 32).c_str());
                    break;
                }
            }
        }
        check(allOK, "R1: every off+len<=16 window's in-window set equals the engine set phrase-wide (the <=16 dimming unchanged)");
        (void)bad;
    }

    // ── 2. R2/R3/R4 — the named window classes, per bar, against the engine oracle ──
    {
        struct Case { int len, off; const char* what; };
        const Case cases[] = {
            {16,  0, "(16,0)  the module default — bar 1 only"},
            {16, 16, "(16,16) bar 2 only"},
            {16, 12, "(16,12) crosses the bar line into bar 2"},
            {16, 24, "(16,24) WRAPS past 32 back to 1"},
            {16,  9, "(16,9)  straddles the bar line"},
            {32,  0, "(32,0)  the full phrase"},
            { 1,  0, "(1,0)   a single cell at the phrase head"},
            { 1, 17, "(1,17)  a single cell in bar 2"},
            { 1, 31, "(1,31)  a single cell at the ring end"},
            { 3, 29, "(3,29)  a short wrap (29,30,31,0,1)"},
            {20, 25, "(20,25) the D3-documented wrap class"},
            { 7, 20, "(7,20)  a non-power window mid-bar-2"},
        };
        for (const Case& c : cases) {
            SandsLaneWindowCore lane;
            lane.setDisplayLOR(c.len, c.off, 0);
            const uint64_t eng = SequencerEngine::windowMaskBits(c.len, c.off);
            check(setEqualsEngine(lane, eng, 0, 16),
                  (std::string("R2: ") + c.what + " — the BAR-1 half's set == the engine set ∩ [0,16)").c_str());
            check(setEqualsEngine(lane, eng, 16, 16),
                  (std::string("R3: ") + c.what + " — the BAR-2 half's set == the engine set ∩ [16,32)").c_str());
            check(setEqualsEngine(lane, eng, 0, dm::kPatternLengthCap),
                  (std::string("R4: ") + c.what + " — the whole-phrase set == the engine set (both bars light their intersections)").c_str());
        }
    }

    // ── 3. R5 — display only: the display L/O/R moves the window, the edit edges stay ──
    {
        SandsLaneWindowCore lane;
        lane.length = 16; lane.offset = 0; lane.rotation = 0;
        lane.syncDisplayToEdit();
        check(lane.barInWindow(5) && !lane.barInWindow(20), "synced: the (16,0) window holds cells 0..15");
        // the CV-applied display arrives: the window moves WITHOUT the edit values
        lane.setDisplayLOR(8, 20, 0);
        check(lane.barInWindow(20) && !lane.barInWindow(5), "R5: the DISPLAY window follows setDisplayLOR (reads the engine window, never recomputes)");
        check(lane.length == 16 && lane.offset == 0, "R5: the EDIT values untouched by the display push (dimming is display only)");
        check(lane.editStartBar() == 0, "R5: the edit edges stay put (drag hit-testing acts on the user's window)");
        check(lane.lorDiverged(), "the divergence flag fires when display != edit (the ghost echo's trigger)");
        lane.syncDisplayToEdit();
        check(!lane.lorDiverged(), "syncDisplayToEdit restores the identity (no CV -> display == edit)");
    }

    // ── 4. R5 — rotation never moves the window (it rides the pattern indexing) ──
    {
        SandsLaneWindowCore lane;
        bool rotInvariant = true;
        lane.setDisplayLOR(16, 12, 0);
        const std::string base = setStr(lane, 0, 32);
        for (int rot = 0; rot < 32; ++rot) {
            lane.setDisplayLOR(16, 12, rot);
            rotInvariant &= (setStr(lane, 0, 32) == base);
        }
        check(rotInvariant, "R5: the window set is rotation-invariant on every phrase rotation (the window is on the displayed cells)");
    }

    // ── 5. the edit-window mirror + the window-relative index ──
    {
        SandsLaneWindowCore lane;
        lane.length = 16; lane.offset = 12; lane.rotation = 0;
        lane.syncDisplayToEdit();
        bool editOK = true, idxOK = true;
        for (int c = 0; c < 32; ++c) {
            editOK &= (lane.editBarInWindow(c) == lane.barInWindow(c));   // synced: the mirrors agree
            const int idx = lane.windowIndexOf(c);
            if (lane.barInWindow(c)) idxOK &= (idx == ((c - 12) + 32) % 32);
            else                     idxOK &= (idx == -1);
        }
        check(editOK, "the edit mirror == the display math when synced (one window, two readers)");
        check(idxOK, "windowIndexOf: 0-based from the start cell across the wrap, -1 outside");
    }

    // ── 6. the clamp defence: over-range requests cannot produce an out-of-domain window ──
    {
        SandsLaneWindowCore lane;
        lane.setDisplayLOR(999, 999, -5);
        check(lane.dispLength == 32, "the length clamp holds at the cap (defence-in-depth, the engine clamps first)");
        check(lane.dispOffset == ((999 % 32) + 32) % 32, "the offset lands ring-modded");
        bool inDomain = true;
        for (int c = 0; c < 32; ++c)
            inDomain &= (lane.barInWindow(c) == (((SequencerEngine::windowMaskBits(32, lane.dispOffset) >> c) & 1ULL) != 0ULL));
        check(inDomain, "even the clamped over-range request yields a window the engine closed form agrees with");
    }

    std::printf("test_sands_grid_windows: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
