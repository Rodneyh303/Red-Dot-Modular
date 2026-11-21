#pragma once
// src/ui/ModuleLookup.hpp — THE MODULE-LOOKUP AUTHORITY, typed accessors
// (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START). What consumers call. The raw walk
// helpers (findMonsoonEitherSide / findPairHubEitherSide / resolveFollowedT /
// isConnectedAndClaimed / assignPairIdT) are SERVICE-INTERNAL after this lands:
// a call site outside this header family (ModuleLookup*.hpp, VisualExpanderHelpers
// .hpp, IntertropicalPairing.hpp, MonsoonDiscovery.hpp) or outside an
// onExpanderChange handler fails the CI grep-gate (test_module_lookup's structural
// half — walking the chain in a process()/draw() hot path is the anti-pattern this
// authority kills).
//
// RATES: the memo (ModuleLookupCore.hpp) keys on (module, key, epoch). Engine
// process() and widget draw lambdas both pay one relaxed atomic load + compares;
// the WALK happens once per module per topology generation per thread. The lookup
// dividers (monLookupDiv / itLookupDiv_ / targetScanDiv_ / hostLookupDiv_ /
// ixScanDiv_ / Changi scan divs) are deleted as each module migrates — with
// event-driven resolution there is nothing to throttle.
//
// BEHAVIOUR-IDENTITY: same resolved pointers as the polled/divider versions (the
// walks are the same shared helpers), only WHEN they run changes (event-once via
// the epoch, not every-N) — and strictly fresher: a divider could be up to 512
// samples stale; the epoch converges on the next call after the event.
#include "ModuleLookupCore.hpp"
#include "VisualExpanderHelpers.hpp"   // findMonsoonEitherSide / isConnectedAndClaimed (service-internal)
#include "IntertropicalPairing.hpp"    // findPairHubEitherSide / resolveFollowedT (service-internal)

namespace redDot {
namespace detail {
// Named (not lambda) walk thunks so every TU instantiates the SAME memo — one
// resolution per generation shared across the whole plugin, not per-TU. All take
// (module, key); the keyless ones ignore it (the memo's Walk signature is uniform
// so resolveFollowedT — whose int IS the key — fits directly).
inline Monsoon* walkMonsoon_(rack::Module* m, int) { return findMonsoonEitherSide(m); }
inline bool walkConnectedAndClaimed_(rack::Module* m, int) { return isConnectedAndClaimed(m); }
template <class T>
inline T* walkPairHub_(rack::Module* m, int) { return findPairHubEitherSide<T>(m); }
}  // namespace detail

// "Who is my Monsoon?" — the segment-rule host, memoized per (module, epoch).
inline Monsoon* lookupMonsoon(rack::Module* self) {
    return lookupMemoizedT(self, 0, &detail::walkMonsoon_);
}

// "Am I a connected + claimed expander?" — the ConnectMark question, memoized.
inline bool lookupConnectedAndClaimed(rack::Module* self) {
    return lookupMemoizedT(self, 0, &detail::walkConnectedAndClaimed_);
}

// "Which pair hub is mine?" — the nearest hub of type T either side, memoized.
template <class T>
inline T* lookupPairHub(rack::Module* self) {
    return lookupMemoizedT(self, 0, &detail::walkPairHub_<T>);
}

// "Who do I follow?" — followId > 0: the hub with that pairId anywhere in the rack
// (a getModuleIds scan); followId == 0: the nearest hub. The followId is part of the
// memo key (turning the knob re-resolves; a topology change re-resolves via the epoch).
template <class T>
inline T* lookupFollowed(rack::Module* self, int followId) {
    return lookupMemoizedT(self, followId, &resolveFollowedT<T>);
}

// The Intertropical-specialized forms (Lantern / ChangiT3 follow an IT hub).
inline Intertropical* lookupIntertropical(rack::Module* self, int followId) {
    return lookupFollowed<Intertropical>(self, followId);
}

}  // namespace redDot
