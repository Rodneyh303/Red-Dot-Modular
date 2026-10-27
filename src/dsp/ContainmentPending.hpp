#pragma once
// ContainmentPending — the phrase-boundary pending machinery for the containment verbs.
// Authority: CC_PROMPT_CONTAINMENT_BUILD_PLAN.md §"TRANSFORM APPLICATION — at PHRASE BOUNDARY
// (copy CA exactly)" + CC_PROMPT_CONTAINMENT_MODULE_SPEC.md. Step 3 of the build plan.
//
// REUSE, not re-invention: the chronological ORDER rides redDot::CAPendingOrder AS-IS (the plan's
// explicit instruction — it is generic over rows/armed/stamp, rack-free). This header adds what
// containment-specific logic hangs AROUND it: the verb-row model (7 rows = the panel flank order:
// the ROLES trio, the FOLD trio, and RANDOM — the 2026-10-27 ruling UNCLOSED the vocabulary for
// it: a genuinely new decision class, not synonymy), the latch/stamp rules, the apply dispatch
// onto the pure verbs (ContainmentTransforms.hpp), the RANDOM row's own Philox stream +
// reverse-draw (the CA dice model — requirement 1/2 of the ruling), the EXPAND trajectory ring
// (the CA TRUE-REVERSE precedent, Rodney's ruling R3), the TRUE-REVERSE request queue (the
// global-reverse consumer — the CA engine shape MonsoonChangeAlleyV2.hpp carries, mirrored here
// for containment's one plane), and the commit-side bookkeeping (push results,
// reset-when-empty).
//
// WHAT IS DELIBERATELY ABSENT HERE (the R7 grep-gate discipline, test_rate_manager R7): no raw
// sequencing-rate primitives of any kind — no clock-divider member, no grid-pulse member, no
// self-owned clock. The apply is called FROM the host
// (Monsoon::process -> expanderManager.sync) at the phrase boundary / unlock flush, exactly the
// CA pattern (MonsoonChangeAlleyV2::applyPendingTransforms). Between boundaries the graph is
// HELD — latching a verb NEVER touches the graph (the stable skeleton; correlation reconfigures
// within it).
//
// AXIS MASK (the CA LOCK_SCOPE_MENU semantics, carried so the machinery is complete): bit0 = the
// single containment plane. An out-of-axis armed row (mask bit clear) is SKIPPED by the apply
// loop: not applied, not disarmed, stamp KEPT — it commits at a later in-axis call, in its
// original chronological position (no jump-to-front). The counter therefore must NOT reset while
// any row remains armed (the reset-when-empty rule — CAPendingOrder.hpp header rule 4). At step 3
// containment has no LockManager Control entry, so the driver passes the default (commit plane);
// the mask machinery is exercised by test_containment_pending and narrows here when a scope bit
// lands.
//
// Pure / rack-free / header-only (the ContainmentGraph.hpp discipline): the whole pending path is
// unit-testable without the SDK. Audio-thread-only use (control-rate boundary), single-threaded —
// the module composes this struct; there is no cross-thread hand-off in it.

#include <cstdint>
#include <cstring>
#include "ContainmentGraph.hpp"
#include "ContainmentTransforms.hpp"
#include "CAPendingOrder.hpp"   // redDot::CAPendingOrder — REUSED AS-IS (the plan's instruction)

namespace dotModular { namespace ct {

// ── ContainmentTrajectory — the EXPAND memory (R3) ─────────────────────────────────────────
// A single-stream ring of COMMITTED graph states, the CATrajectoryBuffer precedent (CA_DICE_
// COUNTER_MODEL.md) narrowed to containment's one plane. The module pushes at every phrase-
// boundary commit — the HELD state entering the boundary AND the post-verb result — so the ring
// is a sparse trajectory of real committed states (pushIfChanged: unchanged states cost nothing,
// static passages never consume depth). EXPAND pops one entry: it steps the trajectory back one
// commit and restores that level ("restore the level the last COLLAPSE removed" — verb-agnostic
// like CA's true-reverse: any commit's level, reached through lossy collapses because states are
// recorded, not causes). NOT a synthesized inverse: collapse is lossy (T9,
// test_containment_transforms), and this memory is what R3 bought instead of one.
//
// WHY THE PRE-STATE PUSH (the one divergence from CATrajectoryBuffer, and why it is safe): CA's
// ring records post-commit states only, so true-reverse cannot step back across the FIRST
// recorded commit (stepBack needs >= 2 live entries). Containment's EXPAND contract is exactly
// "undo the last collapse" — the collapse-expand pair is the feature, not a session-long
// performance flourish — so the held state entering the first commit must already be in the ring.
// Pushing the held state (which IS the previous commit's result, verbatim) at each commit makes
// the trajectory complete from the first commit; pushIfChanged keeps it sparse (the held state
// usually equals the newest entry and is skipped). End-of-buffer: STOP, don't wrap (wrapping
// would silently replay the WRONG history — the CATrajectoryBuffer rule).
//
// Ring model: base = physical slot of the OLDEST entry; count = live entries (0..DEPTH). Newest
// = (base+count-1) % DEPTH; push appends at (base+count) % DEPTH, sliding base when full.
// stepBack pops the newest; a later push overwrites the popped slot = branching (the reversed
// future is discarded, exactly like undo-then-redo-branch). Fixed capacity, allocated once —
// NO audio-thread allocation. 65536 states x 32 B = 2 MiB, the CA depth rationale at one stream.
struct ContainmentTrajectory {
    static constexpr int DEPTH = 65536;           // committed states (~2 MiB; "effectively unlimited")
    static constexpr int SERIAL_TAIL = 256;       // bounded tail persisted to JSON (~8 KiB), step 4 wires it
    static constexpr uint32_t SERIAL_VERSION = 1; // layout guard; bump on any change

    struct State { uint16_t parents[VOICES]; };

    State ring[DEPTH];
    int   base  = 0;   // physical slot of the oldest live entry
    int   count = 0;   // number of live entries (0..DEPTH)

    void clear() { base = 0; count = 0; }
    int size() const { return count; }

    // Push a committed state IF it differs from the newest live entry. Called at each phrase-
    // boundary commit (held state before the verbs, result after — see the header note).
    void pushIfChanged(const ContainmentGraph& g) {
        if (count > 0) {
            const State& newest = ring[(base + count - 1) % DEPTH];
            if (std::memcmp(newest.parents, g.parents, sizeof(g.parents)) == 0) return;  // unchanged -> skip
        }
        if (count >= DEPTH) {
            // Full: overwrite the oldest (slide the window). The newest stays the newest.
            std::memcpy(ring[base].parents, g.parents, sizeof(g.parents));
            base = (base + 1) % DEPTH;
        } else {
            std::memcpy(ring[(base + count) % DEPTH].parents, g.parents, sizeof(g.parents));
            ++count;
        }
    }

    // Step one commit BACK: pop the newest live entry and restore the PREVIOUS committed state
    // into out. Returns true when a state was restored; false at the buffer start (0 or 1 live
    // entries -> nothing to reverse to -> STOP, don't wrap; the caller's consume still disarms
    // the row so the affordance stays consistent — the CA true-reverse rule).
    bool stepBack(ContainmentGraph& out) {
        if (count <= 1) return false;
        --count;   // pop the newest
        const int prevSlot = (base + count - 1) % DEPTH;
        std::memcpy(out.parents, ring[prevSlot].parents, sizeof(out.parents));
        return true;
    }

    // ── Serialisation: bounded TAIL, newest-first, version-guarded (the CATrajectoryBuffer shape).
    // Rack-free: the raw blob is exposed here; the module owns the json_t plumbing (wired at step 4
    // with the store/undo work — until then the ring is runtime-only, a performance buffer).
    struct SerialBlob {
        uint32_t version;
        int      stateSize;
        int      count;
        State    states[SERIAL_TAIL];
    };

    SerialBlob serialise() const {
        SerialBlob b{};
        b.version = SERIAL_VERSION;
        b.stateSize = (int)VOICES;
        int n = count;
        if (n > SERIAL_TAIL) n = SERIAL_TAIL;
        b.count = n;
        for (int i = 0; i < n; ++i) {   // newest-first: newest is at (base+count-1), walk back
            const int idx = (base + count - 1 - i + DEPTH) % DEPTH;
            b.states[i] = ring[idx];
        }
        return b;
    }

    // Restore a SerialBlob. A stale/mismatched blob (wrong version / stateSize, count out of
    // range, or a state that fails DAG validation) is REJECTED: returns false, buffer untouched.
    // The tail is restored oldest-first so stepBack walks it in the right order; reversing past
    // the restored tail hits the stop condition cleanly.
    bool deserialise(const SerialBlob& b) {
        if (b.version != SERIAL_VERSION) return false;
        if (b.stateSize != (int)VOICES)  return false;
        if (b.count < 0 || b.count > SERIAL_TAIL) return false;
        for (int i = 0; i < b.count; ++i) {
            // a saved state is a committed state, but the corrupt-save writer is why the graph
            // guard exists (the step-1 lesson): validate every restored state, reject the blob
            // if ANY of them is not a DAG.
            ContainmentGraph probe;
            for (int v = 0; v < VOICES; ++v) probe.parents[v] = b.states[i].parents[v];
            if (!probe.isValid()) return false;
        }
        clear();
        for (int i = b.count - 1; i >= 0; --i) {   // blob is newest-first; push oldest-first
            std::memcpy(ring[(base + count) % DEPTH].parents, b.states[i].parents, VOICES * sizeof(uint16_t));
            ++count;
        }
        return true;
    }
};

// ── ContainmentPending — the verb rows + latch/stamp + boundary apply ──────────────────────
// ROW ORDER = the panel flank order, top to bottom (ContainmentPanelMm::VERB_ID in
// MonsoonContainment.hpp, generated by panel_src/gen_containment.py) — keep the three in
// lockstep: ROLES trio (ROTATE / REFLECT / INVERT), FOLD quartet (COLLAPSE / COLLAPSE-ALL /
// DECLUTTER / EXPAND), then RANDOM (the 2026-10-27 ruling: the seed verb as the row after
// the fold family, the vocabulary unclosed for a genuinely new decision class). DECLUTTER
// is the reduction gesture pulled forward (Rodney 2026-10-27: "it would be better to pull
// declutter forward since now it can only be applied to random") — the explicit trigger for
// the landed reduceTransitive pure verb, so the manual graph earns the same strip the
// Random-reduce flag gives the draw. Row index is the chronological tie-break
// (same-sample burst -> row order).
struct ContainmentPending {
    static constexpr int N_VERBS = 8;
    enum Verb { V_ROTATE = 0, V_REFLECT = 1, V_INVERT = 2, V_COLLAPSE = 3, V_COLLAPSE_ALL = 4,
                V_DECLUTTER = 5, V_EXPAND = 6, V_RANDOM = 7 };
    static constexpr unsigned AXIS_PLANE = 1u;   // bit0 = the single containment plane

    bool     armed[N_VERBS] = {};
    uint32_t stamp[N_VERBS] = {};
    uint32_t counter = 0;      // monotonic; stamped on unarmed->armed; reset ONLY when empty
    int      rotateStep = 1;   // ROTATE's <=1 param (default 1); context-menu config, clamped at apply

    // ── the RANDOM row's own draw (requirement 1: OWN Philox stream) ───────────────
    // Domain-separated: a FIXED domain key ("CONTAINM") — never the shared SEED jack, never
    // another stream's key — and the counter is a SIGNED addressable PAGE index (the dice
    // model: position, NOT a seed — seed-keying would jump to a different sequence per count
    // instead of stepping the draw). reverseArmed is the reverse-draw request (requirement 2):
    // it arms the SAME row (one stamp, the lamp lights — the CA latchRow choke point) with the
    // backward direction; direction = the LATEST arm. randomMode picks the page interpretation
    // (0 = OUTRIGHT stateless new forest, 1 = SCATTER state-dependent modify); randomDensity
    // is the 1..10 dial (p = density/10); randomIslands is the 1..16 partition dial (path B,
    // Rodney 2026-10-27: the drawn forest's island count — each island internally connected
    // with exactly one root, islands mutually disconnected, size-1 islands = free single
    // voices; consumed by OUTRIGHT, ignored by SCATTER) — context-menu config, the
    // rotateStep precedent. The draw SPANS THE POOL (the polyphony at apply); rows above the
    // pool clear to free singles ("size of forest linked to polyphony at draw with other
    // pins cleared").
    // (The domain constant + page strides live with the draws in ContainmentTransforms.hpp.)
    int64_t  randomCounter = 0;
    int      randomMode    = 0;
    int      randomDensity = 3;
    int      randomIslands = 1;
    bool     randomReduce  = false;   // the seed verb's reduction post-pass — the banked ONE CALL
                                      // (IDEAS_PARKED.md): when on, an OUTRIGHT draw runs
                                      // reduceTransitive(g) after the boost, stripping the page's
                                      // implied shortcut pins to the load-bearing spine ("a flag,
                                      // not forced: random shortcut edges may be exactly the
                                      // variety wanted" — off keeps the dice's full variety).
                                      // SCATTER ignores it (a modify pass over current edges; the
                                      // manual-graph strip is the DECLUTTER row, V_DECLUTTER).
    bool     reverseArmed  = false;   // runtime-only (like armed[] — a save never carries a request)

    // ── the TRUE-REVERSE request queue (the global-reverse consumer) ────────────────
    // The CA engine shape (MonsoonChangeAlleyV2.hpp trueRevRequested[], CATrajectoryBuffer.hpp):
    // a PERFORMANCE-class gesture (jack OR button) that arms ONE committed-state trajectory
    // step-back, consumed at the phrase boundary BEFORE the verb bracket — so a pure true-reverse
    // publishes NO undo (the modulation-class rule: undo is for USER actions; CA_DICE_COUNTER_
    // MODEL.md CORRECTION^2) and a mixed boundary's undo snapshot brackets only the verbs. The
    // consume lives in ContainmentModuleCore::applyAtBoundary (which owns the undo bracket), NOT
    // in applyAll's verb loop — this flag is a separate queue, NOT a verb row: no stamp, anyArmed()
    // unaffected, idempotent while lit (the CA rule: further presses/triggers are ignored). At the
    // trajectory's buffer start the step-back STOPS (state unchanged) but the request is still
    // spent — the CA affordance-consistency rule (the lamp clears either way). Runtime-only: a
    // save never carries a request (the reverseArmed rule). The trajectory it walks is the SAME
    // ring EXPAND steps — one consume = one commit back, verb-agnostic (records RESULTS, not
    // causes), which is exactly what makes RANDOM's state-dependent SCATTER commits reversible
    // where the dice reverse alone is not (requirement 3 of the 2026-10-27 ruling).
    bool     trueRevRequested = false;

    // The queue's arm (the CA true-reverse jack/button action): idempotent — arming while the
    // request is pending changes nothing (one queued request, the lamp is the affordance).
    void latchTrueReverse() { trueRevRequested = true; }

    // The single choke point for arming (trig jacks + fire buttons at step 4; tests here). Stamp
    // ONLY on the unarmed->armed transition: re-arming an armed row keeps its original stamp and
    // does NOT move later in the order (the CA latchRow rule — latchRow is the single choke point
    // so triggers, buttons and scatter-back all inherit it; here armVerb is that choke point).
    void latch(int verb) {
        if (verb < 0 || verb >= N_VERBS) return;
        if (!armed[verb]) stamp[verb] = counter++;
        armed[verb] = true;
        if (verb == V_RANDOM) reverseArmed = false;   // a plain latch is a FORWARD fire: the latest arm owns the direction
    }

    // The reverse-draw arm (requirement 2 — the CA back-jack action, the same choke point):
    // arms the RANDOM row backward. If the row was already armed forward, the stamp is kept
    // and the direction flips (the latest request wins — one pending row, one decision).
    void latchRandomReverse() {
        latch(V_RANDOM);
        reverseArmed = true;
    }

    bool anyArmed() const {
        for (int r = 0; r < N_VERBS; ++r) if (armed[r]) return true;
        return false;
    }

    // Apply all ARMED rows whose axis is in axisMask, in chronological order (ascending stamp,
    // ties by row index = the panel flank order — redDot::CAPendingOrder reused AS-IS). Returns
    // the number of rows applied. THE ONLY writer of the graph besides pin edits; runs at the
    // phrase boundary (host-driven), never per-sample.
    //
    // Order-of-operations (the CA shape, MonsoonChangeAlleyV2::applyPendingTransforms):
    //   early return when nothing in-axis is armed  -> rows persist, counter NOT reset;
    //   pre-push the held state (completes the trajectory from the first commit — see the ring
    //     header note; pushIfChanged makes this free in steady state);
    //   per row, in stamp order: verb dispatch onto the pure verbs (the single-plane axis gate is
    //     the early return; the per-row gate site is marked below for when axes arrive); EXPAND
    //     steps the trajectory back one commit (STOP at buffer start — state unchanged, row still
    //     consumed); disarm the row;
    //   post-push the committed result (records RESULTS, not causes — that is what makes EXPAND
    //     verb-agnostic and able to step back through a lossy collapse);
    //   reset the counter ONLY when the pending set is now fully empty (resetting at every
    //     boundary would give new arms stamp 0 < the persistent rows' stamps -> they would jump
    //     to the FRONT of later commits — the exact wrong the rule forbids).
    int applyAll(ContainmentGraph& g, int activeCount, ContainmentTrajectory& traj,
                 unsigned axisMask = AXIS_PLANE) {
        const bool inAxis = (axisMask & AXIS_PLANE) != 0;
        if (!inAxis || !anyArmed()) return 0;

        traj.pushIfChanged(g);   // the held state enters the trajectory (skipped when unchanged)

        bool armedArr[N_VERBS]; uint32_t stampArr[N_VERBS];
        for (int r = 0; r < N_VERBS; ++r) { armedArr[r] = armed[r]; stampArr[r] = stamp[r]; }
        int order[N_VERBS];
        const int n = redDot::CAPendingOrder::orderRows(N_VERBS, armedArr, stampArr, order);

        int applied = 0;
        // SCOPE: with ONE plane the axis gate is the early return above — every row reaching this
        // loop is in-axis (CA's per-row gate is load-bearing only because its rows carry three
        // different axis bits). When containment grows per-verb axes, the per-row gate sits HERE:
        //   if (!(axisMask & axisBitFor(row))) continue;   // out-of-axis: stay armed, stamp kept
        // and the early return narrows to "no IN-AXIS row armed" — the persisted-row machinery
        // (orderRows including all armed rows, stamps surviving, no jump-to-front) is already
        // exactly that shape, which is what test_containment_pending pins.
        for (int oi = 0; oi < n; ++oi) {
            const int row = order[oi];
            switch (row) {
                case V_ROTATE:      rotateRoles(g, activeCount, rotateStep); break;
                case V_REFLECT:     reflectRoles(g, activeCount); break;
                case V_INVERT:      invert(g); break;
                case V_COLLAPSE:    collapse(g); break;
                case V_COLLAPSE_ALL: collapseAll(g); break;
                case V_DECLUTTER:
                    // The reduction GESTURE (the parked trigger pulled forward, Rodney 2026-10-27):
                    // the landed pure verb over the CURRENT graph — the strip is closure-preserving
                    // (T11 mask-exact), delete-only, idempotent, order-free, root-set invariant, so
                    // the whole-plane fold-family convention applies (COLLAPSE/COLLAPSE-ALL are
                    // whole-graph too; the POOL span is the DRAWS' convention, not the fold's).
                    // Explicit-trigger-only (§8): the strip deletes deliberate shortcut offers
                    // (C1's offer union and C4's hold-follow read DIRECT edges), so it never runs
                    // as a background pass. Reversible through the SAME trajectory bracket as
                    // every row (pre-push held state, post-push result — EXPAND steps back).
                    reduceTransitive(g);
                    break;
                case V_EXPAND: {
                    // Trajectory memory, not an inverse (R3): pop the newest committed state and
                    // restore the level before it. At the buffer start stepBack returns false —
                    // STOP (state unchanged, don't wrap) — but the row is still consumed, the CA
                    // true-reverse affordance rule (the lamp clears; the request is spent).
                    ContainmentGraph restored;
                    if (traj.stepBack(restored)) g = restored;
                    break;
                }
                case V_RANDOM: {
                    // The RANDOM row (IDEAS_PARKED.md, the 2026-10-27 ruling): move the page
                    // pointer in the armed direction FIRST, then apply the page — so the OUTRIGHT
                    // mode's graph is always f(randomCounter) and the reverse-draw IS the exact
                    // state restore (the stateless model's reverse-as-undo), while SCATTER
                    // re-applies the previous page to the CURRENT graph (the CA generative scrub
                    // — draw-reversible, NOT state-restorable; the trajectory bracket above and
                    // the undo publication below own the state side, requirements 3 and 4).
                    randomCounter += reverseArmed ? -1 : 1;
                    const uint64_t pageBase = (uint64_t)randomCounter
                        * (randomMode == 0 ? RANDOM_PAGE_OUTRIGHT : RANDOM_PAGE_SCATTER);
                    redDot::PhiloxRng rng;                   // stateless facade: the key is the draw's
                    rng.setKey(redDot::philoxMakeKey(RANDOM_STREAM_DOMAIN));
                    if (randomMode == 0) seedForest(g, rng, pageBase, randomDensity,
                                                    randomIslands, activeCount, randomReduce);
                    else                 scatterEdges(g, rng, pageBase, randomDensity, activeCount);
                    reverseArmed = false;                    // consumed either way (the CA lamp rule)
                    break;
                }
            }
            armed[row] = false;
            ++applied;
        }

        traj.pushIfChanged(g);   // the committed RESULT enters the trajectory

        // Reset-when-empty (CAPendingOrder.hpp rule 4): only on a complete clear. An out-of-axis
        // batch (impossible at step 3's single plane via the early return, but the rule is stated
        // here where it lives) or a partial commit keeps the counter so persistent rows stay
        // chronologically comparable with future arms.
        if (!anyArmed()) counter = 0;
        return applied;
    }
};

}} // namespace dotModular::ct
