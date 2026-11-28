// test_intertropical_scene.cpp — Intertropical's scene-advance state machine, headless, across
// the three drive modes. Intertropical (the scene sequencer) had ZERO test coverage; this closes
// that and pins its drive-mode behaviour.
//
// THE FINDING (measured 2026-11-26): Intertropical advances scenes on `eng.lastStepResult.wrapped`
// — the ENGINE's own phrase-wrap flag (the net-steps atPhraseEdge) — NOT the LockManager column
// heuristic (playedColumn < prevStep) that CA/Cantonment use. So, unlike those two, Intertropical
// is CORRECT IN REVERSE: it crosses exactly one scene per phrase in forward clock, reverse clock,
// and phase drive. It dodged the reverse-inversion defect by consuming the sound signal — the very
// signal #371/#372 propose CA/Cantonment move onto. This suite pins that, and the scene state
// machine (the repeat hold, the loopLen wrap, the read-at-boundary membership, and the post-reset
// one-wrap swallow that fixes the 0->1 off-by-one).
//
// Two layers: S* the core in isolation; E* end-to-end through the REAL SequencerEngine per mode.
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "SequencerEngine.hpp"
#include "RateManager.hpp"
#include "ui/IntertropicalSceneCore.hpp"
using redDot::IntertropicalSceneCore;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

static IntertropicalSceneCore identityScenes() {
    IntertropicalSceneCore sc;
    for (int s = 0; s < IntertropicalSceneCore::N_SCENES; ++s) sc.sceneMask[s] = (uint16_t)(1u << s);
    sc.loopLen = IntertropicalSceneCore::N_SCENES;   // repeats default 1
    return sc;
}

// Feed a scripted (playedColumn, wrapped) stream — the core sees one (col, wrapped) per block.
static void feed(IntertropicalSceneCore& sc, int col, bool wrapped) { sc.onStep(col, wrapped); }

// ── Core unit laws ───────────────────────────────────────────────────────────────────────────
static void unit_laws() {
    // S1 — a plain wrap advances exactly one scene (repeats all 1).
    {
        auto sc = identityScenes();
        feed(sc, 0, false);                 // establish lastStepIndex (first step: no advance)
        feed(sc, 1, false);
        CHECK(sc.activeScene == 0, "S1: no advance before a wrap");
        feed(sc, 0, true);                  // the wrap
        CHECK(sc.activeScene == 1, "S1: one wrap -> scene 1");
        CHECK(sc.liveMask == sc.sceneMask[1], "S1: membership sampled AT the boundary");
    }
    // S2 — repeats hold: a scene with repeats=3 crosses THREE boundaries before advancing.
    {
        auto sc = identityScenes();
        sc.repeats[0] = 3;
        feed(sc, 0, false);
        for (int w = 1; w <= 2; ++w) { feed(sc, 1, false); feed(sc, 0, true);
            CHECK(sc.activeScene == 0, "S2: scene 0 HOLDS through its repeats"); }
        feed(sc, 1, false); feed(sc, 0, true);
        CHECK(sc.activeScene == 1, "S2: scene advances on the repeats-th boundary");
    }
    // S3 — loopLen wraps the active scene (NOT N_SCENES): loopLen=3 cycles 0,1,2,0,...
    {
        auto sc = identityScenes();
        sc.loopLen = 3;
        feed(sc, 0, false);
        int seen[4];
        for (int k = 0; k < 4; ++k) { feed(sc, 1, false); feed(sc, 0, true); seen[k] = sc.activeScene; }
        CHECK(seen[0] == 1 && seen[1] == 2 && seen[2] == 0 && seen[3] == 1, "S3: active scene wraps at loopLen=3");
    }
    // S4 — the post-reset wrap is swallowed (the 0->1 off-by-one guard).
    {
        auto sc = identityScenes();
        feed(sc, 0, false); feed(sc, 1, false);
        sc.onReset();                       // sync to scene 0, arm the swallow
        CHECK(sc.activeScene == 0 && sc.justReset, "S4: reset syncs to scene 0 and arms the swallow");
        feed(sc, 1, false);                 // re-establish lastStepIndex (reset set it to -1)
        feed(sc, 0, true);                  // the engine's post-reset wrap — must be SWALLOWED
        CHECK(sc.activeScene == 0, "S4: the first post-reset wrap does NOT advance (swallowed)");
        feed(sc, 1, false); feed(sc, 0, true);
        CHECK(sc.activeScene == 1, "S4: the NEXT wrap advances normally");
    }
    // S5 — a block with no column change is a no-op even if wrapped is (stale) true.
    {
        auto sc = identityScenes();
        feed(sc, 2, false);
        feed(sc, 2, true);                  // same column -> not a new step -> ignored
        CHECK(sc.activeScene == 0, "S5: no column change -> no advance (the si != lastStepIndex guard)");
    }
}

// ── End-to-end through the real engine, per drive mode ─────────────────────────────────────────
// March the engine `phrases` phrases; feed (playedColumn, wrapped) to the core each block; return
// the scene-advance count. mode: 0 = clock fwd, 1 = clock rev, 2 = phase (snapToPhaseStep).
static int marchScenes(int lenG, int phrases, int mode) {
    SequencerEngine eng; eng.reset();
    eng.updateWindow((float)lenG, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    PatternInput in;
    auto sc = identityScenes();
    redDot::RateManager rm;
    const int dir = (mode == 1) ? -1 : +1;
    int advances = 0;
    for (int i = 0; i < lenG * phrases + 1; ++i) {
        if (mode == 2) eng.snapToPhaseStep((long)i * 6, 6);
        rm.advance(redDot::Rate::Step, true);
        eng.executeModeA(rm, in.noteValue, in, dir);
        if (sc.onStep(eng.playedColumn(), eng.lastStepResult.wrapped)) ++advances;
    }
    return advances;
}

static void end_to_end() {
    const int lenG = 4, phrases = 5;
    // E1 — clock FORWARD: exactly one scene advance per phrase.
    CHECK(marchScenes(lenG, phrases, 0) == phrases,
          "E1 clock FWD: one scene advance per phrase");
    // E2 — clock REVERSE: ALSO one per phrase (the engine wrap flag is direction-agnostic —
    // the thing that makes Intertropical correct where CA/Cantonment's column heuristic inverts).
    CHECK(marchScenes(lenG, phrases, 1) == phrases,
          "E2 clock REVERSE: one scene advance per phrase (direction-agnostic wrap)");
    // E3 — PHASE drive (snapToPhaseStep sweeps): one per phrase.
    CHECK(marchScenes(lenG, phrases, 2) == phrases,
          "E3 phase drive: one scene advance per phrase");
    // E4 — a non-dividing window length still gives one advance per phrase, both directions.
    CHECK(marchScenes(7, 4, 0) == 4, "E4 fwd lenG=7: one per phrase");
    CHECK(marchScenes(7, 4, 1) == 4, "E4 rev lenG=7: one per phrase");
}

int main() {
    std::printf("== test_intertropical_scene ==\n");
    std::printf("core: repeat hold / loopLen wrap / read-at-boundary / post-reset swallow / no-op\n");
    unit_laws();
    std::printf("end-to-end: scene advance once per phrase in clock-fwd, clock-rev, and phase\n");
    end_to_end();
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
