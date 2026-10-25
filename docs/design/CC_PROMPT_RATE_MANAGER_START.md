# CC PROMPT — Compute Rate Manager: the SINGLE rate authority (centralize ALL sequencing rate computation)

Implements RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md. Prereqs MET (trunk d4637a9): resolver, strong types,
SweepSig all merged. REQUIREMENT (Rodney): this is NOT a partial framework — CENTRALIZE ALL sequencing
rate computation into one authority, and make bypassing it STRUCTURALLY IMPOSSIBLE (ideally a compile
error). A half-migration (manager + leftover raw dividers) is WORSE than none — it's two authorities, the
exact split this kills. Branch off lane-expander-refactor.

## SCOPE — the SEQUENCING rate authority (what "all rate computation" means here)
IN SCOPE (the timing that computes the music — all must route through the manager, raw primitives DELETED):
- Monsoon.hpp lightDivider (~90Hz), controlDivider (~1500Hz).
- clock/phase → gridPulse / pulseEdge / sixteenthEdge / quarterEdge (Monsoon.cpp:949, the step/PPQN edges).
- the resolver step-cache (SequencerEngine effectiveProbability gating) + publishGlobal (MonsoonSandsManager,
  6 refs) + the merged SweepSig gate (StraitsSandsMacroVisual) — these are proto-producers; FOLD IN.
- the Macro sync-population block (MonsoonExpanderManager::sync, the profiled 3%).
- ModeController's controlDivider.process() gates (:~178 and 2 others), PatternEngine/SequencerEngine
  divider gates.
ALSO IN SCOPE — the LOOKUP DIVIDERS are N copies of ONE question (Rodney): monLookupDiv
(findMonsoonEitherSide), Lantern itLookupDiv_ (resolveFollowedIT), Interchange targetScanDiv_
(resolveFollowedT), Intertropical hostLookupDiv_, MicroTuning ixScanDiv_, Changi scan divs — these each
PRIVATELY POLL the SAME underlying thing: the rack EXPANDER/MODULE ADJACENCY ("who is my host / target /
followed module"). N modules × N private ClockDividers × N copies of the same findMonsoon/resolveFollowed
walk. That IS the duplication this effort kills — one question computed in N places at N rates.
=> UNIFY into ONE TOPOLOGY-RESOLUTION authority, and make it EVENT-DRIVEN, not polled: Rack fires
onExpanderChange when adjacency changes, so resolve the topology/adjacency ONCE on that event, PUBLISH to
a slot, and every module READS the slot (no private walk, no private divider). This is a different
PRODUCER KIND than the rate-gated recomputes (it's event-gated on topology-change, not rate-gated), but it
belongs to the same manager as a producer. The lookup dividers DELETE (their walks collapse to one shared
resolve + slot reads). Likely a further CPU win (N adjacency walks → 1; poll → event).
  TWO PRODUCER KINDS the manager hosts: (a) RATE-gated recompute-on-input-change (sync, resolver, SweepSig
  — fingerprint + tier tick); (b) EVENT-gated topology resolution (the lookup dividers → one
  onExpanderChange-driven adjacency slot). Both: computed ONCE, read by all, bypass impossible.
  CROSS-MODULE: the topology slot is read by Lantern/Macro/Interchange/MicroTuning/Changi (different
  modules) — so the adjacency authority must be reachable across modules (a shared resolve + published
  result), the one genuinely cross-module piece. Do it carefully; it's the highest-value unification.

## THE ENFORCEMENT — "compile error to not use it" = DELETE THE ALTERNATIVES
There is no C++ type that makes "I gated work on a raw divider" fail to compile (timing is control flow,
not a typed value). So enforcement = REMOVE THE RAW PRIMITIVES so there is nothing to bypass WITH:
1. DELETE the in-scope ClockDivider members (lightDivider, controlDivider) and the raw gridPulse/pulseEdge
   reads from Monsoon. The ONLY way to get a Control/Step/PPQN/Phrase tick becomes rateManager.ticked(r)
   / a declared producer. A deleted member cannot be .process()'d — deletion IS the compile-time enforcement.
2. PRODUCERS ARE MANAGER-DISPATCHED: work is a lambda passed to declareProducer(Rate, fn, fingerprint);
   the manager is its ONLY caller (no public "run now"). "Run at the wrong rate" is unwritable — you
   declare a rate, the manager dispatches.
3. CI GREP-GATE (backstop for reintroduction): a test/lint that FAILS if any IN-SCOPE file reintroduces a
   raw sequencing-rate gate — `ClockDivider` member in Monsoon.hpp/engines, `.pulseEdge`/`gridPulse`
   outside the manager, raw `% controlRate`. (The grep-gate is the belt; deletion is the braces.)

## FRAMEWORK (RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md is the authority)
- enum class Rate { Sample, Frame, Control, PPQN, Step, Phrase } (identity, not an ordering).
- RateTier { Rate; Source (AudioClock/VideoClock/WallClockDiv/Periodic/PhaseMap/GateEdge/TickCount); tick;
  generation }. Per-MODE RateChain (clock/phase have PPQN; gate does NOT — structural absence).
- RateManager: declareProducer(Rate, fn, fingerprint)→Slot; read(Slot); ticked(Rate). Manager owns WHEN;
  producer runs at its tier's tick AND only when its input fingerprint changed (skip-on-unchanged — the
  SweepSig pattern, now the ONE change-detection mechanism; publishGlobal + SweepSig fold into it).
- Frame=VideoClock, NEVER drives other tiers (frame-rate independence). controlDivider's SR-aware pattern
  (fixed Hz regardless of sample rate) is the model the Control tier generalizes.

## MIGRATION (one in-scope site per commit, behaviour-identical, harness + CPU green each step)
1. RateManager + the per-mode chains + Control/Frame/Step/PPQN/Phrase tiers, driven FIRST by the existing
   edges (pure wrapper — behaviour-identical). Harness green.
2. Convert each in-scope gate to a producer/ticked() call, ONE per commit: controlDivider consumers →
   ticked(Control); lightDivider → ticked(Frame); gridPulse → Step producer; the resolver/publishGlobal/
   SweepSig/Macro-sync → declared producers with fingerprints. Harness + CPU green after each.
3. DELETE the raw in-scope ClockDivider members + raw edge reads. Add the CI grep-gate. Nothing in-scope
   gates on a raw primitive anymore.
The MACRO-ONLY 3% dies in step 2 (the sync-population producer with its fingerprint = sync Macro==East).

## CONTRACT / VERIFY
- Macro-only == East+Macro in MONSOON_PROFILE sync; Rack both ~1%; the 3% gone.
- Behaviour bit-identical through every step (harness green each commit; the music is unchanged — this is
  pure timing-plumbing centralization + the one skip-on-unchanged win).
- ZERO in-scope raw rate primitives remain: grep ClockDivider in Monsoon.hpp/engines = 0 (in-scope ones);
  grep gridPulse/pulseEdge outside the manager = 0. The CI grep-gate enforces it.
- run_all.sh green incl the grep-gate test + the sync skip-on-unchanged test. MONSOON_PROFILE OFF for release.
- SR + frame-rate invariant tests: raising SR scales ONLY Sample-tier cost; changing frame rate perturbs
  ONLY Frame consumers (per the design).

## DONE =
- RateManager is the SOLE source of in-scope ticks; raw in-scope dividers/edges DELETED; producers are
  manager-dispatched lambdas; publishGlobal + SweepSig folded into the one producer/slot mechanism.
- CI grep-gate forbids reintroducing a raw sequencing-rate gate.
- Macro-only ~1%; behaviour bit-identical; run_all.sh + grep-gate + SR/frame invariants green.
- PR: the in-scope sites converted, the OUT-of-scope raw dividers named + why excluded, sync ns + Rack %.

## NOT THIS PROMPT
- LaneId→named-enum refinement; the slew re-test — separate follow-ups.
