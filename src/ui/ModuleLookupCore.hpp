#pragma once
// src/ui/ModuleLookupCore.hpp — THE MODULE-LOOKUP AUTHORITY, core (CC_PROMPT_
// MODULE_LOOKUP_AUTHORITY_START). The fourth authority: resolver = values, strong
// types = addressing, rate manager = timing, THIS = WHO (adjacency / host / follow).
//
// THE CONTRACT (Rodney's ruling): onExpanderChange is THE trigger — the epoch moves
// ONLY when an onExpanderChange handler fires. Every suite module overrides
// onExpanderChange to bump the epoch; resolution is event-driven, never polled.
//
// WHY AN EPOCH AT ALL (the multi-hop hole): Rack fires onExpanderChange only on the
// modules whose IMMEDIATE left/right slot changed (SDK Module.hpp: "Called after an
// expander is added, removed, or changed on either the left or right side of the
// Module"). A chain-walk result can change WITHOUT the walker's own slots changing:
// in [Monsoon][A][Macro], inserting a new Monsoon between Monsoon and A re-segments
// the row — Macro's findMonsoonEitherSide now resolves the NEW Monsoon — but Macro's
// left/right expanders are untouched, so Macro gets NO event and an event-only cache
// goes stale. The epoch is the event's FAN-OUT: any expander-slot change anywhere
// fires on SOMEONE's handler (Rack dispatches to every module whose slot moved), the
// bump is global, and every consumer converges on its next read. Static rack ⇒ the
// epoch never moves ⇒ zero walks. The only per-call cost is one relaxed atomic load
// and two pointer/integer compares.
//
// THE MEMO: consumers ask the SAME questions from two threads at different rates
// (engine process(); widget draw lambdas). lookupMemoizedT keys the answer on
// (self, key, epoch) per THREAD — one resolution per module per topology generation
// per thread, everything else is a cache hit. It is THE test seam: the rig drives it
// with fake nodes (the test_monsoon_discovery pattern) and COUNTS walks.
//
// This header is deliberately LEAN (nothing rack- or Monsoon-shaped) so the unit rig
// can include it; the Monsoon-typed accessors live in ModuleLookup.hpp.
#include <atomic>
#include <cstdint>

namespace redDot {

// The one global topology generation. Monotonic; starts at 1 so 0 means "never
// resolved" for first-time epoch caches.
inline std::atomic<uint64_t>& topologyEpoch() {
    static std::atomic<uint64_t> epoch{1};
    return epoch;
}

// THE event hook — call from every suite module's onExpanderChange override. This is
// the ONLY thing that moves the epoch (the event is the sole origin of re-resolution;
// nothing else may bump it — the grep-gate pins the call sites to handlers).
inline void bumpTopologyEpoch() {
    topologyEpoch().fetch_add(1, std::memory_order_relaxed);
}

// The explicit-cache guard, for modules that keep their own resolved members and
// re-resolve with the shared helpers (e.g. Monsoon::updateExpanderPointers): returns
// true exactly once per generation, then false until the next bump.
inline bool lookupEpochStale(uint64_t& cachedEpoch) {
    const uint64_t now = topologyEpoch().load(std::memory_order_relaxed);
    if (now == cachedEpoch) return false;
    cachedEpoch = now;
    return true;
}

// The memo core. `walk(self, key)` is THE resolution (a service-internal walk helper
// or the fake-node walk in the test — the key rides along so a parameterized
// question like resolveFollowedT(self, followId) fits the Walk signature exactly).
// `key` extends the identity when the question has a parameter (e.g. a followed-
// target id). Result is cached per (self, key, epoch) on this thread until ANY of
// the three moves.
template <typename Self, typename Key, typename Walk>
inline auto lookupMemoizedT(Self* self, Key key, Walk walk) -> decltype(walk(self, key)) {
    struct Memo {
        Self* self = nullptr;
        Key key = Key{};
        uint64_t epoch = 0;
        decltype(walk(nullptr, Key{})) result{};
    };
    thread_local Memo memo;
    const uint64_t e = topologyEpoch().load(std::memory_order_relaxed);
    if (memo.self == self && memo.key == key && memo.epoch == e)
        return memo.result;
    memo.self = self;
    memo.key = key;
    memo.epoch = e;
    memo.result = walk(self, key);
    return memo.result;
}

}  // namespace redDot
