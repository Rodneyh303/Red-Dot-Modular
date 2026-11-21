// test_lantern_mask — Lantern must show what the OUTPUT does: post-mask by default (Rodney, Rack 2026-11-21:
// "Lantern is not showing the impact of the painted mask nor of absolute mode. It shows notes where the VCV
// scope doesn't"). The mask is an OUTPUT-stage gate (outputMaskPasses zeroes gate + CV in MonsoonOutputGenerator);
// Lantern samples the engine's gate state, which is BEFORE that mute, so masked steps drew as notes.
//
// Pinned on the real engine + LanternRecordCore (the test_lantern_bars harness shape):
//   1. POST (default): a column the mask mutes records Inactive for EVERY row; unmuted columns are untouched.
//   2. PRE (postMask=false): the pre-mask view — the muted columns still show the notes (composing view).
//   3. Cycle everywhere (no mask): POST == PRE, cell for cell (the default path is untouched).
//   4. Abs1 window AND a painted overlay both mute (the two mask sources).
// Build (run_all.sh): test_lantern_mask|$SE $GS $PE $CE
#include <cstdio>
#include <cstdint>
#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/LanternRecordCore.hpp"
namespace dm = dotModular;
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { if (g_fail < 30) std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static void author(SequencerEngine& eng, PatternInput& in) {
    for (int d = 0; d < 12; ++d) in.semiWeights[d] = 0.6f;
    for (int v = 1; v <= 16; ++v) {
        const auto vs = dm::VoiceResolver::slot(v);
        eng.voices[vs].restProb = 0.f; eng.voices[vs].legatoProb = 0.f; eng.voices[vs].accentProb = 0.f;
    }
    for (int i = 0; i < dm::kPatternLengthCap; ++i) {
        const float t = (float)i / (float)dm::kPatternLengthCap;
        eng.pe.melodyRandom[i] = 0.05f + 0.9f * t; eng.pe.rhythmRandom[i] = 0.15f + 0.7f * t;
        eng.pe.octaveRandom[i] = t; eng.pe.qmixRandom[i] = t; eng.pe.accentRandom[i] = t;
        eng.pe.variationRandom[i] = t; eng.pe.legatoRandom[i] = t;
    }
    eng.numPolyVoices = 3;
}
// march one 16-step phrase; returns the recorded bar-0 cells
static void march(SequencerEngine& eng, LanternRecordCore& rec, PatternInput& in, bool post) {
    rec.postMask = post;
    redDot::RateManager rm;
    for (int i = 0; i < 16; ++i) {
        rm.advance(redDot::Rate::Step, true);
        auto r = eng.executeModeA(rm, in.noteValue, in, +1);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        const auto e = rec.beginSample(eng);
        if (e.valid) rec.recordMonsoonSource(eng, e);
    }
}
static bool inWin(int col, int O, int L) { return col >= O && col < O + L; }

int main() {
    const int O = 4, L = 4;                       // the abs1 window [4,8)
    auto build = [&](SequencerEngine& eng, PatternInput& in, bool abs1) {
        eng.reset(); author(eng, in);
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        if (abs1) {
            eng.maskMacroKind_ = SequencerEngine::MASK_DYNAMIC;
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = L;
            eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = O;
        }
    };
    // reference run with NO mask (what the pre-mask view must equal)
    LanternRecordCore refRec;
    { SequencerEngine eng; PatternInput in; build(eng, in, false); march(eng, refRec, in, true); }

    for (int post = 0; post <= 1; ++post) {
        SequencerEngine eng; PatternInput in; build(eng, in, true);
        LanternRecordCore rec; march(eng, rec, in, post != 0);
        int mutedSeen = 0, soundingOutside = 0, wrongInside = 0;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 16; ++col) {
                const auto& c = rec.cells[0][row][col]; const auto& r = refRec.cells[0][row][col];
                if (inWin(col, O, L)) { if (c.type != r.type) ++wrongInside; }
                else { ++mutedSeen; if (c.type != lantern::NoteType::Inactive) ++soundingOutside;
                       if (!post) CHECK(c.type == r.type, "PRE: a masked column still shows the pre-mask note"); }
            }
        if (post) {
            CHECK(soundingOutside == 0, "POST: every column outside the abs1 window is Inactive for every row");
            CHECK(wrongInside == 0, "POST: columns inside the window are exactly the unmasked recording");
        } else {
            CHECK(soundingOutside > 0, "PRE: the masked columns do show notes (the toggle changes something)");
        }
        CHECK(mutedSeen == 4 * (16 - L), "swept every muted cell");
    }
    // cycle everywhere: POST == PRE cell for cell
    { SequencerEngine e1, e2; PatternInput i1, i2; build(e1, i1, false); build(e2, i2, false);
      LanternRecordCore a, b; march(e1, a, i1, true); march(e2, b, i2, false);
      bool same = true;
      for (int row = 0; row < 4; ++row) for (int col = 0; col < 16; ++col)
          same = same && a.cells[0][row][col].type == b.cells[0][row][col].type && a.cells[0][row][col].pitchV == b.cells[0][row][col].pitchV;
      CHECK(same, "no mask: POST == PRE cell for cell"); }
    if (g_fail) { std::printf("test_lantern_mask: %d FAILED\n", g_fail); return 1; }
    std::printf("test_lantern_mask: OK\n");
    return 0;
}
