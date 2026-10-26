/**
 * test_containment_transforms.cpp — Containment step 2: pure verbs + property harness
 * (CC_PROMPT_CONTAINMENT_BUILD_PLAN.md §TESTS: "known-answer (spec chain example inverted:
 * full reversal; rotate/reflect conjugation; collapse union; expand restore), per-verb
 * closure property test over seeded random DAGs, active-pool discipline (rows >= activeCount
 * untouched), composition marches").
 *
 * THE PRINCIPLE UNDER TEST (ContainmentTransforms.hpp): a containment transform is admissible
 * IFF it is an endomorphism on acyclic graphs. Each verb carries its closure argument inline
 * in the header; THIS suite re-proves every one of those arguments in CI on every run — the
 * test is the guarantee. Per verb:
 *   ROTATE / REFLECT — conjugation by a bijection (identity above the active pool) is a graph
 *     isomorphism: edge count, nesting depth, and every above-pool row are preserved; acyclicity
 *     and self-bit-freedom survive (a cycle in the image would pull back to one in the source).
 *   INVERT — the dual (full transpose) of a DAG is a DAG (a reversed cycle implies a forward
 *     cycle); edge count and depth preserved; an involution.
 *   COLLAPSE — every new edge v->g was an old length-2 path, so new reachability ⊆ old: the
 *     longest chain strictly shortens (d_new <= floor(d_old/2)); iterated, the march terminates
 *     on the EMPTY graph (the unique fixpoint) within <= 16 applications.
 *   COLLAPSE-ALL — ancestorClosure(v) & rootsMask: every new edge points at a former root, so
 *     depth <= 1; idempotent (after flattening, each voice's closure-roots are its parents).
 *   EXPAND — NOT HERE (R3): collapse is LOSSY (T9 demonstrates two distinct DAGs sharing one
 *     collapse image), so expand is trajectory MEMORY in the module layer (the CA TRUE-REVERSE
 *     precedent; the ring lands with the pending machinery, step-3 seam). The involutions that
 *     DO self-reverse meanwhile (invert, reflect, even-step rotates) are pinned in T0/T7.
 *
 * Seeded-Philox random DAGs are built incrementally under the same edit checks (accepted
 * addParent calls only — the edit-level slice lives in test_containment_edits.cpp S7; this
 * suite shares that generator idiom, as its header promised).
 *
 * Compile: g++ -std=c++20 -Itest -Isrc -I${RACK_DIR}/dep/include test/test_containment_transforms.cpp
 * (registered in test/run_all.sh as header-only; PhiloxRng needs the simde include from RACK_DIR)
 */
#include "../src/dsp/ContainmentGraph.hpp"
#include "../src/dsp/ContainmentTransforms.hpp"
#include "../src/dsp/PhiloxRng.hpp"
#include <cstdio>
#include <cstring>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

using namespace dotModular::ct;

// ── helpers ──────────────────────────────────────────────────────────────────
static bool sameState(const ContainmentGraph& a, const ContainmentGraph& b) {
    return std::memcmp(a.parents, b.parents, sizeof(a.parents)) == 0;
}
static int totalEdges(const ContainmentGraph& g) {
    int n = 0;
    for (int v = 0; v < VOICES; ++v) n += __builtin_popcount(g.parents[v]);
    return n;
}
static bool noSelfBits(const ContainmentGraph& g) {
    for (int v = 0; v < VOICES; ++v)
        if ((g.parents[v] >> v) & 1u) return false;
    return true;
}
// Longest upward nesting chain, in hops (0 = no edges anywhere). Memoized DP over the DAG —
// NOT ancestorClosure's depth out-param: the wavefront counts max SHORTEST distance (a direct
// edge shadows a longer path), which under-counts exactly the dense multi-parent graphs this
// suite must judge. (Doc mismatch caught here 2026-10-26; the graph header's comment now says so.)
static int longestUp(const ContainmentGraph& g, int v, int* memo) {
    if (memo[v] >= 0) return memo[v];
    int best = 0;
    for (int u = 0; u < VOICES; ++u)
        if ((g.parents[v] >> u) & 1u) {
            const int d = 1 + longestUp(g, u, memo);   // valid DAG only (call sites validate first)
            if (d > best) best = d;
        }
    memo[v] = best;
    return best;
}
static int depthOf(const ContainmentGraph& g) {
    int memo[VOICES];
    for (int v = 0; v < VOICES; ++v) memo[v] = -1;
    int best = 0;
    for (int v = 0; v < VOICES; ++v) {
        const int d = longestUp(g, v, memo);
        if (d > best) best = d;
    }
    return best;
}
// Parent bits >= A held by pool rows (the pass-through zone of ROTATE/REFLECT).
static int abovePoolBits(const ContainmentGraph& g, int A) {
    int n = 0;
    for (int v = 0; v < A; ++v) n += __builtin_popcount((unsigned)(g.parents[v] >> A));
    return n;
}
// Above-pool bits held by DEAD rows (>= A): must be untouched by the pool verbs.
static int deadAboveBits(const ContainmentGraph& g, int A) {
    int n = 0;
    for (int v = A; v < VOICES; ++v) n += __builtin_popcount((unsigned)(g.parents[v] >> A));
    return n;
}
// sigma = pi^-1 for the pool maps (pi = shift-within-pool / mirror, identity above the pool).
// Mirrors clampStep's normalization: span<=1 -> no shift; step 0/span -> 1.
static void sigmaOfShift(int A, int step, int* sigma) {
    int st = 0;
    if (A > 1) {
        st = step % A;
        if (st < 0) st += A;
        if (st == 0) st = 1;
    }
    for (int v = 0; v < VOICES; ++v)
        sigma[v] = (v < A) ? ((v - st + A) % A) : v;
}
static void sigmaOfMirror(int A, int* sigma) {
    for (int v = 0; v < VOICES; ++v)
        sigma[v] = (v < A) ? (A - 1 - v) : v;
}
// THE CLOSURE PROOF AS AN ORACLE (the plan's conjugation law, no forward-map reimplementation):
// every new edge a->b implies old edge sigma(a)->sigma(b). (a,b) |-> (sigma a, sigma b) is a
// bijection on pairs, so combined with an equal edge count this is the EXACT law
// new_parents[pi(v)] == pi_bits(parents[v]) — and a new cycle would pull back to an old one.
static bool pullbackOk(const ContainmentGraph& before, const ContainmentGraph& after, const int* sigma) {
    for (int a = 0; a < VOICES; ++a)
        for (int b = 0; b < VOICES; ++b)
            if ((after.parents[a] >> b) & 1u)
                if (!((before.parents[sigma[a]] >> sigma[b]) & 1u)) return false;
    return true;
}
// The WARPDIAG contract extended to transforms: any state a verb produces must be a
// loadable save (the deserialize guard accepts it, byte-identical).
static bool roundTripOk(const ContainmentGraph& g) {
    uint16_t raw[VOICES];
    g.serializeTo(raw);
    ContainmentGraph h;
    return h.deserializeFrom(raw) && sameState(g, h);
}
// Random DAG built incrementally under the same edit checks (accepted addParent only).
static void buildRandomDag(ContainmentGraph& g, redDot::PhiloxRng& rng, int attempts) {
    for (int i = 0; i < attempts; ++i) {
        const int v = (int)(rng() & 15u);
        const int u = (int)(rng() & 15u);
        g.addParent(v, u);   // rejects (cycles/self) no-op — the graph stays a DAG by construction
    }
}

int main() {
    // ── T0. INVERT known-answers: full reversal (the spec chain, the diamond) ──
    {
        ContainmentGraph g;
        CHK(g.addParent(2, 3) && g.addParent(3, 4), "invert: setup chain 2<3<4 accepted");
        invert(g);
        CHK(g.isContainedIn(4, 3) && g.isContainedIn(3, 2),
            "invert: chain 2<3<4 reverses to 4<3<2 (full edge reversal, R1-clean)");
        CHK(!g.isContainedIn(2, 3) && !g.isContainedIn(3, 4), "invert: forward edges are gone");
        CHK(g.isRoot(2) && !g.isRoot(4), "invert: the most-contained voice (2) became the root");
        CHK(totalEdges(g) == 2 && depthOf(g) == 2 && g.isValid(),
            "invert: transpose preserves edge count and depth; still valid");

        // diamond 3 in {1,2}, both in 0 -> dual: 0 in {1,2}... both dual parents on the co-root,
        // the most-contained voice 3 becomes the root. Multi-parent makes the dual total.
        ContainmentGraph d;
        CHK(d.addParent(3, 1) && d.addParent(3, 2) && d.addParent(1, 0) && d.addParent(2, 0),
            "invert: setup diamond accepted");
        invert(d);
        CHK(d.parentCount(0) == 2 && d.isContainedIn(0, 1) && d.isContainedIn(0, 2),
            "invert: the co-root 0 becomes most-contained with BOTH dual parents");
        CHK(d.isRoot(3) && d.isContainedIn(1, 3) && d.isContainedIn(2, 3),
            "invert: the most-contained voice 3 became the root, children re-hung on it");
        CHK(d.isValid() && totalEdges(d) == 4 && depthOf(d) == 2,
            "invert: diamond dual is a valid DAG, edge count + depth preserved");

        // involution: invert is self-reverse (the expand-deferral leans on this)
        redDot::PhiloxRng rng0(0xC0FFEEull);
        ContainmentGraph r;
        buildRandomDag(r, rng0, 60);
        ContainmentGraph snap = r;
        invert(r);
        invert(r);
        CHK(sameState(r, snap), "invert: double trigger restores byte-identical (involution)");
        CHK(noSelfBits(r) && r.isValid(), "invert: march kept the graph valid throughout");
    }

    // ── T1. ROTATE conjugation: shape preserved, roles moved, composes ─────────
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 2) && g.addParent(2, 5), "rotate: setup chain 0<2<5 accepted");
        rotateRoles(g, 16, 1);
        CHK(g.isContainedIn(1, 3) && g.isContainedIn(3, 6) && !g.isContainedIn(0, 2),
            "rotate: role of v moves to v+1 — 0<2<5 becomes 1<3<6 (conjugation)");
        CHK(totalEdges(g) == 2 && depthOf(g) == 2, "rotate: edge count + depth preserved (isomorphism)");

        rotateRoles(g, 16, 1);   // composes per trigger: second trigger = net step 2
        CHK(g.isContainedIn(2, 4) && g.isContainedIn(4, 7),
            "rotate: composes per trigger — two step-1 triggers = net step 2");

        // clampStep cannot select nothing: step 0 and step span both behave as step 1
        ContainmentGraph a, b, c;
        a.addParent(0, 1);
        b = a; c = a;
        rotateRoles(a, 16, 0);
        rotateRoles(b, 16, 1);
        rotateRoles(c, 16, 16);
        CHK(sameState(a, b), "rotate: step 0 clamps to step 1 (config cannot select a no-op)");
        CHK(sameState(b, c), "rotate: step == span clamps to step 1 (16 % 16 = 0 -> 1)");

        // cyclic order: step-1 rotate applied `active` times is the identity
        ContainmentGraph cyc;
        CHK(cyc.addParent(0, 1) && cyc.addParent(2, 3), "rotate: setup pool-5 pair accepted");
        ContainmentGraph cyc0 = cyc;
        rotateRoles(cyc, 5, 1);
        CHK(!sameState(cyc, cyc0), "rotate: one trigger actually moved the roles (non-vacuous)");
        for (int k = 1; k < 5; ++k) rotateRoles(cyc, 5, 1);
        CHK(sameState(cyc, cyc0), "rotate: `active` step-1 triggers return to the start (order = active)");

        // above-pool pass-through: row 9 in 10 survives a pool-6 rotation untouched (smoke case)
        ContainmentGraph p;
        CHK(p.addParent(0, 2) && p.addParent(2, 5) && p.addParent(9, 10),
            "rotate: setup pool-6 chain + outside row accepted");
        rotateRoles(p, 6, 1);
        CHK(p.isContainedIn(9, 10), "rotate: row 9 >= active 6 untouched (manual pins persist)");
        CHK(p.isContainedIn(1, 3) && p.isContainedIn(3, 0),
            "rotate: pool roles moved 0<2<5 -> 1<3<0 (wrap within the pool)");
        CHK(p.isValid(), "rotate: mixed pool/outside graph stays valid");
    }

    // ── T2. REFLECT conjugation: the similitude mirror, self-inverse ───────────
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 1), "reflect: setup chain 0<1 accepted");
        reflectRoles(g, 16);
        CHK(g.isContainedIn(15, 14) && !g.isContainedIn(0, 1),
            "reflect: mirror v -> 15-v moves 0<1 to 15<14");
        CHK(totalEdges(g) == 1 && depthOf(g) == 1 && g.isValid(),
            "reflect: edge count + depth preserved; valid");

        // self-inverse: a double trigger restores the original role assignment
        reflectRoles(g, 16);
        CHK(g.isContainedIn(0, 1), "reflect: double trigger restores (involution)");

        // the smoke's conjugation case, pool 6: mirror of 1<3, 3<0 is 4<2, 2<5; row 9<10 intact
        ContainmentGraph p;
        CHK(p.addParent(0, 2) && p.addParent(2, 5) && p.addParent(9, 10),
            "reflect: setup pool-6 chain + outside row accepted");
        rotateRoles(p, 6, 1);          // -> 1<3, 3<0 (T1 case)
        reflectRoles(p, 6);
        CHK(p.isContainedIn(4, 2) && p.isContainedIn(2, 5),
            "reflect: pool-6 mirror of 1<3, 3<0 is 4<2, 2<5");
        CHK(p.isContainedIn(9, 10), "reflect: row 9 >= active 6 untouched");
        CHK(p.isValid(), "reflect: mixed pool/outside graph stays valid");
    }

    // ── T3. COLLAPSE union: grandparent re-link, total under multi-parent ──────
    {
        ContainmentGraph g;
        CHK(g.addParent(2, 3) && g.addParent(3, 4), "collapse: setup chain 2<3<4 accepted");
        collapse(g);
        CHK(g.isContainedIn(2, 4) && !g.isContainedIn(2, 3) && !g.isContainedIn(3, 4),
            "collapse: chain 2<3<4 re-links 2 to grandparent 4; 3 detaches");
        CHK(g.isRoot(3) && g.isRoot(4), "collapse: old mid/root rows fall to roots");
        CHK(g.isValid(), "collapse: chain collapse stays valid");

        // convergent union: both branches agree — {0} ∪ {0} = {0}, no duplication
        ContainmentGraph c1;
        c1.addParent(3, 1); c1.addParent(3, 2); c1.addParent(1, 0); c1.addParent(2, 0);
        collapse(c1);
        CHK(c1.parentCount(3) == 1 && c1.isContainedIn(3, 0),
            "collapse: convergent branches dedupe to the single grandparent");
        CHK(c1.isRoot(1) && c1.isRoot(2) && c1.isValid(), "collapse: mid tier detaches; valid");

        // divergent union: grandparents differ — {0} ∪ {5} = BOTH (the totality R1 bought:
        // no tie-break rule anywhere, the OR-union is the whole story)
        ContainmentGraph c2;
        c2.addParent(3, 1); c2.addParent(3, 2); c2.addParent(1, 0); c2.addParent(2, 5);
        collapse(c2);
        CHK(c2.parentCount(3) == 2 && c2.isContainedIn(3, 0) && c2.isContainedIn(3, 5),
            "collapse: divergent branches union to BOTH grandparents (multi-parent, no tie-break)");
        CHK(c2.isValid() && depthOf(c2) == 1, "collapse: divergent union valid, depth dropped to 1");

        // the longest chain strictly shortens: depth 3 chain -> depth 1
        ContainmentGraph ch;
        ch.addParent(1, 0); ch.addParent(2, 1); ch.addParent(3, 2);
        CHK(depthOf(ch) == 3, "collapse: chain depth is 3 before");
        collapse(ch);
        CHK(depthOf(ch) == 1, "collapse: one application strictly shortens the longest chain (3 -> 1)");

        // empty graph is a fixpoint
        ContainmentGraph e;
        collapse(e);
        CHK(totalEdges(e) == 0, "collapse: empty graph is a fixpoint (no-op)");
    }

    // ── T4. COLLAPSE-ALL: flatten ONTO ITS ROOTS (flat-single-root, idempotent) ─
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 1) && g.addParent(1, 2) && g.addParent(2, 3) && g.addParent(9, 10),
            "collapseAll: setup chain 0<1<2<3 + separate component accepted");
        collapseAll(g);
        CHK(g.isContainedIn(0, 3) && g.isContainedIn(1, 3) && g.isContainedIn(2, 3),
            "collapseAll: 0,1,2 become DIRECT children of root 3 (flat-single-root, not detached)");
        CHK(g.parentCount(0) == 1 && g.parentCount(1) == 1 && g.parentCount(2) == 1,
            "collapseAll: each flattened voice carries exactly its root set");
        CHK(g.isRoot(3) && g.isRoot(10), "collapseAll: roots stay roots");
        CHK(g.isContainedIn(9, 10), "collapseAll: separate component flattens onto ITS OWN root");
        CHK(depthOf(g) == 1 && g.isValid(), "collapseAll: depth <= 1 by construction; valid");

        // multi-root component: 3 in {1,2}, 1 in 0, 2 in 5 -> 3 flattens onto BOTH roots {0,5}
        ContainmentGraph m;
        m.addParent(3, 1); m.addParent(3, 2); m.addParent(1, 0); m.addParent(2, 5);
        collapseAll(m);
        CHK(m.parentCount(3) == 2 && m.isContainedIn(3, 0) && m.isContainedIn(3, 5),
            "collapseAll: multi-root component flattens onto BOTH roots (multi-parent visible)");
        CHK(m.isContainedIn(1, 0) && m.isContainedIn(2, 5),
            "collapseAll: mid-tier voices flatten onto their own roots, components distinct");
        CHK(m.isValid() && depthOf(m) == 1, "collapseAll: multi-root flatten valid, depth 1");

        // idempotent: after flattening, each voice's closure-roots are exactly its parents
        ContainmentGraph before = g;
        collapseAll(g);
        CHK(sameState(g, before), "collapseAll: double trigger is byte-identical (idempotent)");

        ContainmentGraph e;
        collapseAll(e);
        CHK(totalEdges(e) == 0, "collapseAll: empty graph is a fixpoint (no-op)");
    }

    // ── T5. ACTIVE-POOL discipline: the plan's conjugation law ─────────────────
    // The plan's formula is new_parents[pi(v)] = pi_bits(parents[v]) with pi = identity above
    // the pool: dead rows KEEP their slot and their above-pool bits (all-above rows are
    // byte-untouched — manual pins that outlived a polyphony reduction persist); a dead row's
    // pin INTO the live pool conjugates with the pool (it follows the role, not the bare
    // voice); bits pointing at voices >= A pass through unchanged.
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 2) && g.addParent(2, 5)       // inside pool 6
            && g.addParent(9, 10)                        // all-above row (pure outside)
            && g.addParent(8, 1)                         // dead row pointing INTO the pool
            && g.addParent(3, 9),                        // live row pointing ABOVE the pool
            "pool: setup 6-voice pool with all three outside-edge shapes accepted");
        ContainmentGraph before = g;
        rotateRoles(g, 6, 2);
        CHK(g.isContainedIn(2, 4) && g.isContainedIn(4, 1),
            "pool: rotate shift-2 moves 0<2<5 to 2<4<1 (5 wraps to pool start)");
        CHK(g.isContainedIn(5, 9) && !g.isContainedIn(3, 9),
            "pool: live row's above-pool bit passes through (row 3->5, still pointing at 9)");
        CHK(g.isContainedIn(8, 3) && !g.isContainedIn(8, 1),
            "pool: dead row's pin follows the role (8 in 1 -> 8 in pi(1)=3) per the plan formula");
        CHK(g.isContainedIn(9, 10),
            "pool: the all-above row 9<10 survives verbatim (manual pins beyond the pool persist)");
        CHK(deadAboveBits(g, 6) == deadAboveBits(before, 6),
            "pool: above-pool bits of dead rows unchanged by rotate");
        CHK(abovePoolBits(g, 6) == abovePoolBits(before, 6),
            "pool: rotate creates/destroys no above-pool bits (pass-through, count preserved)");
        CHK(totalEdges(g) == totalEdges(before) && depthOf(g) == depthOf(before) && g.isValid(),
            "pool: rotate is an isomorphism on the full 16 (identity above + bijection inside)");
        int sig[VOICES];
        sigmaOfShift(6, 2, sig);
        CHK(pullbackOk(before, g, sig),
            "pool: sigma-pullback oracle — rotate's image is exactly pi-conjugation of the source");

        ContainmentGraph m = before;
        reflectRoles(m, 6);
        CHK(m.isContainedIn(8, 4) && m.isContainedIn(9, 10) && m.isContainedIn(2, 9),
            "pool: reflect — dead row's pin mirrors (8 in 1 -> 8 in 4), all-above row intact, 3's bit -> row 2");
        CHK(deadAboveBits(m, 6) == deadAboveBits(before, 6) && abovePoolBits(m, 6) == abovePoolBits(before, 6),
            "pool: reflect creates/destroys no above-pool bits, dead rows' above-bits untouched");
        CHK(totalEdges(m) == totalEdges(before) && depthOf(m) == depthOf(before) && m.isValid(),
            "pool: reflect is an isomorphism on the full 16");
        sigmaOfMirror(6, sig);
        CHK(pullbackOk(before, m, sig),
            "pool: sigma-pullback oracle — reflect's image is exactly pi-conjugation of the source");

        // pool of ONE: both verbs are the identity map (clampStep span<=1 -> 0; mirror self-maps)
        ContainmentGraph one = before;
        rotateRoles(one, 1, 3);
        reflectRoles(one, 1);
        CHK(sameState(one, before), "pool: activeCount 1 makes rotate/reflect exact no-ops");

        // the partial-conjugation closure worry, hand-computed: a dead row feeding a pool
        // member rotates cleanly with the pool (2 in 0 + 0 in 1, pool {0,1}, shift 1)
        ContainmentGraph mix;
        CHK(mix.addParent(0, 1) && mix.addParent(2, 0), "pool: setup dead-feeds-live chain accepted");
        rotateRoles(mix, 2, 1);
        CHK(mix.isContainedIn(1, 0) && mix.isContainedIn(2, 1) && !mix.isContainedIn(2, 0),
            "pool: chain 2<0<1 rotates to 2<1<0 as one piece (no seam at the pool edge)");
        CHK(mix.isValid() && depthOf(mix) == 2, "pool: partial conjugation closed — valid, shape intact");
    }

    // ── T6. per-verb CLOSURE MARCH — the guarantee (600 seeded random DAGs) ────
    // Built incrementally under the same edit checks; every verb applied on top of the
    // previous result; after EVERY application: acyclic + no self-bits + the verb's own
    // shape law + save/loadable (the WARPDIAG contract extended to transforms).
    {
        int badValid = -1, badPool = -1, badShape = -1, badDepth = -1, badSer = -1, badIdem = -1;
        int builtNontrivial = 0;
        for (int seed = 1; seed <= 600 && badValid < 0; ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x9E3779B97F4A7C15ull);
            ContainmentGraph g;
            buildRandomDag(g, rng, 80);
            if (totalEdges(g) > 0) ++builtNontrivial;
            const int A = 1 + (int)(rng() % 16u);

            int sig[VOICES];

            // ROTATE: conjugation law (pullback oracle) + isomorphism laws + validity
            ContainmentGraph snap = g;
            const int e0 = totalEdges(g), d0 = depthOf(g);
            const int stepA = 1 + (int)(rng() % 16u);
            rotateRoles(g, A, stepA);
            if (!g.isValid() || !noSelfBits(g)) { badValid = seed; break; }
            sigmaOfShift(A, stepA, sig);
            if (!pullbackOk(snap, g, sig)) { badPool = seed; break; }
            if (deadAboveBits(g, A) != deadAboveBits(snap, A)
                || abovePoolBits(g, A) != abovePoolBits(snap, A)) { badPool = seed; break; }
            if (totalEdges(g) != e0 || depthOf(g) != d0) { badShape = seed; break; }
            if (!roundTripOk(g)) { badSer = seed; break; }

            // REFLECT: same laws (keep marching — reflect sees the rotated graph)
            snap = g;
            reflectRoles(g, A);
            if (!g.isValid() || !noSelfBits(g)) { badValid = seed; break; }
            sigmaOfMirror(A, sig);
            if (!pullbackOk(snap, g, sig)) { badPool = seed; break; }
            if (deadAboveBits(g, A) != deadAboveBits(snap, A)
                || abovePoolBits(g, A) != abovePoolBits(snap, A)) { badPool = seed; break; }
            if (totalEdges(g) != e0 || depthOf(g) != d0) { badShape = seed; break; }
            if (!roundTripOk(g)) { badSer = seed; break; }

            // INVERT: transpose laws (no pool — full 16)
            snap = g;
            invert(g);
            if (!g.isValid() || !noSelfBits(g)) { badValid = seed; break; }
            if (totalEdges(g) != e0 || depthOf(g) != d0) { badShape = seed; break; }
            if (!roundTripOk(g)) { badSer = seed; break; }

            // COLLAPSE: reachability shrinks — the longest chain strictly shortens
            const int db = depthOf(g);
            collapse(g);
            if (!g.isValid() || !noSelfBits(g)) { badValid = seed; break; }
            const int da = depthOf(g);
            if (db > 0 && da >= db) { badDepth = seed; break; }
            if (da == 0 && totalEdges(g) != 0) { badDepth = seed; break; }
            if (!roundTripOk(g)) { badSer = seed; break; }

            // COLLAPSE-ALL: flatten onto roots, idempotent
            collapseAll(g);
            if (!g.isValid() || !noSelfBits(g)) { badValid = seed; break; }
            if (depthOf(g) > 1) { badDepth = seed; break; }
            ContainmentGraph hold = g;
            collapseAll(g);
            if (!sameState(g, hold)) { badIdem = seed; break; }
            if (!roundTripOk(g)) { badSer = seed; break; }
        }
        if (badValid >= 0) std::printf("  detail: validity broke at seed %d\n", badValid);
        if (badPool  >= 0) std::printf("  detail: pool discipline broke at seed %d\n", badPool);
        if (badShape >= 0) std::printf("  detail: shape law broke at seed %d\n", badShape);
        if (badDepth >= 0) std::printf("  detail: collapse depth law broke at seed %d\n", badDepth);
        if (badSer   >= 0) std::printf("  detail: round-trip broke at seed %d\n", badSer);
        if (badIdem  >= 0) std::printf("  detail: collapse-all idempotence broke at seed %d\n", badIdem);
        CHK(builtNontrivial > 550, "march: the generator actually built non-trivial DAGs");
        CHK(badValid < 0, "march: acyclic + no self-bits after EVERY verb (600 seeds x 5 verbs)");
        CHK(badPool  < 0, "march: rows >= active byte-untouched under rotate/reflect");
        CHK(badShape < 0, "march: rotate/reflect/invert preserve edge count + depth");
        CHK(badDepth < 0, "march: collapse strictly shortens; collapse-all lands depth <= 1");
        CHK(badSer   < 0, "march: every post-verb state round-trips (deserialize accepts, identity)");
        CHK(badIdem  < 0, "march: collapse-all idempotent on every random DAG");
    }

    // ── T7. group laws: the involutions + the dihedral algebra ─────────────────
    // The expand-deferral note ("the involutions — invert, reflect, even-step rotates —
    // self-reverse meanwhile") is a LOAD-BEARING claim: pin it per verb over random DAGs.
    {
        int badInv = -1, badRef = -1, badEven = -1;
        for (int seed = 1; seed <= 200 && (badInv < 0 && badRef < 0 && badEven < 0); ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0xD1B54A32D192ED03ull + 7);
            ContainmentGraph g;
            buildRandomDag(g, rng, 60);
            const int A = 2 + 2 * (int)(rng() % 8u);   // even pools: 2..16

            ContainmentGraph s1 = g;
            invert(s1); invert(s1);
            if (!sameState(s1, g)) { badInv = seed; continue; }

            ContainmentGraph s2 = g;
            reflectRoles(s2, A); reflectRoles(s2, A);
            if (!sameState(s2, g)) { badRef = seed; continue; }

            ContainmentGraph s3 = g;
            rotateRoles(s3, A, A / 2); rotateRoles(s3, A, A / 2);
            if (!sameState(s3, g)) { badEven = seed; continue; }
        }
        if (badInv  >= 0) std::printf("  detail: invert involution broke at seed %d\n", badInv);
        if (badRef  >= 0) std::printf("  detail: reflect involution broke at seed %d\n", badRef);
        if (badEven >= 0) std::printf("  detail: even-step rotate involution broke at seed %d\n", badEven);
        CHK(badInv  < 0, "group: invert is an involution (200 random DAGs)");
        CHK(badRef  < 0, "group: reflect is an involution within any even pool (200 random DAGs)");
        CHK(badEven < 0, "group: rotate by active/2 is an involution (200 random even pools)");

        // conjugations compose: three step-2 triggers == one step-6 trigger (C_pi o C_pi = C_pi^2)
        ContainmentGraph a, b;
        a.addParent(0, 3); a.addParent(5, 9); a.addParent(2, 10);
        b = a;
        rotateRoles(a, 16, 2); rotateRoles(a, 16, 2); rotateRoles(a, 16, 2);
        rotateRoles(b, 16, 6);
        CHK(sameState(a, b), "group: three step-2 rotates == one step-6 rotate (composition)");

        // dihedral: reflect-then-rotate(1) == rotate(-1)-then-reflect (the mirror and the
        // shift commute only through the sign — the group relation rho pi_s = pi_-s rho)
        ContainmentGraph d1, d2;
        d1.addParent(0, 2);
        d2 = d1;
        reflectRoles(d1, 8); rotateRoles(d1, 8, 1);
        rotateRoles(d2, 8, 7); reflectRoles(d2, 8);   // 7 = -1 mod 8 (clampStep normalizes)
        CHK(sameState(d1, d2), "group: dihedral relation reflect o rotate(1) == rotate(-1) o reflect");
    }

    // ── T8. composition marches ────────────────────────────────────────────────
    {
        // iterated collapse: strict decrease every application, empty fixpoint in <= 16
        int badStrict = -1, badTerm = -1, badFix = -1, worstSteps = 0;
        for (int seed = 1; seed <= 200 && (badStrict < 0 && badTerm < 0 && badFix < 0); ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x2545F4914F6CDD1Dull + 11);
            ContainmentGraph g;
            buildRandomDag(g, rng, 90);
            int d = depthOf(g), steps = 0;
            while (d > 0 && steps < 32) {
                collapse(g);
                if (!g.isValid()) { badStrict = seed; break; }
                const int d2 = depthOf(g);
                if (d2 >= d) { badStrict = seed; break; }   // must strictly shorten
                d = d2;
                ++steps;
            }
            if (badStrict >= 0) continue;
            if (d != 0) { badTerm = seed; continue; }       // must reach the empty graph
            if (steps > worstSteps) worstSteps = steps;
            if (steps > 16) { badTerm = seed; continue; }
            ContainmentGraph hold = g;
            collapse(g);
            if (!sameState(g, hold)) { badFix = seed; continue; }   // empty is a fixpoint
        }
        if (badStrict >= 0) std::printf("  detail: strict shortening broke at seed %d\n", badStrict);
        if (badTerm  >= 0) std::printf("  detail: termination broke at seed %d\n", badTerm);
        if (badFix   >= 0) std::printf("  detail: fixpoint broke at seed %d\n", badFix);
        CHK(badStrict < 0, "march: iterated collapse strictly shortens the longest chain every step");
        CHK(badTerm   < 0, "march: iterated collapse reaches the empty fixpoint within 16 applications");
        CHK(badFix    < 0, "march: the empty graph is a fixpoint (further triggers no-op)");
        (void)worstSteps;

        // mixed verb march (the step-0 smoke gate, grown to CI scale): 200 DAGs x 100 random
        // verb applications — validity re-proven after EVERY application, save/loadable at rest
        int badMixed = -1, badMixedSer = -1;
        for (int seed = 1; seed <= 200 && (badMixed < 0 && badMixedSer < 0); ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x9E3779B97F4A7C15ull + 3);
            ContainmentGraph g;
            buildRandomDag(g, rng, 70);
            for (int step = 0; step < 100; ++step) {
                const int verb = (int)(rng() % 5u);
                const int A = 1 + (int)(rng() % 16u);
                switch (verb) {
                    case 0: rotateRoles(g, A, 1 + (int)(rng() % 16u)); break;
                    case 1: reflectRoles(g, A); break;
                    case 2: invert(g); break;
                    case 3: collapse(g); break;
                    case 4: collapseAll(g); break;
                }
                if (!g.isValid() || !noSelfBits(g)) { badMixed = seed; break; }
            }
            if (badMixed >= 0) break;
            if (!roundTripOk(g)) { badMixedSer = seed; break; }
        }
        if (badMixed    >= 0) std::printf("  detail: mixed march validity broke at seed %d\n", badMixed);
        if (badMixedSer >= 0) std::printf("  detail: mixed march round-trip broke at seed %d\n", badMixedSer);
        CHK(badMixed    < 0, "march: 200 DAGs x 100 mixed verb applications — valid after EVERY step");
        CHK(badMixedSer < 0, "march: the marched state is still a loadable save at rest");
    }

    // ── T9. EXPAND seam (R3): collapse is LOSSY — no synthesized inverse exists ─
    // Two DISTINCT DAGs share one collapse image, so expand cannot be a pure verb: it is
    // trajectory MEMORY in the module layer (the CA TRUE-REVERSE precedent — a ring of
    // committed graph states pushed at each phrase-boundary commit; EXPAND pops one). The
    // seam is reserved for step 3 (CAPendingOrder reuse + Rate::Phrase apply). What the
    // pure layer offers meanwhile: the involutions self-reverse (T0/T7), so invert/reflect/
    // even-step rotates are their own expand.
    {
        ContainmentGraph chain, redundant;
        CHK(chain.addParent(0, 1) && chain.addParent(1, 2),   // 0<1<2 — a genuine 2-level chain
            "expand: setup chain 0<1<2 accepted");
        CHK(redundant.addParent(0, 1) && redundant.addParent(1, 2) && redundant.addParent(0, 2),
            "expand: setup chain + redundant direct edge accepted (0<1<2 plus 0<2)");
        CHK(!sameState(chain, redundant), "expand: the two source DAGs are distinct");
        collapse(chain);
        collapse(redundant);
        CHK(sameState(chain, redundant),
            "expand: LOSSY — chain and chain+redundant-edge share one collapse image {0<2}");
        CHK(chain.isContainedIn(0, 2) && chain.parentCount(0) == 1 && chain.isRoot(1) && chain.isRoot(2),
            "expand: the shared image is the grandparent re-link, mid tier detached");
        CHK(chain.isValid(), "expand: the shared image is itself a valid DAG");
    }

    std::printf(fail ? "\n%d passed, %d FAILED\n" : "\n%d passed, 0 failed\n", pass, fail);
    return fail ? 1 : 0;
}
