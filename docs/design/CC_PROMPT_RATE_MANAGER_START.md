# CC PROMPT — Compute Rate Manager: the SINGLE rate authority (centralize ALL sequencing rate computation)

Implements RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md. Prereqs MET: resolver, strong types, SweepSig, AND the module-lookup authority all merged
(so adjacency/lookup is already event-driven + out of scope here — this prompt is PURE rate-gated work). REQUIREMENT (Rodney): this is NOT a partial framework — CENTRALIZE ALL sequencing
rate computation into one authority, and make bypassing it STRUCTURALLY IMPOSSIBLE (ideally a compile
error). A half-migration (manager + leftover raw dividers) is WORSE than none — it's two authorities, the
exact split this kills. Branch off lane-expander-refactor.

## ★ START HERE — STAGE 0: the Macro-only CPU fix (prove the framework on the measured 3%)
Rodney: start with the Macro CPU issue. Before the full migration, land the framework + the FIRST producer
= the measured Macro-only 3%, so the manager proves itself on a real win immediately.
MEASURED (MONSOON_PROFILE, both configs): Macro-only 3% vs East+Macro 1% is ENTIRELY the "sync" section
(controlDiv sync 515-578ns Macro-only vs 33ns East+Macro). ROOT CAUSE: MonsoonExpanderManager::sync, the
Macro-drives-poly population block (polyLenERef/polyOffERef/polyRotERef + spread taps, v=1..effPolyVoices
× 7 lanes) runs EVERY control cycle UNCONDITIONALLY when Macro is present. East-absent → Macro owns the
whole grid → full 15×7 re-population every cycle, IDENTICAL values ~1500×/sec = the ~500ns. (The
findMonsoonEitherSide walk there is already memoized via lookupMonsoon post module-lookup-authority — the
remaining cost is the POPULATION, not the walk.)
STAGE 0 = RateManager + the Control tier (driven by the existing controlDivider edge, behaviour-identical
wrapper) + make the sync-population a Control-rate PRODUCER with an input fingerprint (shadow macroBase
[7][*], effPolyVoices, the spread params the taps read, macro/east presence; memcmp each cycle; SKIP the
15×7 population when unchanged). Verify: MONSOON_PROFILE sync Macro-only == East+Macro; Rack both ~1%;
populated arrays BIT-IDENTICAL (perturb an input → re-runs; static → skips). This is the SweepSig pattern
as the first declareProducer. Land Stage 0 green before the rest.


## SCOPE — the SEQUENCING rate authority (what "all rate computation" means here)
IN SCOPE (the timing that computes the music — all must route through the manager, raw primitives DELETED):
- Monsoon.hpp lightDivider (~90Hz), controlDivider (~1500Hz).
- clock/phase → gridPulse / pulseEdge / sixteenthEdge / quarterEdge (Monsoon.cpp:949, the step/PPQN edges).
- the resolver step-cache (SequencerEngine effectiveProbability gating) + publishGlobal (MonsoonSandsManager,
  6 refs) + the merged SweepSig gate (StraitsSandsMacroVisual) — these are proto-producers; FOLD IN.
- the Macro sync-population block (MonsoonExpanderManager::sync, the profiled 3%).
- ModeController's controlDivider.process() gates (:~178 and 2 others), PatternEngine/SequencerEngine
  divider gates.
MODULE LOOKUP / ADJACENCY = NOT this prompt (its OWN authority): the lookup dividers (monLookupDiv/
itLookupDiv_/targetScanDiv_/hostLookupDiv_/ixScanDiv_/Changi) and the findMonsoon/resolveFollowed walks
are a SEPARATE concern — "who are the modules" vs "when does work run". They go to
CC_PROMPT_MODULE_LOOKUP_AUTHORITY (event-driven on onExpanderChange, one adjacency authority). Doing that
one first SIMPLIFIES this: the rate manager then owns ONLY rate-gated recompute (sync/resolver/SweepSig),
never adjacency. So this prompt is purely RATE-gated producers; the lookup dividers are deleted THERE, not
here.

## EAST-VISUAL DIVIDERS (CC found; disposition)
- probOutDiv (StraitsEastSandsVisual.cpp:1559, /32 + prob-CV-out recompute): SAME SweepSig/recompute-gate
  class as Macro's prob-out sweep → IN SCOPE, fold into the manager as a declared producer (same fingerprint
  pattern). Leaving it = two mechanisms for one pattern. Do it in the cross-module step.
- gateModDiv (StraitsEastSandsVisual.cpp:1503, /8 → sets gateModScan_): a Sample-tier EDGE-DETECTOR cadence
  (NOT a recompute gate — gates are ms, /8 catches every edge). Disposition: it is NOT in the recompute/
  Stage-0 work, BUT it is NOT "out of scope" either — edge detectors ARE Sample-tier detector-PRODUCERS in
  the design (gate-mode Step is triggered by a Sample-rate edge detector). So MIGRATE it through the manager
  as a detector-producer, BEHAVIOUR-IDENTICAL: preserve the /8 cadence exactly (changing /8 is a separate
  timing-ACCURACY question, not this plumbing migration). The end state has NO raw gateModDiv.process() —
  it is a manager-owned detector at /8. (Calling it "out of scope" would leave a raw divider = the straggler
  the enforcement-by-deletion forbids.)

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
