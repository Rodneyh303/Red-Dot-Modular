// test_lock_mode_commit.cpp — the LOCK contract for the CA / Cantonment boundary commit, headless.
// Lock + undo were implemented months ago; this re-pins the lock half of the commit path (the
// boundary-drive-mode suites #371/#372 cover the UNLOCKED path) before the boundary rework, driving
// the REAL dotModular::LockManager (rack-free) + the REAL cores.
//
// THE CONTRACT (MonsoonLockManager.hpp): queueFires() = (boundary && !locked) || unlockEdge, where
// unlockEdge = (prevLocked && !locked). So:
//   - UNLOCKED, at a boundary      -> commit.
//   - LOCKED, at a boundary        -> NO commit; armed transforms QUEUE (stay armed).
//   - the UNLOCK edge (locked->unlocked) -> FLUSH: commit the queue, even away from a boundary.
// Cantonment rides queueFires() directly (MonsoonExpanderManager.cpp: "copy CA exactly"; "No
// live-under-lock scope bit yet"). CA adds a LIVE-UNDER-LOCK path: locked + a CA scope bit fires
// that axis live, with the mask narrowed to the live axes (transcribed below, from the manager).
//
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include <memory>
#include "MonsoonLockManager.hpp"
#include "../src/dsp/CantonmentModuleCore.hpp"
#include "../src/dsp/ChangeAlleyModuleCore.hpp"
using dotModular::LockManager;
using dotModular::ct::CantonmentPending;
using redDot::ChangeAlleyModuleCore;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

// A controllable lock driver over the real LockManager: tick a column, read queueFires().
struct Lock {
    bool locked = false;
    LockManager lk{locked};
    bool tick(int col) { lk.tick(col); return lk.queueFires(); }
    // climb to the wrap and return queueFires() AT the wrap step (the boundary).
    bool crossBoundary(int lenG) {
        for (int c = 1; c < lenG; ++c) lk.tick(c);
        lk.tick(0);                    // 0 < lenG-1 => boundaryNow
        return lk.queueFires();
    }
};

// ── Cantonment: queueFires() -> applyAtBoundary, no live exceptions ────────────────────────────
static void cantonment_lock() {
    std::printf("Cantonment: queue under lock, flush on unlock\n");
    const int active = 5;
    // L1 — UNLOCKED boundary commits.
    {
        auto c = std::make_unique<dotModular::ct::CantonmentModuleCore>(); c->resetAll();
        c->setPin(1, 0, true);                       // a real edge so INVERT has material
        c->pending.latch(CantonmentPending::V_INVERT);
        Lock lock;                                   // unlocked
        if (lock.crossBoundary(8)) c->applyAtBoundary(active);
        CHECK(!c->pending.armed[CantonmentPending::V_INVERT], "L1: unlocked boundary commits (verb disarmed)");
    }
    // L2 — LOCKED boundary does NOT commit; the verb QUEUES.
    {
        auto c = std::make_unique<dotModular::ct::CantonmentModuleCore>(); c->resetAll();
        c->setPin(1, 0, true);
        c->pending.latch(CantonmentPending::V_INVERT);
        Lock lock; lock.locked = true;
        const bool fires = lock.crossBoundary(8);
        CHECK(!fires, "L2: queueFires() is FALSE at a boundary while locked");
        if (fires) c->applyAtBoundary(active);
        CHECK(c->pending.armed[CantonmentPending::V_INVERT], "L2: the verb stays armed under lock (queued)");
    }
    // L3 — the UNLOCK edge flushes the queued verb (no boundary needed).
    {
        auto c = std::make_unique<dotModular::ct::CantonmentModuleCore>(); c->resetAll();
        c->setPin(1, 0, true);
        c->pending.latch(CantonmentPending::V_INVERT);
        Lock lock; lock.locked = true;
        lock.crossBoundary(8);                       // armed, locked, nothing committed
        CHECK(c->pending.armed[CantonmentPending::V_INVERT], "L3: still armed before the unlock");
        lock.locked = false;                         // unlock
        const bool fires = lock.tick(0);             // same column: no boundary, but the unlock edge
        CHECK(fires, "L3: queueFires() TRUE on the unlock edge (the flush)");
        if (fires) c->applyAtBoundary(active);
        CHECK(!c->pending.armed[CantonmentPending::V_INVERT], "L3: the queued verb flushes on unlock");
    }
}

// ── CA: the manager's fire/axis DECISION (transcribed, MonsoonExpanderManager.cpp ~108-117) ────
static bool caFires(bool queueFires, bool locked, uint32_t scope) {
    const bool r = locked && (scope & dotModular::SB_CA_R);
    const bool m = locked && (scope & dotModular::SB_CA_M);
    const bool q = locked && (scope & dotModular::SB_CA_Q);
    return queueFires || r || m || q;
}
static unsigned caAxisMask(bool queueFires, bool locked, uint32_t scope) {
    const bool r = locked && (scope & dotModular::SB_CA_R);
    const bool m = locked && (scope & dotModular::SB_CA_M);
    const bool q = locked && (scope & dotModular::SB_CA_Q);
    return queueFires ? 0b111u : ((r ? 0b001u : 0u) | (m ? 0b010u : 0u) | (q ? 0b100u : 0u));
}
static int caRow(int verb, int side, int type) {
    return verb * ChangeAlleyModuleCore::SIDES * ChangeAlleyModuleCore::TYPES
         + side * ChangeAlleyModuleCore::TYPES + type;
}
static std::unique_ptr<ChangeAlleyModuleCore> caArmed(int row) {
    auto c = std::make_unique<ChangeAlleyModuleCore>(); c->resetAll();
    for (int i = 0; i < ChangeAlleyModuleCore::N_SCATTER; ++i) c->corrKey[i] = 0xC0FFEEu + i;
    c->armRow(row, 4, 0, 1, true, false);
    return c;
}
static void ca_lock() {
    std::printf("Change Alley: queue under lock, flush on unlock, live-under-lock narrowing\n");
    const int active = 5;
    const int rhythmRow = caRow(0, 0, 0), melodyRow = caRow(0, 0, 1);
    // L4 — UNLOCKED boundary commits all axes (0b111).
    {
        auto c = caArmed(rhythmRow);
        Lock lock; const bool qf = lock.crossBoundary(8);
        CHECK(caFires(qf, false, 0), "L4: fires at an unlocked boundary");
        if (caFires(qf, false, 0)) c->applyAtBoundary(active, caAxisMask(qf, false, 0));
        CHECK(!c->pendingRows[rhythmRow].armed, "L4: unlocked boundary commits (0b111 all axes)");
    }
    // L5 — LOCKED, no live bit: queue.
    {
        auto c = caArmed(rhythmRow);
        Lock lock; lock.locked = true; const bool qf = lock.crossBoundary(8);
        CHECK(!caFires(qf, true, 0), "L5: locked + no live CA bit does NOT fire");
        CHECK(c->pendingRows[rhythmRow].armed, "L5: the row stays armed under lock (queued)");
    }
    // L6 — UNLOCK edge flushes all axes.
    {
        auto c = caArmed(rhythmRow);
        Lock lock; lock.locked = true; lock.crossBoundary(8);
        CHECK(c->pendingRows[rhythmRow].armed, "L6: still armed before unlock");
        lock.locked = false; const bool qf = lock.tick(0);
        CHECK(caFires(qf, false, 0), "L6: fires on the unlock edge");
        if (caFires(qf, false, 0)) c->applyAtBoundary(active, caAxisMask(qf, false, 0));
        CHECK(!c->pendingRows[rhythmRow].armed, "L6: the queue flushes on unlock");
    }
    // L7 — LIVE under lock: locked + SB_CA_R commits the RHYTHM row live, MELODY stays queued.
    {
        auto c = std::make_unique<ChangeAlleyModuleCore>(); c->resetAll();
        for (int i = 0; i < ChangeAlleyModuleCore::N_SCATTER; ++i) c->corrKey[i] = 0xC0FFEEu + i;
        c->armRow(rhythmRow, 4, 0, 1, true, false);
        c->armRow(melodyRow, 4, 0, 1, true, false);
        Lock lock; lock.locked = true;
        const bool qf = lock.crossBoundary(8);          // false (locked, no unlock edge)
        const uint32_t scope = dotModular::SB_CA_R;      // rhythm opted live under lock
        CHECK(caFires(qf, true, scope), "L7: a live CA_R bit fires under lock");
        CHECK(caAxisMask(qf, true, scope) == 0b001u, "L7: the mask narrows to rhythm only");
        if (caFires(qf, true, scope)) c->applyAtBoundary(active, caAxisMask(qf, true, scope));
        CHECK(!c->pendingRows[rhythmRow].armed, "L7: the rhythm row commits live under lock");
        CHECK(c->pendingRows[melodyRow].armed, "L7: the melody row stays queued (out of the live axis)");
    }
}

int main() {
    std::printf("== test_lock_mode_commit ==\n");
    cantonment_lock();
    ca_lock();
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
