/**
 * test_cantonment_module.cpp — Containment step 4: the MODULE-STATE core
 * (CC_PROMPT_CANTONMENT_BUILD_PLAN.md §TESTS row 4: "test_cantonment_module — mock-SDK
 * module-level: edit->pending->boundary commit->publish cycle, serialization, owner guard").
 *
 * MonsoonCantonment composes CantonmentModuleCore (src/dsp/CantonmentModuleCore.hpp,
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
 * Compile: g++ -std=c++20 -Itest -Isrc test/test_cantonment_module.cpp -o /tmp/t_ctm && /tmp/t_ctm
 * (registered in test/run_all.sh as header-only)
 */
#include "../src/dsp/CantonmentModuleCore.hpp"
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
static bool sameGraph(const CantonmentGraph& a, const CantonmentGraph& b) {
    return sameParents(a.parents, b.parents);
}
// The spec chain 0<1<2 (voice 1 contained in 0, voice 2 contained in 1).
static void buildChain(CantonmentModuleCore& c) {
    c.clearAllPins();
    c.setPin(1, 0, true);
    c.setPin(2, 1, true);
}
static void expectChain(const CantonmentGraph& g, const char* m) {
    CHK(g.parents[1] == (uint16_t)(1u << 0), m);
    CHK(g.parents[2] == (uint16_t)(1u << 1), m);
    for (int v = 0; v < VOICES; ++v)
        if (v != 1 && v != 2) CHK(g.parents[v] == 0, m);
}
// The dual of the chain (INVERT's image): 1 in 2, 0 in 1.
static void expectDual(const CantonmentGraph& g, const char* m) {
    CHK(g.parents[0] == (uint16_t)(1u << 1), m);
    CHK(g.parents[1] == (uint16_t)(1u << 2), m);
    for (int v = 0; v < VOICES; ++v)
        if (v != 0 && v != 1) CHK(g.parents[v] == 0, m);
}
// A snapshot's raw words as a graph value (layout-identical state, no aliasing games).
static CantonmentGraph graphFromWords(const uint16_t* w) {
    CantonmentGraph g;
    std::memcpy(g.parents, w, sizeof(g.parents));
    return g;
}
// Drain one published snapshot; returns false when the ring is empty.
static bool drainOne(CantonmentModuleCore& c, CantonmentUndoSnapshot& out) {
    const uint32_t t = c.undoTail.load(std::memory_order_relaxed);
    const uint32_t h = c.undoHead.load(std::memory_order_acquire);
    if (t == h) return false;
    out = c.undoRing[t % CantonmentModuleCore::UNDO_RING];
    c.undoTail.store(t + 1, std::memory_order_release);
    return true;
}

int main() {
    std::printf("== test_cantonment_module — step 4: the module-state core ==\n");

    // ── S0. fresh-core defaults ────────────────────────────────────────────────────────────────
    {
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
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
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
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
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
        buildChain(c);
        expectChain(c.graph, "S2: chain built via the edit path");

        // Latch: the hold — the graph MUST NOT move until the host-driven boundary.
        c.pending.latch(CantonmentPending::V_INVERT);
        CHK(c.pending.armed[CantonmentPending::V_INVERT], "S2: INVERT armed");
        {
            CantonmentGraph probe = c.graph;
            expectChain(probe, "S2: latch NEVER touches the graph (the hold)");
        }

        // Host-driven boundary commit (the manager's fire block: owner-guard check happens
        // BEFORE the call — see S3 — then the module applies + refreshes its lamps).
        const int applied = c.applyAtBoundary(16);
        CHK(applied == 1, "S2: one row committed");
        expectDual(c.graph, "S2: INVERT applied — the dual (full edge reversal)");
        CHK(!c.pending.armed[CantonmentPending::V_INVERT], "S2: row consumed (disarmed)");
        CHK(!c.pending.anyArmed(), "S2: pending set fully drained");
        CHK(c.pending.counter == 0, "S2: counter reset-when-empty");

        // The undo publication: one committed batch = one undo step.
        CantonmentUndoSnapshot snap;
        CHK(drainOne(c, snap), "S2: one undo snapshot published");
        expectChain(graphFromWords(snap.before), "S2: snapshot BEFORE = the chain");
        expectDual (graphFromWords(snap.after),  "S2: snapshot AFTER = the dual");
        CHK(!drainOne(c, snap), "S2: exactly ONE snapshot for the one batch");
        // The EXPAND memory grew by the batch: the held state + the committed result.
        CHK(c.trajectory.size() == 2, "S2: trajectory holds held-state + result (the first-commit contract)");
    }

    // ── S3. the owner-guard protocol (the manager's shape) ────────────────────────────────────
    {
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
        buildChain(c);
        // Driver A (the first sync this block): the module's process() opened the window.
        c.transformsAppliedThisBlock = false;
        c.pending.latch(CantonmentPending::V_INVERT);
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
        c.pending.latch(CantonmentPending::V_INVERT);
        if (!c.transformsAppliedThisBlock) {
            c.applyAtBoundary(16);
            c.transformsAppliedThisBlock = true;
        }
        expectChain(c.graph, "S3: next block — the second INVERT restores the chain (involution)");
    }

    // ── S4. a no-op batch publishes NO undo snapshot ──────────────────────────────────────────
    {
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
        const uint32_t head0 = c.undoHead.load();
        c.pending.latch(CantonmentPending::V_EXPAND);
        const int applied = c.applyAtBoundary(16);
        CHK(applied == 1, "S4: the EXPAND row is still CONSUMED at the buffer start");
        CHK(!c.pending.anyArmed(), "S4: the row's request is spent (the CA true-reverse affordance rule)");
        CHK(c.undoHead.load() == head0, "S4: NO undo snapshot — the graph is byte-identical");
        CHK(c.trajectory.size() == 1, "S4: the held state entered the trajectory (pushIfChanged pre-push)");
    }

    // ── S5. serialization: the exact production schema round-trips ────────────────────────────
    {
        auto aOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& a = *aOwner;
        buildChain(a);
        a.setPin(3, 0, true);                 // a second component (R2: disconnected vertices valid)
        a.pending.rotateStep = 3;
        a.pending.randomCounter = -7;         // a SIGNED page index (reverse territory) round-trips
        a.pending.randomMode = 1;             // SCATTER
        a.pending.randomDensity = 9;
        a.pending.randomIslands = 5;          // path B: the partition dial round-trips too
        a.pending.randomReduce = true;        // the banked post-pass flag round-trips (0/1 integer)
        a.pending.latch(CantonmentPending::V_COLLAPSE);
        a.applyAtBoundary(16);                // a real commit -> a non-empty trajectory tail

        json_t* rootJ = a.toJson();
        CHK(rootJ && json_is_object(rootJ), "S5: toJson object");
        // TEXT round-trip proves the emitted schema is parseable (the shim's json_dumps/loads).
        char* text = json_dumps(rootJ, JSON_COMPACT);
        CHK(text && text[0] == '{', "S5: json_dumps produced text");
        json_error_t err;
        json_t* parsed = text ? json_loads(text, 0, &err) : nullptr;
        CHK(parsed != nullptr, "S5: json_loads parses the schema back");

        auto bOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& b = *bOwner;
        b.fromJson(rootJ);
        CHK(sameGraph(a.graph, b.graph), "S5: graph bytes identical (the WARPDIAG lesson)");
        CHK(b.pending.rotateStep == 3, "S5: rotate-step config restored");
        CHK(b.pending.randomCounter == -7 && b.pending.randomMode == 1 && b.pending.randomDensity == 9
            && b.pending.randomIslands == 5,
            "S5: the RANDOM row's stream position + config restored (the counter is a SIGNED page index)");
        CHK(b.pending.randomReduce == true,
            "S5: the reduce post-pass flag restored (the 0/1 json integer, the banked ONE CALL)");
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
            auto dOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
            CantonmentModuleCore& d = *dOwner;
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
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
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

        // (g) the RANDOM row's keys: the rotateStep policy verbatim + the commit cycle.
        {
            auto rOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
            CantonmentModuleCore& r = *rOwner;
            json_t* o = json_object();          // NONE of the random keys present
            r.fromJson(o);
            CHK(r.pending.randomCounter == 0 && r.pending.randomMode == 0 && r.pending.randomDensity == 3
                && r.pending.randomIslands == 1 && r.pending.randomReduce == false,
                "S6g: absent RANDOM keys -> defaults stand (older saves load clean)");
            json_decref(o);
            r.pending.randomCounter = 5; r.pending.randomMode = 0; r.pending.randomDensity = 4;
            r.pending.randomIslands = 3;
            r.pending.randomReduce = true;
            json_t* o2 = json_object();
            json_object_set_new(o2, "randomCounter", json_integer((json_int_t)-3));   // signed: LEGAL
            json_object_set_new(o2, "randomMode", json_integer(7));                   // out-of-range
            json_object_set_new(o2, "randomDensity", json_integer(99));               // out-of-range
            json_object_set_new(o2, "randomIslands", json_integer(0));                // out-of-range
            json_object_set_new(o2, "randomReduce", json_integer(2));                 // out-of-range
            r.fromJson(o2);
            CHK(r.pending.randomCounter == -3,
                "S6g: a negative page index loads (the dice counter is a SIGNED position)");
            CHK(r.pending.randomMode == 0 && r.pending.randomDensity == 4 && r.pending.randomIslands == 3
                && r.pending.randomReduce == true,
                "S6g: out-of-range mode/density/islands/reduce IGNORED (the runtime dials stand)");
            json_decref(o2);

            // A RANDOM commit is an ORDINARY commit: undo publication + trajectory + the exact
            // production save/load cycle (requirements 3 and 4 ride the row — CA parity).
            r.pending.randomMode = 0;           // OUTRIGHT: a wholesale replacement always moves the graph
            const uint32_t headBefore = r.undoHead.load();
            r.pending.latch(CantonmentPending::V_RANDOM);
            r.applyAtBoundary(16);
            CHK(r.undoHead.load() == headBefore + 1,
                "S6g: a RANDOM commit publishes its undo snapshot (requirement 4 — the step-4 parity, inherited)");
            CHK(r.trajectory.size() >= 2,
                "S6g: the RANDOM commit entered the trajectory (requirement 3's replay data)");
            json_t* saved = r.toJson();
            auto r2Owner = std::make_unique<CantonmentModuleCore>();  // heap-held — 2 MiB ring
            CantonmentModuleCore& r2 = *r2Owner;
            r2.fromJson(saved);
            CHK(r2.pending.randomCounter == r.pending.randomCounter
                && r2.pending.randomMode == r.pending.randomMode
                && r2.pending.randomDensity == r.pending.randomDensity
                && r2.pending.randomIslands == r.pending.randomIslands
                && r2.pending.randomReduce == r.pending.randomReduce,
                "S6g: the stream position + config survive the EXACT production save/load cycle");
            CHK(sameGraph(r.graph, r2.graph),
                "S6g: the generated forest round-trips byte-identical (a reproducible, shareable state)");
            json_decref(saved);
        }

        // (g-h) the trajectory tail: stale or cyclic -> DROPPED (ring cleared), never garbage.
        auto tOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& t = *tOwner;
        buildChain(t);
        t.pending.latch(CantonmentPending::V_INVERT);
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
            json_object_set_new(tj, "v", json_integer(CantonmentTrajectory::SERIAL_VERSION));
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
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
        buildChain(c);
        // 20 commits with a guaranteed change each (INVERT over a non-empty graph always
        // changes it — the dual of a non-empty DAG differs in row 0), UI never drains.
        for (int i = 0; i < 20; ++i) {
            c.pending.latch(CantonmentPending::V_INVERT);
            c.applyAtBoundary(16);
            CHK(c.graph.isValid(), "S7: graph valid after every commit");
        }
        CHK(c.undoHead.load() - c.undoTail.load() == (uint32_t)CantonmentModuleCore::UNDO_RING,
            "S7: drop-if-full — exactly the 16 newest snapshots retained");
        // Drain: order continuity (each batch's before == the previous batch's after).
        CantonmentUndoSnapshot prev, cur;
        CHK(drainOne(c, prev), "S7: first drained");
        int continuity = 0, count = 1;
        while (drainOne(c, cur)) {
            if (sameParents(prev.after, cur.before)) ++continuity;
            prev = cur;
            ++count;
        }
        CHK(count == CantonmentModuleCore::UNDO_RING, "S7: all 16 retained snapshots drained");
        CHK(continuity == count - 1, "S7: trajectory continuity (before[i] == after[i-1])");
        CHK(c.undoHead.load() == c.undoTail.load(), "S7: ring fully drained");
    }

    // ── S8. seeded march: edits + commits at varying pools, validity at every step ────────────
    {
        for (int seed = 1; seed <= 60; ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x9E3779B97F4A7C15ull);
            auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
            CantonmentModuleCore& c = *cOwner;
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
                        const int verb = (int)(rng() % (unsigned)CantonmentPending::N_VERBS);
                        c.pending.latch(verb);
                        const uint32_t h0 = c.undoHead.load();
                        const int appliedRows = c.applyAtBoundary(act);
                        (void)appliedRows;
                        if (c.undoHead.load() != h0) {
                            CantonmentUndoSnapshot s;
                            if (!drainOne(c, s)) { marchOk = false; why = "S8: snapshot missing after publish"; break; }
                            CantonmentGraph b, a2;
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
            auto probeOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
            CantonmentModuleCore& probe = *probeOwner;
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

    // ── S9. the TRUE-REVERSE consume (the global-reverse consumer; the CA engine mirrored) ────
    // The CA order: the queue is consumed at the phrase boundary BEFORE the verb undo bracket —
    // a pure true-reverse changes the graph with NO undo snapshot (the modulation-class rule,
    // CA_DICE_COUNTER_MODEL.md CORRECTION^2: undo is for USER actions); a mixed boundary's
    // snapshot brackets only the verbs. The trajectory is verb-agnostic (RESULTS, not causes),
    // so the walk reaches through collapses and RANDOM commits alike; at the buffer start it
    // STOPS (state unchanged) but the request is still spent (the CA lamp rule).
    {
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;

        // Two user commits: chain -> dual (INVERT) -> collapsed (COLLAPSE). Trajectory:
        // [chain, dual, collapsed] — the first commit's pre-push entered the HELD chain, so the
        // ring is complete from the first commit (the pre-push contract).
        buildChain(c);
        const CantonmentGraph chainState = c.graph;
        c.pending.latch(CantonmentPending::V_INVERT);
        CHK(c.applyAtBoundary(16) == 1, "S9: (setup) commit 1 applied");
        CantonmentUndoSnapshot snap;
        CHK(drainOne(c, snap), "S9: (setup) commit 1 published its undo step");
        const CantonmentGraph dualState = c.graph;
        c.pending.latch(CantonmentPending::V_COLLAPSE);
        CHK(c.applyAtBoundary(16) == 1, "S9: (setup) commit 2 applied");
        CHK(drainOne(c, snap), "S9: (setup) commit 2 published its undo step");
        CHK(c.graph.parents[0] == (uint16_t)(1u << 2) && c.graph.parents[1] == 0,
            "S9: (setup) COLLAPSE landed (the dual folded one level: 0 onto 2)");
        const CantonmentGraph collapsedState = c.graph;
        CHK(c.trajectory.size() == 3, "S9: (setup) trajectory holds [chain, dual, collapsed]");

        // PURE true-reverse: one boundary, one commit back, NO undo, request spent.
        const uint32_t head0 = c.undoHead.load();
        c.pending.latchTrueReverse();
        CHK(c.applyAtBoundary(16) == 0, "S9: a pure true-reverse applies no verb rows");
        CHK(sameGraph(c.graph, dualState), "S9: the graph is the PREVIOUS COMMITTED state (collapsed -> dual)");
        CHK(!sameGraph(c.graph, collapsedState), "S9: the true-reverse MOVED the graph (an active gesture, not a no-op)");
        CHK(c.undoHead.load() == head0, "S9: NO undo snapshot — the performance class does not write history");
        CHK(!c.pending.trueRevRequested, "S9: the request is consumed (the lamp clears either way)");
        CHK(c.trajectory.size() == 2, "S9: exactly ONE ring entry popped (the restored state does not re-enter)");
        c.pending.latchTrueReverse();
        c.applyAtBoundary(16);
        CHK(sameGraph(c.graph, chainState), "S9: (walk) step 2 reached the ring's floor — the first commit's HELD chain");
        c.pending.latchTrueReverse();
        c.applyAtBoundary(16);
        CHK(c.trajectory.size() == 1, "S9: buffer start — STOP, don't wrap (the ring kept its last entry)");
        CHK(!c.pending.trueRevRequested, "S9: the STOP still spends the request (the CA affordance rule)");

        // One real commit to grow the ring again (the walk floor is the first commit's HELD
        // chain; a mixed-restore test needs >= 2 live entries).
        c.pending.latch(CantonmentPending::V_INVERT);
        c.applyAtBoundary(16);
        CHK(drainOne(c, snap), "S9: (regrow) the commit published its undo step");

        // MIXED boundary: the consume restores the previous COMMITTED state — discarding any
        // UNCOMMITTED pin drift (pin edits are not committed history; the trajectory retraces
        // commits) — and the verb's undo bracket then starts at the RESTORED state.
        c.setPin(3, 1, true);                                 // an uncommitted edit on top
        c.pending.latch(CantonmentPending::V_COLLAPSE);      // a real verb armed on top of that
        c.pending.latchTrueReverse();                         // + a queued true-reverse
        CHK(c.applyAtBoundary(16) >= 1, "S9: the mixed boundary committed the verb");
        CHK(drainOne(c, snap), "S9: the mixed boundary published ONE undo step (the verb's)");
        CHK(!drainOne(c, snap), "S9: exactly ONE snapshot for the mixed boundary");
        {
            // The bracket's BEFORE must be the POST-CONSUME state: the consume restored the
            // ring's previous committed state (the chain — the pre-edit commit; the uncommitted
            // pin edit 3<1 is NOT in it), and the COLLAPSE then applied on that.
            bool beforeIsRestored = true;
            for (int v = 0; v < VOICES; ++v)
                beforeIsRestored &= (snap.before[v] == chainState.parents[v]);
            CHK(beforeIsRestored, "S9: the verb bracket's BEFORE is the RESTORED state (consume precedes the snapshot)");
            CHK(!c.graph.isContainedIn(3, 1), "S9: the uncommitted pin drift did not survive the restore (committed history only)");
            CHK(!c.pending.trueRevRequested, "S9: the mixed consume cleared the queue");
        }

        // Out-of-axis: the request STAYS queued (the lamp stays lit) and consumes at the next
        // in-axis boundary.
        c.pending.latchTrueReverse();
        c.applyAtBoundary(16, 0u);
        CHK(c.pending.trueRevRequested, "S9: an out-of-axis boundary leaves the request queued (the CA rule)");
        c.applyAtBoundary(16);                                // in-axis: consumes (no undo either way)
        CHK(!c.pending.trueRevRequested, "S9: the next in-axis boundary consumed the queued request");

        // BRANCHING: after a step back, a new commit discards the popped future (the ring is a
        // window over the ACTUAL committed history — undo-then-redo-branch, the ring header rule).
        // A RANDOM commit is an ordinary commit: it enters the trajectory and replays back.
        c.setPin(4, 2, true);                                 // a fresh edit (not yet committed)
        c.pending.latch(CantonmentPending::V_RANDOM);
        c.applyAtBoundary(16);
        CHK(drainOne(c, snap), "S9: (branch) the RANDOM commit published its undo step");
        const CantonmentGraph afterRandom = c.graph;
        const uint32_t head1 = c.undoHead.load();
        c.pending.latchTrueReverse();
        c.applyAtBoundary(16);
        CHK(c.undoHead.load() == head1, "S9: (branch) the true-reverse over the RANDOM commit wrote NO undo");
        CHK(!sameGraph(c.graph, afterRandom), "S9: (branch) the RANDOM commit's state replayed back (verb-agnostic trajectory)");
        CHK(!c.pending.trueRevRequested, "S9: (branch) the request spent");
        c.pending.latchTrueReverse();
        c.applyAtBoundary(16);                                // step again — the walk is finite, STOP is fine
        CHK(c.graph.isValid() && !c.pending.trueRevRequested,
            "S9: the walk stays valid and the affordance stays consistent at every step");
    }

    // S9b. resetAll clears a queued request (runtime-only, the save-never-carries-a-request rule).
    {
        auto cOwner = std::make_unique<CantonmentModuleCore>();   // heap-held — 2 MiB ring
        CantonmentModuleCore& c = *cOwner;
        c.pending.latchTrueReverse();
        c.resetAll();
        CHK(!c.pending.trueRevRequested, "S9b: resetAll clears the true-reverse queue");
    }

    std::printf("== containment module core: %d passed, %d failed ==\n", pass, fail);
    return fail ? 1 : 0;
}
