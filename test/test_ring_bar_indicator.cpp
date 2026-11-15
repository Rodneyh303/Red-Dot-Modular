// STEPS_32 STAGE 3 — the ring + the inner-disc bar indicator pins (the P6 ring-map list,
// "D10 as re-scoped by D15"). The Lantern suite (test_lantern_bars) landed Stage 3's first
// block; this suite lands the display law for the Monsoon light ring itself:
//
//   THE 16-STAY FORM (D15): the panel's 16 ring lamps STAY — lamp k carries masters k and
//   k+16 (twice-around-16), and the rendered master is the CURRENT LAP's, the lap being
//   D10's split test on the playhead (stepIndex >= 16). The same split drives the TWO BAR
//   LEDs INSIDE the ring center (the inner-disc bar indicator — the disc spec's "upper =
//   bar 1, lower = bar 2" in its two-LED form): BAR 1 while the playhead is in steps 0..15,
//   BAR 2 in 16..31, lit EXCLUSIVELY from the playhead's own gate and variable — one gate,
//   two surfaces; where no discrete playhead exists (mode D) and before the first tick,
//   BOTH are silent.
//
//   THE WINDOW MAP (D10 (1), the ghost arc): an out-of-window lamp lights a dim ghost
//   instead of the dead 0.0. The four standing levels keep their values VERBATIM — playhead
//   1.0 > comet trail 0.5 > in-window note 0.35 > in-window rest 0.07 — and the ghost pins
//   STRICTLY BELOW the in-window rest, so a ghost can never read as a rest (the hierarchy
//   is the law; the exact value is a P5 calibration lever).
//
//   THE BAR INDICATOR INVARIANT 2 (the clock reading): the active segment changes on the
//   same tick as lamp 0 of the new bar — a lag of even one step breaks the clock reading.
//
//   THE ≤16 FACES: at the module-default (16,0) window the playhead never leaves bar 1,
//   the lap never flips (one-bar windows render STABLE — the D15 claim), no ghost can
//   render (the current-lap masters 0..15 are all in-window), and every lamp read equals
//   the pre-twice-around formula bit-for-bit. P6: "the full window renders no ghosts and
//   the BAR LEDs alone carry the lap".
//
//   THE TRAIL REPAIR: the 2b flip widened kMaskRing to 32 and silently killed the comet
//   trail on the whole second bar (`lightIdx == trailIdx` with a 0..15 lightIdx could
//   never match a trailIdx ≥ 16); the twice-around lamp landing (lamp = trail master & 15,
//   window test on the trail master itself) restores the law "the trailing LED is the step
//   the playhead just left" — including across the bar line, where the step just left
//   belongs to the other lap.
//
// The oracle is expectedLevel() below — the D15 model in one place: the current-lap
// master's window membership, the trail reading the TRAIL MASTER, the playhead on lamp
// stepIndex % 16. Every landed frame of every march is compared lamp-by-lamp against it.
// With restProb 0 everywhere (the deterministic-authoring discipline) every in-window cell
// is a NOTE, so the levels are exactly 1.0 / 0.5 / 0.35 / ghost — the 0.07 rest level never
// fires (its place in the hierarchy is pinned by the constant checks in section 0).
// All reads are the engine's own display functions (SequencerEngine::getStepLightBrightness
// / getBarLightBrightness) — the exact surface the UIManager feed consumes; the digests
// never capture this path (THE GOLDEN LAW's display face), so these pins are pure addition.
//
// Build (run_all.sh): test_ring_bar_indicator|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <set>

#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"

namespace dm = dotModular;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// every voice sounds on every visited cell (the deterministic-authoring discipline):
// restProb 0 against the factory rest row (1.0) -> isNote everywhere in-window.
static void authorDeterministic(SequencerEngine& eng) {
    for (int v = 1; v <= 16; ++v)
        eng.voices[dm::VoiceResolver::slot(v)].restProb = 0.f;
}

// the D15 flip-table model (scripts/d15_ring_flip_table.py) — lamp k (0-indexed here; the
// script is 1-indexed) ghosts on lap L iff its CURRENT-LAP master (L*16 + k) is out of W.
static std::set<int> ghostsForLap(const std::set<int>& win, int lap) {
    std::set<int> g;
    for (int k = 0; k < 16; ++k)
        if (win.count(lap * 16 + k) == 0) g.insert(k);
    return g;
}

// the expected brightness of lamp k at the engine's CURRENT landed frame, window set win:
// playhead 1.0 > trail 0.5 (the TRAIL MASTER's own membership — across the bar line the
// step just left belongs to the other lap) > in-window note 0.35 > ghost.
static float expectedLevel(const SequencerEngine& eng, int k, const std::set<int>& win) {
    const int lap         = (eng.stepIndex >= 16) ? 1 : 0;                       // D10's split test
    const int lamp        = ((eng.stepIndex % 16) + 16) % 16;                    // the sanctioned display landing
    const int trailMaster = ((eng.stepIndex - eng.lastPlayDir) % dm::kMaskRing + dm::kMaskRing) % dm::kMaskRing;
    const int trailLamp   = trailMaster % 16;                                    // the twice-around lamp landing
    if (k == lamp) return 1.0f;
    if (k == trailLamp && win.count(trailMaster)) return 0.5f;
    if (win.count(lap * 16 + k)) return 0.35f;
    return SequencerEngine::kStepGhostBrightness;
}

// the whole ring equals the model on the current landed frame
static bool ringMatchesModel(const SequencerEngine& eng, const std::set<int>& win) {
    for (int k = 0; k < 16; ++k)
        if (eng.getStepLightBrightness(k) != expectedLevel(eng, k, win)) return false;
    return true;
}

int main() {
    std::printf("test_ring_bar_indicator — the ring + the inner-disc bar indicator (Stage 3, D10/D15)\n");

    // ── 0. the hierarchy is the law (the D10 levels verbatim; the ghost strictly below) ──
    check(SequencerEngine::kStepGhostBrightness > 0.f,
          "the ghost is visible (> 0 — the far half is no longer dead-dark, the D8 amendment)");
    check(SequencerEngine::kStepGhostBrightness < 0.07f,
          "the ghost pins STRICTLY BELOW the in-window rest (a ghost can never read as a rest)");
    check(SequencerEngine::kBarIndicatorDimBrightness > 0.f &&
          SequencerEngine::kBarIndicatorDimBrightness < 1.f,
          "the inactive bar's LED dims — distinct from both the lit state and the unlit ring lamps");

    // ── 1. THE ≤16 FACE: the module-default (16,0) window is bit-identical and STABLE ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // the knob's default push
        authorDeterministic(eng);
        const std::set<int> win = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

        bool inBar1 = true, ringOK = true, noGhost = true, barStable = true, barExcl = true;
        for (int i = 0; i < 32; ++i) {
            eng.advancePlayhead(+1);
            inBar1 &= (eng.stepIndex >= 0 && eng.stepIndex < 16);
            ringOK &= ringMatchesModel(eng, win);
            for (int k = 0; k < 16; ++k)
                noGhost &= (eng.getStepLightBrightness(k) != SequencerEngine::kStepGhostBrightness);
            const float b1 = eng.getBarLightBrightness(0), b2 = eng.getBarLightBrightness(1);
            barStable &= (b1 == 1.f && b2 == SequencerEngine::kBarIndicatorDimBrightness);
            barExcl   &= (b1 != b2);
        }
        check(inBar1,   "(16,0): the walk stays inside bar 1 (the lap never flips — GATE 1's domain)");
        check(ringOK,   "(16,0): every lamp equals the pre-twice-around model on every step (playhead 1.0, trail 0.5, notes 0.35)");
        check(noGhost,  "(16,0): no ghost can render (the current-lap masters 0..15 are all in-window)");
        check(barStable, "(16,0): BAR 1 lit, BAR 2 dim on EVERY step — one-bar windows render STABLE (the D15 claim)");
        check(barExcl,  "(16,0): the BAR LEDs never read alike (lit exclusive vs dim)");
    }

    // ── 2. THE FULL WINDOW (32,0): twice-around content, no ghosts, the BAR LEDs alone ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(32.f, 0.f, false, 0.f, 0.f, false);   // the maximum window
        authorDeterministic(eng);
        std::set<int> win; for (int c = 0; c < 32; ++c) win.insert(c);

        bool ringOK = true, noGhost = true; int wrapLandings = 0; bool trailAtWrap = true;
        int  barFlips = 0;  int prevLit = 0;  bool firstSample = true, exclOK = true, tellOK = true;
        for (int i = 0; i < 65; ++i) {   // two full passes + the wrap home
            eng.advancePlayhead(+1);
            ringOK &= ringMatchesModel(eng, win);
            for (int k = 0; k < 16; ++k)
                noGhost &= (eng.getStepLightBrightness(k) != SequencerEngine::kStepGhostBrightness);
            // the trail REPAIR: at the ring wrap (31 -> 0) the step just left is master 31,
            // whose LAMP is 15 — the cross-lap trail the 2b flip killed.
            if (eng.stepIndex == 0 && i > 0) { ++wrapLandings; trailAtWrap &= (eng.getStepLightBrightness(15) == 0.5f); }
            const float b1 = eng.getBarLightBrightness(0), b2 = eng.getBarLightBrightness(1);
            const int lit = (b1 == 1.f) ? 0 : 1;
            exclOK &= (b1 != b2);
            exclOK &= (b1 == 1.f) == (eng.stepIndex < 16);
            exclOK &= (b2 == 1.f) == (eng.stepIndex >= 16);
            // BAR INDICATOR invariant 2: at every lamp-0 landing the NEW bar's LED is lit.
            if (eng.stepIndex % 16 == 0)
                tellOK &= ((eng.stepIndex < 16 && b1 == 1.f) || (eng.stepIndex >= 16 && b2 == 1.f));
            if (!firstSample && lit != prevLit) ++barFlips;
            prevLit = lit; firstSample = false;
        }
        check(ringOK,  "(32,0): every lamp equals the twice-around model on EVERY step of BOTH laps");
        check(noGhost, "(32,0): the full window renders NO ghosts (P6 — the BAR LEDs alone carry the lap)");
        check(wrapLandings == 2 && trailAtWrap,
              "(32,0): at both ring-wrap landings the trail lands on lamp 15 (master 31's lamp — the 2b kill repaired)");
        check(exclOK,  "(32,0): the BAR LEDs are exclusive and follow stepIndex's bar layer on every step");
        check(tellOK,  "(32,0): at every lamp-0 tick the NEW bar's LED is already lit (BAR INDICATOR invariant 2)");
        check(barFlips == 4, "(32,0): exactly 4 BAR flips over two passes + the wrap home (15->16 and 31->0, twice)");
    }

    // ── 3. THE GHOST ARC at a D3 wrap window (24,16): the map is the CURRENT-LAP master's ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 24.f, 0.f, false);   // cells 24..31 + 0..7 (the D3 wrap class)
        authorDeterministic(eng);
        const std::set<int> win = {24,25,26,27,28,29,30,31,0,1,2,3,4,5,6,7};

        bool ringOK = true, ghostSetOK = true, barOK = true;
        for (int i = 0; i < 32; ++i) {   // two passes of the 16-cell window walk (wraps 7 -> 24)
            eng.advancePlayhead(+1);
            ringOK &= ringMatchesModel(eng, win);
            // the ghost SET equals the flip-table model's CURRENT-LAP set, outside the
            // playhead/trail lamps those levels override
            const int lap = (eng.stepIndex >= 16) ? 1 : 0;
            const std::set<int> ghosts = ghostsForLap(win, lap);
            const int lamp        = ((eng.stepIndex % 16) + 16) % 16;
            const int trailMaster = ((eng.stepIndex - eng.lastPlayDir) % dm::kMaskRing + dm::kMaskRing) % dm::kMaskRing;
            for (int k = 0; k < 16; ++k) {
                if (k == lamp || (k == trailMaster % 16 && win.count(trailMaster))) continue;
                ghostSetOK &= ((eng.getStepLightBrightness(k) == SequencerEngine::kStepGhostBrightness)
                               == (ghosts.count(k) > 0));
            }
            const float b1 = eng.getBarLightBrightness(0), b2 = eng.getBarLightBrightness(1);
            barOK &= ((b1 == 1.f) == (eng.stepIndex < 16)) && ((b2 == 1.f) == (eng.stepIndex >= 16));
        }
        check(ringOK,    "(24,16): every lamp equals the model on every frame (ghosts on the out-of-window current-lap masters)");
        check(ghostSetOK, "(24,16): the ghost set == the flip-table model's CURRENT-LAP set at every frame");
        check(barOK,     "(24,16): the BAR LEDs follow the walk through the wrap (bar-2 cells -> bar-1 cells -> back)");
    }

    // ── 4. A STRADDLING WINDOW (9,16): the flip lands exactly on the bar-line frame ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 9.f, 0.f, false);   // cells 9..24 — crosses the bar line
        authorDeterministic(eng);
        const std::set<int> win = {9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24};

        int flips = 0; int prevLit = -1; bool first = true, tellOK = true, ringOK = true, ghostSetOK = true;
        for (int i = 0; i < 16; ++i) {   // one pass: 9..15 (bar 1), 16..24 (bar 2)
            eng.advancePlayhead(+1);
            ringOK &= ringMatchesModel(eng, win);
            const int lap = (eng.stepIndex >= 16) ? 1 : 0;
            const std::set<int> ghosts = ghostsForLap(win, lap);
            const int lamp        = ((eng.stepIndex % 16) + 16) % 16;
            const int trailMaster = ((eng.stepIndex - eng.lastPlayDir) % dm::kMaskRing + dm::kMaskRing) % dm::kMaskRing;
            for (int k = 0; k < 16; ++k) {
                if (k == lamp || (k == trailMaster % 16 && win.count(trailMaster))) continue;
                ghostSetOK &= ((eng.getStepLightBrightness(k) == SequencerEngine::kStepGhostBrightness)
                               == (ghosts.count(k) > 0));
            }
            const float b1 = eng.getBarLightBrightness(0);
            const int lit = (b1 == 1.f) ? 0 : 1;
            if (!first && lit != prevLit) {
                ++flips;
                // BAR INDICATOR invariant 2: the flip lands on a lamp-0 frame of the new bar
                tellOK &= (eng.stepIndex % 16 == 0);
            }
            prevLit = lit; first = false;
        }
        check(ringOK,     "(9,16): every lamp equals the model through the straddled pass");
        check(ghostSetOK, "(9,16): the ghost set is the CURRENT-LAP model's set on every frame (the D15 flip table)");
        check(flips == 1, "(9,16): ONE BAR flip per pass at the playhead's tick crossing (the designed tell)");
        check(tellOK,     "(9,16): the flip lands on a lamp-0 frame of the new bar (invariant 2 — the clock reading)");
        eng.advancePlayhead(+1);   // the pass boundary: the window wrap 24 -> 9
        check(eng.stepIndex == 9 && eng.getBarLightBrightness(0) == 1.f,
              "(9,16): the window-wrap landing restores lap 0 (two flips per full cycle; the pass starts where it started)");
    }

    // ── 5. MODE D (3) — no discrete playhead: BOTH bar LEDs silent, no playhead lamp ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(32.f, 0.f, false, 0.f, 0.f, false);
        authorDeterministic(eng);
        eng.modeSelect = 3;
        eng.advancePlayhead(+1);
        bool silent = true, noPh = true, contentOK = true;
        for (int i = 0; i < 8; ++i) {
            eng.advancePlayhead(+1);
            silent &= (eng.getBarLightBrightness(0) == 0.f && eng.getBarLightBrightness(1) == 0.f);
            for (int k = 0; k < 16; ++k) {
                const float b = eng.getStepLightBrightness(k);
                noPh      &= (b != 1.0f && b != 0.5f);   // neither playhead nor trail
                contentOK &= (b == 0.35f);               // the full window: every lamp is in-window content
            }
        }
        check(silent,    "mode D: both BAR LEDs silent wherever the discrete playhead is absent (D10)");
        check(noPh,      "mode D: no playhead lamp and no comet trail (the continuous mode has no discrete step)");
        check(contentOK, "mode D: the window map still renders (the standing content arc)");
    }

    // ── 6. PRE-START: the -1 park keeps every lamp and both BAR LEDs dark ──
    {
        SequencerEngine eng; eng.reset();   // stepIndex == -1 before the first advance
        check(eng.stepIndex == -1, "a fresh engine parks the playhead at -1");
        bool noPh = true;
        for (int k = 0; k < 16; ++k) noPh &= (eng.getStepLightBrightness(k) != 1.0f);
        check(noPh, "pre-start: no lamp reads the playhead level (the % 16 landing is guarded — -1 % 16 would light lamp 15)");
        check(eng.getBarLightBrightness(0) == 0.f && eng.getBarLightBrightness(1) == 0.f,
              "pre-start: both BAR LEDs silent");
    }

    // ── 7. THE ATOMIC RE-SCOPE: indicator, dim set and playhead lamp move together ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 12.f, 0.f, false);   // cells 12..27 — crosses the bar line
        authorDeterministic(eng);
        for (int i = 0; i < 4; ++i) eng.advancePlayhead(+1);      // 12, 13, 14, 15
        check(eng.stepIndex == 15, "the walk sits on cell 15 (bar 1's last cell)");
        const float b1_before = eng.getBarLightBrightness(0);
        const float ph_before = eng.getStepLightBrightness(15);
        eng.advancePlayhead(+1);
        check(eng.stepIndex == 16, "the next landing is cell 16 — the bar line");
        const float b1_after  = eng.getBarLightBrightness(0);
        const float b2_after  = eng.getBarLightBrightness(1);
        const float ph_after  = eng.getStepLightBrightness(0);
        check(b1_before == 1.f,                     "before the tick: BAR 1 lit");
        check(b1_after != 1.f && b2_after == 1.f,   "after the tick: BAR 2 lit (exclusive — one gate, two surfaces)");
        check(ph_before == 1.f && ph_after == 1.f,  "the playhead lamp moved 15 -> lamp 0 in the SAME landing");
        // the ghost set swapped with them: at stepIndex 15 (lap 0) the out-of-window masters
        // are 0..11 (lamps 0..11 ghost) and 28..31 wait in the far half; at 16 (lap 1) the
        // in-window masters are 16..27 (lamps 0..11) and the OUT set is exactly 28..31
        // (lamps 12..15) — the map re-rendered from the ONE landed frame. Lamp 15 rides
        // OVER the map at this landing: the step just left is master 15 (in-window), so
        // the trail's 0.5 overrides what would have been its ghost.
        bool swapOK = true;
        const int trailMaster = ((eng.stepIndex - eng.lastPlayDir) % dm::kMaskRing + dm::kMaskRing) % dm::kMaskRing;
        for (int k = 0; k < 16; ++k) {
            const bool ghost = (eng.getStepLightBrightness(k) == SequencerEngine::kStepGhostBrightness);
            if (k <= 11) swapOK &= !ghost;          // masters 16..27: ALL in-window on lap 1
            else if (k == trailMaster % 16) swapOK &= !ghost;   // the trail override (master 15 in-window)
            else         swapOK &= ghost;           // masters 28..31: ALL out on lap 1
        }
        check(swapOK, "the ghost set swapped WITH the indicator and the playhead (the atomic re-scope)");
    }

    std::printf("test_ring_bar_indicator: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
