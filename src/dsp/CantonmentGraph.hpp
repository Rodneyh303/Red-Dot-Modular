#pragma once
// CantonmentGraph — the articulation-envelope routing state.
// Authority: CC_PROMPT_CANTONMENT_MODULE_SPEC.md, AMENDED by Rodney's rulings (2026-10-26):
//   R1. MULTI-PARENT ACYCLIC DAG is the state space; containment = the INTERSECTION of the
//       parent containments (gate(v) = clamp(own, ∩ resolved parents)). Supersedes the spec's
//       single-parent forest invariant and the authority doc's "DELIBERATELY EXCLUDED
//       multi-parent" section (the deferred niche is claimed deliberately).
//   R2. DISCONNECTED VERTICES ARE VALID — any acyclic graph is a valid state. Spanning is NOT
//       required: isolated voices (no parents, no children) and separate components coexist
//       freely. The all-zeros default IS the all-disconnected graph = the full-equality case
//       ("mono not special": V1 is one containable voice among 16, nothing privileged).
//   R3. EXPAND restores the level the last COLLAPSE removed, via the CA TRUE-REVERSE precedent
//       (committed-state trajectory memory) — NOT a synthesized inverse (collapse is lossy).
//       Implemented in the module layer where the trajectory ring lives (see the build plan);
//       this header holds the lossless pure verbs only.
//
// WHAT DOES NOT CHANGE: containment is CYCLE-FATAL. It COMPOSES derived gates up a dependency
// chain (needs evaluation order + a base case); correlation only READS pre-existing draws
// (order-free, cycle-safe — that is why CA may modulate into cyclic states and we never may).
// A cycle has no base case -> the recursion does not terminate. Acyclicity is enforced at every
// edit AND every transform; the DAG keeps the recursion well-founded (a topological order exists
// iff the graph is acyclic; roots are the base case).
//
// Pure / rack-free / header-only so the standalone replica tests compile it directly (the
// ChangeAlleyTransforms.hpp discipline: no Rack SDK, no deps beyond <cstdint>/<cstring>).
//
// STATE REPRESENTATION: parents[16] bitmask. Bit u of parents[v] set = "voice v is contained
// in voice u". Capacity 15 parents per voice (self-bit permanently 0). This is the natural
// encoding for R1: COLLAPSE is a bitwise OR-union, INVERT is a matrix transpose, and the
// ancestor check is a bitmask wavefront. Serialization = the raw uint16 array (16 words).

#include <cstdint>
#include <cstring>

namespace dotModular { namespace ct {

static constexpr int VOICES = 16;

struct CantonmentGraph {
    // Bit u of parents[v] = v is contained in u. Zero-initialized = uncontained (R2 default).
    uint16_t parents[VOICES] = {};

    // ── predicates ────────────────────────────────────────────────────────────
    bool isContainedIn(int v, int u) const {
        return (parents[v] >> u) & 1u;
    }

    int parentCount(int v) const {
        return __builtin_popcount(parents[v]);
    }

    bool isRoot(int v) const { return parents[v] == 0; }

    // Bitmask of all voices that contain no one and are contained by no one (isolated, R2).
    uint16_t isolatedMask() const {
        uint16_t contained = 0;   // voices that ARE contained by someone
        uint16_t contains = 0;    // voices that contain someone
        for (int v = 0; v < VOICES; ++v) {
            contained |= parents[v];
            contains |= (parents[v] != 0) ? (uint16_t)(1u << v) : 0u;
        }
        return (uint16_t)~(contained | contains);
    }

    // FULL ancestor closure of v: every voice reachable upward via parent edges (v's parents,
    // their parents, ...). Excludes v itself unless v is on a cycle (which the invariants
    // forbid). Used by the edit check, the property tests, and (later) the decision manager's
    // topological evaluation. Returns the closure mask; *depth (optional) = wavefront ROUNDS =
    // the max SHORTEST upward distance — NOT the longest chain: with multi-parent a direct
    // edge can shadow a longer path (0->2->8 alongside 0->8 reads depth 1). The property
    // harness computes the true longest chain with a memoized DP when it needs one.
    uint16_t ancestorClosure(int v, int* depth = nullptr) const {
        uint16_t closure = 0, frontier = parents[v];
        int d = 0;
        while (frontier) {
            closure |= frontier;
            ++d;
            uint16_t next = 0;
            for (int w = 0; w < VOICES; ++w)
                if ((frontier >> w) & 1u) next |= parents[w];
            frontier = (uint16_t)(next & ~closure);
        }
        if (depth) *depth = d;
        return closure;
    }

    // ── the edit checks (R1: reject anything that would break a DAG) ──────────
    // True if setting v ⊂ u would close a cycle: v is in u's ancestor closure (the new edge
    // v->u completes v->u->…->v). Bitmask wavefront — <= 16 rounds of 16-bit ORs, EDIT rate
    // only, never per-sample.
    bool wouldCycle(int v, int u) const {
        if (v == u) return true;                       // self-containment
        return (ancestorClosure(u) >> v) & 1u;
    }

    // Full validation (deserialize guard + property tests): no self-bits, acyclic.
    // Acyclicity via Kahn peel: repeatedly strip parent-less voices; a DAG strips all 16.
    bool isValid() const {
        for (int v = 0; v < VOICES; ++v)
            if ((parents[v] >> v) & 1u) return false;
        uint16_t remaining = 0;
        for (int v = 0; v < VOICES; ++v) remaining |= (uint16_t)(1u << v);
        bool stripped[VOICES] = {};
        for (int round = 0; round < VOICES; ++round) {
            bool progressed = false;
            for (int v = 0; v < VOICES; ++v) {
                if (stripped[v]) continue;
                if ((uint16_t)(parents[v] & remaining) == 0) {
                    stripped[v] = true;
                    remaining &= (uint16_t)~(1u << v);
                    progressed = true;
                }
            }
            if (!progressed) break;
        }
        return remaining == 0;
    }

    // ── edits (mutating; each returns false and changes NOTHING when rejected) ──
    // addParent: v becomes contained in u. Rejected (no change) when u == v or when v is in
    // u's ancestor closure. Removing/adding is the USER's deliberate routing; transforms are
    // the only other writers, and every transform is closure-proven DAG-preserving.
    bool addParent(int v, int u) {
        if (wouldCycle(v, u)) return false;
        if (isContainedIn(v, u)) return true;          // idempotent re-pin = accepted no-op
        parents[v] |= (uint16_t)(1u << u);
        return true;
    }

    void removeParent(int v, int u) {
        parents[v] &= (uint16_t)~(1u << u);            // detaching can never cycle
    }

    void clearParents(int v) {
        parents[v] = 0;                                // detach v to an isolated root (R2)
    }

    // ── serialization (the array IS the state; helpers for json round-trip identity) ──
    void serializeTo(uint16_t* out) const {
        for (int v = 0; v < VOICES; ++v) out[v] = parents[v];
    }
    // Deserialize with validation: on an invalid blob (corrupt save / hand-edited json) reset
    // to the all-disconnected default rather than admit a cycle into the engine. Returns false
    // when the input was rejected.
    bool deserializeFrom(const uint16_t* in) {
        CantonmentGraph candidate;
        for (int v = 0; v < VOICES; ++v) candidate.parents[v] = in[v];
        if (!candidate.isValid()) return false;
        *this = candidate;
        return true;
    }
};

}} // namespace dotModular::ct
