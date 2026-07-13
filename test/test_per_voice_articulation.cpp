// EAST_EXTRA_LANES stage 2 — per-voice articulation, clamped to the mono event grid.
// Proves the three properties the design rests on:
//   1. flag OFF  → every voice's nvIdx == mono's (bit-identical to today)
//   2. flag ON, identity LOR → still identical (doubly inert)
//   3. flag ON, non-identity LOR → voices diverge, but ALWAYS hold <= mono's hold (the clamp)
#include "SequencerEngine.hpp"
#include "NoteValues.hpp"
#include <cstdio>
#include <cmath>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++passed; }
    else    { ++failed; std::printf("  FAIL: %s\n", what); }
}

int main() {
    SequencerEngine e;
    e.reset();
    PatternInput in{};
    in.variationAmount   = 0.85f;   // window widens toward shorter notes
    in.noteVariationMask = 0b111;   // all triplet/1-32 values legal

    // a shared mono VARIATION shape
    for (int i = 0; i < 16; ++i) e.pe.variationRandom[i] = (float)((i * 7 + 3) % 16) / 16.f;

    e.lastNoteVal_ = 4.f;   // NOTE_VALUE = 1/8

    // ── 1. flag OFF: identical to mono, for every voice and step ──
    e.perVoiceArticulation = false;
    for (int step = 0; step < 64; ++step) {
        e.totalStepsElapsed = step;
        e.lastStepResult.nvIdx = 2 + (step % 4);
        for (int v = 0; v < 15; ++v)
            check(e.nvIdxForVoice(v, in) == e.lastStepResult.nvIdx, "flag off -> mono nvIdx");
    }

    // ── 2. flag ON, identity LOR (len 16, off 0, rot 0 — as reset() now seeds) ──
    e.perVoiceArticulation = true;
    for (int step = 0; step < 64; ++step) {
        e.totalStepsElapsed = step;
        // mono reads its own VAR strand at this step; give the voice the same window
        int monoIdx = e.getStrandIdx(step, 16, 0, 0) & 0x0F;
        e.lastStepResult.nvIdx = e.getNoteLenIdx(e.lastNoteVal_, in, e.pe.variationRandom[monoIdx]);
        for (int v = 0; v < 15; ++v)
            check(e.nvIdxForVoice(v, in) == e.lastStepResult.nvIdx, "identity LOR -> mono nvIdx");
    }

    // ── 3. flag ON, per-voice LOR windows: divergence + the clamp invariant ──
    e.polyLORRef(1, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_LEN) = 6;   // V3
    e.polyLORRef(2, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_LEN) = 12;  // V4
    e.polyLORRef(2, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_OFF) = 3;
    e.polyLORRef(2, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_ROT) = 2;

    int diverged = 0, checked = 0;
    for (int step = 0; step < 96; ++step) {
        e.totalStepsElapsed = step;
        int monoIdx = e.getStrandIdx(step, 16, 0, 0) & 0x0F;
        e.lastStepResult.nvIdx = e.getNoteLenIdx(e.lastNoteVal_, in, e.pe.variationRandom[monoIdx]);
        float monoDur = noteValueSteps(e.lastStepResult.nvIdx);
        for (int v : {1, 2}) {
            int nv = e.nvIdxForVoice(v, in);
            float dur = noteValueSteps(nv);
            // THE CLAMP: a voice may release early, never hold past mono's next event.
            check(dur <= monoDur + 1e-6f, "clamp: voice hold <= mono hold");
            // and the clamp is implemented as max() on the index — verify that too
            check(nv >= e.lastStepResult.nvIdx, "clamp: nv index >= mono index");
            ++checked;
            if (nv != e.lastStepResult.nvIdx) ++diverged;
        }
    }
    check(diverged > 0, "non-identity LOR actually diverges");
    std::printf("  divergence: %d/%d voice-steps differ from mono\n", diverged, checked);

    // ── 4. table contract the clamp relies on: slowest -> fastest ──
    for (int i = 0; i + 1 < NUM_NOTE_VALUES; ++i)
        check(noteValueSteps(i) > noteValueSteps(i + 1), "NOTE_VALUES strictly decreasing");

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
