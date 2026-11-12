// test_range_strip_gesture.cpp — CC_FIX_VOICE_RANGE_STRIP_INTERACTION: the
// voice-range strip's THREE interaction faults, fixed and pinned — plus the
// ADDITIONS: D the preset bank deleted, E the strip/lane drag owners separated,
// F the engine-path lane clamps (the midpoint snap) and G the named constants.
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
//   F/G. the lane-window BOUNDS are named constants at the cap (kLaneOffsetMax 31 /
//       kLaneLengthMax 32, defined beside kPatternLengthCap), and NO bounding literal
//       (the 0..15/1..16 float pairs, the % 16 wraps) remains in the two managers'
//       live lane-window code — the class that caused the midpoint snap.
//
// East and Macro share ONE editor (SandsVisualEditorV4) — one code path, no
// per-host divergence to pin. No engine, store, geometry or rotation-convention
// change (the doc's DO-NOT); the suite pins pure gesture math.
//
// Build (run_all.sh): test_range_strip_gesture|

#include <cstdio>
#include <cstdint>
#include <string>

#include "ui/SandsRangeGestureCore.hpp"
#include "dsp/engines/PatternEngine.hpp"   // the lane-window constants (§G) — <rack.hpp> resolves to test/rack.hpp

using redDot::SandsRangeGestureCore;

// §G: the named bounds ARE the cap-derived lane domains (compile-time — a drifted
// definition fails the build, not just the runtime gate below).
static_assert(dotModular::kLaneOffsetMax == dotModular::kPatternLengthCap - 1,
              "kLaneOffsetMax is the largest step INDEX (the cap minus one)");
static_assert(dotModular::kLaneLengthMax == dotModular::kPatternLengthCap,
              "kLaneLengthMax is the largest window LENGTH (the cap)");

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// slurp a text file (the D/G gates read the live sources; run_all runs from repo root)
static bool slurpFile(const char* path, std::string& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

// strip // and /* */ comments (string literals kept intact) — the §G audit checks
// LIVE code only; the recorded-allowed hits (default-value 16s, the retired debug
// blocks) live in comments, and the doc's gate is "no bounding literal in the
// window path", not "no number anywhere".
static std::string stripComments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool inLine = false, inBlock = false, inStr = false;
    for (size_t i = 0; i < src.size(); ++i) {
        const char c = src[i];
        const char n = (i + 1 < src.size()) ? src[i + 1] : '\0';
        if (inLine) {
            if (c == '\n') { inLine = false; out += c; }
        } else if (inBlock) {
            if (c == '*' && n == '/') { inBlock = false; ++i; out += ' '; }
        } else if (inStr) {
            out += c;
            if (c == '\\' && n) { out += n; ++i; }   // keep escaped chars verbatim
            else if (c == '"') inStr = false;
        } else {
            if (c == '/' && n == '/')      { inLine = true;  ++i; }
            else if (c == '/' && n == '*') { inBlock = true; ++i; }
            else if (c == '"')             { inStr = true;  out += c; }
            else out += c;
        }
    }
    return out;
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

    // ── GATE D: the preset bank is GONE (the grep-in-test precedent — the doc's
    // "no reference to presetBank or applyPreset in src/ui"; a reintroduction fails
    // the battery, not just review) ─────────────────────────────────────────────
    {
        FILE* f = std::fopen("src/ui/SandsVisualEditorV4.hpp", "rb");
        bool ok = (f != nullptr);
        std::string src;
        if (f) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) src.append(buf, n);
            std::fclose(f);
        }
        const char* gone[] = { "presetBank", "PRESET_BANK_SIZE", "PresetSeed",
                               "initializePresets", "loadPreset", "drawPresetPanel",
                               "showPresetPanel", "selectedPreset" };
        for (const char* sym : gone)
            ok = ok && src.find(sym) == std::string::npos;
        check(ok, "D: the vestigial preset bank is fully gone from the live editor "
                  "(no bank, seeding, apply path, panel or flag)");
    }

    // ── GATE E: the strip and the lanes share the ONE core — the lane drags run the
    // SAME press-anchored edge law as the strip (one function family, both owners;
    // a lane drag never writes the strip's lor[] and vice versa by construction —
    // two owners, set at press, per the doc) ────────────────────────────────────
    {
        bool ok = true;
        // a lane press at offset 4 len 6, dragged 3 cells right on the START edge:
        // the lane's window follows the same law as the strip's (the same functions).
        const int grabOff = 4, grabLen = 6, delta = 3;
        const int off = SandsRangeGestureCore::startOff(delta, grabOff, RING);
        const int len = SandsRangeGestureCore::startLen(delta, grabOff, grabLen, RING);
        ok = ok && off == 7 && len == 3;                    // the front edge at 7, the back stays at 9
        ok = ok && SandsRangeGestureCore::endLen(delta, grabOff, grabLen, RING) == 9;
        check(ok, "E: the lane edge drags run the strip's press-anchored law (one core, both owners)");
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

    // ── GATE F/G: the lane-window constants — the named bounds exist, derive from
    // the cap, and mean what §G says: OFFSET is the largest step INDEX, LENGTH the
    // largest window; ROTATION deliberately has NO constant (per-L, clamped to the
    // lane's length−1 at each site — the law gates A/C above pin at the gesture
    // level). The gesture core's ring IS the lane length max (one domain). ──────
    {
        bool ok = true;
        ok = ok && dotModular::kLaneLengthMax  == dotModular::kPatternLengthCap;
        ok = ok && dotModular::kLaneOffsetMax  == dotModular::kPatternLengthCap - 1;
        ok = ok && dotModular::kLaneLengthMax  == 32;
        ok = ok && dotModular::kLaneOffsetMax  == 31;
        ok = ok && dotModular::kLaneOffsetMax  <  dotModular::kLaneLengthMax;   // an offset can address the last cell
        ok = ok && RING == dotModular::kLaneLengthMax;   // the editor/gesture ring == the lane length max
        check(ok, "F/G: kLaneOffsetMax/kLaneLengthMax defined beside kPatternLengthCap at 31/32");
    }

    // ── GATE G (§F+G): no bounding literal remains in the lane-window path — the
    // two managers' LIVE code (comments stripped). The pinned forms are exactly the
    // F bug class: the 0..15/1..16 float bound pairs (applyMonoCV/applyMacroCV/
    // addCV/eastDelta/endpointDeltas/combineLOR/eastLorVal lo-hi args) and the
    // % 16 wraps. The recorded-allowed literals (the default-value 16s, the
    // voice-count clamps, the retired debug blocks) are either comments or the
    // voice domain — none is a window bound. ────────────────────────────────────
    {
        const char* files[] = { "src/dsp/managers/MonsoonSandsManager.cpp",
                                "src/dsp/managers/MonsoonExpanderManager.cpp" };
        const char* bounds[] = { "0.f, 15.f", "1.f, 16.f", "% 16", "%16", "% 15", "%15" };
        bool ok = true;
        for (const char* file : files) {
            std::string src;
            ok = ok && slurpFile(file, src);
            const std::string code = stripComments(src);
            for (const char* b : bounds)
                ok = ok && code.find(b) == std::string::npos;
        }
        check(ok, "G: no bounding literal in the managers' lane-window path (the named constants own the bounds)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
