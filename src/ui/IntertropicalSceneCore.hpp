#pragma once
// IntertropicalSceneCore — Intertropical's scene-advance state machine, RACK-FREE (the
// CantonmentModuleCore / ChangeAlleyModuleCore precedent: testable headless). Extracted
// 2026-11-26 (Rodney): the advance logic was inline in Intertropical::process(), so the scene
// sequencer — which crosses scenes on the engine's phrase-boundary wrap — had ZERO test coverage
// across the drive modes. This holds the pure state machine; the Module keeps the Rack wiring
// (host lookup, the RESET pulse read, output routing).
//
// THE CONTRACT (verbatim from Intertropical::process, "boundary-crossing advance"):
//   - Per process block the module samples the engine's played column and its phrase-wrap flag.
//   - A scene holds for `repeats[scene]` boundary crossings, then advances; the active scene
//     wraps at `loopLen` (NOT N_SCENES).
//   - Membership (`liveMask`) is sampled AT the boundary (read-at-boundary rule), every wrap.
//   - A host reset syncs to scene 0 and SWALLOWS the first post-reset wrap (the reset itself makes
//     the engine report wrapped=true, which would otherwise advance 0->1 — the off-by-one).
//   - "direction-agnostic": the advance keys off the engine's own wrap flag, so it is only as
//     correct as that flag is per drive mode / direction (what the headless end-to-end test pins).
#include <array>
#include <cstdint>

namespace redDot {

struct IntertropicalSceneCore {
    static constexpr int N_SCENES   = 8;
    static constexpr int MAX_REPEAT = 4;

    // ── config (authored by the module's params / store) ─────────────────────────────────────
    std::array<uint16_t, N_SCENES> sceneMask{};      // bit v set => voice v is IN the scene
    std::array<uint8_t, N_SCENES>  repeats{{1,1,1,1,1,1,1,1}};  // 1..MAX_REPEAT crossings per scene
    uint8_t  loopLen             = N_SCENES;         // scenes to cycle (1..N_SCENES)

    // ── live playback state (not persisted; derived from host phase) ─────────────────────────
    int      activeScene   = 0;
    int      repeatPos     = 0;
    int      lastStepIndex = -1;
    bool     justReset     = false;
    uint16_t liveMask      = 0;

    int getRepeats(int s) const { return (s >= 0 && s < N_SCENES) ? (int)repeats[s] : 1; }
    int getLoopLen()      const { int n = loopLen; return n < 1 ? 1 : (n > N_SCENES ? N_SCENES : n); }

    // The host RESET pulse: sync to scene 0, arm the one-wrap swallow. Mirrors the module's reset
    // branch (which also forces a re-sample via lastStepIndex = -1).
    void onReset() {
        activeScene   = 0;
        repeatPos     = 0;
        liveMask      = sceneMask[0];
        lastStepIndex = -1;
        justReset     = true;
    }

    // One process block: the engine's played column now, and its phrase-wrap flag for this block
    // (eng.lastStepResult.wrapped). Returns true iff the SCENE advanced this block (telemetry; the
    // module ignores the return — it reads liveMask / activeScene).
    bool onStep(int playedColumn, bool wrapped) {
        bool advanced = false;
        const int si = playedColumn;
        if (lastStepIndex >= 0 && si != lastStepIndex) {   // a new step was processed
            if (wrapped) {
                if (justReset) {
                    justReset = false;                     // swallow exactly the post-reset wrap
                } else {
                    repeatPos++;
                    if (repeatPos >= getRepeats(activeScene)) {
                        repeatPos   = 0;
                        activeScene = (activeScene + 1) % getLoopLen();
                        advanced    = true;
                    }
                    liveMask = sceneMask[activeScene];     // read-at-boundary, every wrap
                }
            }
        }
        lastStepIndex = si;
        return advanced;
    }
};

} // namespace redDot
