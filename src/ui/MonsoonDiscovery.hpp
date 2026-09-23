#pragma once
// ── Monsoon expander discovery — the SINGLE, order-independent binding rule ───────────────────────
// Spec: docs/design/CONNECTION_MODEL_SPEC.md §1 (segment rule / claim-by-scan) + §1.2(a).
//
// PROBLEM this replaces: the old findMonsoonEitherSide walked left/right expander hops right-first,
// through ANYTHING, with NO boundary — so whether an expander "connected" depended on its DIRECTION to
// and HOP DISTANCE from the nearest Monsoon, i.e. its POSITION in the row (CONNECTION_UI_MODEL.md §4).
// Reordering the row changed the lit set. The fix: walk with the SAME boundary rules the
// MonsoonExpanderManager scan already uses (stop at a foreign module; a second Monsoon is a boundary;
// hop THROUGH recognised suite modules). That makes binding a function of TOPOLOGY (is there a Monsoon
// in my segment?), not order.
//
// The core walk is TEMPLATED on the node type so it is unit-testable without a real rack::Module
// (test/test_monsoon_discovery.cpp drives it with a fake node). Production instantiates it on
// rack::Module via the thin wrappers at the bottom.

#include <rack.hpp>
#include "../Monsoon.hpp"   // extern model globals (modelMonsoon, modelLantern, …)

namespace redDot {

// ── Recognised-suite predicate ────────────────────────────────────────────────────────────────────
// A module is part of the Monsoon suite chain (hop-through) iff its model is one of these. Anything
// else is FOREIGN and terminates the walk on that side. modelMonsoon is deliberately NOT in this set:
// it is a BOUNDARY (a second Monsoon starts a new segment), handled separately by the caller's IsHost.
//
// Keep this in lockstep with MonsoonExpanderManager's scan recognised-model list (the two must agree on
// what counts as "suite" vs "foreign", or connect-mark lighting and data-caching would disagree).
inline bool isSuiteChainModel(const rack::Model* m) {
    if (!m) return false;
    // Live suite models only. modelMonsoonChangeAlleyExpander / modelMonsoonTemasekExpander are
    // DECLARED extern in Monsoon.hpp but NOT DEFINED (deprecated/retired modules — see
    // CONNECTION_UI_MODEL.md §2 "Deprecated (ignore)"), so referencing them here would be a link
    // error. Keep this list in lockstep with MonsoonExpanderManager's scan recognised-model set.
    return m == modelMonsoonInterchangeExpander
        || m == modelMonsoonRafflesExpander
        || m == modelMonsoonJunctionExpander
        || m == modelMonsoonChangeAlleyV2
        || m == modelMonsoonStraitsExpander
        || m == modelMonsoonCausewayPolyExpander
        || m == modelMonsoonChangiExpander
        || m == modelMonsoonChangiT2Expander   // claimed expander (manager caches it) — was MISSING here,
                                               //   so a ChangiT2 mid-chain wrongly acted as a foreign
                                               //   boundary and un-bound everything past it (CONNECTION_
                                               //   UI_MODEL §4 bug class). Now hopped through like Changi.
        || m == modelMonsoonChangiT3Expander   // observer/follower (reads via IntertropicalPairing, not a
                                               //   Monsoon-claimed slot) — hop-only, same fix as Lantern.
        || m == modelMonsoonShophouseExpander
        || m == modelSikit
        || m == modelColonnades
        || m == modelColonnadesDuo
        || m == modelMonsoonShophouseMicro
        || m == modelMonsoonSandsVisualExpander
        || m == modelStraitsEastSandsVisual
        || m == modelStraitsSandsMacroVisual
        || m == modelLantern;   // observer — hop-only (never a boundary, never a host)
}

// ── Templated one-direction segment walk ────────────────────────────────────────────────────────
// Walk from `start` following NextFn (left or right expander) up to maxDepth hops. Return the first
// node for which IsHostFn(node) is true. STOP (return null) at the first node that is neither a host
// nor a recognised suite member (IsSuiteFn) — that node is a foreign boundary. A host is itself a
// boundary in the sense that the walk returns it (we never walk PAST a host looking for a farther one).
//
// Node must expose: ->model (comparable to rack::Model*), ->leftExpander.module, ->rightExpander.module
// (or whatever NextFn dereferences). The template keeps this pure/testable.
template <class Node, class NextFn, class IsHostFn, class IsSuiteFn>
inline Node* walkSegmentForHost(Node* start, NextFn next, IsHostFn isHost, IsSuiteFn isSuite,
                                int maxDepth = 12) {
    Node* curr = start;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (isHost(curr)) return curr;          // found the host for this segment
        if (!isSuite(curr)) return nullptr;     // foreign module (or a second host handled by isHost
                                                //  returning false here would be wrong — see note) → boundary
        curr = next(curr);
    }
    return nullptr;
}

// ── Both-sides host search (the order-independent replacement for findMonsoonEitherSide) ──────────
// Walk right, then left, each side bounded by the segment rule. Returns the first host found. Because
// each side stops at the first foreign module AND the walk returns the first host it reaches, a Monsoon
// separated from `self` by a foreign module is correctly NOT returned (different segment). A SECOND
// Monsoon farther out never "wins" over a foreign boundary nearer in — order-independent by construction.
template <class Node, class LeftFn, class RightFn, class IsHostFn, class IsSuiteFn>
inline Node* findHostBothSides(Node* self, LeftFn left, RightFn right,
                               IsHostFn isHost, IsSuiteFn isSuite, int maxDepth = 12) {
    if (!self) return nullptr;
    if (Node* h = walkSegmentForHost(right(self), right, isHost, isSuite, maxDepth)) return h;
    if (Node* h = walkSegmentForHost(left(self),  left,  isHost, isSuite, maxDepth)) return h;
    return nullptr;
}

}  // namespace redDot
