// test_change_alley_boundary_drive_modes.cpp — the Change Alley V2 BOUNDARY SEAM under the three
// drive modes (clock / phase / gate, sub-gate streams included). The CA sibling of
// test_cantonment_boundary_drive_modes.
//
// THE GAP (Rodney, 2026-11-26): the four CA suites (pending-order / transforms / true-reverse /
// qmix-source) all drive the transform machinery directly — none drive the BOUNDARY that fires
// it. CA receives its fire decision from the SAME seam Cantonment does (Monsoon.cpp):
//
//     lockManager.tick(engine.pos_.playedColumn());
//     expanderManager.sync(engine, lockManager.queueFires());   // <- the bool is caQueueFires
//
// and MonsoonExpanderManager::sync (lines ~100-120) ORs that straight into CA's fire gate:
//
//     const bool fireNow = caQueueFires || caLiveR || caLiveM || caLiveQ;
//     if (fireNow && !v2->transformsAppliedThisBlock)
//         v2->applyPendingTransforms(vActive, axisMask);
//
// So CA inherits, verbatim, the boundary heuristic `boundaryNow_ = (playedColumn < prevStep_)`
// — INCLUDING its reverse inversion. CA's applyPendingTransforms lives on the Rack Module (not a
// rack-free core like Cantonment's), so this suite drives the REAL part of the seam that IS
// rack-free — the SequencerEngine + the real dotModular::LockManager that produce caQueueFires —
// and pins the manager's fire/axis DECISION as a documented transcription (gate 5).
//
//   gate 1 (clock FORWARD)  — pinned correct: caQueueFires true exactly AT the wrap, once/phrase.
//   gate 2 (clock REVERSE)  — THE INHERITED DEFECT, characterized: the heuristic inverts — true on
//           every non-boundary step, silent at the true low->high wrap — so CA's scatter/transform
//           commit lands on the NEXT step, not the boundary. EXPECTED TO MOVE with the engine-edge
//           fix (pos_.atPhraseEdge()). Shared root with Cantonment and with rateManager Rate::Phrase.
//   gate 3 (PHASE drive)    — forward sweep pinned; backward sweep characterized (same family).
//   gate 4 (GATE mode + SUB GATES) — composite edge stream: a ratchet burst spanning the wrap
//           produces exactly one fire; ghost-sparse and held cadences never fire off-boundary.
//   gate 5 (CA fire/axis DECISION) — transcribed from MonsoonExpanderManager.cpp with provenance:
//           a QUEUE fire commits ALL THREE axes (0b111 — the q-mix-inclusive rule, the bugfix that
//           replaced 0b11); locked + no live CA bit => NO fire (frozen); locked + a live CA bit =>
//           fire with the mask NARROWED to the live axes, independent of boundary/direction.
//
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "MonsoonLockManager.hpp"
using dotModular::LockManager;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

// The rig: the rack-free half of CA's seam — the real engine + the real LockManager, ticked at
// Monsoon's control cadence. queueFires() IS the caQueueFires CA receives.
struct Rig {
    SequencerEngine eng;
    LockManager lock{eng.locked};
    int fires = 0;
    Rig(int lenG, int offG) {
        eng.reset();
        eng.updateWindow((float)lenG, 0.f, false, (float)offG, 0.f, false);
        eng.numPolyVoices = 4;
        lock.tick(eng.playedColumn());   // swallow the power-on edge (col -1 vs prevStep_ 0)
        (void)lock.queueFires();
    }
    bool controlTick() { lock.tick(eng.playedColumn()); const bool f = lock.queueFires(); if (f) ++fires; return f; }
};

// gate 1 — clock FORWARD: caQueueFires exactly at the wrap, once per phrase.
static void gate1_clock_forward(int lenG, int offG) {
    Rig r(lenG, offG);
    char msg[200];
    for (int k = 0; k < 3 * lenG; ++k) {
        r.eng.advancePlayhead(+1, nullptr);
        const bool fired = r.controlTick();
        const bool boundary = (k > 0 && k % lenG == 0);
        std::snprintf(msg, sizeof(msg),
            "CA clock FWD lenG=%d offG=%d k=%d: caQueueFires=%d but boundary=%d (fire exactly at the wrap)",
            lenG, offG, k, (int)fired, (int)boundary);
        CHECK(fired == boundary, msg);
    }
}

// gate 2 — clock REVERSE: the inherited inversion, characterized. EXPECTED TO MOVE.
static void gate2_clock_reverse(int lenG, int offG) {
    Rig r(lenG, offG);
    char msg[256];
    int rawFires = 0;
    for (int k = 0; k < 3 * lenG; ++k) {
        r.eng.advancePlayhead(-1, nullptr);
        const bool fired = r.controlTick();
        if (fired) ++rawFires;
        const bool trueBoundary = (k > 0 && k % lenG == 0);
        std::snprintf(msg, sizeof(msg),
            "CA clock REV lenG=%d offG=%d k=%d: fired=%d trueBoundary=%d — today's heuristic fires on"
            " every NON-boundary step, silent AT the boundary (EXPECTED TO MOVE)", lenG, offG, k,
            (int)fired, (int)trueBoundary);
        CHECK(fired == (k > 0 && !trueBoundary), msg);
    }
    std::snprintf(msg, sizeof(msg),
        "CA clock REV lenG=%d offG=%d: fire count %d != every step but k=0 and the two wraps (%d)",
        lenG, offG, rawFires, 3 * lenG - 3);
    CHECK(rawFires == 3 * lenG - 3, msg);
}

// gate 3 — PHASE drive (snapToPhaseStep): forward pinned; backward characterized.
static void gate3_phase_drive(int lenG) {
    char msg[200];
    {
        Rig r(lenG, 0);
        for (int k = 0; k < 3 * lenG; ++k) {
            r.eng.snapToPhaseStep((long)k * 6, 6);
            const bool fired = r.controlTick();
            const bool boundary = (k > 0 && k % lenG == 0);
            std::snprintf(msg, sizeof(msg), "CA PHASE fwd lenG=%d k=%d: fired=%d boundary=%d", lenG, k, (int)fired, (int)boundary);
            CHECK(fired == boundary, msg);
        }
    }
    {
        Rig r(lenG, 0);
        int rawFires = 0;
        const int start = 3 * lenG;
        for (int k = 0; k < 3 * lenG; ++k) { r.eng.snapToPhaseStep((long)(start - k) * 6, 6); if (r.controlTick()) ++rawFires; }
        std::snprintf(msg, sizeof(msg),
            "CA PHASE back lenG=%d: a backward sweep fires on nearly every snap (got %d of %d; > 2*lenG — EXPECTED TO MOVE)",
            lenG, rawFires, 3 * lenG);
        CHECK(rawFires > 2 * lenG, msg);
    }
}

// gate 4 — GATE mode + SUB GATES (forward): ratchet bursts / ghost sparsity / holds.
static void gate4_gate_mode_subgates(int lenG) {
    char msg[220];
    Rig r(lenG, 0);
    const int script[] = {1, 0, 0, 1, 3, 0, 0, 4, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1};
    int globalStep = 0, boundariesSeen = 0;
    for (size_t b = 0; b < sizeof(script) / sizeof(script[0]); ++b) {
        bool blockHasBoundary = false;
        for (int s = 0; s < script[b]; ++s) {
            r.eng.advancePlayhead(+1, nullptr);
            ++globalStep;
            if (globalStep > 1 && (globalStep - 1) % lenG == 0) blockHasBoundary = true;
        }
        const bool fired = r.controlTick();
        std::snprintf(msg, sizeof(msg),
            "CA GATE lenG=%d block=%zu steps=%d gStep=%d: fired=%d blockHasBoundary=%d (ratchet burst fires IFF it spans the wrap)",
            lenG, b, script[b], globalStep, (int)fired, (int)blockHasBoundary);
        CHECK(fired == blockHasBoundary, msg);
        if (blockHasBoundary) ++boundariesSeen;
    }
    std::snprintf(msg, sizeof(msg), "CA GATE lenG=%d: script covered %d boundaries (want 2)", lenG, boundariesSeen);
    CHECK(boundariesSeen == 2, msg);
}

// gate 5 — the CA fire + axisMask DECISION, transcribed from MonsoonExpanderManager::sync
// (lines ~108-117). This mirrors the manager's rule so a change there that is not reflected here
// FAILS, flagging the drift; it does not drive the Module (applyPendingTransforms is Rack-bound).
// Keep in lockstep with the manager source.
static unsigned caAxisMask(bool caQueueFires, bool locked, uint32_t scopeLiveMask) {
    const bool caLiveR = locked && (scopeLiveMask & dotModular::SB_CA_R) != 0;
    const bool caLiveM = locked && (scopeLiveMask & dotModular::SB_CA_M) != 0;
    const bool caLiveQ = locked && (scopeLiveMask & dotModular::SB_CA_Q) != 0;
    const bool fireNow = caQueueFires || caLiveR || caLiveM || caLiveQ;
    if (!fireNow) return 0u;   // no fire -> sentinel: caller distinguishes via caFires()
    return caQueueFires ? 0b111u
                        : ((caLiveR ? 0b001u : 0u) | (caLiveM ? 0b010u : 0u) | (caLiveQ ? 0b100u : 0u));
}
static bool caFires(bool caQueueFires, bool locked, uint32_t scopeLiveMask) {
    const bool caLiveR = locked && (scopeLiveMask & dotModular::SB_CA_R) != 0;
    const bool caLiveM = locked && (scopeLiveMask & dotModular::SB_CA_M) != 0;
    const bool caLiveQ = locked && (scopeLiveMask & dotModular::SB_CA_Q) != 0;
    return caQueueFires || caLiveR || caLiveM || caLiveQ;
}
static void gate5_fire_and_axis_decision() {
    // (a) a QUEUE fire (boundary/unlock) commits ALL THREE axes — the q-mix-inclusive rule.
    CHECK(caFires(true, false, 0u), "CA decision: a queue fire while unlocked must fire");
    CHECK(caAxisMask(true, false, 0u) == 0b111u,
          "CA decision: a queue fire commits ALL THREE axes (0b111 — the q-mix bugfix, was 0b11)");
    CHECK(caAxisMask(true, true, dotModular::SB_CA_R) == 0b111u,
          "CA decision: a queue fire commits all axes even under lock (the boundary flush wins)");
    // (b) locked + no live CA bit => frozen, no fire.
    CHECK(!caFires(false, true, 0u), "CA decision: locked with no live CA bit must NOT fire off-boundary");
    CHECK(!caFires(false, true, dotModular::SB_SANDS_R),
          "CA decision: a NON-CA live bit must not unfreeze CA");
    // (c) locked + a live CA bit => fire, mask narrowed to the live axes, no boundary needed.
    CHECK(caFires(false, true, dotModular::SB_CA_R), "CA decision: locked + live CA_R fires live");
    CHECK(caAxisMask(false, true, dotModular::SB_CA_R) == 0b001u, "CA decision: live CA_R narrows to rhythm only");
    CHECK(caAxisMask(false, true, dotModular::SB_CA_M) == 0b010u, "CA decision: live CA_M narrows to melody only");
    CHECK(caAxisMask(false, true, dotModular::SB_CA_Q) == 0b100u, "CA decision: live CA_Q narrows to q-mix only");
    CHECK(caAxisMask(false, true, dotModular::SB_CA_R | dotModular::SB_CA_Q) == 0b101u,
          "CA decision: two live CA bits narrow to exactly those axes");
    // (d) unlocked + no boundary => no fire (the common idle block).
    CHECK(!caFires(false, false, 0u), "CA decision: unlocked, no boundary => idle, no fire");
}

int main() {
    std::printf("== test_change_alley_boundary_drive_modes ==\n");
    std::printf("gate 1: clock FORWARD — caQueueFires exactly at the boundary, once per phrase\n");
    gate1_clock_forward(8, 0);
    gate1_clock_forward(8, 4);
    gate1_clock_forward(12, 5);
    std::printf("gate 2: clock REVERSE — the inherited inverted heuristic, characterized (EXPECTED TO MOVE)\n");
    gate2_clock_reverse(8, 0);
    gate2_clock_reverse(8, 4);
    std::printf("gate 3: PHASE drive — forward pinned; backward characterized\n");
    gate3_phase_drive(8);
    std::printf("gate 4: GATE mode + sub gates — ratchet bursts / ghost sparsity / holds (forward)\n");
    gate4_gate_mode_subgates(8);
    std::printf("gate 5: CA fire + axisMask decision (transcribed from the manager; all-axes-on-boundary, live narrowing)\n");
    gate5_fire_and_axis_decision();
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
