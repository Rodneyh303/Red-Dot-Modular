// test_output_mask_gate.cpp — LOR_EDITING_BUG_LIST bug 16 (the AUDIO regression
// report: "Output mask has NO effect. Painting the mask does not gate the output")
// answered with the doc's own demanded pins, END-TO-END from the EDITOR-STORE
// shapes (exactly where the strip paint and the mask menus write) through
// pushMaskEditor to the composed output gate at EVERY global step:
//
//   1. ALL-ON CYCLE (the defaults) == NO gate — every voice, every step passes,
//      bit-identical to the pre-mask behaviour (the ≤16 goldens' precondition).
//   2. CYCLE + PAINTED BITS OFF — exactly those steps silenced; every other
//      voice (the Macro tier, cycle) unaffected.
//   3. PAINTED CHANGED -> THE GATE CHANGES (the doc: "the written-but-never-read
//      smell becomes a test") — repaint a cell and the gate follows.
//   4. ABS1 + A NON-FULL LOR — SILENT at global steps outside [O, O+L), sounds
//      inside; INCLUDING the WRAPPED window (the doc's missing-behaviour pin).
//   5. THE MACRO TIER'S PAINT gates the followers (the global mask).
//   6. THE OUTPUT STAGE READS THE GATE (the structural pin, the D grep-in-test
//      precedent): MonsoonOutputGenerator applies outputMaskPasses at the GLOBAL
//      step (engine.stepIndex) on BOTH the mono and the poly paths — the
//      composition is LIVE at the output, not merely defined on the engine.
//   7. THE WRITE-ONLY ECHO IS GONE: maskPainted_/maskMacroPainted_ (the raw
//      store echo the bug report flagged as "written but never read") are
//      deleted — the working read is the ANCHORED copy (effectiveOverlayPattern),
//      and this pin keeps the smell from coming back.
//
// THE AUDIT THIS TEST RECORDS (the commit message carries the full report): the
// report's located cause — "maskPainted_ is written but never READ; the
// composition at the output gate was dropped" — is DISPROVEN on this branch:
// the working read path is maskAnchored_/maskMacroAnchored_ via
// effectiveOverlayPattern (maintained by pushMaskEditor's anchor law, both
// tiers), the composition is outputMaskPasses' (abs1 window AND painted
// overlay, keyed on the GLOBAL step), and the output stage calls it for mono
// and poly. The reads stay ungated BY DESIGN (b134490's model — the mask is an
// OUTPUT gate in global-position space; keying it to the read cell would make
// it inert). The write-only maskPainted_ echo was real and is now deleted.
//
// Build (run_all.sh): test_output_mask_gate|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <string>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;
using dotModular::VoiceSlot;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

static bool slurpFile(const char* path, std::string& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}
static size_t countOccurrences(const std::string& hay, const std::string& needle) {
    size_t c = 0, pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) { ++c; pos += needle.size(); }
    return c;
}

int main() {
    const uint64_t ALL = dotModular::kMaskLenMask;   // the ring's all-on

    // ── THE EDITOR-STORE SHAPES (Monsoon's arrays — where the UI writes) ────────
    uint64_t macroBits = ALL; float macroKind = 0.f, macroFollowR = 0.f;
    uint64_t eastBits[16];      float eastKind[16], eastFollowR[16], eastLocal[16];
    float eastLor[16 * 3], eastLorDir[16], eastLorLocal[16];
    float macroLor[3] = {16, 0, 0}; float macroLorDir = 0.f;
    for (int v = 0; v < 16; ++v) {
        eastBits[v] = ALL; eastKind[v] = 0.f; eastFollowR[v] = 0.f; eastLocal[v] = 0.f;
        eastLor[v * 3 + 0] = 16; eastLor[v * 3 + 1] = 0; eastLor[v * 3 + 2] = 0;
        eastLorDir[v] = 0.f; eastLorLocal[v] = 0.f;
    }
    SequencerEngine eng;
    auto pushLor = [&]() { eng.pushVoiceLorEditor(macroLor, macroLorDir, eastLor, eastLorDir, eastLorLocal); };
    auto pushMask = [&]() { eng.pushMaskEditor(&macroBits, macroKind, macroFollowR,
                                               eastBits, eastKind, eastFollowR, eastLocal); };
    auto passes = [&](int slot, int s) { return eng.outputMaskPasses(VoiceResolver::slot(slot + 1), s); };

    // ── GATE 1: the defaults (all-on cycle) == NO gate ───────────────────────────
    pushLor(); pushMask();
    {
        bool ok = true;
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 32; ++s)
                ok = ok && passes(v, s);
        check(ok, "1: the all-cycle default — every voice passes every step "
                  "(no gating; the pre-mask behaviour bit-identical)");
    }

    // ── GATE 2: cycle + painted bits OFF — exactly those steps silenced ─────────
    {
        // Voice slot 3 (V4): independent mask tier, kind PAINTED (2), cells 4,5,20 off.
        eastLocal[3] = 1.f; eastKind[3] = 2.f;
        eastBits[3] = ALL & ~((1ull << 4) | (1ull << 5) | (1ull << 20));
        pushMask();
        bool ok = true;
        for (int s = 0; s < 32; ++s)
            ok = ok && passes(3, s) == !(s == 4 || s == 5 || s == 20);
        for (int v = 0; v < 16; ++v) {             // every OTHER voice: the Macro
            if (v == 3) continue;                  // tier (cycle) — unaffected
            for (int s = 0; s < 32; ++s)
                ok = ok && passes(v, s);
        }
        check(ok, "2: cycle + painted bits off — exactly those steps silenced; "
                  "the other voices (the Macro tier) unaffected");

        // ── GATE 3: painted CHANGED -> the gate changes (the smell becomes a test)
        eastBits[3] |= (1ull << 4);                // repaint cell 4 ON
        pushMask();
        ok = passes(3, 4);
        eastBits[3] = ALL;                          // all back on
        pushMask();
        ok = ok && passes(3, 5);
        eastBits[3] = ALL & ~(1ull << 31);          // a bar-2 cell off
        pushMask();
        ok = ok && !passes(3, 31) && passes(3, 30);
        eastBits[3] = ALL; eastKind[3] = 0.f; eastLocal[3] = 0.f;   // restore
        pushMask();
        ok = ok && passes(3, 31);
        check(ok, "3: painted changed -> the gate changes (repaint ON, all-on, a "
                  "bar-2 cell off, restore) — the mask is READ, live");
    }

    // ── GATE 4: abs1 + a non-full LOR — silent OUTSIDE [O, O+L), incl. the wrap ──
    {
        // Voice slot 5 (V6): own LOR row {L=8, O=20} (contiguous 20..27) + kind
        // ABS1 (DYNAMIC). The read runs the full phrase; the OUTPUT plays the
        // window only.
        eastLorLocal[5] = 1.f;
        eastLor[5 * 3 + 0] = 8; eastLor[5 * 3 + 1] = 20; eastLor[5 * 3 + 2] = 0;
        eastLocal[5] = 1.f; eastKind[5] = 1.f; eastBits[5] = ALL;
        pushLor(); pushMask();
        bool ok = true;
        for (int s = 0; s < 32; ++s)
            ok = ok && passes(5, s) == (s >= 20 && s < 28);
        // the WRAPPED window {L=4, O=30}: the region 30,31,0,1 (the wrap carries
        // across the seam; the real end cuts).
        eastLor[5 * 3 + 0] = 4; eastLor[5 * 3 + 1] = 30;
        pushLor(); pushMask();
        for (int s = 0; s < 32; ++s)
            ok = ok && passes(5, s) == (s >= 30 || s < 2);
        // the FULL window == no gate (the identity).
        eastLor[5 * 3 + 0] = 32; eastLor[5 * 3 + 1] = 0;
        pushLor(); pushMask();
        for (int s = 0; s < 32; ++s)
            ok = ok && passes(5, s);
        check(ok, "4: abs1 + a non-full LOR — silent outside [O, O+L), the wrapped "
                  "window carries across the seam, the full window is identity");
    }

    // ── GATE 5: the MACRO tier's paint gates the followers ──────────────────────
    {
        // The GLOBAL mask (Macro's strip): kind PAINTED, cells 2 and 17 off. Every
        // FOLLOWER voice (mask tier not local) is gated by it; the independent
        // voices keep their own rows.
        macroKind = 2.f;
        macroBits = ALL & ~((1ull << 2) | (1ull << 17));
        pushMask();
        bool ok = true;
        for (int v = 0; v < 16; ++v) {
            if (v == 5) continue;                    // slot 5 still independent (gate 4)
            for (int s = 0; s < 32; ++s)
                ok = ok && passes(v, s) == !(s == 2 || s == 17);
        }
        ok = ok && passes(5, 2);                     // the independent voice unaffected
        macroKind = 0.f; macroBits = ALL;            // restore
        pushMask();
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 32; ++s)
                if (v != 5) ok = ok && passes(v, s);
        check(ok, "5: the Macro tier's paint gates every follower; the independent "
                  "voice keeps its own row");
    }

    // ── GATE 6: the OUTPUT STAGE reads the gate (the structural pin) ────────────
    {
        std::string og;
        bool ok = slurpFile("src/dsp/managers/MonsoonOutputGenerator.cpp", og);
        // BOTH paths apply the gate, keyed on the GLOBAL step (engine.stepIndex):
        // the mono block and the poly block — the composition is LIVE at the output.
        ok = ok && countOccurrences(og, "engine.outputMaskPasses(") >= 2;
        ok = ok && og.find("const int maskStep = engine.stepIndex") != std::string::npos;
        check(ok, "6: the output stage applies outputMaskPasses at the GLOBAL step "
                  "on both the mono and the poly paths (the composition is live)");
    }

    // ── GATE 7: the write-only echo is GONE (the flagged smell, deleted) ─────────
    {
        std::string hpp, cpp;
        bool ok = slurpFile("src/dsp/engines/SequencerEngine.hpp", hpp)
               && slurpFile("src/dsp/engines/SequencerEngine.cpp", cpp);
        ok = ok && hpp.find("maskPainted_") == std::string::npos
               && cpp.find("maskPainted_") == std::string::npos
               && hpp.find("maskMacroPainted_") == std::string::npos
               && cpp.find("maskMacroPainted_") == std::string::npos;
        // and the WORKING read is present: the anchored copy feeds the gate.
        ok = ok && hpp.find("effectiveOverlayPattern") != std::string::npos;
        check(ok, "7: the write-only maskPainted_ echo is deleted; the working read "
                  "(the anchored copy) stands");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
