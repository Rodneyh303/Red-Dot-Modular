// test_change_alley_module.cpp — the Change Alley V2 MODULE-STATE core, headless. The sibling of
// test_cantonment_module, and the thing that was IMPOSSIBLE before the core existed: CA's whole
// commit cycle used to live on the Rack Module (applyPendingTransforms), so #372 could only
// TRANSCRIBE the manager's fire/axis rule. With ChangeAlleyModuleCore (rack-free) the real
// arm -> boundary-commit -> undo -> trajectory cycle is driven directly.
//
// Two layers:
//   S0-S9  the core cycle in isolation (defaults, the hold, commit, axis gating, chronological
//          order, the q-mix 0b111 all-axis rule, scatter counters, true-reverse consume + STOP,
//          undo snapshots, reset).
//   E1     END-TO-END through the REAL seam: SequencerEngine + the real dotModular::LockManager
//          feeding queueFires() into core.applyAtBoundary — a verb committing exactly at the
//          forward phrase boundary, and (characterized) the reverse heuristic firing the commit
//          off-boundary. This is the join #372 had to mock.
//
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include <memory>
#include "engines/SequencerEngine.hpp"
#include "MonsoonLockManager.hpp"
#include "../src/dsp/ChangeAlleyModuleCore.hpp"
using redDot::ChangeAlleyModuleCore;
using dotModular::LockManager;
using Core = ChangeAlleyModuleCore;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

static std::unique_ptr<Core> freshCore() {
    auto c = std::make_unique<Core>();
    c->resetAll();
    for (int i = 0; i < Core::N_SCATTER; ++i) c->corrKey[i] = 0xC0FFEEu + (uint64_t)i;   // deterministic keys
    return c;
}
// collapse rows live at verb 0; rowId = verb*SIDES*TYPES + side*TYPES + type.
static int rowOf(int verb, int side, int type) { return verb * Core::SIDES * Core::TYPES + side * Core::TYPES + type; }

int main() {
    std::printf("== test_change_alley_module ==\n");

    // S0 — fresh defaults: identity diagonal, nothing armed, counter 0.
    {
        auto c = freshCore();
        bool diag = true;
        for (int v = 0; v < Core::N_VOICES; ++v)
            if (c->rhythmSrc[v] != v || c->melodySrc[v] != v || c->qmixSrc[v] != v) diag = false;
        CHECK(diag, "S0: fresh core is the identity diagonal on all three planes");
        bool none = true; for (auto& p : c->pendingRows) if (p.armed) none = false;
        CHECK(none, "S0: no row armed");
        CHECK(c->pendingStampCounter == 0, "S0: stamp counter 0");
        CHECK(c->applyAtBoundary(8) == 0, "S0: boundary with nothing armed commits nothing");
    }

    // S1 — the hold: arming NEVER touches the planes until the boundary.
    {
        auto c = freshCore();
        c->armRow(rowOf(0, 0, 0), 4, 0, 1, true, false);   // collapse, side 0, rhythm
        CHECK(c->pendingRows[rowOf(0, 0, 0)].armed, "S1: row armed");
        bool diag = true;
        for (int v = 0; v < Core::N_VOICES; ++v) if (c->rhythmSrc[v] != v) diag = false;
        CHECK(diag, "S1: arming did NOT move the plane (the hold)");
    }

    // S2 — the cycle: boundary commits, row disarmed, plane changed, one undo snapshot published.
    {
        auto c = freshCore();
        c->armRow(rowOf(0, 0, 0), 4, 0, 1, true, false);
        const uint32_t h0 = c->undoHead.load();
        const int applied = c->applyAtBoundary(8, 0b111u);
        CHECK(applied == 1, "S2: one row committed");
        CHECK(!c->pendingRows[rowOf(0, 0, 0)].armed, "S2: row consumed (disarmed)");
        CHECK(c->undoHead.load() == h0 + 1, "S2: one undo snapshot published");
        bool moved = false;
        for (int v = 0; v < Core::N_VOICES; ++v) if (c->rhythmSrc[v] != v) moved = true;
        CHECK(moved, "S2: the rhythm plane actually changed");
    }

    // S3 — axis gating: a melody row under a rhythm-only mask stays armed and the plane is untouched.
    {
        auto c = freshCore();
        const int mrow = rowOf(0, 0, 1);   // collapse, melody (type 1)
        c->armRow(mrow, 4, 0, 1, true, false);
        const int applied = c->applyAtBoundary(8, 0b001u);   // rhythm axis only
        CHECK(applied == 0, "S3: melody row does not commit under a rhythm-only mask");
        CHECK(c->pendingRows[mrow].armed, "S3: out-of-axis row stays armed");
        bool diag = true; for (int v = 0; v < Core::N_VOICES; ++v) if (c->melodySrc[v] != v) diag = false;
        CHECK(diag, "S3: melody plane untouched");
        // now the in-axis commit lands it
        CHECK(c->applyAtBoundary(8, 0b010u) == 1, "S3: melody commits under the melody mask");
        CHECK(!c->pendingRows[mrow].armed, "S3: row consumed on the in-axis boundary");
    }

    // S4 — the q-mix 0b111 rule (the manager bugfix): a q-mix row (type 2) commits on a QUEUE fire.
    {
        auto c = freshCore();
        const int qrow = rowOf(0, 0, 2);   // collapse, q-mix (type 2)
        c->armRow(qrow, 4, 0, 1, true, false);
        CHECK(c->applyAtBoundary(8, 0b111u) == 1, "S4: q-mix row commits under 0b111 (the all-axis queue fire)");
        CHECK(!c->pendingRows[qrow].armed, "S4: q-mix row consumed");
        // under the OLD 0b11 it would have stayed armed forever — guard that regression:
        auto c2 = freshCore();
        c2->armRow(qrow, 4, 0, 1, true, false);
        CHECK(c2->applyAtBoundary(8, 0b011u) == 0, "S4: q-mix row does NOT commit under 0b011 (the old bug's mask)");
        CHECK(c2->pendingRows[qrow].armed, "S4: and it stays armed (why 0b111 was the fix)");
    }

    // S5 — chronological order: two armed rows commit ascending-stamp (arm order), not row order.
    {
        auto c = freshCore();
        const int late = rowOf(0, 0, 0);   // lower row index
        const int early = rowOf(2, 1, 0);  // higher row index, armed FIRST
        c->armRow(early, 4, 0, 1, true, false);    // stamp 0
        c->armRow(late,  4, 0, 1, true, false);    // stamp 1
        CHECK(c->pendingRows[early].stamp < c->pendingRows[late].stamp,
              "S5: the first-armed row carries the lower stamp regardless of row index");
        CHECK(c->applyAtBoundary(8, 0b111u) == 2, "S5: both committed");
        CHECK(c->pendingStampCounter == 0, "S5: stamp counter reset when the pending set fully drains");
    }

    // S6 — scatter verb bumps its counter (fwd +1 / back -1), per stream.
    {
        auto c = freshCore();
        const int srow = rowOf(Core::V_SCATTER, 0, 0);   // scatter, side 0, rhythm, domain
        const int ci = (0 * Core::SCATTER_TYPES + 0) * 2 + 0;
        c->armRow(srow, 4, 0, +1, true, false);
        c->applyAtBoundary(8, 0b111u);
        CHECK(c->scatterCounter[ci] == 1, "S6: a fwd scatter bumps the stream counter +1");
        c->armRow(srow, 4, 0, -1, true, false);
        c->applyAtBoundary(8, 0b111u);
        CHECK(c->scatterCounter[ci] == 0, "S6: a back scatter bumps it -1");
    }

    // S7 — true-reverse: consume steps the committed-state trajectory back; STOP (no wrap) at start.
    {
        auto c = freshCore();
        // commit two different collapses to lay down trajectory on the rhythm stream.
        c->armRow(rowOf(0, 0, 0), 2, 0, 1, true, false); c->applyAtBoundary(8, 0b111u);
        std::array<uint8_t, Core::N_VOICES> afterFirst = c->rhythmSrc;
        c->armRow(rowOf(0, 0, 0), 4, 0, 1, true, false); c->applyAtBoundary(8, 0b111u);
        // arm true-reverse on rhythm (stream 0), commit -> should restore the previous state.
        c->armTrueReverse(0);
        CHECK(c->trueRevRequested[0], "S7: true-reverse queued on the rhythm stream");
        c->applyAtBoundary(8, 0b111u);
        CHECK(!c->trueRevRequested[0], "S7: true-reverse consumed at the boundary");
        CHECK(c->rhythmSrc == afterFirst, "S7: stepped back to the previous committed rhythm state");
        // keep stepping back to the buffer start; eventually STOP leaves state unchanged but still clears.
        for (int i = 0; i < 8; ++i) { c->armTrueReverse(0); c->applyAtBoundary(8, 0b111u); }
        CHECK(!c->trueRevRequested[0], "S7: at the buffer start the request still clears (STOP, no wrap)");
    }

    // S8 — the undo snapshot captures before != after on the committed plane.
    {
        auto c = freshCore();
        c->armRow(rowOf(0, 0, 0), 2, 0, 1, true, false);
        const uint32_t h0 = c->undoHead.load();
        c->applyAtBoundary(8, 0b111u);
        const auto& snap = c->undoRing[h0 % Core::UNDO_RING];
        bool diffBefore = false, matchAfter = true;
        for (int v = 0; v < Core::N_VOICES; ++v) {
            if (snap.beforeR[v] != v) diffBefore = true;              // before == the identity we started from
            if (snap.afterR[v] != c->rhythmSrc[v]) matchAfter = false; // after == the committed state
        }
        CHECK(!diffBefore, "S8: undo 'before' captured the pre-commit (identity) rhythm state");
        CHECK(matchAfter, "S8: undo 'after' captured the post-commit rhythm state");
    }

    // S9 — resetAll restores the identity diagonal and clears pending/trajectory/counters.
    {
        auto c = freshCore();
        c->armRow(rowOf(0, 0, 0), 2, 0, 1, true, false); c->applyAtBoundary(8, 0b111u);
        c->resetAll();
        bool diag = true; for (int v = 0; v < Core::N_VOICES; ++v) if (c->rhythmSrc[v] != v) diag = false;
        CHECK(diag, "S9: resetAll restores the identity diagonal");
        bool none = true; for (auto& p : c->pendingRows) if (p.armed) none = false;
        CHECK(none, "S9: resetAll clears pending");
        CHECK(c->pendingStampCounter == 0, "S9: resetAll clears the stamp counter");
    }

    // E1 — END TO END through the REAL seam: engine + real LockManager -> queueFires -> core.
    // This is the join #372 could only mock. Forward: the verb commits exactly at the wrap.
    {
        const int lenG = 8;
        auto c = freshCore();
        SequencerEngine eng; eng.reset();
        eng.updateWindow((float)lenG, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 4;
        LockManager lock{eng.locked};
        lock.tick(eng.playedColumn()); (void)lock.queueFires();   // swallow the power-on edge

        c->armRow(rowOf(0, 0, 0), 4, 0, 1, true, false);          // armed before the phrase runs
        int commitStep = -1;
        for (int k = 0; k < 2 * lenG; ++k) {
            eng.advancePlayhead(+1, nullptr);
            lock.tick(eng.playedColumn());
            if (lock.queueFires()) {
                const bool wasArmed = c->pendingRows[rowOf(0, 0, 0)].armed;
                c->applyAtBoundary(eng.numPolyVoices + 1, 0b111u);
                if (wasArmed && !c->pendingRows[rowOf(0, 0, 0)].armed && commitStep < 0) commitStep = k;
            }
        }
        CHECK(commitStep == lenG, "E1 forward: the verb committed exactly at the first phrase boundary");
    }
    {   // E1 reverse: the inherited heuristic fires the commit OFF the boundary (characterized).
        const int lenG = 8;
        auto c = freshCore();
        SequencerEngine eng; eng.reset();
        eng.updateWindow((float)lenG, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 4;
        LockManager lock{eng.locked};
        lock.tick(eng.playedColumn()); (void)lock.queueFires();
        c->armRow(rowOf(0, 0, 0), 4, 0, 1, true, false);
        int commitStep = -1;
        for (int k = 0; k < 2 * lenG; ++k) {
            eng.advancePlayhead(-1, nullptr);
            lock.tick(eng.playedColumn());
            if (lock.queueFires()) {
                const bool wasArmed = c->pendingRows[rowOf(0, 0, 0)].armed;
                c->applyAtBoundary(eng.numPolyVoices + 1, 0b111u);
                if (wasArmed && !c->pendingRows[rowOf(0, 0, 0)].armed && commitStep < 0) commitStep = k;
            }
        }
        CHECK(commitStep == 1,
              "E1 reverse: today's heuristic commits the verb on the FIRST reverse step, not the boundary"
              " (the inherited defect, EXPECTED TO MOVE with the engine-edge fix)");
    }

    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
