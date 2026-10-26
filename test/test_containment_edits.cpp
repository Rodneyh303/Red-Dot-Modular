/**
 * test_containment_edits.cpp — Containment step 1: edit checks + serialization
 * (CC_PROMPT_CONTAINMENT_BUILD_PLAN.md §TESTS: "self reject, ancestor-reject (wavefront),
 * valid accept, remove/clear, autosave round-trip identity, default all-zeros").
 *
 * The graph is pure state (multi-parent acyclic DAG, R1) whose invariants are enforced at
 * EDIT rate: self-loop reject, ancestor-closure wavefront reject (never per-sample — the
 * check is O(16 rounds of 16-bit ORs) and only runs when the user pins). remove/clear are
 * ALWAYS valid (detaching cannot cycle). Serialization is the RAW uint16[16] array — no
 * bit-packing, no derived fields, the same 16 words in and out (the WARPDIAG lesson) —
 * with a reject-invalid-blob guard so a corrupt or hand-edited save can never promote a
 * cycle into the editor state (falls back to the R2 all-disconnected default).
 *
 * Seeded-Philox random-DAG march: every accepted edit must leave the graph valid (acyclic
 * + no self-bits) — the edit-level slice of the plan's "the test is the closure proof in
 * CI" principle. The transform property harness (step 2) shares this generator idiom.
 *
 * Compile: g++ -std=c++20 -Itest -Isrc test/test_containment_edits.cpp -o /tmp/t_cte && /tmp/t_cte
 * (registered in test/run_all.sh as header-only; PhiloxRng is rack-free)
 */
#include "../src/dsp/ContainmentGraph.hpp"
#include "../src/dsp/PhiloxRng.hpp"
#include <cstdio>
#include <cstring>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

using namespace dotModular::ct;

static int totalEdges(const ContainmentGraph& g) {
    int n = 0;
    for (int v = 0; v < VOICES; ++v) n += __builtin_popcount(g.parents[v]);
    return n;
}
static bool sameState(const ContainmentGraph& a, const ContainmentGraph& b) {
    return std::memcmp(a.parents, b.parents, sizeof(a.parents)) == 0;
}
// Rejection is TOTAL: a refused edit must leave all 16 words byte-identical.
static bool unchangedAfterReject(const ContainmentGraph& before, const ContainmentGraph& after) {
    return sameState(before, after);
}

int main() {
    // ── S0. default state = the R2 all-disconnected graph ─────────────────────
    {
        ContainmentGraph g;
        bool allZero = true;
        for (int v = 0; v < VOICES; ++v) allZero &= (g.parents[v] == 0);
        CHK(allZero, "default: all 16 parent words zero");
        CHK(g.isValid(), "default: valid (vacuously acyclic, no self-bits)");
        bool allRoot = true;
        for (int v = 0; v < VOICES; ++v) allRoot &= g.isRoot(v);
        CHK(allRoot, "default: every voice is a root");
        CHK(g.isolatedMask() == 0xFFFF, "default: all 16 voices isolated (R2 full-equality case)");
        bool closuresEmpty = true;
        for (int v = 0; v < VOICES; ++v) closuresEmpty &= (g.ancestorClosure(v) == 0);
        CHK(closuresEmpty, "default: every ancestor closure empty");
        uint16_t raw[VOICES];
        g.serializeTo(raw);
        bool rawZero = true;
        for (int v = 0; v < VOICES; ++v) rawZero &= (raw[v] == 0);
        CHK(rawZero, "default: serialized autosave blob is 16 zero words");
    }

    // ── S1. self-loop reject ──────────────────────────────────────────────────
    {
        ContainmentGraph g;
        ContainmentGraph before = g;
        CHK(g.wouldCycle(7, 7), "self: wouldCycle(v,v) true on empty graph");
        CHK(!g.addParent(7, 7), "self: addParent(v,v) rejected");
        CHK(unchangedAfterReject(before, g), "self: rejection changed nothing");
        // even with other edges present, the self test is structural, not closure-based
        CHK(g.addParent(1, 0), "self: setup edge accepted");
        before = g;
        CHK(g.wouldCycle(1, 1), "self: wouldCycle(v,v) true on non-empty graph");
        CHK(!g.addParent(1, 1), "self: addParent(v,v) rejected on non-empty graph");
        CHK(unchangedAfterReject(before, g), "self: rejection changed nothing (non-empty)");
    }

    // ── S2. valid accept + the classic 2-cycle reject ─────────────────────────
    {
        ContainmentGraph g;
        CHK(g.addParent(1, 0), "accept: addParent(1,0) accepted");
        CHK(g.isContainedIn(1, 0), "accept: isContainedIn(1,0)");
        CHK(g.parentCount(1) == 1, "accept: voice 1 has exactly one parent");
        CHK(!g.isRoot(1) && g.isRoot(0), "accept: 1 is nested, 0 is its root");
        CHK(g.isolatedMask() == 0xFFFC, "accept: isolated mask drops 0 and 1");
        CHK(g.isValid(), "accept: single edge valid");
        ContainmentGraph before = g;
        CHK(g.wouldCycle(0, 1), "2-cycle: 0 is in closure(1) after 1->0");
        CHK(!g.addParent(0, 1), "2-cycle: addParent(0,1) rejected (would close 1->0->1)");
        CHK(unchangedAfterReject(before, g), "2-cycle: rejection changed nothing");
    }

    // ── S3. deep chain: the wavefront must see multi-hop ancestors ────────────
    {
        ContainmentGraph g;
        bool built = true;
        for (int k = 0; k < VOICES - 1; ++k) built &= g.addParent(k + 1, k); // 1 in 0 ... 15 in 14
        CHK(built, "chain: 15-deep chain built entirely via accepted edits");
        int depth = -1;
        uint16_t cl = g.ancestorClosure(15, &depth);
        CHK(cl == 0x7FFF, "chain: closure(15) = all 14 ancestors below it");
        CHK(depth == 15, "chain: closure depth = longest chain = 15");
        bool noSelf = (cl & (1u << 15)) == 0;
        CHK(noSelf, "chain: closure excludes v itself on valid graphs");
        ContainmentGraph before = g;
        CHK(g.wouldCycle(0, 15), "chain: wouldCycle(0,15) sees the 15-hop path (wavefront, not one-level)");
        CHK(!g.addParent(0, 15), "chain: closing the chain into a ring REJECTED");
        CHK(unchangedAfterReject(before, g), "chain: rejection changed nothing");
        // mid-chain re-parent: 8 already in 7; add 8 in 3 (an ancestor) — R1 multi-parent
        CHK(g.addParent(8, 3), "multi: addParent(8,3) accepted (8 gains ancestor 3 as 2nd parent)");
        CHK(g.parentCount(8) == 2, "multi: voice 8 now has two parents");
        before = g;
        CHK(g.wouldCycle(3, 8), "multi: wouldCycle(3,8) via 8->3 direct edge");
        CHK(!g.addParent(3, 8), "multi: addParent(3,8) rejected (8 is already 3's child)");
        CHK(unchangedAfterReject(before, g), "multi: rejection changed nothing");
    }

    // ── S4. diamond: the wavefront OR-unions across branches ──────────────────
    {
        ContainmentGraph g;
        bool built = g.addParent(3, 1) && g.addParent(3, 2)   // 3 in 1 AND 3 in 2
                   && g.addParent(1, 0) && g.addParent(2, 0); // both branches root at 0
        CHK(built, "diamond: multi-parent diamond built (3 in {1,2}, both in 0)");
        CHK(g.ancestorClosure(3) == 0x0007, "diamond: closure(3) = {1,2,0} across BOTH branches");
        ContainmentGraph before = g;
        CHK(g.wouldCycle(0, 3), "diamond: wouldCycle(0,3) — 0 reached via both branches");
        CHK(!g.addParent(0, 3), "diamond: rooting 0 under 3 rejected (closes the diamond)");
        CHK(unchangedAfterReject(before, g), "diamond: rejection changed nothing");
    }

    // ── S5. idempotent re-pin is an accepted no-op ────────────────────────────
    {
        ContainmentGraph g;
        g.addParent(1, 0);
        int edges = totalEdges(g);
        CHK(g.addParent(1, 0), "idempotent: re-pin returns true (accepted no-op)");
        CHK(totalEdges(g) == edges, "idempotent: edge count unchanged");
    }

    // ── S6. remove/clear ALWAYS valid (detaching cannot cycle) ────────────────
    {
        ContainmentGraph g;
        g.addParent(1, 0);
        g.addParent(2, 1);
        g.addParent(2, 0);          // 2 in {1,0}
        g.removeParent(2, 1);
        CHK(!g.isContainedIn(2, 1) && g.isContainedIn(2, 0), "remove: drops exactly the named edge");
        ContainmentGraph before = g;
        g.removeParent(9, 4);       // edge that never existed
        CHK(unchangedAfterReject(before, g), "remove: removing a non-edge is a valid no-op");
        g.clearParents(2);
        CHK(g.parents[2] == 0 && g.isRoot(2), "clear: multi-parent row detaches to a root");
        CHK((g.isolatedMask() >> 2) & 1u, "clear: detached voice re-enters the isolated set");
        before = g;
        g.clearParents(3);          // already a root
        CHK(unchangedAfterReject(before, g), "clear: clearing an empty row is a valid no-op");
        CHK(g.isValid(), "remove/clear: graph still valid after detachments");
    }

    // ── S7. seeded-Philox random-DAG march + RAW round-trip identity ──────────
    // The autosave contract (WARPDIAG lesson): serializeTo -> deserializeFrom is the
    // IDENTITY on every reachable state, and the state stays valid at every step.
    {
        int badValid = -1, badRt = -1, badSelfRt = -1, badEdgeCount = -1, badRemoveValid = -1;
        int builtDags = 0;
        for (int seed = 1; seed <= 600 && badValid < 0; ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x9E3779B97F4A7C15ull);
            ContainmentGraph g;
            for (int attempt = 0; attempt < 80; ++attempt) {
                int v = (int)(rng() & 15u);
                int u = (int)(rng() & 15u);
                if (rng() & 1u) {
                    ContainmentGraph snap = g;
                    bool accepted = g.addParent(v, u);
                    if (accepted) {
                        if (!g.isValid()) { badValid = seed; break; }
                    } else {
                        if (!unchangedAfterReject(snap, g)) { badValid = seed; break; }
                    }
                } else {
                    g.removeParent(v, u);        // ALWAYS valid, even non-edges
                    if (!g.isValid()) { badValid = seed; break; }
                }
            }
            if (badValid >= 0) break;
            if (totalEdges(g) > 0) ++builtDags;
            // round-trip into a fresh graph
            uint16_t raw[VOICES];
            g.serializeTo(raw);
            ContainmentGraph g2;
            if (!g2.deserializeFrom(raw) || !sameState(g, g2)) badRt = seed;
            // round-trip into ITSELF (the actual autosave flow: save, then load back over self)
            ContainmentGraph g3 = g;
            if (!g3.deserializeFrom(raw) || !sameState(g, g3)) badSelfRt = seed;
            // edge count survives verbatim
            if (totalEdges(g2) != totalEdges(g)) badEdgeCount = seed;
            // removal after round-trip keeps validity (detaching cannot cycle — march it)
            ContainmentGraph g4 = g2;
            for (int v = 0; v < VOICES; ++v) g4.clearParents(v);
            if (!g4.isValid() || totalEdges(g4) != 0) badRemoveValid = seed;
        }
        if (badValid   >= 0) std::printf("  detail: validity broke at seed %d\n", badValid);
        if (badRt      >= 0) std::printf("  detail: round-trip identity broke at seed %d\n", badRt);
        if (badSelfRt  >= 0) std::printf("  detail: self round-trip broke at seed %d\n", badSelfRt);
        if (badEdgeCount >= 0) std::printf("  detail: edge-count drift at seed %d\n", badEdgeCount);
        if (badRemoveValid >= 0) std::printf("  detail: post-roundtrip clear broke at seed %d\n", badRemoveValid);
        CHK(builtDags > 300, "march: the generator actually built non-trivial DAGs");
        CHK(badValid < 0, "march: isValid holds after EVERY accepted edit / removal (600 seeds)");
        CHK(badRt < 0, "march: serializeTo -> deserializeFrom(fresh) is the identity");
        CHK(badSelfRt < 0, "march: serializeTo -> deserializeFrom(self) is the identity (autosave flow)");
        CHK(badEdgeCount < 0, "march: edge count survives the round-trip verbatim");
        CHK(badRemoveValid < 0, "march: clears after a round-trip stay valid (detach cannot cycle)");
    }

    // ── S8. deserialize reject-invalid-blob guard ─────────────────────────────
    // Cycles are UNREACHABLE via the edit API — that is exactly why the deserialize guard
    // must exist: a hand-edited or corrupt save is the only writer that can propose one.
    {
        ContainmentGraph g;
        for (int k = 0; k < VOICES - 1; ++k) g.addParent(k + 1, k);   // the 15-chain
        ContainmentGraph original = g;
        uint16_t raw[VOICES];
        g.serializeTo(raw);

        // planted self-bit
        uint16_t corrupt[VOICES];
        std::memcpy(corrupt, raw, sizeof(raw));
        corrupt[5] |= (uint16_t)(1u << 5);
        ContainmentGraph target = original;
        CHK(!target.deserializeFrom(corrupt), "guard: planted self-bit REJECTED");
        CHK(sameState(target, original), "guard: rejected blob left the state untouched");

        // planted 2-cycle (0 and 1 mutually contain)
        std::memcpy(corrupt, raw, sizeof(raw));
        corrupt[0] |= 1u << 1;  // 0 in 1
        corrupt[1] |= 1u << 0;  // 1 in 0  (already) — ring of 2
        target = original;
        CHK(!target.deserializeFrom(corrupt), "guard: planted 2-cycle REJECTED");
        CHK(sameState(target, original), "guard: 2-cycle rejection left the state untouched");

        // planted 3-cycle among 4,5,6 (edges 6->5, 5->4 already exist in the chain)
        std::memcpy(corrupt, raw, sizeof(raw));
        corrupt[4] |= 1u << 6;  // 4 in 6 closes 4->5->6->4? 4 in 5, 5 in 6 existed; add 4 in 6 -> ring
        target = original;
        CHK(!target.deserializeFrom(corrupt), "guard: planted 3-cycle REJECTED");
        CHK(sameState(target, original), "guard: 3-cycle rejection left the state untouched");

        // recovery: after rejections, a VALID blob still loads
        target = ContainmentGraph{};
        raw[7] |= 1u << 0;      // 7 additionally in 0 — still acyclic (multi-parent)
        CHK(target.deserializeFrom(raw), "guard: valid multi-parent blob accepted after rejects");
        CHK(target.parentCount(7) == 2, "guard: loaded blob carries the added second parent verbatim");

        // all-zeros blob into a non-default graph = the legitimate "clear everything" load
        ContainmentGraph dirty = original;
        uint16_t zeros[VOICES] = {};
        CHK(dirty.deserializeFrom(zeros), "guard: all-zeros blob accepted");
        CHK(totalEdges(dirty) == 0 && dirty.isolatedMask() == 0xFFFF,
            "guard: all-zeros blob lands on the R2 all-disconnected default");
    }

    // ── S9. isValid direct probes (the deserialize guard's inner oracle) ──────
    {
        ContainmentGraph g;
        CHK(g.isValid(), "oracle: empty graph valid");
        g.parents[9] = (uint16_t)(1u << 9);            // self-bit, hand-written
        CHK(!g.isValid(), "oracle: self-bit invalid");
        g.parents[9] = 0;
        g.parents[0] = 1u << 1;  g.parents[1] = 1u << 0;
        CHK(!g.isValid(), "oracle: 2-cycle invalid");
        g.parents[0] = 0; g.parents[1] = 0;
        g.parents[2] = 1u << 3;  g.parents[3] = 1u << 4;  g.parents[4] = 1u << 2;
        CHK(!g.isValid(), "oracle: 3-cycle invalid");
        for (int v = 0; v < VOICES; ++v) g.parents[v] = 0;
        for (int k = 0; k < VOICES - 1; ++k) g.addParent(k + 1, k);
        CHK(g.isValid(), "oracle: 15-deep chain valid (Kahn peel strips all 16)");
    }

    std::printf(fail ? "\n%d passed, %d FAILED\n" : "\n%d passed, 0 failed\n", pass, fail);
    return fail ? 1 : 0;
}
