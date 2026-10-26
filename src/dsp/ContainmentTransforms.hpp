#pragma once
// ContainmentTransforms — the DAG-closed gesture verbs over ContainmentGraph.
// Authority: CC_PROMPT_CONTAINMENT_MODULE_SPEC.md (gesture-vs-edit split: transforms are
// no/few-param TRIGGERED gestures, applied to the CURRENT graph so repeated triggers march —
// the CA model), AMENDED by Rodney's rulings (2026-10-26): multi-parent DAG + intersection
// semantics; disconnected vertices valid; EXPAND = trajectory memory (module layer, not here).
//
// PRINCIPLE (the spec's, upgraded to DAGs): a containment transform is admissible IFF it is an
// endomorphism on acyclic graphs. Each verb below carries its closure argument inline; the
// property test (seeded random DAGs x every verb -> assert acyclic + no self-bits) re-proves
// them all in CI on every run — the test is the guarantee.
//
// ACTIVE-POOL DISCIPLINE (the CA convention): only rows < activeCount are transformed; rows
// >= activeCount are LEFT UNTOUCHED (manual pins persist beyond the live pool). Parent bits
// pointing at voices >= activeCount are likewise left untouched (they reference voices outside
// the permuted pool — e.g. pins that outlived a polyphony reduction). ROTATE/REFLECT remap
// within [0, activeCount) and pass higher bits through unchanged.
//
// Pure / rack-free / header-only (the ChangeAlleyTransforms.hpp discipline).

#include <cstdint>
#include <cstring>
#include "ContainmentGraph.hpp"

namespace dotModular { namespace ct {

// Mirrors ca::clampStep (ChangeAlleyTransforms.hpp): 0 and span are no-ops, so the config
// cannot select nothing; coprime steps traverse the whole pool, steps sharing a factor
// produce sub-cycles. Local copy — ct does not depend on CA's header.
inline int clampStep(int step, int span) {
    if (span <= 1) return 0;
    int st = step % span;
    if (st < 0) st += span;
    return (st == 0) ? 1 : st;
}

// Bit u of mask -> bit map[u]. Bits whose target equals the source stay (fixpoints are fine —
// a voice may keep its role under a rotation step that happens to fix it).
inline uint16_t remapBits(uint16_t mask, const int* map) {
    uint16_t out = 0;
    for (int u = 0; u < VOICES; ++u)
        if ((mask >> u) & 1u) out |= (uint16_t)(1u << map[u]);
    return out;
}

// Build the active-pool permutation with IDENTITY above it (bits >= active pass through).
// forward shift: role of voice v moves to voice (v + step) mod active.
inline void buildShiftMap(int activeCount, int step, int* map) {
    const int st = clampStep(step, activeCount);
    for (int v = 0; v < VOICES; ++v)
        map[v] = (v < activeCount) ? ((v + st) % activeCount) : v;
}

inline void buildMirrorMap(int activeCount, int* map) {
    for (int v = 0; v < VOICES; ++v)
        map[v] = (v < activeCount) ? (activeCount - 1 - v) : v;
}

// ── ROTATE ROLES (<=1 param: step, default 1) ─────────────────────────────────────────────
// Similitude: conjugate the graph by the pool shift pi — new_parents[pi(v)] = pi(parents[v]).
// The nesting SHAPE is identical; the voices rotate through it ("the hierarchy rotates through
// the voices"). CLOSED: conjugation by a bijection is a graph isomorphism; if the new graph had
// a cycle or a self-bit, applying pi^-1 would map it back into the old graph — contradiction.
inline void rotateRoles(ContainmentGraph& g, int activeCount, int step) {
    int map[VOICES];
    buildShiftMap(activeCount, step, map);
    uint16_t out[VOICES];
    for (int v = 0; v < VOICES; ++v) out[map[v]] = remapBits(g.parents[v], map);
    for (int v = 0; v < VOICES; ++v) g.parents[v] = out[v];
}

// ── REFLECT ROLES (0 params) ───────────────────────────────────────────────────────────────
// The similitude mirror: conjugation by v -> activeCount-1-v. Same isomorphism argument as
// ROTATE (a reflection is a bijection) — closed. Self-inverse as a permutation, so a double
// trigger restores the original role assignment.
inline void reflectRoles(ContainmentGraph& g, int activeCount) {
    int map[VOICES];
    buildMirrorMap(activeCount, map);
    uint16_t out[VOICES];
    for (int v = 0; v < VOICES; ++v) out[map[v]] = remapBits(g.parents[v], map);
    for (int v = 0; v < VOICES; ++v) g.parents[v] = out[v];
}

// ── INVERT (0 params) ──────────────────────────────────────────────────────────────────────
// Full edge reversal: every containment edge v->u becomes u->v ("turn the nesting inside
// out"); the most-contained voice(s) become root(s). CLEAN BECAUSE MULTI-PARENT (Rodney's
// ruling): the dual of a single-parent forest is only a forest when it is a chain, but the
// dual of any DAG is a DAG — a cycle in the reversed edges would imply a cycle in the forward
// edges. No canonical tie-break rule is needed anywhere.
inline void invert(ContainmentGraph& g) {
    uint16_t out[VOICES] = {};
    for (int v = 0; v < VOICES; ++v) {
        uint16_t m = g.parents[v];
        while (m) {
            const int u = __builtin_ctz(m);
            m &= (uint16_t)(m - 1);
            out[u] |= (uint16_t)(1u << v);
        }
    }
    // Self-bits cannot appear: old bit v of parents[v] is always 0, so no write ever targets
    // out[v] from v itself.
    for (int v = 0; v < VOICES; ++v) g.parents[v] = out[v];
}

// ── COLLAPSE (0 params) ────────────────────────────────────────────────────────────────────
// Skip one nesting level: new_parents[v] = UNION of parents[u] over all u in parents[v]
// (grandparent re-link). Multi-parent makes this TOTAL — the OR-union needs no tie-break
// (the single-parent world needed a "lowest child wins" rule; that rule is gone with R1).
// CLOSED, two ways:
//   1. Every new edge v->g was a length-2 old path (v->u->g), so the new reachability is a
//      subset of the old — the old graph is acyclic, so the new one is.
//   2. No self-bit can appear: bit v in parents[u] means u ⊂ v, while u in parents[v] means
//      v ⊂ u — both at once would be the 2-cycle the invariants forbid.
// Composes per trigger: each application shortens the longest chain by >= 1; the march flattens
// toward the all-stars fixpoint (roots + direct children only).
inline void collapse(ContainmentGraph& g) {
    uint16_t out[VOICES];
    for (int v = 0; v < VOICES; ++v) {
        uint16_t m = g.parents[v], acc = 0;
        while (m) {
            const int u = __builtin_ctz(m);
            m &= (uint16_t)(m - 1);
            acc |= g.parents[u];
        }
        out[v] = acc;
    }
    for (int v = 0; v < VOICES; ++v) g.parents[v] = out[v];
}

// COLLAPSE-ALL: flatten each voice ONTO ITS ROOTS — new_parents[v] = the roots reachable
// upward from v (the ancestor closure intersected with the root mask). The spec's words are
// "flatten a subtree TOWARD ITS ROOT", and the landing arrangement is the authority doc's
// FLAT-SINGLE-ROOT (the universal-envelope case): 0<1<2<3 becomes 0,1,2 all direct children
// of 3 — NOT everyone detached. (Iterating the one-level collapse instead marches to the
// EMPTY graph — the wrong gesture.) Idempotent by construction: after flattening, each
// voice's closure-roots are exactly its parents. CLOSED: every new edge points at a former
// root (no parents), so the result has depth <= 1 — trivially acyclic, and a self-bit cannot
// appear (v reaches itself only on a cycle, which the invariants forbid).
inline void collapseAll(ContainmentGraph& g) {
    uint16_t roots = 0;
    for (int v = 0; v < VOICES; ++v)
        if (g.isRoot(v)) roots |= (uint16_t)(1u << v);
    uint16_t out[VOICES];
    for (int v = 0; v < VOICES; ++v)
        out[v] = (uint16_t)(g.ancestorClosure(v) & roots);
    for (int v = 0; v < VOICES; ++v) g.parents[v] = out[v];
}

// ── EXPAND — NOT HERE (R3) ─────────────────────────────────────────────────────────────────
// Restore the level the last COLLAPSE removed. Collapse is LOSSY (many graphs collapse to the
// same graph), so expand is NOT a synthesized inverse — it is trajectory MEMORY: the module
// layer keeps the CA TRUE-REVERSE precedent (a ring of COMMITTED graph states pushed at each
// phrase-boundary commit; EXPAND pops one). The seam is reserved; the ring lands with the
// pending machinery (CC_PROMPT_CONTAINMENT_BUILD_PLAN.md, step 3/follow-up).

}} // namespace dotModular::ct
