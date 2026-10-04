// Per-voice articulation, clamped to the mono event grid.
// SANDS CONSOLIDATION Step 5: perVoiceArticulation flag removed — draw is always-on.
// Updated for the per-voice VAR draw model (f9c4189): a delegated VAR voice shares mono's
// STEP (reading position) but rolls/reads its OWN per-voice draw (polyRandom(bank, PL_VARIATION)),
// NOT mono's variationRandom. The old test asserted the obsolete mono-mirror (delegated → mono
// nvIdx); this version asserts the new contract:
//   1. delegated (default) → voice reads its OWN per-voice draw at MONO's step, clamped to mono
//   2. identity LOR → still mono's step (doubly inert on position), own draw, clamped
//   3a. dialed LOR but DELEGATED → mono's step (the dialed LOR is ignored), own draw, clamped
//   3b. dialed LOR + Local East → voice uses its OWN step (per-voice tick) + own draw, diverges,
//       but ALWAYS holds <= mono's (the clamp)
//   4. table contract the clamp relies on: slowest -> fastest
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

    // A shared mono VARIATION shape (read by mono's getNoteLenIdx).
    for (int i = 0; i < 16; ++i) e.pe.variationRandom[i] = (float)((i * 7 + 3) % 16) / 16.f;

    // PER-VOICE VAR draws (the f9c4189 model): each voice has its own 16-step draw in
    // polyRandom(bank, PL_VARIATION). Seed a distinct, voice-dependent shape so a delegated
    // voice (mono step + own draw) is provably different from mono and from other voices.
    for (int v = 0; v < 15; ++v)
        for (int i = 0; i < 16; ++i)
            e.pe.polyRandom(v, SequencerEngine::PL_VARIATION)[i] =
                (float)(((i * 7 + 3) + (v + 1) * 13) % 16) / 16.f;

    e.lastNoteVal_ = 4.f;   // NOTE_VALUE = 1/8

    // Expected nvIdx for a DELEGATED voice at a given mono step: read the voice's OWN per-voice
    // draw at mono's step, then clamp to the mono event grid (a voice may release early, never
    // hold past mono's next note — NOTE_VALUES is slowest->fastest, so max() picks the shorter).
    auto expectedDelegated = [&](int v, int monoIdx)->int {
        float rVoice = e.pe.polyRandom(v, SequencerEngine::PL_VARIATION)[monoIdx];
        int nv = e.getNoteLenIdx(e.lastNoteVal_, in, rVoice);
        return (nv > e.lastStepResult.nvIdx) ? nv : e.lastStepResult.nvIdx;
    };

    // ── 1. delegated (default) → voice's own draw at mono's step, clamped (NOT mono's nvIdx) ──
    for (int step = 0; step < 64; ++step) {
        e.totalStepsElapsed = step;
        int monoIdx = e.getStrandIdx(step, 16, 0, 0) & 0x0F;
        e.lastStepResult.nvIdx = e.getNoteLenIdx(e.lastNoteVal_, in, e.pe.variationRandom[monoIdx]);
        for (int v = 0; v < 15; ++v)
            check(e.nvIdxForVoice(v, in) == expectedDelegated(v, monoIdx),
                  "delegated -> voice's own draw at mono step, clamped");
    }

    // ── 2. identity LOR (len 16, off 0, rot 0 — as reset() seeds) → still mono's step (position
    //    inert), own draw, clamped. Same expectation as case 1. ──
    for (int step = 0; step < 64; ++step) {
        e.totalStepsElapsed = step;
        int monoIdx = e.getStrandIdx(step, 16, 0, 0) & 0x0F;
        e.lastStepResult.nvIdx = e.getNoteLenIdx(e.lastNoteVal_, in, e.pe.variationRandom[monoIdx]);
        for (int v = 0; v < 15; ++v)
            check(e.nvIdxForVoice(v, in) == expectedDelegated(v, monoIdx),
                  "identity LOR -> mono step, own draw, clamped");
    }

    // ── 3. per-voice LOR windows: delegation default, then divergence + clamp ──
    e.polyLORRef(1, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_LEN) = 6;   // V3
    e.polyLORRef(2, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_LEN) = 12;  // V4
    e.polyLORRef(2, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_OFF) = 3;
    e.polyLORRef(2, SequencerEngine::EDITOR_LANE_VARIATION, SequencerEngine::LOR_ROT) = 2;

    // 3a. DELEGATION DEFAULT (§4d): even with a dialed LOR, a delegated voice reads MONO's step
    //     (the dialed LOR is ignored) + its own draw → same expectation as case 1. Divergence from
    //     mono's nvIdx comes from the per-voice draw, NOT the dialed LOR.
    for (int step = 0; step < 64; ++step) {
        e.totalStepsElapsed = step;
        int monoIdx = e.getStrandIdx(step, 16, 0, 0) & 0x0F;
        e.lastStepResult.nvIdx = e.getNoteLenIdx(e.lastNoteVal_, in, e.pe.variationRandom[monoIdx]);
        for (int v : {1, 2})
            check(e.nvIdxForVoice(v, in) == expectedDelegated(v, monoIdx),
                  "delegated (default) -> mono step (dialed LOR ignored), own draw, clamped");
    }

    // 3b. Flip V3/V4 VAR to Local East, THEN the dialed LOR takes effect: the voice uses its OWN
    //     step (per-voice tick) + its own draw, and diverges. The clamp still holds.
    e.setVarlegLocalEast(1, 0, true);   // V3 VAR: Local East (opt out of mono delegation)
    e.setVarlegLocalEast(2, 0, true);   // V4 VAR: Local East

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
    check(diverged > 0, "non-identity LOR actually diverges (Local East)");
    std::printf("  divergence: %d/%d voice-steps differ from mono\n", diverged, checked);

    // ── 4. table contract the clamp relies on: slowest -> fastest ──
    for (int i = 0; i + 1 < NUM_NOTE_VALUES; ++i)
        check(noteValueSteps(i) > noteValueSteps(i + 1), "NOTE_VALUES strictly decreasing");

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
