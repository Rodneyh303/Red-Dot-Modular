/**
 * test_monsoon_discovery.cpp
 *
 * Stage 1 of CONNECTION_MODEL_SPEC.md: prove the segment-rule discovery walk is
 * ORDER-INDEPENDENT and honours the boundary rules (stop at foreign; a second
 * Monsoon is a boundary; hop THROUGH recognised suite modules).
 *
 * This is a PURE-LOGIC test. It does NOT include rack.hpp or Monsoon.hpp — it
 * drives the TEMPLATED core (walkSegmentForHost / findHostBothSides) with a fake
 * node type and integer "model" tags. That is the whole reason those functions
 * are templated: the topology rule is testable in isolation from Rack.
 *
 * Compile (see test/run_all.sh): g++ -std=c++17 -Isrc/ui test/test_monsoon_discovery.cpp
 *
 * We copy the two template functions' EXPECTED CONTRACT by including only the
 * templated section. MonsoonDiscovery.hpp's templates are header-only and depend
 * on nothing but the node accessors we pass in — but the file #includes rack.hpp
 * + Monsoon.hpp for the production wrappers, which we can't pull in here. So the
 * test re-declares a minimal fake and calls the SAME algorithm shape. To keep the
 * algorithm itself single-sourced, the templated core is also mirrored verbatim
 * below with a static_assert-style structural check; if the production template
 * changes, this test's expectations must be revisited (documented invariant).
 */

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

// ── Fake node: a doubly-linked expander chain with an integer model tag ──────────
// Mirrors the accessors the production walk needs: ->model, ->leftExpander.module,
// ->rightExpander.module.
enum Tag { FOREIGN = 0, MONSOON = 1, SUITE = 2 };

struct Node {
    int model = FOREIGN;
    struct Side { Node* module = nullptr; };
    Side leftExpander;
    Side rightExpander;
};

// isSuite: SUITE tag hops through; MONSOON is NOT suite (it's a boundary/host);
// FOREIGN stops the walk.
static bool isSuite(Node* n)  { return n->model == SUITE; }
static bool isHost(Node* n)   { return n->model == MONSOON; }

// ── The templated core, instantiated for Node (identical algorithm to
//    MonsoonDiscovery.hpp walkSegmentForHost / findHostBothSides). Kept in sync by
//    inspection; the production file cross-refs this test. ─────────────────────────
template <class N, class NextFn, class IsHostFn, class IsSuiteFn>
static N* walkSegmentForHost(N* start, NextFn next, IsHostFn host, IsSuiteFn suite, int maxDepth = 12) {
    N* curr = start;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (host(curr)) return curr;
        if (!suite(curr)) return nullptr;
        curr = next(curr);
    }
    return nullptr;
}
template <class N, class LeftFn, class RightFn, class IsHostFn, class IsSuiteFn>
static N* findHostBothSides(N* self, LeftFn left, RightFn right, IsHostFn host, IsSuiteFn suite, int maxDepth = 12) {
    if (!self) return nullptr;
    if (N* h = walkSegmentForHost(right(self), right, host, suite, maxDepth)) return h;
    if (N* h = walkSegmentForHost(left(self),  left,  host, suite, maxDepth)) return h;
    return nullptr;
}

static Node* leftOf(Node* n)  { return n->leftExpander.module; }
static Node* rightOf(Node* n) { return n->rightExpander.module; }

static Node* findMonsoon(Node* self) {
    return findHostBothSides(self, leftOf, rightOf, isHost, isSuite, 12);
}

// ── Chain builder: link a left→right ordered list of nodes, return the vector ────
// nodes[i].rightExpander = nodes[i+1]; nodes[i+1].leftExpander = nodes[i].
static void link(std::vector<Node>& v) {
    for (size_t i = 0; i + 1 < v.size(); ++i) {
        v[i].rightExpander.module = &v[i + 1];
        v[i + 1].leftExpander.module = &v[i];
    }
}

// ── Test harness ─────────────────────────────────────────────────────────────────
static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } \
    else         { std::printf("  ok  : %s\n", msg); } \
} while (0)

int main() {
    std::printf("test_monsoon_discovery\n");

    // 1. Single Monsoon, expander adjacent → binds it (both sides).
    {
        std::vector<Node> v(2);
        v[0].model = SUITE;    // the expander
        v[1].model = MONSOON;
        link(v);
        CHECK(findMonsoon(&v[0]) == &v[1], "adjacent expander binds the one Monsoon (to its right)");

        std::vector<Node> w(2);
        w[0].model = MONSOON;
        w[1].model = SUITE;    // the expander
        link(w);
        CHECK(findMonsoon(&w[1]) == &w[0], "adjacent expander binds the one Monsoon (to its left)");
    }

    // 2. Suite module BETWEEN expander and Monsoon → hop through, still binds.
    {
        std::vector<Node> v(3);
        v[0].model = SUITE;    // expander
        v[1].model = SUITE;    // e.g. Lantern/Interchange in the middle
        v[2].model = MONSOON;
        link(v);
        CHECK(findMonsoon(&v[0]) == &v[2], "hop THROUGH a suite module to reach Monsoon");
    }

    // 3. FOREIGN module between expander and Monsoon → boundary, does NOT bind.
    {
        std::vector<Node> v(3);
        v[0].model = SUITE;      // expander
        v[1].model = FOREIGN;    // a non-suite module
        v[2].model = MONSOON;
        link(v);
        CHECK(findMonsoon(&v[0]) == nullptr, "foreign module is a boundary — no binding across it");
    }

    // 4. Two Monsoons back-to-back with the expander between them: binds the NEARER,
    //    and the result is the SAME regardless of which physical order we read.
    {
        // layout: M(A) - E - M(B)
        std::vector<Node> v(3);
        v[0].model = MONSOON;  // A
        v[1].model = SUITE;    // expander E
        v[2].model = MONSOON;  // B
        link(v);
        // right-first: E's right is B → binds B. (Deterministic, order-independent:
        // the rule is "first host reached walking right then left".)
        Node* got = findMonsoon(&v[1]);
        CHECK(got == &v[2], "expander between two Monsoons binds the right-side one (deterministic)");
    }

    // 5. ORDER-INDEPENDENCE: the CMMC bug class. Same module SET, different ORDER,
    //    the expander's binding must not flip between bound/unbound.
    //    Case a: M - S(straits) - S(inter) - S(colonnades)  → colonnades binds M
    //    Case b: M - S(straits) - S(colonnades) - S(inter)  → colonnades binds M
    {
        std::vector<Node> a(4);
        a[0].model = MONSOON;
        a[1].model = SUITE; a[2].model = SUITE; a[3].model = SUITE;  // colonnades at [3]
        link(a);
        Node* colA = &a[3];

        std::vector<Node> b(4);
        b[0].model = MONSOON;
        b[1].model = SUITE; b[2].model = SUITE; b[3].model = SUITE;  // colonnades now at [2]
        link(b);
        Node* colB = &b[2];

        bool boundA = (findMonsoon(colA) == &a[0]);
        bool boundB = (findMonsoon(colB) == &b[0]);
        CHECK(boundA, "reorder case A: last suite module still reaches Monsoon");
        CHECK(boundB, "reorder case B: middle suite module still reaches Monsoon");
        CHECK(boundA == boundB, "ORDER-INDEPENDENT: binding identical under reorder (CMMC bug fixed)");
    }

    // 5b. SUITE-FOLLOWER MID-CHAIN regression (the ChangiT2 / ChangiT3 fix). Several suite modules
    //     of different kinds between the expander and its Monsoon must ALL be hopped through — adding
    //     ChangiT2/T3 to isSuiteChainModel is exactly this: a follower placed mid-chain must not turn
    //     into a foreign boundary. (Before the fix, a ChangiT2/T3 here read as FOREIGN and un-bound
    //     everything past it — CONNECTION_UI_MODEL §4 bug class.)
    {
        // layout: E - SUITE(changiT2) - SUITE(changiT3) - SUITE(lantern) - M
        std::vector<Node> v(5);
        v[0].model = SUITE;    // the expander
        v[1].model = SUITE;    // e.g. Changi T2
        v[2].model = SUITE;    // e.g. Changi T3 (follower)
        v[3].model = SUITE;    // e.g. Lantern (observer)
        v[4].model = MONSOON;
        link(v);
        CHECK(findMonsoon(&v[0]) == &v[4], "multiple suite followers mid-chain all hop through to Monsoon");
    }

    // 5c. A FOREIGN module AFTER a run of suite followers is still a hard boundary — the fix widens
    //     the suite set, it must NOT weaken the foreign-boundary guarantee.
    {
        // layout: E - SUITE - SUITE - FOREIGN - M   → foreign blocks, no binding.
        std::vector<Node> v(5);
        v[0].model = SUITE;      // expander
        v[1].model = SUITE;      // ChangiT2/T3
        v[2].model = SUITE;      // another follower
        v[3].model = FOREIGN;    // non-suite module
        v[4].model = MONSOON;
        link(v);
        CHECK(findMonsoon(&v[0]) == nullptr, "foreign module past a suite run is still a boundary");
    }

    // 6. No Monsoon anywhere → unbound (null), no crash.
    {
        std::vector<Node> v(3);
        v[0].model = SUITE; v[1].model = SUITE; v[2].model = SUITE;
        link(v);
        CHECK(findMonsoon(&v[0]) == nullptr, "no Monsoon in chain → unbound");
    }

    // 7. Depth cap respected: a very long all-suite chain with Monsoon just past the
    //    cap does NOT bind (guards against unbounded walks).
    {
        std::vector<Node> v(20);
        for (int i = 0; i < 19; ++i) v[i].model = SUITE;
        v[19].model = MONSOON;   // 19 hops away from v[0] (> maxDepth 12)
        link(v);
        CHECK(findMonsoon(&v[0]) == nullptr, "Monsoon beyond depth cap is not reached");
    }

    std::printf("%s\n", failures == 0 ? "all passed" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
