// test_sands_distribution_role.cpp — the distribution sub-expander's dual-role
// walk (src/dsp/SandsDistributionRole.hpp). Compile: see test/run_all.sh
// (header-only, no companion sources; rack-free — the walk is a pure template
// driven here by fake chain nodes).
//
// The spec (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md §DUAL ROLE) defines
// exactly two hosts: the MACRO visual (→ CROSS-VOICE) and the East visual (→
// PER-VOICE). The walk is LEFTWARD through the family chain with the segment
// rule; the safe default is PER-VOICE (CROSS-VOICE must be claimed by finding
// the Macro visual first). Covered:
//   1. host on the first hop (both hosts, both verdicts)
//   2. the safe defaults (bare left link, foreign boundary, non-host start)
//   3. hop-through: own-family subpanels between the expander and its host
//   4. the foreign boundary kills a farther host (the CONNECTION_UI_MODEL §4
//      bug class — a foreign mid-chain module must NOT pass the walk through)
//   5. first-host-wins (a host nearer the expander beats a farther one — the
//      walk never goes PAST a host)
//   6. the kMaxDepth guard (a pathological family run resolves, not hangs)
//   7. purity: the walk mutates nothing, repeated calls agree
#include <cstdio>
#include "SandsDistributionRole.hpp"   // via -Isrc/dsp (the run_all include set)

using namespace SandsDistributionRole;
static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    ++checks;
    if (!ok) { std::printf("  FAIL: %s\n", what); ++failures; }
}

// ── fake chain ────────────────────────────────────────────────────────────────
// Node mirrors the rack shape the production NextFn dereferences: ->model plus
// the left link. Models are address identities.
namespace {
struct Node {
    const void* model = nullptr;
    Node* left = nullptr;
};
const int mMacro = 0, mEast = 0, mExp = 0, mForeign = 0, mSuite = 0;   // identity anchors
#define MODEL(x) (&x)

Node* leftOf(Node* n) { return n ? n->left : nullptr; }
Host hostOf(const Node* n) {
    if (!n || !n->model) return Host::NONE;
    if (n->model == MODEL(mMacro)) return Host::MACRO;
    if (n->model == MODEL(mEast))  return Host::EAST;
    return Host::NONE;
}
bool isSuite(const Node* n) {
    if (!n || !n->model) return false;
    return n->model == MODEL(mMacro) || n->model == MODEL(mEast)
        || n->model == MODEL(mExp) || n->model == MODEL(mSuite);
}

// the production call shape, on fakes (expander node in, host verdict out)
Host resolveFrom(Node* expander) {
    return resolve(expander ? expander->left : nullptr, leftOf, hostOf, isSuite);
}
} // namespace

int main() {
    std::printf("== SandsDistributionRole (the dual-role walk) ==\n");

    // 1. host on the first hop
    {
        Node macro{MODEL(mMacro)}, east{MODEL(mEast)}, exp{MODEL(mExp), &macro};
        Node expE{MODEL(mExp), &east};
        check(resolveFrom(&exp) == Host::MACRO, "Macro visual on the first hop -> CROSS-VOICE");
        check(resolveFrom(&expE) == Host::EAST, "East visual on the first hop -> PER-VOICE claim");
    }

    // 2. the safe defaults
    {
        Node exp{MODEL(mExp), nullptr};
        check(resolveFrom(&exp) == Host::NONE, "bare left link -> PER-VOICE (safe default)");
        Node foreign{MODEL(mForeign)};
        Node expF{MODEL(mExp), &foreign};
        check(resolveFrom(&expF) == Host::NONE, "foreign left neighbour -> PER-VOICE");
    }

    // 3. hop-through: own-family subpanels between the expander and its host
    {
        Node macro{MODEL(mMacro)};
        Node mid{MODEL(mExp), &macro};       // another distribution expander in between
        Node exp{MODEL(mExp), &mid};
        check(resolveFrom(&exp) == Host::MACRO, "family subpanel hops through to the Macro host");
        Node east{MODEL(mEast)};
        Node suiteMid{MODEL(mSuite), &east}; // a suite (non-family, hop-legal) module in between
        Node expS{MODEL(mExp), &suiteMid};
        check(resolveFrom(&expS) == Host::EAST, "suite module hops through to the East host");
    }

    // 4. the foreign boundary kills a farther host
    {
        Node macro{MODEL(mMacro)};
        Node foreign{MODEL(mForeign), &macro};
        Node exp{MODEL(mExp), &foreign};
        check(resolveFrom(&exp) == Host::NONE,
              "foreign mid-chain module is a boundary even with a Macro behind it");
    }

    // 5. first host wins — the walk never goes PAST a host
    {
        Node east{MODEL(mEast)};
        Node macro{MODEL(mMacro), &east};    // Macro nearer, East farther
        Node exp{MODEL(mExp), &macro};
        check(resolveFrom(&exp) == Host::MACRO, "nearer Macro wins over a farther East");
    }

    // 6. the depth guard: a 20-deep family run with the host at the far end
    //    resolves PER-VOICE at kMaxDepth instead of hanging or walking on
    {
        Node macro{MODEL(mMacro)};
        Node* tail = &macro;
        for (int i = 0; i < 20; ++i) { Node* n = new Node{MODEL(mExp), tail}; tail = n; }
        Node* exp = tail;
        check(resolveFrom(exp) == Host::NONE, "depth exhaustion -> PER-VOICE (no hang, no walk-on)");
        while (exp != &macro) { Node* p = exp->left; delete exp; exp = p; }
    }

    // 7. purity: repeated calls agree (the walk mutates nothing)
    {
        Node macro{MODEL(mMacro)};
        Node exp{MODEL(mExp), &macro};
        const Host a = resolveFrom(&exp);
        const Host b = resolveFrom(&exp);
        check(a == b && a == Host::MACRO, "the walk is pure — repeated calls agree");
    }

    if (failures == 0) { std::printf("ALL PASS (%d checks)\n", checks); return 0; }
    std::printf("%d FAILURE(S) of %d checks\n", failures, checks);
    return 1;
}
