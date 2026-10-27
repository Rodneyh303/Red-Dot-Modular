#pragma once
// CantonmentModuleCore — the MODULE-STATE core of MonsoonCantonment, rack-free so the module
// layer is CI-testable (test_cantonment_module, the build plan's step-4 row). The whole editor
// state is ONE unit here: the containment DAG (CantonmentGraph), the pending verb rows
// (CantonmentPending), the EXPAND trajectory ring (CantonmentTrajectory), the owner-guard
// flag, the pin-edit path, the boundary-apply with the undo-snapshot publication, the save
// schema (json), and the reset. The Rack module (MonsoonCantonment.hpp) composes this and adds
// ONLY the SDK surface: params/lights/triggers, the widget, and the history push (APP->history
// is UI-thread-only, so the audio thread can only PUBLISH snapshots — the widget drains them).
//
// Authority: CC_PROMPT_CANTONMENT_BUILD_PLAN.md step 4 ("Module integration + one-plane
// multi-pin UI + rejection affordance + store/undo + engine-side publish") + §"MODULE SHAPE"
// (store-backed pin edits via StoreEditAction; one committed phrase-boundary batch = one undo
// step — the CA precedent). The undo-ring + json-tail shapes mirror MonsoonChangeAlleyV2
// (TransformUndoSnapshot / trajectory tail), narrowed to containment's ONE plane.
//
// Pure / rack-free / header-only (the CantonmentGraph.hpp discipline), EXCEPT the save schema,
// which is jansson-typed: <jansson.h> is included DIRECTLY (the MonotoneCurve discipline — not
// via rack.hpp) so the standalone harness resolves it to test/jansson.h while the plugin build
// uses Rack's bundled libjansson. The exact production schema runs against both.
//
// THE R7 NOTE (test_rate_manager R7 + the step-3 lesson): no sequencing-rate primitives of any
// kind live here — the boundary apply is HOST-driven (Monsoon::process -> expanderManager.sync),
// the module never owns a clock.

#include <cstdint>
#include <cstring>
#include <atomic>
#include <jansson.h>              // DIRECT include — test shim vs Rack's bundled lib (see header note)
#include "CantonmentGraph.hpp"
#include "CantonmentTransforms.hpp"
#include "CantonmentPending.hpp"

namespace dotModular { namespace ct {

// One committed phrase-boundary batch = one undo step (the CA precedent, DAW_PARAM_AUDIT 5b):
// the audio thread snapshots before/after into this SPSC ring; the widget's step() (UI thread)
// drains it and pushes one Rack history action per batch. Commits are phrase-boundary-
// infrequent, so 16 slots is ample (the MonsoonChangeAlleyV2::TransformUndoSnapshot rationale).
struct CantonmentUndoSnapshot {
    uint16_t before[VOICES];
    uint16_t after [VOICES];
};

struct CantonmentModuleCore {
    // ── state (the step-1/2/3 members, now one unit) ──────────────────────────────────────────
    CantonmentGraph      graph;        // the DAG: bit u of parents[v] = v contained in u
    CantonmentPending    pending;      // 7 verb rows (latch/stamp/order) + rotateStep config + the true-reverse queue
    CantonmentTrajectory trajectory;   // the EXPAND memory (committed-state ring)

    // OWNER GUARD (the CA idiom, MonsoonChangeAlleyV2::transformsAppliedThisBlock):
    // applyPendingTransforms MUTATES the graph, so only ONE driver may call it per block. Reset
    // by the module at the top of process() each block; the FIRST driver to apply sets it (the
    // manager's fire block) — a second reader Monsoon skips the mutation. Runtime only.
    bool transformsAppliedThisBlock = false;

    // ── undo publication (SPSC ring; producer = audio thread, consumer = widget/UI thread) ────
    static constexpr int UNDO_RING = 16;
    CantonmentUndoSnapshot undoRing[UNDO_RING];
    std::atomic<uint32_t> undoHead{0};   // producer (audio) writes, then advances
    std::atomic<uint32_t> undoTail{0};   // consumer (UI) reads, then advances

    // ── the pin-edit path (THE single route the UI + tests take; StoreEditAction backs it) ────
    // Add is CHECKED: a pin that would close a cycle is REJECTED (false, state unchanged — the
    // caller shows the rejection affordance). Remove is ALWAYS valid (detaching cannot cycle).
    bool setPin(int v, int u, bool contained) {
        if (v < 0 || v >= VOICES || u < 0 || u >= VOICES) return false;
        if (contained) return graph.addParent(v, u);
        graph.removeParent(v, u);
        return true;
    }

    // Clear EVERY pin (the R2 all-disconnected default). The context-menu gesture; undo rides
    // the widget's whole-graph snapshot action (one gesture = one undo step). Never invalid.
    void clearAllPins() {
        for (int v = 0; v < VOICES; ++v) graph.parents[v] = 0;
    }

    // ── the host-driven boundary commit WITH the undo publication ─────────────────────────────
    // The module's applyPendingTransforms calls this (the manager decides WHEN — the CA shape).
    // The snapshot brackets exactly the batch applied THIS call; a batch that leaves the graph
    // byte-identical (an EXPAND consumed at the trajectory buffer start) publishes NO snapshot —
    // zero-size undo steps never record (the StoreEditAction house rule).
    //
    // TRUE-REVERSE consume (the global-reverse consumer) runs FIRST, BEFORE the verb snapshot —
    // the CA order (MonsoonChangeAlleyV2::applyPendingTransforms consumes the queue ahead of the
    // verb path): a pure true-reverse changes the graph with NO undo snapshot (the modulation-
    // class rule — undo is for USER actions, CA_DICE_COUNTER_MODEL.md CORRECTION^2), and a mixed
    // boundary's snapshot brackets only the verbs (its BEFORE is the already-restored state).
    // Gated by the SAME axisMask as the verbs; an out-of-axis request STAYS queued (the lamp
    // stays lit, the CA out-of-axis rule). At the trajectory's buffer start stepBack returns
    // false — STOP (state unchanged, don't wrap), the request still spent (the CA lamp rule:
    // the affordance clears even though nothing moved). The restored state is the ring's new
    // newest, so the verb path's pre-push below skips it — no double entry.
    int applyAtBoundary(int activeCount, unsigned axisMask = CantonmentPending::AXIS_PLANE) {
        if (pending.trueRevRequested && (axisMask & CantonmentPending::AXIS_PLANE)) {
            pending.trueRevRequested = false;
            CantonmentGraph restored;
            if (trajectory.stepBack(restored)) graph = restored;
        }
        const CantonmentGraph before = graph;
        const int applied = pending.applyAll(graph, activeCount, trajectory, axisMask);
        if (applied > 0) {
            const CantonmentGraph after = graph;
            if (std::memcmp(before.parents, after.parents, sizeof(before.parents)) != 0)
                publishUndo(before, after);
        }
        return applied;
    }

    // Rack reset (fresh-module state): the R2 all-disconnected graph, no pending rows, the
    // empty trajectory, the step config back to its default. Undo snapshots already published
    // stay published (the CA resetToIdentity rule — drained actions are self-contained).
    void resetAll() {
        graph = CantonmentGraph{};
        pending = CantonmentPending{};
        trajectory.clear();
        transformsAppliedThisBlock = false;
    }

    // ── the save schema (dataToJson/dataFromJson delegate here — single-sourced so the module
    // test round-trips the EXACT production schema against the jansson shim) ───────────────────
    //
    // Keys:
    //   "containmentParents" [16] — the step-1 schema verbatim: the RAW uint16 words ARE the
    //     state (the WARPDIAG lesson: no bit-packing, no derived fields). Policy: a wholly
    //     ABSENT key keeps whatever the module already holds (partial-patch load semantics);
    //     ANY malformation — wrong length, non-integer, out-of-range, or a blob that fails
    //     re-validation (acyclic + no self-bits) — falls back to the R2 all-disconnected
    //     default. A corrupt or hand-edited save never promotes a cycle into the editor state.
    //   "rotateStep" — the context-menu config (the plan: transform step config via context
    //     menu, not param slots). Absent in step-1/2/3 saves -> the default (1) stands. Values
    //     outside 1..15 are IGNORED on load (the verb's clampStep owns the runtime clamp).
    //   "randomCounter" / "randomMode" / "randomDensity" / "randomIslands" / "randomReduce" —
    //     the RANDOM row's stream position (SIGNED page index — the dice model's addressable
    //     draw address; persisted so a generated forest is a REPRODUCIBLE, shareable state)
    //     and its context-menu config. The rotateStep policy verbatim: absent (older saves) ->
    //     the defaults stand (0 / 0 outright / 3 / 1 island / reduce off); out-of-range
    //     mode/density/islands/reduce IGNORED on load (the runtime dials re-clamp; the
    //     reverse-draw REQUEST is runtime-only and never saved — a save never carries a
    //     pending row).
    //   "trajectory" {v, n, states} — the EXPAND ring's bounded TAIL (CantonmentTrajectory::
    //     SERIAL_TAIL newest-first states, version-guarded; the CA tail shape narrowed to the
    //     one stream). Absent (old saves) -> the ring is untouched (a fresh module loads
    //     empty). Present-but-stale — wrong version/shape, a state that fails DAG validation —
    //     is DROPPED (the ring clears), never restored as garbage (the CA deserialise rule).
    json_t* toJson() const {
        json_t* rootJ = json_object();
        json_t* arrJ = json_array();
        uint16_t raw[VOICES];
        graph.serializeTo(raw);
        for (int v = 0; v < VOICES; ++v)
            json_array_append_new(arrJ, json_integer(raw[v]));
        json_object_set_new(rootJ, "containmentParents", arrJ);

        json_object_set_new(rootJ, "rotateStep", json_integer(pending.rotateStep));
        json_object_set_new(rootJ, "randomCounter", json_integer((json_int_t)pending.randomCounter));
        json_object_set_new(rootJ, "randomMode", json_integer(pending.randomMode));
        json_object_set_new(rootJ, "randomDensity", json_integer(pending.randomDensity));
        json_object_set_new(rootJ, "randomIslands", json_integer(pending.randomIslands));
        json_object_set_new(rootJ, "randomReduce", json_integer(pending.randomReduce ? 1 : 0));

        const CantonmentTrajectory::SerialBlob blob = trajectory.serialise();
        json_t* tj = json_object();
        json_object_set_new(tj, "v", json_integer((json_int_t)blob.version));
        json_object_set_new(tj, "n", json_integer(blob.count));
        json_t* statesJ = json_array();
        for (int i = 0; i < blob.count; ++i) {
            json_t* st = json_array();
            for (int v = 0; v < VOICES; ++v)
                json_array_append_new(st, json_integer(blob.states[i].parents[v]));
            json_array_append_new(statesJ, st);
        }
        json_object_set_new(tj, "states", statesJ);
        json_object_set_new(rootJ, "trajectory", tj);
        return rootJ;
    }

    void fromJson(json_t* rootJ) {
        if (!rootJ || !json_is_object(rootJ)) return;

        // (1) the graph — the step-1 policy verbatim (see the schema note above).
        json_t* arrJ = json_object_get(rootJ, "containmentParents");
        if (arrJ) {
            const bool malformed =
                !json_is_array(arrJ)
                || json_array_size(arrJ) != (size_t)VOICES;
            if (malformed) {
                graph = CantonmentGraph{};
            } else {
                uint16_t raw[VOICES];
                bool ok = true;
                for (int v = 0; v < VOICES; ++v) {
                    json_t* eJ = json_array_get(arrJ, v);
                    if (!json_is_integer(eJ)) { ok = false; break; }
                    const json_int_t x = json_integer_value(eJ);
                    if (x < 0 || x > 0xFFFF) { ok = false; break; }
                    raw[v] = (uint16_t)x;
                }
                if (!ok || !graph.deserializeFrom(raw)) graph = CantonmentGraph{};
            }
        }

        // (2) the rotate-step config.
        if (json_t* rsJ = json_object_get(rootJ, "rotateStep"))
            if (json_is_integer(rsJ)) {
                const int x = (int)json_integer_value(rsJ);
                if (x >= 1 && x <= 15) pending.rotateStep = x;
            }

        // (2b) the RANDOM row's stream position + config (the rotateStep policy verbatim:
        // absent -> defaults stand; out-of-range -> ignored, the runtime dials re-clamp).
        if (json_t* rcJ = json_object_get(rootJ, "randomCounter"))
            if (json_is_integer(rcJ)) pending.randomCounter = (int64_t)json_integer_value(rcJ);
        if (json_t* rmJ = json_object_get(rootJ, "randomMode"))
            if (json_is_integer(rmJ)) {
                const int x = (int)json_integer_value(rmJ);
                if (x == 0 || x == 1) pending.randomMode = x;
            }
        if (json_t* rdJ = json_object_get(rootJ, "randomDensity"))
            if (json_is_integer(rdJ)) {
                const int x = (int)json_integer_value(rdJ);
                if (x >= 1 && x <= 10) pending.randomDensity = x;
            }
        if (json_t* riJ = json_object_get(rootJ, "randomIslands"))
            if (json_is_integer(riJ)) {
                const int x = (int)json_integer_value(riJ);
                if (x >= 1 && x <= 16) pending.randomIslands = x;
            }
        if (json_t* rrJ = json_object_get(rootJ, "randomReduce"))
            if (json_is_integer(rrJ)) {
                const int x = (int)json_integer_value(rrJ);
                if (x == 0 || x == 1) pending.randomReduce = (x != 0);
            }

        // (3) the EXPAND trajectory tail.
        if (json_t* tj = json_object_get(rootJ, "trajectory")) {
            bool ok = json_is_object(tj);
            CantonmentTrajectory::SerialBlob blob{};
            if (ok) {
                json_t* vJ      = json_object_get(tj, "v");
                json_t* nJ      = json_object_get(tj, "n");
                json_t* statesJ = json_object_get(tj, "states");
                ok = json_is_integer(vJ) && json_is_integer(nJ) && json_is_array(statesJ);
                if (ok) {
                    blob.version   = (uint32_t)json_integer_value(vJ);
                    blob.stateSize = VOICES;
                    const json_int_t n = json_integer_value(nJ);
                    ok = (n >= 0 && n <= (json_int_t)CantonmentTrajectory::SERIAL_TAIL)
                         && json_array_size(statesJ) == (size_t)n;
                    if (ok) {
                        blob.count = (int)n;
                        for (int i = 0; i < blob.count && ok; ++i) {
                            json_t* st = json_array_get(statesJ, i);
                            if (!json_is_array(st) || json_array_size(st) != (size_t)VOICES) { ok = false; break; }
                            for (int v = 0; v < VOICES; ++v) {
                                json_t* eJ = json_array_get(st, v);
                                if (!json_is_integer(eJ)) { ok = false; break; }
                                const json_int_t x = json_integer_value(eJ);
                                if (x < 0 || x > 0xFFFF) { ok = false; break; }
                                blob.states[i].parents[v] = (uint16_t)x;
                            }
                        }
                    }
                }
            }
            if (!ok || !trajectory.deserialise(blob)) trajectory.clear();
        }
    }

private:
    // Drop-if-full publish (the CA undoRing rule: "drop if UI hasn't drained (never in
    // practice)" — phrase-boundary commits are infrequent relative to the UI drain rate).
    void publishUndo(const CantonmentGraph& b, const CantonmentGraph& a) {
        const uint32_t h = undoHead.load(std::memory_order_relaxed);
        const uint32_t t = undoTail.load(std::memory_order_acquire);
        if (h - t < (uint32_t)UNDO_RING) {
            CantonmentUndoSnapshot& s = undoRing[h % UNDO_RING];
            std::memcpy(s.before, b.parents, sizeof(s.before));
            std::memcpy(s.after,  a.parents, sizeof(s.after));
            undoHead.store(h + 1, std::memory_order_release);
        }
    }
};

}} // namespace dotModular::ct
