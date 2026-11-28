// test_cantonment_boundary_drive_modes.cpp — the Cantonment BOUNDARY SEAM under the three
// drive modes (clock / phase / gate, sub-gate streams included).
//
// THE GAP THIS CLOSES (Rodney, 2026-11-26): the four Cantonment suites (edits / transforms /
// pending / module) all MOCK the boundary — they prove "given boundary=true the commit is
// right", never that the boundary ARRIVES right. The real seam, verbatim from Monsoon.cpp:
//
//     lockManager.tick(engine.pos_.playedColumn());                       // line ~1829
//     expanderManager.sync(engine, lockManager.queueFires());             // line ~1841
//         -> cachedCantonment->applyPendingTransforms(...)                // manager line ~134
//
// and lockManager.tick's boundary is a COLUMN HEURISTIC (MonsoonLockManager.hpp):
//
//     boundaryNow_ = (playedColumn < prevStep_);        // step wrapped => phrase boundary
//
// This file drives that seam with a REAL SequencerEngine + the REAL dotModular::LockManager +
// the REAL CantonmentModuleCore — no mocks — one "control tick" per block, exactly Monsoon's
// cadence (ticks happen whether or not a step occurred; in gate mode several steps can land
// between ticks).
//
// WHAT IS PINNED, AND WHAT IS CHARACTERIZED:
//   gate 1 (clock FORWARD)  — correct law, pinned: an armed verb commits exactly AT the next
//           phrase boundary, once per phrase, zero/plain windows alike.
//   gate 2 (clock REVERSE)  — THE DEFECT, characterized (measured 2026-11-26): in reverse every
//           ordinary step has col < prev, so the heuristic fires on EVERY non-boundary step and
//           is SILENT exactly at the true boundary (the low->high wrap) — a perfect inversion
//           (21 fires / 24 steps, k=8 and k=16 the only quiet steps). An armed verb therefore
//           commits on the NEXT STEP, not at the boundary. EXPECTED TO MOVE when the boundary
//           rides the engine's own edge (pos_.atPhraseEdge() / the wrapped flag) instead of a
//           column comparison; this gate pins today's behaviour so that fix is a deliberate
//           re-pin, not a surprise. NOTE the blast radius: Monsoon feeds the SAME boundaryNow()
//           to rateManager.advance(Rate::Phrase, ...), so everything phrase-rated inverts in
//           reverse too — not just Cantonment/CA.
//   gate 3 (PHASE drive)    — snapToPhaseStep sweeps: forward sweep pinned (one commit per
//           cycle, at the wrap); backward sweep characterized (the same inversion family).
//   gate 4 (GATE mode + SUB GATES) — the composite edge stream, forward: a RATCHET burst
//           (Gate 2, several steps between two control ticks) spanning the boundary fires
//           exactly once and the commit lands there; a burst inside the phrase fires zero; a
//           GHOST-sparse cadence (Gate 3, many tick gaps between steps) fires only at the
//           boundary; a HELD stretch (ticks, no steps) fires zero.
//
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include <memory>   // the Cantonment core is ~2 MiB (the trajectory ring) — heap-held
#include "engines/SequencerEngine.hpp"
#include "MonsoonLockManager.hpp"
#include "../src/dsp/CantonmentModuleCore.hpp"
using dotModular::LockManager;
using namespace dotModular::ct;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

// ── The rig: the Monsoon seam, no mocks ─────────────────────────────────────────────────────
struct Rig {
    SequencerEngine eng;
    LockManager lock{eng.locked};
    std::unique_ptr<CantonmentModuleCore> core;
    int fires = 0;            // control ticks where queueFires() was true
    int commits = 0;          // applyAtBoundary calls that consumed the armed verb
    Rig(int lenG, int offG) : core(std::make_unique<CantonmentModuleCore>()) {
        eng.reset();
        eng.updateWindow((float)lenG, 0.f, false, (float)offG, 0.f, false);
        eng.numPolyVoices = 4;
        core->setPin(1, 0, true);   // a real edge, so the verbs act on material (the S2 chain idiom)
        core->setPin(2, 1, true);
        // Monsoon ticks the lock manager every control block from power-on; the first tick sees
        // the pre-start column (-1) against prevStep_'s initial 0 and fires (pre-start flush —
        // existing behaviour, outside these gates). Swallow it here so the gates measure the
        // transport, not the power-on edge.
        lock.tick(eng.playedColumn());
        (void)lock.queueFires();
    }
    void arm() { core->pending.latch(CantonmentPending::V_INVERT); }
    bool armed() const { return core->pending.armed[CantonmentPending::V_INVERT]; }
    // One CONTROL TICK, Monsoon's order: (steps already advanced by the caller) -> lock.tick on
    // the played column -> queueFires -> the manager applies.
    bool controlTick() {
        lock.tick(eng.playedColumn());
        const bool f = lock.queueFires();
        if (f) {
            ++fires;
            const bool wasArmed = armed();
            core->applyAtBoundary(eng.numPolyVoices + 1);
            if (wasArmed && !armed()) ++commits;
        }
        return f;
    }
};

// gate 1 — clock FORWARD: the armed verb commits exactly at the phrase boundary, once per phrase.
static void gate1_clock_forward(int lenG, int offG) {
    Rig r(lenG, offG);
    char msg[256];
    r.arm();
    for (int k = 0; k < 3 * lenG; ++k) {
        r.eng.advancePlayhead(+1, nullptr);
        const bool fired = r.controlTick();
        const bool boundary = (k > 0 && k % lenG == 0);   // the wrap steps (k=lenG, 2*lenG)
        std::snprintf(msg, sizeof(msg),
            "clock FWD lenG=%d offG=%d k=%d: queueFires=%d but boundary=%d (must fire exactly at the wrap)",
            lenG, offG, k, (int)fired, (int)boundary);
        CHECK(fired == boundary, msg);
        if (boundary) {
            std::snprintf(msg, sizeof(msg),
                "clock FWD lenG=%d offG=%d k=%d: boundary fired but the armed verb was not consumed",
                lenG, offG, k);
            CHECK(!r.armed(), msg);
            r.arm();   // re-arm for the next phrase
        } else {
            std::snprintf(msg, sizeof(msg),
                "clock FWD lenG=%d offG=%d k=%d: verb consumed OFF the boundary", lenG, offG, k);
            CHECK(r.armed(), msg);
        }
    }
}

// gate 2 — clock REVERSE: TODAY'S behaviour, characterized. EXPECTED TO MOVE with the fix.
static void gate2_clock_reverse(int lenG, int offG) {
    Rig r(lenG, offG);
    char msg[256];
    int rawFires = 0;
    bool committedImmediately = false;
    for (int k = 0; k < 3 * lenG; ++k) {
        if (k == 2) r.arm();                       // armed mid-phrase, two steps in
        r.eng.advancePlayhead(-1, nullptr);
        const bool fired = r.controlTick();
        if (fired) ++rawFires;
        const bool trueBoundary = (k > 0 && k % lenG == 0);   // the low->high wrap steps
        // THE INVERSION (the defect, pinned as-is): fires on every ordinary reverse step,
        // silent exactly at the true boundary.
        std::snprintf(msg, sizeof(msg),
            "clock REV lenG=%d offG=%d k=%d: fired=%d trueBoundary=%d — today's heuristic must fire"
            " on every NON-boundary step and stay silent AT the boundary (EXPECTED TO MOVE with the"
            " engine-edge fix)", lenG, offG, k, (int)fired, (int)trueBoundary);
        CHECK(fired == (k > 0 && !trueBoundary), msg);
        if (k == 3 && !r.armed()) committedImmediately = true;
    }
    std::snprintf(msg, sizeof(msg),
        "clock REV lenG=%d offG=%d: the verb armed at k=2 must (today) commit on the very next step"
        " — NOT at the phrase boundary (EXPECTED TO MOVE with the engine-edge fix)", lenG, offG);
    CHECK(committedImmediately, msg);
    std::snprintf(msg, sizeof(msg),
        "clock REV lenG=%d offG=%d: fire count %d != 3*lenG-1-2 = %d (every step but k=0 and the two wraps)",
        lenG, offG, rawFires, 3 * lenG - 3);
    CHECK(rawFires == 3 * lenG - 3, msg);
}

// gate 3 — PHASE drive (snapToPhaseStep): forward sweep pinned; backward sweep characterized.
static void gate3_phase_drive(int lenG) {
    char msg[256];
    {   // forward sweep: pulsePos marches one step (6 pulses) per control tick.
        Rig r(lenG, 0);
        r.arm();
        for (int k = 0; k < 3 * lenG; ++k) {
            r.eng.snapToPhaseStep((long)k * 6, 6);
            const bool fired = r.controlTick();
            const bool boundary = (k > 0 && k % lenG == 0);
            std::snprintf(msg, sizeof(msg),
                "PHASE fwd lenG=%d k=%d: queueFires=%d but cycle boundary=%d", lenG, k, (int)fired, (int)boundary);
            CHECK(fired == boundary, msg);
            if (boundary) { CHECK(!r.armed(), "PHASE fwd: boundary fired but verb not consumed"); r.arm(); }
        }
    }
    {   // backward sweep: the same inversion family as clock reverse — characterized as-is.
        Rig r(lenG, 0);
        int rawFires = 0;
        const int start = 3 * lenG;                 // sweep from cycle 3 back to 0
        for (int k = 0; k < 3 * lenG; ++k) {
            r.eng.snapToPhaseStep((long)(start - k) * 6, 6);
            if (r.controlTick()) ++rawFires;
        }
        std::snprintf(msg, sizeof(msg),
            "PHASE back lenG=%d: a backward sweep today fires on nearly every snap (got %d of %d;"
            " must be > 2*lenG — EXPECTED TO MOVE with the engine-edge fix)", lenG, rawFires, 3 * lenG);
        CHECK(rawFires > 2 * lenG, msg);
    }
}

// gate 4 — GATE mode, the composite edge stream (sub gates): steps arrive in bursts (RATCHET,
// Gate 2), sparsely across tick gaps (GHOST, Gate 3), or not at all (held) — the control ticks
// keep their own cadence regardless. Forward: all pinned as correct law.
static void gate4_gate_mode_subgates(int lenG) {
    char msg[256];
    Rig r(lenG, 0);
    r.arm();
    // Script of control blocks: {steps this block}. lenG=8: boundary steps are global steps 8,16.
    // Blocks: ghost-sparse start (1,0,0,1), a ratchet burst INSIDE the phrase (3), a held stretch
    // (0,0), then a ratchet burst SPANNING the boundary (4: steps 6,7,8,9 — the wrap inside the
    // burst), sparse to the next boundary, a hold, then exactly onto it.
    const int script[] = {1, 0, 0, 1, 3, 0, 0, 4, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1};
    int globalStep = 0;
    int boundariesSeen = 0;
    for (size_t b = 0; b < sizeof(script) / sizeof(script[0]); ++b) {
        bool blockHasBoundary = false;
        for (int s = 0; s < script[b]; ++s) {
            r.eng.advancePlayhead(+1, nullptr);
            ++globalStep;
            if (globalStep > 1 && (globalStep - 1) % lenG == 0) blockHasBoundary = true;
        }
        const bool fired = r.controlTick();
        std::snprintf(msg, sizeof(msg),
            "GATE mode lenG=%d block=%zu (steps=%d, globalStep=%d): fired=%d but blockHasBoundary=%d"
            " — a ratchet burst must fire IFF it spans the wrap; ghosts/holds never spuriously fire",
            lenG, b, script[b], globalStep, (int)fired, (int)blockHasBoundary);
        CHECK(fired == blockHasBoundary, msg);
        if (blockHasBoundary) {
            ++boundariesSeen;
            CHECK(!r.armed(), "GATE mode: boundary block fired but the armed verb was not consumed");
            r.arm();
        } else {
            CHECK(r.armed(), "GATE mode: verb consumed off the boundary");
        }
    }
    std::snprintf(msg, sizeof(msg), "GATE mode lenG=%d: script covered %d boundaries (want 2)", lenG, boundariesSeen);
    CHECK(boundariesSeen == 2, msg);
}

int main() {
    std::printf("== test_cantonment_boundary_drive_modes ==\n");
    std::printf("gate 1: clock FORWARD — commit exactly at the boundary, once per phrase\n");
    gate1_clock_forward(8, 0);
    gate1_clock_forward(8, 4);
    gate1_clock_forward(12, 5);
    std::printf("gate 2: clock REVERSE — today's inverted heuristic, characterized (EXPECTED TO MOVE)\n");
    gate2_clock_reverse(8, 0);
    gate2_clock_reverse(8, 4);
    std::printf("gate 3: PHASE drive — forward sweep pinned; backward sweep characterized\n");
    gate3_phase_drive(8);
    std::printf("gate 4: GATE mode + sub gates — ratchet bursts, ghost sparsity, holds (forward)\n");
    gate4_gate_mode_subgates(8);
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
