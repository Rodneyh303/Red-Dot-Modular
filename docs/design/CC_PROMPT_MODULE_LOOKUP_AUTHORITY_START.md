# CC PROMPT — Module-Lookup Authority: ONE event-driven source for module adjacency / host / follow

The FOURTH authority (alongside resolver=values, strong-types=addressing, rate-manager=timing):
MODULE LOOKUP = "who is adjacent / who is my host / who do I follow". Today it is N modules each
PRIVATELY walking the rack to answer the SAME question, throttled by N private dividers. Centralize it:
resolve ONCE on topology change, publish, everyone reads. Branch off lane-expander-refactor. This is a
SIBLING of the rate-manager work (CC_PROMPT_RATE_MANAGER) — do it SEPARATELY; it de-tangles the rate
manager (which then owns only rate-gated recompute, not adjacency).

## THE DUPLICATION (surveyed)
Helpers EXIST but resolution is NOT centrally cached — every module walks the chain itself:
- Primitives (src/ui/VisualExpanderHelpers.hpp, src/ui/IntertropicalPairing.hpp): findMonsoonEitherSide,
  findPairHubEitherSide<T>, resolveFollowedT<T>, assignPairIdT<T>, findIntertropicalEitherSide — all walk
  leftExpander/rightExpander chains + APP->engine->getModule() rack scans.
- ~100+ CALL SITES across 10+ modules (StraitsEastSands 15, MonsoonStraits 11, Macro 9, Lantern 8,
  StraitsLane 7, MicroTuning 6, SandsManager 5, Sikit 5, ChangiT3 5, ChangeAlley 5, …) each calling a
  walk, many per-frame or divider-throttled (monLookupDiv, itLookupDiv_, targetScanDiv_, hostLookupDiv_,
  ixScanDiv_, Changi scan divs — the lookup dividers).
Same question, resolved over and over, in N places, at N rates. The functions are shared; the RESULT is
not cached centrally.

## THE AUTHORITY — resolve ONCE on topology change, publish, read
- Rack fires onExpanderChange() when adjacency changes. That is the ONLY time the walk result can change
  (Rack rewrites expander pointers between process() calls, never during). So resolution is EVENT-DRIVEN,
  NOT polled: resolve on onExpanderChange, cache the result, every consumer READS the cache.
- ONE lookup service: a module caches its own resolved adjacency (host Monsoon, followed target, pair hub,
  pairId) refreshed on onExpanderChange; consumers read the cached pointers, never walk. For cross-module
  reads (a module needs ANOTHER module's resolved view) the result is published where the reader can get it
  (the host's cached view, read-only).
- DELETE the lookup dividers (monLookupDiv/itLookupDiv_/targetScanDiv_/hostLookupDiv_/ixScanDiv_/Changi):
  with event-driven resolution there is nothing to throttle — the walk runs on the event, not on a timer.
- CONSISTENCY: every module resolves adjacency the SAME way (the shared helpers, called ONCE on the event),
  so there is ONE definition of "who is my host/target" — no module rolls its own walk inline.

## ENFORCEMENT ("consistent / can't bypass")
- The raw walk helpers (findMonsoonEitherSide/resolveFollowed/…) become INTERNAL to the lookup service;
  consumers call the service's cached accessor (e.g. module.host(), module.followed()), NOT the walk.
- DELETE the per-module lookup dividers → nothing to poll with.
- CI grep-gate: a walk helper (findMonsoon/resolveFollowed/leftExpander/rightExpander traversal) called
  OUTSIDE the lookup service / onExpanderChange handler → FAIL. (Walking the chain in a process()/draw()
  hot path is the anti-pattern this kills.)

## MIGRATION (incremental, behaviour-identical, one module per commit)
1. The lookup service: each module gets an onExpanderChange-driven resolve of its adjacency into cached
   members (most modules already cache — e.g. cachedMon_; the change is DRIVE IT FROM THE EVENT, not a
   divider). Start with Monsoon + Macro (the hot ones).
2. Convert each module's call sites from walk-every-N (divider) or walk-inline to READ the cached accessor.
   One module per commit; behaviour-identical (same pointer, resolved on the event instead of polled).
3. Delete the lookup dividers + any inline walks; add the CI grep-gate.
Behaviour is identical (same resolved pointers); only WHEN/WHERE they're resolved changes (event-once vs
poll-N). CPU win: N walks → resolve-on-change-only; the divider polls vanish.

## CONTRACT / VERIFY
- Adjacency behaviour unchanged: host/target/followed resolve to the SAME module as before, across
  connect/disconnect/reorder (Rack feel-test: move expanders around, everything re-links correctly on the
  event). A test that simulates onExpanderChange and asserts the cached accessor updates.
- Zero lookup dividers remain; zero walk-helper calls outside the service/event handler (grep-gate).
- MONSOON_PROFILE: the expander-lookup cost (ctrl_raf + any per-module walk) drops toward ~0 when the rack
  is static (no topology change → no walk). run_all.sh green.

## DONE =
- Event-driven (onExpanderChange) adjacency resolution; consumers read cached accessors; walk helpers
  internal-only; lookup dividers DELETED; CI grep-gate.
- Adjacency bit-identical across connect/disconnect/reorder; walk-when-static ≈ 0.
- run_all.sh + grep-gate green. PR: call sites converted, dividers deleted, before/after walk cost.

## RELATION TO THE RATE MANAGER
Do this SEPARATELY from CC_PROMPT_RATE_MANAGER. After this lands, the rate manager owns ONLY rate-gated
recompute (sync/resolver/SweepSig) — adjacency is NOT its concern (it's event-driven, owned here). The two
are complementary authorities: WHEN work runs (rate manager) vs WHO modules are (this). Either order works;
doing lookup first SIMPLIFIES the rate manager (drops its topology-producer).

## NOT THIS PROMPT
- Rate-gated recompute (rate manager). LaneId→enum, slew re-test (follow-ups).
