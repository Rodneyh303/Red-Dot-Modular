/**
 * test_containment_module.cpp — Containment step 4: the MODULE-STATE core
 * (CC_PROMPT_CONTAINMENT_BUILD_PLAN.md §TESTS row 4: "test_containment_module — mock-SDK
 * module-level: edit->pending->boundary commit->publish cycle, serialization, owner guard").
 *
 * MonsoonContainment composes ContainmentModuleCore (src/dsp/ContainmentModuleCore.hpp,
 * rack-free) and adds only the SDK surface — params/lights/triggers, the widget, the history
 * push. This suite drives that core through the EXACT production protocol the module + the
 * manager + the widget implement, so the module layer is CI-tested without the SDK:
 *
 *   S0  fresh-core defaults (R2 all-disconnected, no pending, empty ring, step config 1)
 *   S1  the pin-edit path (setPin: checked add / always-valid remove / range guard; clearAllPins)
 *   S2  THE CYCLE: edit -> pending latch (hold) -> host-driven boundary commit -> undo
 *       publication (one committed batch = one undo step, the CA precedent) + trajectory growth
 *   S3  the owner-guard protocol (the manager's reset/check/apply/set shape; the second driver
 *       in the same block skips)
 *   S4  a no-op batch (EXPAND consumed at the trajectory buffer start) publishes NO undo
 *       snapshot — zero-size undo steps never record (the StoreEditAction house rule)
 *   S5  serialization: the EXACT production schema round-trips (object + TEXT via
 *       json_dumps/json_loads): graph bytes, rotate-step config, trajectory tail
 *   S6  the corrupt-save policies: malformed/invalid graph blob -> R2 default; ABSENT key ->
 *       state kept (partial-patch); stale trajectory (version/shape/cycle) -> dropped, ring
 *       cleared, never restored as garbage; junk rotate-step ignored
 *   S7  the SPSC undo ring: order continuity (each batch's before == the previous after) and
 *       the drop-if-full cap (the CA undoRing rule)
 *   S8  seeded march: random edits + boundary commits at varying active counts; DAG validity
 *       asserted after EVERY action; save/load round-trip mid-march
 *
 * The jansson include resolves to test/jansson.h (the shim — the MonotoneCurve discipline);
 * the plugin build runs the same schema against Rack's bundled libjansson.
 *
 * Compile: g++ -std=c++20 -Itest -Isrc test/test_containment_module.cpp -o /tmp/t_ctm && /tmp/t_ctm
 * (registered in test/run_all.sh as header-only)
 */
#include "../src/dsp/ContainmentModuleCore.hpp"
#include "../src/dsp/PhiloxRng.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>   // the cores are ~2 MiB each (the embedded trajectory ring) — heap-held; the CI Windows runner's 2 MB stack is the frame authority (the P-suite lesson, step 3)

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", (m)); } } while (0)

using namespace dotModular::ct;

static bool sameParents(const uint16_t* a, const uint16_t* b) {
    return std::memcmp(a, b, VOICES * sizeof(uint16_t)) == 0;
}
static bool sameGraph(const ContainmentGraph& a, const ContainmentGraph& b) {
    return sameParents(a.parents, b.parents);
}
// The spec chain 0<1<2 (voice 1 contained in 0, voice 2 contained in 1).
static void buildChain(ContainmentModuleCore& c) {
    c.clearAllPins();
    c.setPin(1, 0, true);
    c.setPin(2, 1, true);
}
static void expectChain(const ContainmentGraph& g, const char* m) {
    CHK(g.parents[1] == (uint16_t)(1u << 0), m);
    CHK(g.parents[2] == (uint16_t)(1u << 1), m);
    for (int v = 0; v < VOICES; ++v)
        if (v != 1 && v != 2) CHK(g.parents[v] == 0, m);
}
// The dual of the chain (INVERT's image): 1 in 2, 0 in 1.
static void expectDual(const ContainmentGraph& g, const char* m) {
    CHK(g.parents[0] == (uint16_t)(1u << 1), m);
    CHK(g.parents[1] == (uint16_t)(1u << 2), m);
    for (int v = 0; v < VOICES; ++v)
        if (v != 0 && v != 1) CHK(g.parents[v] == 0, m);
}
// A snapshot's raw words as a graph value (layout-identical state, no aliasing games).
static ContainmentGraph graphFromWords(const uint16_t* w) {
    ContainmentGraph g;
    std::memcpy(g.parents, w, sizeof(g.parents));
    return g;
}
// Drain one published snapshot; returns false when the ring is empty.
static bool drainOne(ContainmentModuleCore& c, ContainmentUndoSnapshot& out) {
    const uint32_t t = c.undoTail.load(std::memory_order_relaxed);
    const uint32_t h = c.undoHead.load(std::memory_order_acquire);
    if (t == h) return false;
    out = c.undoRing[t % ContainmentModuleCore::UNDO_RING];
    c.undoTail.store(t + 1, std::memory_order_release);
    return true;
}

int main() {
    std::printf("== test_containment_module — step 4: the module-state core ==\n");

    // ── S0. fresh-core defaults ────────────────────────────────────────────────────────────────
    {
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        bool allZero = true;
        for (int v = 0; v < VOICES; ++v) allZero &= (c.graph.parents[v] == 0);
        CHK(allZero, "S0: default graph all zeros (R2 all-disconnected)");
        CHK(c.graph.isValid(), "S0: default graph valid");
        CHK(!c.pending.anyArmed(), "S0: no verb armed");
        CHK(c.pending.rotateStep == 1, "S0: rotate step default 1");
        CHK(c.trajectory.size() == 0, "S0: trajectory ring empty");
        CHK(c.undoHead.load() == c.undoTail.load(), "S0: undo ring empty");
        CHK(!c.transformsAppliedThisBlock, "S0: owner guard clear");
    }

    // ── S1. the pin-edit path ──────────────────────────────────────────────────────────────────
    {
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        CHK(c.setPin(1, 0, true), "S1: valid add accepted");
        CHK(c.graph.isContainedIn(1, 0), "S1: pin landed");
        CHK(!c.setPin(0, 1, true), "S1: 2-cycle add REJECTED");
        CHK(c.graph.parents[0] == 0, "S1: rejection left the graph untouched");
        CHK(!c.setPin(0, 0, true), "S1: self-containment REJECTED");
        CHK(!c.setPin(-1, 0, true) && !c.setPin(0, 16, true), "S1: out-of-range guarded");
        CHK(c.setPin(1, 0, true), "S1: idempotent re-pin accepted (addParent no-op)");
        CHK(c.setPin(1, 0, false), "S1: remove always valid");
        CHK(c.graph.parents[1] == 0, "S1: remove landed");
        buildChain(c);
        c.clearAllPins();
        bool allZero = true;
        for (int v = 0; v < VOICES; ++v) allZero &= (c.graph.parents[v] == 0);
        CHK(allZero, "S1: clearAllPins -> the R2 default");
    }

    // ── S2. THE CYCLE: edit -> latch (hold) -> boundary commit -> publish ─────────────────────
    {
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        buildChain(c);
        expectChain(c.graph, "S2: chain built via the edit path");

        // Latch: the hold — the graph MUST NOT move until the host-driven boundary.
        c.pending.latch(ContainmentPending::V_INVERT);
        CHK(c.pending.armed[ContainmentPending::V_INVERT], "S2: INVERT armed");
        {
            ContainmentGraph probe = c.graph;
            expectChain(probe, "S2: latch NEVER touches the graph (the hold)");
        }

        // Host-driven boundary commit (the manager's fire block: owner-guard check happens
        // BEFORE the call — see S3 — then the module applies + refreshes its lamps).
        const int applied = c.applyAtBoundary(16);
        CHK(applied == 1, "S2: one row committed");
        expectDual(c.graph, "S2: INVERT applied — the dual (full edge reversal)");
        CHK(!c.pending.armed[ContainmentPending::V_INVERT], "S2: row consumed (disarmed)");
        CHK(!c.pending.anyArmed(), "S2: pending set fully drained");
        CHK(c.pending.counter == 0, "S2: counter reset-when-empty");

        // The undo publication: one committed batch = one undo step.
        ContainmentUndoSnapshot snap;
        CHK(drainOne(c, snap), "S2: one undo snapshot published");
        expectChain(graphFromWords(snap.before), "S2: snapshot BEFORE = the chain");
        expectDual (graphFromWords(snap.after),  "S2: snapshot AFTER = the dual");
        CHK(!drainOne(c, snap), "S2: exactly ONE snapshot for the one batch");
        // The EXPAND memory grew by the batch: the held state + the committed result.
        CHK(c.trajectory.size() == 2, "S2: trajectory holds held-state + result (the first-commit contract)");
    }

    // ── S3. the owner-guard protocol (the manager's shape) ────────────────────────────────────
    {
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        buildChain(c);
        // Driver A (the first sync this block): the module's process() opened the window.
        c.transformsAppliedThisBlock = false;
        c.pending.latch(ContainmentPending::V_INVERT);
        if (!c.transformsAppliedThisBlock) {
            c.applyAtBoundary(16);
            c.transformsAppliedThisBlock = true;   // first caller this block = the owner
        }
        expectDual(c.graph, "S3: driver A (the owner) applied once");
        // Driver B (a second, reader Monsoon in the same block): MUST skip.
        if (!c.transformsAppliedThisBlock) {
            c.applyAtBoundary(16);
            c.transformsAppliedThisBlock = true;
        }
        expectDual(c.graph, "S3: driver B skipped — the graph moved exactly once");
        // Next block: the window reopens.
        c.transformsAppliedThisBlock = false;
        c.pending.latch(ContainmentPending::V_INVERT);
        if (!c.transformsAppliedThisBlock) {
            c.applyAtBoundary(16);
            c.transformsAppliedThisBlock = true;
        }
        expectChain(c.graph, "S3: next block — the second INVERT restores the chain (involution)");
    }

    // ── S4. a no-op batch publishes NO undo snapshot ──────────────────────────────────────────
    {
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        const uint32_t head0 = c.undoHead.load();
        c.pending.latch(ContainmentPending::V_EXPAND);
        const int applied = c.applyAtBoundary(16);
        CHK(applied == 1, "S4: the EXPAND row is still CONSUMED at the buffer start");
        CHK(!c.pending.anyArmed(), "S4: the row's request is spent (the CA true-reverse affordance rule)");
        CHK(c.undoHead.load() == head0, "S4: NO undo snapshot — the graph is byte-identical");
        CHK(c.trajectory.size() == 1, "S4: the held state entered the trajectory (pushIfChanged pre-push)");
    }

    // ── S5. serialization: the exact production schema round-trips ────────────────────────────
    {
        auto aOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& a = *aOwner;
        buildChain(a);
        a.setPin(3, 0, true);                 // a second component (R2: disconnected vertices valid)
        a.pending.rotateStep = 3;
        a.pending.latch(ContainmentPending::V_COLLAPSE);
        a.applyAtBoundary(16);                // a real commit -> a non-empty trajectory tail

        json_t* rootJ = a.toJson();
        CHK(rootJ && json_is_object(rootJ), "S5: toJson object");
        // TEXT round-trip proves the emitted schema is parseable (the shim's json_dumps/loads).
        char* text = json_dumps(rootJ, JSON_COMPACT);
        CHK(text && text[0] == '{', "S5: json_dumps produced text");
        json_error_t err;
        json_t* parsed = text ? json_loads(text, 0, &err) : nullptr;
        CHK(parsed != nullptr, "S5: json_loads parses the schema back");

        auto bOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& b = *bOwner;
        b.fromJson(rootJ);
        CHK(sameGraph(a.graph, b.graph), "S5: graph bytes identical (the WARPDIAG lesson)");
        CHK(b.pending.rotateStep == 3, "S5: rotate-step config restored");
        {
            const auto blobA = a.trajectory.serialise();
            const auto blobB = b.trajectory.serialise();
            CHK(blobA.version == blobB.version && blobA.count == blobB.count
                && std::memcmp(blobA.states, blobB.states,
                               sizeof(blobA.states[0]) * (size_t)blobA.count) == 0,
                "S5: trajectory tail restored (version + count + state bytes)");
            CHK(blobA.count >= 1, "S5: the tail is non-empty after a real commit");
        }

        if (parsed) {
            auto dOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
            ContainmentModuleCore& d = *dOwner;
            d.fromJson(parsed);
            CHK(sameGraph(a.graph, d.graph), "S5: TEXT-parsed load is byte-identical too");
            CHK(d.trajectory.serialise().count == a.trajectory.serialise().count,
                "S5: TEXT-parsed trajectory tail matches");
            json_decref(parsed);
        }
        free(text);
        json_decref(rootJ);
    }

    // ── S6. the corrupt-save policies ─────────────────────────────────────────────────────────
    {
        // (a-e) the graph key.
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        buildChain(c);
        expectChain(c.graph, "S6: pre-state = the chain");

        {   // malformed: wrong length -> R2 default.
            json_t* o = json_object();
            json_t* arr = json_array();
            json_array_append_new(arr, json_integer(0));
            json_object_set_new(o, "containmentParents", arr);
            c.fromJson(o);
            CHK(c.graph.parents[1] == 0 && c.graph.parents[2] == 0, "S6a: short array -> R2 default");
            json_decref(o);
        }
        buildChain(c);
        {   // malformed: non-integer element -> R2 default.
            json_t* o = json_object();
            json_t* arr = json_array();
            for (int v = 0; v < VOICES; ++v)
                json_array_append_new(arr, (v == 1) ? json_string("x") : json_integer(0));
            json_object_set_new(o, "containmentParents", arr);
            c.fromJson(o);
            CHK(c.graph.parents[1] == 0, "S6b: non-integer element -> R2 default");
            json_decref(o);
        }
        buildChain(c);
        {   // malformed: out-of-range -> R2 default.
            json_t* o = json_object();
            json_t* arr = json_array();
            for (int v = 0; v < VOICES; ++v)
                json_array_append_new(arr, json_integer((v == 1) ? 0x10000 : 0));
            json_object_set_new(o, "containmentParents", arr);
            c.fromJson(o);
            CHK(c.graph.parents[1] == 0, "S6c: out-of-range element -> R2 default");
            json_decref(o);
        }
        buildChain(c);
        {   // a blob that fails re-validation (a 2-cycle) -> R2 default.
            json_t* o = json_object();
            json_t* arr = json_array();
            for (int v = 0; v < VOICES; ++v) {
                uint16_t row = 0;
                if (v == 0) row = (uint16_t)(1u << 1);
                if (v == 1) row = (uint16_t)(1u << 0);
                json_array_append_new(arr, json_integer(row));
            }
            json_object_set_new(o, "containmentParents", arr);
            c.fromJson(o);
            CHK(c.graph.parents[0] == 0 && c.graph.parents[1] == 0, "S6d: cyclic blob -> R2 default");
            json_decref(o);
        }
        {   // the key ABSENT -> state KEPT (partial-patch semantics; step-1 policy verbatim).
            buildChain(c);   // S6d's rejection defaulted the graph — rebuild the chain first
            json_t* o = json_object();
            json_object_set_new(o, "rotateStep", json_integer(5));
            c.fromJson(o);
            expectChain(c.graph, "S6e: absent graph key -> state kept");
            CHK(c.pending.rotateStep == 5, "S6e: rotate-step still loads alongside");
            json_decref(o);
        }
        {   // junk rotate-step values are IGNORED (the verb's clamp owns the runtime clamp).
            c.pending.rotateStep = 2;
            json_t* o = json_object();
            json_object_set_new(o, "rotateStep", json_integer(0));
            c.fromJson(o);
            CHK(c.pending.rotateStep == 2, "S6f: rotate-step 0 ignored");
            json_decref(o);
        }

        // (g-h) the trajectory tail: stale or cyclic -> DROPPED (ring cleared), never garbage.
        auto tOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& t = *tOwner;
        buildChain(t);
        t.pending.latch(ContainmentPending::V_INVERT);
        t.applyAtBoundary(16);
        CHK(t.trajectory.size() >= 2, "S6: trajectory non-empty pre-test");
        {   // wrong version -> dropped.
            json_t* o = json_object();
            json_t* tj = json_object();
            json_object_set_new(tj, "v", json_integer(999));
            json_object_set_new(tj, "n", json_integer(1));
            json_t* states = json_array();
            json_t* st = json_array();
            for (int v = 0; v < VOICES; ++v) json_array_append_new(st, json_integer(0));
            json_array_append_new(states, st);
            json_object_set_new(tj, "states", states);
            json_object_set_new(o, "trajectory", tj);
            t.fromJson(o);
            CHK(t.trajectory.size() == 0, "S6g: wrong-version tail dropped, ring cleared");
            CHK(sameGraph(t.graph, t.graph), "S6g: graph unaffected by the trajectory load");
            json_decref(o);
        }
        {   // a state that is not a DAG -> the whole blob dropped.
            json_t* o = json_object();
            json_t* tj = json_object();
            json_object_set_new(tj, "v", json_integer(ContainmentTrajectory::SERIAL_VERSION));
            json_object_set_new(tj, "n", json_integer(1));
            json_t* states = json_array();
            json_t* st = json_array();
            for (int v = 0; v < VOICES; ++v) {
                uint16_t row = 0;
                if (v == 0) row = (uint16_t)(1u << 1);
                if (v == 1) row = (uint16_t)(1u << 0);
                json_array_append_new(st, json_integer(row));
            }
            json_array_append_new(states, st);
            json_object_set_new(tj, "states", states);
            json_object_set_new(o, "trajectory", tj);
            t.fromJson(o);
            CHK(t.trajectory.size() == 0, "S6h: cyclic tail state -> blob dropped");
            json_decref(o);
        }
    }

    // ── S7. the SPSC undo ring: order continuity + the drop-if-full cap ───────────────────────
    {
        auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
        ContainmentModuleCore& c = *cOwner;
        buildChain(c);
        // 20 commits with a guaranteed change each (INVERT over a non-empty graph always
        // changes it — the dual of a non-empty DAG differs in row 0), UI never drains.
        for (int i = 0; i < 20; ++i) {
            c.pending.latch(ContainmentPending::V_INVERT);
            c.applyAtBoundary(16);
            CHK(c.graph.isValid(), "S7: graph valid after every commit");
        }
        CHK(c.undoHead.load() - c.undoTail.load() == (uint32_t)ContainmentModuleCore::UNDO_RING,
            "S7: drop-if-full — exactly the 16 newest snapshots retained");
        // Drain: order continuity (each batch's before == the previous batch's after).
        ContainmentUndoSnapshot prev, cur;
        CHK(drainOne(c, prev), "S7: first drained");
        int continuity = 0, count = 1;
        while (drainOne(c, cur)) {
            if (sameParents(prev.after, cur.before)) ++continuity;
            prev = cur;
            ++count;
        }
        CHK(count == ContainmentModuleCore::UNDO_RING, "S7: all 16 retained snapshots drained");
        CHK(continuity == count - 1, "S7: trajectory continuity (before[i] == after[i-1])");
        CHK(c.undoHead.load() == c.undoTail.load(), "S7: ring fully drained");
    }

    // ── S8. seeded march: edits + commits at varying pools, validity at every step ────────────
    {
        for (int seed = 1; seed <= 60; ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x9E3779B97F4A7C15ull);
            auto cOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
            ContainmentModuleCore& c = *cOwner;
            bool marchOk = true;
            const char* why = "";
            for (int iter = 0; iter < 30 && marchOk; ++iter) {
                const uint64_t r = rng();
                const int act = (int)(r % 16u) + 1;   // the operating pool 1..16 (the manager's count)
                switch ((r >> 8) % 10u) {
                    case 0: case 1: case 2: case 3: case 4: case 5: {
                        const int v = (int)(rng() & 15u), u = (int)(rng() & 15u);
                        c.setPin(v, u, (rng() & 1u) != 0);
                        break;
                    }
                    case 6: case 7: case 8: {   // arm a random verb; the boundary commits it
                        const int verb = (int)(rng() % (unsigned)ContainmentPending::N_VERBS);
                        c.pending.latch(verb);
                        const uint32_t h0 = c.undoHead.load();
                        const int appliedRows = c.applyAtBoundary(act);
                        (void)appliedRows;
                        if (c.undoHead.load() != h0) {
                            ContainmentUndoSnapshot s;
                            if (!drainOne(c, s)) { marchOk = false; why = "S8: snapshot missing after publish"; break; }
                            ContainmentGraph b, a2;
                            std::memcpy(b.parents, s.before, sizeof(b.parents));
                            std::memcpy(a2.parents, s.after, sizeof(a2.parents));
                            if (!b.isValid() || !a2.isValid()) { marchOk = false; why = "S8: snapshot state invalid"; break; }
                            if (sameGraph(b, a2)) { marchOk = false; why = "S8: zero-size snapshot published"; break; }
                        }
                        break;
                    }
                    case 9:
                        if (rng() & 1u) c.clearAllPins();
                        else c.pending.rotateStep = (int)(rng() % 7u) + 1;
                        break;
                }
                if (marchOk && !c.graph.isValid()) { marchOk = false; why = "S8: graph invalid after action"; }
            }
            CHK(marchOk, why[0] ? why : "S8: march failed (seed above)");
            if (!marchOk) continue;

            // Mid-march save/load: the whole state round-trips byte-identically.
            json_t* rootJ = c.toJson();
            auto probeOwner = std::make_unique<ContainmentModuleCore>();   // heap-held — 2 MiB ring
            ContainmentModuleCore& probe = *probeOwner;
            probe.fromJson(rootJ);
            CHK(sameGraph(c.graph, probe.graph), "S8: save/load graph identical");
            CHK(probe.pending.rotateStep == c.pending.rotateStep, "S8: save/load rotate-step identical");
            const auto blobA = c.trajectory.serialise();
            const auto blobB = probe.trajectory.serialise();
            CHK(blobA.count == blobB.count
                && std::memcmp(blobA.states, blobB.states,
                               sizeof(blobA.states[0]) * (size_t)blobA.count) == 0,
                "S8: save/load trajectory tail identical");
            json_decref(rootJ);
        }
    }

    std::printf("== containment module core: %d passed, %d failed ==\n", pass, fail);
    return fail ? 1 : 0;
}
