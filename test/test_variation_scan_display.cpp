// test_variation_scan_display — the VARIATION lane's gate-mode SCAN-AHEAD chevron.
//
// THE REGRESSION (fix/variation-scan-display): commit 84229f7e (bug 18) retired the
// VAR-scan display at all five Sands sites on the premise that "the scan could leave
// the lane's window", so the VARIATION chevron stopped creeping ahead of the other
// lanes during a gate-mode hold and then rejoining on advance — the visual
// confirmation of the freeze fix that Rodney relies on ("this no longer happens").
//
// THE PREMISE IS FALSE on the engine side. stepAdvanceStrandIdx delegates to
// getStrandIdx, whose result is  (((tick + delta) mod len + len) mod len + off) mod ring
// — ALWAYS inside [off, off+len) on the ring, for ANY signed delta. The scan jumps to a
// DIFFERENT IN-WINDOW cell (ahead of the shared playhead), then rejoins at delta 0. It
// never leaves the lane's own window. So restoring the display is safe.
//
// REPRODUCE-FIRST — the before/after oracle is the STRUCTURAL pin (gates S1-S3): bug 18
// deleted every variationLanePlayhead* caller, so "the caller is present" FAILS on the
// retired engine and PASSES once the helpers + sites are restored. Gates E1-E3 are the
// safety proof: the engine's scanned draw is in-window for every delta, and bit-identical
// to the shared playhead at rest (delta 0).
//
// Compile: see test/run_all.sh (the TESTS[] registry is the source of truth).

#include <cstdio>
#include <cstdlib>
#include <string>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

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

// x is in the window [off, off+len) on a ring of size `ring` iff (x - off) mod ring < len.
static bool inWindow(int x, int off, int len, int ring) {
    int d = ((x - off) % ring + ring) % ring;
    return d >= 0 && d < len;
}

int main() {
    const int RING = SequencerEngine::kMaskRing;
    const int VAR  = dotModular::STRAND_VARIATION;

    // ── E1: mono IDENTITY window (as reset() seeds: len 16, off 0, rot 0) ──────────
    // For every tick and every signed scan delta, the scanned draw is in [0,16); and
    // at rest (delta 0) it equals the shared playhead getVariationStep().
    {
        SequencerEngine e; e.reset();
        const auto mono = VoiceResolver::mono();
        bool ok = true;
        for (int tick = -3; tick < 40; ++tick) {
            e.laneTick_[VAR] = tick;
            for (int delta = -20; delta <= 20; ++delta) {
                int idx = e.stepAdvanceStrandIdx(mono, delta);
                ok = ok && idx >= 0 && idx < RING && inWindow(idx, 0, 16, RING);
            }
            ok = ok && (e.stepAdvanceStrandIdx(mono, 0) == e.getVariationStep());  // bit-identical at rest
        }
        check(ok, "E1: mono identity — scan draw in [0,16) for every (tick,delta); delta 0 == shared playhead");
    }

    // ── E2: mono DIALED window (len 7, off 5, rot 3 — a non-trivial, non-wrapping box) ─
    {
        SequencerEngine e; e.reset();
        e.strandLenRef(VAR) = 7;
        e.strandOffRef(VAR) = 5;
        e.strandRotRef(VAR) = 3;
        const auto mono = VoiceResolver::mono();
        bool ok = true;
        for (int tick = -3; tick < 40; ++tick) {
            e.laneTick_[VAR] = tick;
            for (int delta = -20; delta <= 20; ++delta) {
                int idx = e.stepAdvanceStrandIdx(mono, delta);
                ok = ok && inWindow(idx, 5, 7, RING);   // never slides to rotation+something outside the box
            }
            ok = ok && (e.stepAdvanceStrandIdx(mono, 0) == e.getVariationStep());
        }
        check(ok, "E2: mono dialed (L7 O5 R3) — scan draw stays in [5,12); delta 0 == shared playhead");
    }

    // ── E3: a per-voice LOCAL-EAST window (len 6, off 4, rot 2) ────────────────────
    // The per-voice chevron scans within THIS voice's own window — never mono's, never
    // out of range. (VoiceResolver::slot(n) mints bank.v == n-1.)
    {
        SequencerEngine e; e.reset();
        const auto bank = VoiceResolver::slot(3);   // bank.v == 2
        e.polyLORRef(bank, dotModular::LaneId{SequencerEngine::EDITOR_LANE_VARIATION}, SequencerEngine::LOR_LEN) = 6;
        e.polyLORRef(bank, dotModular::LaneId{SequencerEngine::EDITOR_LANE_VARIATION}, SequencerEngine::LOR_OFF) = 4;
        e.polyLORRef(bank, dotModular::LaneId{SequencerEngine::EDITOR_LANE_VARIATION}, SequencerEngine::LOR_ROT) = 2;
        e.setVarlegLocalEast(bank, 0, true);   // VAR: Local East — the dialed window now applies
        bool ok = true;
        for (int tick = -3; tick < 40; ++tick) {
            e.laneTickV_[bank.v][VAR] = tick;
            for (int delta = -20; delta <= 20; ++delta) {
                int idx = e.stepAdvanceStrandIdx(bank, delta);
                ok = ok && inWindow(idx, 4, 6, RING);
            }
            ok = ok && (e.stepAdvanceStrandIdx(bank, 0) == e.getVariationStepForVoice(bank));
        }
        check(ok, "E3: per-voice Local East (L6 O4 R2) — scan draw in [4,10); delta 0 == voice's own playhead");
    }

    // ── S1-S3: the RESTORATION structural pins (the before/after oracle) ───────────
    // bug 18 deleted every variationLanePlayhead* caller (the no-caller grep was its
    // pin); the restore brings them back. These FAIL on the retired engine, PASS now.
    {
        std::string veh, east, macro;
        bool got = slurpFile("src/ui/VisualExpanderHelpers.hpp", veh)
                && slurpFile("src/StraitsEastSandsVisual.cpp", east)
                && slurpFile("src/StraitsSandsMacroVisual.cpp", macro);
        check(got, "slurp the three UI sources");

        // S1: the helpers exist again and read the engine's SIGNED scan delta.
        check(veh.find("variationLanePlayhead") != std::string::npos
              && veh.find("stepAdvanceScanDelta") != std::string::npos
              && veh.find("stepAdvanceStrandIdx") != std::string::npos,
              "S1: VisualExpanderHelpers restores variationLanePlayhead + reads stepAdvanceScanDelta");

        // S2: East draws the VAR scan (mono sites + the per-voice site).
        check(east.find("variationLanePlayhead(") != std::string::npos
              && east.find("variationLanePlayheadForVoice(") != std::string::npos,
              "S2: East draws variationLanePlayhead / ...ForVoice at the VAR lane");

        // S3: Macro draws THIS view-voice's VAR scan.
        check(macro.find("variationLanePlayheadForVoice(") != std::string::npos,
              "S3: Macro draws variationLanePlayheadForVoice at the VAR lane");

        // the delegation for NON-VAR lanes is untouched (the ONE law still owns them).
        check(veh.find("lanePlayheadBar") != std::string::npos,
              "S4: calcPlayhead still delegates to the shared lanePlayheadBar law");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
