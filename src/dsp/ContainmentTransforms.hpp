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
// ACTIVE-POOL DISCIPLINE (the CA convention; the plan's formula new_parents[pi(v)] =
// pi_bits(parents[v]) with pi = identity above the pool): rows >= activeCount KEEP their row
// slot and their above-pool bits — an all-above row is byte-untouched (manual pins that
// outlived a polyphony reduction persist). A dead row's pin INTO the live pool conjugates
// with the pool: it follows the ROLE, not the bare voice (that is what makes the whole gesture
// the isomorphism below). Parent bits pointing at voices >= activeCount likewise pass through
// unchanged. ROTATE/REFLECT remap within [0, activeCount) and pass higher bits through.
//
// Pure / rack-free / header-only (the ChangeAlleyTransforms.hpp discipline).

#include <cstdint>
#include <cstring>
#include "ContainmentGraph.hpp"
#include "PhiloxRng.hpp"   // the RANDOM draws' addressable core (rack-free; simde via RACK_DIR in tests)

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
// Composes per trigger: each application strictly shortens the longest chain (d_new <=
// floor(d_old/2): every old 2-hop path becomes a new 1-hop edge). Iterated, the march
// therefore terminates on the EMPTY graph — the unique fixpoint — within <= 16 applications.
// The spec's "flatten a subtree TOWARD ITS ROOT" landing is collapseAll's job (below), NOT
// iterated collapse (the step-0 smoke's semantic catch; pinned by test_containment_transforms).
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
//
// DERIVED (the 2026-10-27 anchor ruling, IDEAS_PARKED.md): with anchors = the derived root
// mask the anchored form's membership key always holds (every upward path in a DAG terminates
// at a parentless voice), so this IS collapseAllOnto(g, roots) — byte-identical to the
// pre-anchoring body, compat-pinned per seed in test_containment_transforms. One
// implementation; the shipped gesture is the same function it always was.
inline void collapseAllOnto(ContainmentGraph& g, uint16_t anchors);   // the anchored general, below

inline void collapseAll(ContainmentGraph& g) {
    uint16_t roots = 0;
    for (int v = 0; v < VOICES; ++v)
        if (g.isRoot(v)) roots |= (uint16_t)(1u << v);
    collapseAllOnto(g, roots);
}

// ── COLLAPSE-ALL-ONTO (1 param: the anchor mask — the closure gesture's missing 10%) ───────
// The 2026-10-27 ruling (Rodney: "should we have a closure gesture? should collapseAll take
// parameters?"): NO third closure verb — COLLAPSE and COLLAPSE-ALL already span the axis
// (one level / all the way), and a distinct CLOSER differing only by its mask would be
// synonymy, not a new decision. The missing generality lands HERE instead, as ONE parameter
// on the flatten (the ROTATE "≤1 param: step, default 1" precedent): flatten every voice
// UNDER the anchor set ONTO the set —
//     new_parents[v] = (closure(v) ∩ anchors)  when that is non-empty,
//     parents[v] kept VERBATIM                 otherwise.
// The membership key is load-bearing: the naive mask form (out[v] = closure(v) & A for ALL v)
// would silently DETACH any voice whose ancestors miss A — 0<1<2<3 plus a separate 9<10 with
// A={3} would orphan 9. Detaching is the edit verbs' deliberate business, never a flatten's
// side effect. The anchor's own containment is untouched (A={2} in 0<1<2<3 keeps 2<3:
// "flatten the cluster under 2 onto 2" — the upstream wiring survives).
//
// CLOSED, mask-uniform (the collapseAll argument generalized): 1. every new edge v->a
// satisfies a ∈ old closure(v) (flatten branch: a ∈ closure ∩ anchors; keep branch: old
// edge ⊆ closure), so new reachability ⊆ old — a new cycle would compose to an old one;
// 2. a self-bit would need v ∈ closure(v) — a cycle the invariants forbid. IDEMPOTENT by
// the same shrink: new edges only point upward, so closure_after ⊆ closure_before, and a
// second pass re-derives exactly the same parents (pinned per seed in T10).
// THE HONESTY CONTRACT this bakes in: a flatten can only PROMOTE existing ancestors to
// direct parents — it never invents a containment relation.
//
// WHAT STAYS PARKED (the ruling, kept honest): the full-ancestor form (anchors = 0xFFFF —
// every voice gains ALL its ancestors as direct parents) is the flooding case — C1's offer
// union and C4's hold-follow candidates multiply by the closure size; that is
// closure-as-stored-state arriving through a gesture's back door. It remains ONE CALL away
// (collapseAllOnto(g, 0xFFFF), T10 proves it still a valid, loadable DAG) but no verb row,
// no default, no panel button ships it. Depth/level counts: NO permanently — iterated
// collapse already composes per trigger and the step-0 smoke ruled flatten-toward-root to
// be THIS verb's job, not iterated collapse's. Predicate filters and tie-break modes: NO —
// cleanup-with-knobs is the §8 trap and R1 killed tie-breaks. Trigger wiring (which panel
// row / context voice feeds the mask) is module layer, parked with the containment panel
// work; the pure verb is ready.
inline void collapseAllOnto(ContainmentGraph& g, uint16_t anchors) {
    uint16_t out[VOICES];
    for (int v = 0; v < VOICES; ++v) {
        const uint16_t onto = (uint16_t)(g.ancestorClosure(v) & anchors);
        out[v] = onto ? onto : g.parents[v];   // the membership key: flatten IFF under an anchor
    }
    for (int v = 0; v < VOICES; ++v) g.parents[v] = out[v];
}

// ── REDUCE — the transitive reduction (DECLUTTER/PRUNE's pure verb; the parked verdict,
// pulled forward 2026-10-27 per Rodney's "reduction as an EXPLICIT gesture: YES") ────────────
// Strip every SHADOW EDGE: v->u is redundant iff ANOTHER parent w of v reaches u upward in the
// ORIGINAL graph (u ∈ ancestorClosure(w) — the edge v->w⇝u then carries the same reachability;
// the banked formulation's "∪ {w}" term is vacuous in the bitmask world — one bit per u — so
// the strict form below is exact). What it buys (IDEAS_PARKED.md): after a gesture march the
// drawn pin graph can carry shadow edges — COLLAPSE re-links grandparents, authored shortcuts
// accumulate; REDUCE shows the load-bearing spine. It is COLLAPSE's exact mirror: collapse
// makes implied structure explicit, reduction makes implied structure implicit — and it is the
// seed verb's parked post-pass (a flag, not forced: random shortcut edges may be exactly the
// variety wanted).
//
// THE DECIDING FACT (why this is an EXPLICIT gesture, never auto): the EVENT mechanisms (C1's
// offer union, C4's hold-follow shape pick, D4's first-onsetting container) read the DIRECT
// edge set verbatim — a redundant edge is a musical feature ("the child hears the grandparent's
// onsets directly"), and stripping it CHANGES THE SOUND. Cleanup that changes sound is exactly
// what §8 forbids — so reduce runs only when the user pulls the trigger (stamped,
// EXPAND-reversible), never as a background pass, and never per-sample (edit-rate only; the
// engine is untouched — materializeForest re-derives from the words at the next graph change).
//
// CORRECTNESS (why "compute the full redundant mask first, then strip" is exact, not
// heuristic): every witness read is against the ORIGINAL graph (the `before` snapshot), which
// makes the strip PURE and ORDER-FREE — a witness path from another parent w can never touch
// the deleted edge v->u itself (an upward path from w cannot revisit v: v->w and w⇝v would be
// a cycle), so simultaneous stripping cannot break any witness. Reachability is PRESERVED —
// closure(reduce(g)) == closure(g), mask-exact — by induction along any original route: a
// length-1 hop is never redundant (a redundant edge by definition has a longer alternative)
// and each longer hop's route survives through kept-or-re-proven edges. IDEMPOTENT: a kept
// edge's witness cannot exist in the reduced graph either (reduced closure ⊆ original
// closure), so the second pass strips nothing — the result is THE transitive reduction
// (unique for DAGs). ROOT-SET INVARIANT: stripping needs a SECOND parent, and the
// "stripped-by" relation among a vertex's parents is acyclic (a cycle would be a closure
// cycle), so a maximal parent always survives — reduction never orphans a voice. No pool
// discipline: reduce only DELETES (never re-points), so like collapse/collapseAll it is a
// shape verb over all 16 rows. T11 re-proves all of this per seed (the test is the guarantee).
inline void reduceTransitive(ContainmentGraph& g) {
    const ContainmentGraph before = g;   // every witness read against the ORIGINAL graph
    for (int v = 0; v < VOICES; ++v) {
        const uint16_t parents = before.parents[v];
        if ((parents & (parents - 1)) == 0) continue;   // 0 or 1 parent: no OTHER parent, no witness
        uint16_t closure[VOICES];
        bool have[VOICES] = {};
        uint16_t kept = parents, m = parents;
        while (m) {
            const int u = __builtin_ctz(m);
            m &= (uint16_t)(m - 1);
            uint16_t om = (uint16_t)(parents & ~(1u << u));
            bool redundant = false;
            while (om) {
                const int w = __builtin_ctz(om);
                om &= (uint16_t)(om - 1);
                if (!have[w]) { closure[w] = before.ancestorClosure(w); have[w] = true; }
                if ((closure[w] >> u) & 1u) { redundant = true; break; }
            }
            if (redundant) kept &= (uint16_t)~(1u << u);
        }
        g.parents[v] = kept;
    }
}

// ── RANDOM — the two pure draws (the seed verb; Rodney's ruling 2026-10-27: the gesture
// rows UNCLOSED, V_RANDOM = 6, panel jack + button; IDEAS_PARKED.md) ────────────────────────
// ONE own Philox stream, TWO page interpretations (the CA dice model,
// CA_DICE_COUNTER_MODEL.md): the stream has a FIXED domain key (domain separation — it does
// NOT ride the shared SEED jack), and the caller addresses it by a SIGNED page index; a page
// is a FIXED number of draws, so page N and page N-1 are stable neighbouring draws and
// position-- re-derives the previous draw EXACTLY. NOT seed-keying: the counter is the
// POSITION in the stream — a counter used as a key would jump to a different sequence per
// count instead of stepping the draw (the CA doc's "before" anti-pattern).

// Page strides (fixed over content — the CA "stable 16-wide page" rule generalized): each
// application consumes exactly one page regardless of what the draws decide, so position+1
// is always the NEXT forest/scatter and position-1 always the previous.
static constexpr uint64_t RANDOM_PAGE_OUTRIGHT = 151;   // 15 shuffle + 120 edge coins + 16 boost
static constexpr uint64_t RANDOM_PAGE_SCATTER  = 48;    // 16 voices x 3 draws

// The stream's FIXED domain key seed ("CONTAINM" — the containment module's OWN draw): never
// the shared SEED jack, never another stream's key. philoxMakeKey conditions it once; the
// page index does the addressing (the dice model — key = WHO, counter = WHERE).
static constexpr uint64_t RANDOM_STREAM_DOMAIN = 0x434F4E5441494E4DULL;

// OUTRIGHT NEW FOREST — the STATELESS/ABSOLUTE variation: the page SPECIFIES the graph (the
// graph IS f(page, density)), so reverse-draw IS the exact state restore (at(page-1)
// re-derives the previous forest bit-for-bit — the CA doc's own "when stateless would be
// right" case: a teleport to fresh configs, counter-alone reproducibility). The F-5
// property-suite recipe lifted into the module layer (test_forest_rhythm randomForest):
// a Fisher-Yates TOPOLOGICAL permutation of the 16 slots — each voice may take parents only
// from EARLIER permutation positions, so the result is acyclic BY CONSTRUCTION (the pin
// plane's guarantee reproduced in data); the connectivity boost gives every non-head voice
// at least one parent when the coins drew none (the head of the permutation is the lone
// guaranteed root). V1 is NOT special — D7 proved V1-anywhere legal and the property suite
// marches it. The WHOLE graph is rewritten, above-pool rows included: an outright generation
// is a wholesale replacement, the one verb that is not a shape verb over existing structure
// (store-backed undo + the trajectory bracket own the reversal, and they ride the commit).
//
// PAGE LAYOUT (draws atUniform(page + k), all pure functions of (page, density)):
//   [0,15)    Fisher-Yates: i = 15..1, j = floor(u * (i+1)), swap(perm[i], perm[j])
//   [15,135)  edge coins: for permutation position 1..15, each earlier position in order,
//             coin < p  ->  that parent bit is set
//   [135,151) boost: position 1..15 with no parents draws j = floor(u * pos) and takes
//             perm[j] (15 used, 1 pad — the stride is fixed, the layout never content-dependent)
// density 1..10 -> p = density/10 (the context-menu dial; 10 = every candidate edge, the
// DAG-complete upper shape — still acyclic). The graph is REPLACED: all 16 rows are zeroed
// first — an outright generation is the wholesale replacement (the one verb that is not a
// shape verb over existing structure), which is exactly what makes reverse-draw the exact
// state restore.
inline void seedForest(ContainmentGraph& g, const redDot::PhiloxRng& rng, uint64_t page, int density) {
    if (density < 1) density = 1;
    if (density > 10) density = 10;
    const float p = density * 0.1f;
    for (int v = 0; v < VOICES; ++v) g.parents[v] = 0;   // WHOLESALE replacement, not an accumulate
    int perm[VOICES];
    for (int i = 0; i < VOICES; ++i) perm[i] = i;
    for (int i = VOICES - 1; i >= 1; --i) {
        const int j = (int)(rng.atUniform(page + (uint64_t)(VOICES - 1 - i)) * (float)(i + 1));
        const int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    int coin = VOICES - 1;                                   // draws [15,135)
    for (int pos = 1; pos < VOICES; ++pos) {
        const int v = perm[pos];
        for (int up = 0; up < pos; ++up) {
            const int u = perm[up];
            if (rng.atUniform(page + (uint64_t)coin) < p)
                g.parents[v] |= (uint16_t)(1u << u);         // u is EARLIER in perm: acyclic by construction
            ++coin;
        }
    }
    for (int pos = 1; pos < VOICES; ++pos) {                 // draws [135,151), one per position
        const int v = perm[pos];
        if (g.parents[v] == 0) {
            const int j = (int)(rng.atUniform(page + 135 + (uint64_t)(pos - 1)) * (float)pos);
            g.parents[v] |= (uint16_t)(1u << perm[j]);       // j < pos: an earlier voice, never v itself
        }
    }
}

// MODIFY FOREST — the STATE-DEPENDENT scatter variation (Rodney: "more like change alley
// scatter which randomly modified pins state dependent"): the page drives a pass of CHECKED
// EDIT VERBS over the CURRENT graph — the draw is the HOW-MUCH, the current graph is the
// WHERE-FROM. Path-dependent BY DESIGN (the CA model: scatter builds on what you did,
// successive scatters evolve a trajectory, the six rows compose) — so this verb is
// deliberately NOT an order-free whole-graph rewrite: it is an RNG driver over the edit
// layer (addParent/removeParent carry their own cycle checks), deterministic given (current
// graph, page, density). Draw-reversible != state-restorable for THIS variation: the
// reverse-draw re-applies the previous page to the CURRENT graph — a generative scrub,
// never a state restore; the trajectory/undo machinery owns the state (IDEAS_PARKED.md
// requirement 3 — that is exactly why the global reverse must replay the trajectory).
//
// PAGE LAYOUT (fixed stride 48): voice v in SLOT order 0..15 (the march order of the
// current graph — no permutation; the state-dependent pass walks what is there):
//   3v+0 event coin  < p    -> this voice scatters this pass
//   3v+1 mode coin   < 0.5  -> ADD a parent, else REMOVE one
//   3v+2 pick        (32b)  -> the candidate / selection
// ADD: scan u = pick%16, +1, ... (VOICES steps, u != v): the first u that is a VALID new
//      parent (not already pinned, wouldCycle == false against the CURRENT partially-
//      scattered graph) takes the edge; none valid -> the event is spent, no re-roll. The
//      scan-from-drawn-start keeps ONE draw per voice and makes density meaningful.
// REMOVE: the (pick % popcount)-th set bit of the CURRENT parents[v]; a root skips.
inline void scatterEdges(ContainmentGraph& g, const redDot::PhiloxRng& rng, uint64_t page, int density) {
    if (density < 1) density = 1;
    if (density > 10) density = 10;
    const float p = density * 0.1f;
    for (int v = 0; v < VOICES; ++v) {
        if (!(rng.atUniform(page + (uint64_t)(3 * v)) < p)) continue;          // the event coin
        const bool add = rng.atUniform(page + (uint64_t)(3 * v + 1)) < 0.5f;   // the mode coin
        const uint32_t pick = rng.at(page + (uint64_t)(3 * v + 2));            // the pick
        if (add) {
            const int start = (int)(pick % (uint32_t)VOICES);
            for (int step = 0; step < VOICES; ++step) {
                const int u = (start + step) % VOICES;
                if (u == v) continue;
                if (g.isContainedIn(v, u)) continue;         // already pinned — not a new offer
                if (g.wouldCycle(v, u)) continue;            // the QUERY layer owns validity
                g.addParent(v, u);                           // the edit verb re-checks: the only writer
                break;
            }
        } else {
            const uint16_t m = g.parents[v];
            if (!m) continue;                                // a root has nothing to remove
            int nth = (int)(pick % (uint32_t)__builtin_popcount(m));
            uint16_t mm = m;
            while (mm) {
                const int u = __builtin_ctz(mm);
                mm &= (uint16_t)(mm - 1);
                if (nth-- == 0) { g.removeParent(v, u); break; }
            }
        }
    }
}

// ── EXPAND — NOT HERE (R3) ─────────────────────────────────────────────────────────────────
// Restore the level the last COLLAPSE removed. Collapse is LOSSY (many graphs collapse to the
// same graph), so expand is NOT a synthesized inverse — it is trajectory MEMORY: the module
// layer keeps the CA TRUE-REVERSE precedent (a ring of COMMITTED graph states pushed at each
// phrase-boundary commit; EXPAND pops one). The seam is reserved; the ring lands with the
// pending machinery (CC_PROMPT_CONTAINMENT_BUILD_PLAN.md, step 3/follow-up).

}} // namespace dotModular::ct
