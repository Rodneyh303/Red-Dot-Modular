# CC PROMPT — Module-Lookup Authority: FINISH THE TAIL (convert remaining, flip gate fatal)

Continues CC_PROMPT_MODULE_LOOKUP_AUTHORITY (half landed, merged PR #293 @ c4c15ee). The service
(ModuleLookupCore/ModuleLookup), the event-driven epoch, the memoized accessors, and 13 module
conversions are DONE + green. FINISH the remaining ~38 residual sites, then flip the grep-gate fatal.
Branch off lane-expander-refactor. Each module ONE commit, build+test gated, behaviour-identical.

## THE WORKLIST (the gate prints it: `bash test/run_all.sh module_lookup -v`)
Convert each to the memoized accessors (lookupMonsoon / lookupPairHub<T> / lookupFollowed<T> /
lookupIntertropical), delete the module's lookup divider, drive its cache off onExpanderChange:
- StraitsBase.cpp (findMonsoonEitherSide × ~2 — host-Monsoon theme/value lookups)  ← Rodney: INCLUDE
- StraitsLaneExpander.cpp (findMonsoonEitherSide × ~7 — theme + mod-arc host lookups) ← Rodney: INCLUDE
- Intertropical.cpp/.hpp (delete hostLookupDiv_; getHost() → accessor)
- MicroTuning.cpp (ixScanDiv_ → epoch-gated bound-Interchanges rebuild; pairId one-shot PRESERVED)
- MonsoonChangeAlleyV2.hpp (sites in the .hpp — the fiddly header case)
- MonsoonInterchangeExpander.cpp/.hpp (delete targetScanDiv_)
- MonsoonStraitsExpander.cpp
- MonsoonSandsVisualExpander.hpp (its own divider pair)
- MonsoonShophouseMicro.cpp, MonsoonChangiT3Expander.cpp (residual sites), Sikit.cpp (any remaining)
- MonsoonExpanderManager.cpp/.hpp (residual walk sites)
- Monsoon.cpp/.hpp, StraitsSandsMacroVisual.cpp/.hpp (residual sites not caught in the first pass)
- src/ui/MonsoonDiscovery.hpp (residual)

## TWO SPECIAL CASES — judge per-site, don't blanket-convert
- src/ui/ConnectMark.hpp, src/ui/GoldPolyPort.hpp: findMonsoonEitherSide in WIDGET LAMBDAS (light-theme
  lookups at DRAW time, not audio hot path). Prefer the cached accessor for consistency, BUT if a widget
  has no stable module to key the memo (a free-standing light callback), a direct walk once-per-draw may
  be acceptable — mark it LOOKUP-AUTHORITY-ack'd (the gate's allow-marker) rather than forcing an unsafe
  conversion. Judge each; document which and why in the PR.
- HEADER sites (ChangeAlleyV2.hpp, SandsMacroVisual.hpp, Interchange.hpp, SandsVisualExpander.hpp): walks
  inlined in headers — route through the accessor the same way, but watch for include-order / the memo's
  thread-local being reachable from the header. These are the fiddly ones (ledger items 3-7).

## ALREADY HANDLED (don't re-do)
- src/deprecated/* : the gate ALREADY excludes them (test_module_lookup.cpp:191 — dead code, not in build).
  Do NOT convert deprecated files; they correctly don't count as residuals.

## THE FINAL COMMIT — flip the gate FATAL
When the gate's residual list is empty (excluding deprecated + any documented LOOKUP-AUTHORITY-ack'd
widget walks): flip L5 to FATAL BY DEFAULT (remove the residual-mode / make MODULE_LOOKUP_STRICT the
default). After this, a raw walk outside the service fails the build — the enforcement is live. Run the
full gates: run_all.sh, MODULE_LOOKUP_STRICT, UNIFORMITY_STRICT, both-flag clean rebuilds.

## CONTRACT / VERIFY
- Gate residual list EMPTY (bash test/run_all.sh module_lookup -v prints nothing, excl. deprecated +
  ack'd). Gate FATAL by default; a planted raw walk fails the build (prove it: add one, see red, remove).
- Adjacency bit-identical: host/target/followed resolve to the SAME module across connect/disconnect/
  reorder (Rack feel-test). The epoch-driven cache updates on onExpanderChange.
- MONSOON_PROFILE: lookup/walk cost ≈ 0 when the rack is static (no topology change → no walk), ALL
  modules now (not just the first 13). run_all.sh 79/79+ green; warning-clean build.
- No lookup dividers remain anywhere in the build (hostLookupDiv_/ixScanDiv_/targetScanDiv_/the
  SandsVisualExpander pair — all deleted).

## DONE =
- Every in-build residual converted (Straits base/lane INCLUDED); all lookup dividers deleted.
- ConnectMark/GoldPolyPort judged per-site (converted or ack'd, documented).
- Gate FATAL by default; planted-walk-fails-build proven. Full gates green.
- PR: converted-site count (should reach ~0 residual), the ack'd widget cases + why, Rack feel-test
  (reorder/reconnect re-links on the event; static lookup ≈ 0).

## NOT THIS PROMPT
- The rate manager (next, now trimmed to pure rate-gated recompute). LaneId→enum, slew re-test.
