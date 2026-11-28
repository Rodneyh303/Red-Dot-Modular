#pragma once
// ChangeAlleyModuleCore — the RACK-FREE module-state core for Change Alley V2, the sibling of
// CantonmentModuleCore. Extracted 2026-11-26 (Rodney): CA's pending/transform/undo/trajectory
// machinery was hand-written inside the Rack Module (MonsoonChangeAlleyV2.hpp), a second copy of
// the exact scaffolding CantonmentModuleCore already owns — an ODR that also made CA's whole
// commit path un-testable without the SDK. This holds the PURE state + the boundary-commit logic;
// the Module keeps only the Rack wiring (triggers, lamps, pairing, history-ring drain).
//
// WHAT IS PURE AND LIVES HERE: the three source planes (rhythm/melody/q-mix), the pending rows
// (arm + chronological stamp), the per-stream true-reverse queue, the scatter counters, the
// correlation stream keys, the committed-state trajectory (EXPAND / true-reverse), and the SPSC
// undo ring the UI drains. applyAtBoundary() is the exact body of the Module's old
// applyPendingTransforms() MINUS the two lamp writes — those become a one-line sync the Module
// does afterward by reading pendingRows[].armed / trueRevRequested[] (the CantonmentModuleCore
// pattern: core mutates state, module reflects it to lamps).
//
// WHAT STAYS IN THE MODULE: Rack SchmittTrigger/BooleanTrigger edge detect, lights[], the
// shared-CA pairId/topology, corrKey SEEDING (rack::random / seed::deriveKey — the key DATA lives
// here, its randomisation is the module's), and the history-action push on the UI thread.
//
// DIMS: redeclared rack-free (CA's live in rack-coupled Monsoon.hpp::ChangeAlleyV2Ids). The
// Module static_assert's these against CA:: so a drift fails that build LOUDLY, never silently —
// this header cannot see Monsoon.hpp without pulling Rack, so the reconciliation is at the one
// translation unit that has both.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include "CATrajectoryBuffer.hpp"      // rack-free: the committed-state ring (true-reverse / EXPAND)
#include "CAPendingOrder.hpp"          // rack-free: chronological pending-row ordering
#include "ChangeAlleyTransforms.hpp"   // rack-free: dotModular::ca::applyCorrelation + N_VOICES

namespace redDot {

struct ChangeAlleyModuleCore {
    // ── rack-free dims (Module static_asserts these == CA::… ) ──────────────────────────────
    static constexpr int N_VOICES      = CATrajectoryBuffer::N_VOICES;       // 16
    static constexpr int N_VERBS       = 4;
    static constexpr int SIDES         = 2;
    static constexpr int TYPES         = 3;                                  // rhythm / melody / q-mix
    static constexpr int N_ROWS        = N_VERBS * SIDES * TYPES;            // 24
    static constexpr int SCATTER_TYPES = 3;
    static constexpr int N_SCATTER     = SIDES * SCATTER_TYPES * 2;          // 12
    static constexpr int V_SCATTER     = 3;                                  // the scatter verb id
    static constexpr int UNDO_RING     = 16;

    // one panel row's queued transform (verb,side,type decoded from the row index on apply)
    struct PendingAction {
        bool     armed        = false;
        int      grain        = 4;
        int      leaderOrStep = 0;    // leader offset for Collapse, signed step for Rotate
        int      scatterDelta = 1;    // +1 fwd jack, -1 back jack
        bool     isDomain     = true;
        bool     isInter      = false;
        uint32_t stamp        = 0;    // chronological order; set on the unarmed->armed edge only (0 = unset)
    };

    // one committed boundary batch, for the UI history ring (SPSC: audio writes, UI drains)
    struct UndoSnapshot {
        std::array<uint8_t, N_VOICES> beforeR{}, beforeM{}, beforeQ{};
        std::array<uint8_t, N_VOICES> afterR{},  afterM{},  afterQ{};
        std::array<int64_t, N_SCATTER> counterBefore{}, counterAfter{};
    };

    // ── state ───────────────────────────────────────────────────────────────────────────────
    std::array<uint8_t, N_VOICES> rhythmSrc{};
    std::array<uint8_t, N_VOICES> melodySrc{};
    std::array<uint8_t, N_VOICES> qmixSrc{};
    std::array<PendingAction, N_ROWS> pendingRows{};
    std::array<bool, TYPES> trueRevRequested{};
    std::array<int64_t, N_SCATTER> scatterCounter{};
    std::array<uint64_t, N_SCATTER> corrKey{};          // DATA; the module seeds it
    CATrajectoryBuffer trajectory;
    uint32_t pendingStampCounter = 0;
    bool transformsAppliedThisBlock = false;

    std::array<UndoSnapshot, UNDO_RING> undoRing{};
    std::atomic<uint32_t> undoHead{0};   // producer (audio)
    std::atomic<uint32_t> undoTail{0};   // consumer (UI)

    // ── arming (the pure half of the Module's trigger handlers) ───────────────────────────────
    // Latch a panel row: set its transform params and arm it. The chronological stamp is taken
    // ONLY on the unarmed->armed edge (a re-arm keeps the original stamp — the ordering rule).
    void armRow(int row, int grain, int leaderOrStep, int scatterDelta, bool isDomain, bool isInter) {
        if (row < 0 || row >= N_ROWS) return;
        PendingAction& p = pendingRows[row];
        if (!p.armed) p.stamp = pendingStampCounter++;
        p.grain = grain; p.leaderOrStep = leaderOrStep; p.scatterDelta = scatterDelta;
        p.isDomain = isDomain; p.isInter = isInter; p.armed = true;
    }
    void armTrueReverse(int ty) { if (ty >= 0 && ty < TYPES) trueRevRequested[ty] = true; }

    static unsigned axisBitForType(int type) { return 1u << type; }   // 0=rhythm 1=melody 2=qmix

    // ── the boundary commit — the Module's old applyPendingTransforms body, lamp writes removed ─
    // Returns rows applied (0 = nothing committed). AFTER calling, the Module reflects state to
    // lamps: pending lamp[row] = pendingRows[row].armed, true-rev lamp[ty] = trueRevRequested[ty].
    int applyAtBoundary(int active, unsigned axisMask = 0b111u) {
        // TRUE-REVERSE consume (per stream), gated by the SAME axisMask as the verbs, BEFORE the
        // verb early-return so it fires even with no armed rows. Walks the committed-state
        // trajectory back one phrase; STOP (no wrap) at the buffer start (the consume still clears
        // the request so the lamp affordance is consistent).
        for (int ty = 0; ty < TYPES; ++ty) {
            if (!trueRevRequested[ty]) continue;
            if (!(axisMask & axisBitForType(ty))) continue;   // out-of-axis: stay queued
            trueRevRequested[ty] = false;
            std::array<uint8_t, N_VOICES> restored{};
            uint8_t* tbl = (ty == 0) ? rhythmSrc.data() : (ty == 1) ? melodySrc.data() : qmixSrc.data();
            if (trajectory.stepBack(ty, restored.data()))
                std::memcpy(tbl, restored.data(), N_VOICES);
        }

        // Any armed row whose axis is in the mask? If none, nothing to snapshot or apply.
        bool any = false;
        for (int row = 0; row < N_ROWS; ++row) {
            if (!pendingRows[row].armed) continue;
            if (axisMask & axisBitForType(row % TYPES)) { any = true; break; }
        }
        if (!any) return 0;

        // Snapshot BEFORE (whole matrix + scatter). One boundary commit = one undo step.
        UndoSnapshot snap;
        for (int v = 0; v < N_VOICES; ++v) { snap.beforeR[v] = rhythmSrc[v]; snap.beforeM[v] = melodySrc[v]; snap.beforeQ[v] = qmixSrc[v]; }
        for (int i = 0; i < N_SCATTER; ++i) snap.counterBefore[i] = scatterCounter[i];

        // Chronological apply: armed rows ascending-stamp, ties by row index (verb-major).
        std::array<bool, N_ROWS> armedArr{};
        std::array<uint32_t, N_ROWS> stampArr{};
        for (int row = 0; row < N_ROWS; ++row) { armedArr[row] = pendingRows[row].armed; stampArr[row] = pendingRows[row].stamp; }
        std::array<int, N_ROWS> orderArr{};
        const int nOrdered = redDot::CAPendingOrder::orderRows(N_ROWS, armedArr.data(), stampArr.data(), orderArr.data());

        int applied = 0;
        for (int oi = 0; oi < nOrdered; ++oi) {
            const int row  = orderArr[oi];
            PendingAction& p = pendingRows[row];
            const int verb = row / (SIDES * TYPES);
            const int side = (row / TYPES) % SIDES;
            const int type = row % TYPES;
            if (!(axisMask & axisBitForType(type))) continue;   // out-of-axis: stay armed
            uint8_t* tbl = (type == 0) ? rhythmSrc.data() : (type == 1) ? melodySrc.data() : qmixSrc.data();
            const int ci = (side * SCATTER_TYPES + type) * 2 + (p.isDomain ? 0 : 1);
            if (verb == V_SCATTER)
                scatterCounter[ci] += (int64_t)p.scatterDelta;
            dotModular::ca::applyCorrelation(verb, p.isDomain, p.isInter,
                                             tbl, active, p.grain, p.leaderOrStep,
                                             corrKey[ci], scatterCounter[ci]);
            p.armed = false;
            ++applied;
        }

        // Snapshot AFTER, publish to the undo ring (drop if the UI hasn't drained — never in practice).
        for (int v = 0; v < N_VOICES; ++v) { snap.afterR[v] = rhythmSrc[v]; snap.afterM[v] = melodySrc[v]; snap.afterQ[v] = qmixSrc[v]; }
        for (int i = 0; i < N_SCATTER; ++i) snap.counterAfter[i] = scatterCounter[i];
        const uint32_t h = undoHead.load(std::memory_order_relaxed);
        const uint32_t t = undoTail.load(std::memory_order_acquire);
        if (h - t < (uint32_t)UNDO_RING) {
            undoRing[h % UNDO_RING] = snap;
            undoHead.store(h + 1, std::memory_order_release);
        }

        // Record the POST-commit state per committed axis (results, not causes) — what makes
        // true-reverse verb-agnostic. pushIfChanged skips static passages.
        if (axisMask & 0b001) trajectory.pushIfChanged(0, rhythmSrc.data());
        if (axisMask & 0b010) trajectory.pushIfChanged(1, melodySrc.data());
        if (axisMask & 0b100) trajectory.pushIfChanged(2, qmixSrc.data());

        // Chronological counter resets ONLY when the pending set is now fully empty (out-of-axis
        // rows would still be armed) — else new arms get stamp 0 < persistent rows, jumping ahead.
        bool anyArmed = false;
        for (int row = 0; row < N_ROWS; ++row) if (pendingRows[row].armed) { anyArmed = true; break; }
        if (!anyArmed) pendingStampCounter = 0;

        return applied;
    }

    // Rack reset / init: the identity diagonal, no pending, empty trajectory, queue clear.
    void resetAll() {
        for (int v = 0; v < N_VOICES; ++v) { rhythmSrc[v] = (uint8_t)v; melodySrc[v] = (uint8_t)v; qmixSrc[v] = (uint8_t)v; }
        pendingRows = {};
        trueRevRequested = {};
        scatterCounter = {};
        trajectory.clear();
        pendingStampCounter = 0;
        transformsAppliedThisBlock = false;
    }
};

} // namespace redDot
