# ✅ PREREQUISITE MET — resolver merged (PR #290, trunk 2b3a308). START HERE.
Verified on trunk lane-expander-refactor: effectiveProbability present (6 refs),
macroOwnLaneProbability = 0, /32 + SweepSig = 0, East-leak fixed, CPU down. The clean resolver graph
exists → build the rate manager on it now.
KEY REUSE: publishGlobal (MonsoonSandsManager.cpp:784, 6 refs) is ALREADY a "publish at control rate,
consumers read" pattern = a proto-producer/slot. The rate manager GENERALIZES it — do NOT reinvent;
fold publishGlobal (and the resolver's step-cache) INTO the manager's declareProducer/slot API so there
is ONE producer/slot mechanism, not two. (That's the migration's first concrete win.)
Rate primitives to replace (verified present): lightDivider ~90Hz (Monsoon.cpp:77), controlDivider
~1500Hz (:78), clock/phase → gridPulse/pulseEdge (:946), ppqnSetting. POPULATION still from the
section-profile inventory (unchanged).

────────────────────────────────────────────────────────────────────────────────
# SPEC — Compute Rate Manager (FRAMEWORK) — unify the scattered rate control into one authority

Implements RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md. This spec is the FRAMEWORK (types, API, per-mode
chains, enforcement, migration path). The POPULATION (which quantity at which rate) is filled from the
SECTION-PROFILE inventory + the post-UI-to-resolver graph — DO NOT invent it here. BUILD ORDER: section
profile + point-fix → UI-to-resolver → THIS. (Building the manager before the resolver = scheduling a
graph the resolver migration rewrites; building before the profile = scheduling a guess.)

## WHAT IT REPLACES (current scattered primitives — verified)
- lightDivider (~90Hz, Monsoon.cpp:49 = sampleRate/90) — UI/light rate.
- controlDivider (~1500Hz, Monsoon.cpp:50 = sampleRate/1500) — DNA/control. NOTE: already SR-aware
  (fixed Hz regardless of SR) — the manager GENERALIZES this correct pattern.
- clock.process / phase.process → pulseEdge / sixteenthEdge / quarterEdge (Monsoon.cpp:745) — PPQN/step
  edges, mode-dependent (clock vs phase origin).
- ppqnSetting, stepIndex, ad-hoc "if (gridPulse)" / "if (divider.process())" scattered through process().
These become DECLARATIONS to the manager, not hand-rolled gates.

## THE TYPES
```
enum class Rate { Sample, Frame, Control, PPQN, Step, Phrase };   // identity only — NOT an ordering/subset

struct RateTier {
    Rate rate;
    // source: how this tier's TICK is produced (mode-dependent — see chains)
    enum class Source { AudioClock, VideoClock, WallClockDiv, Periodic, PhaseMap, GateEdge, TickCount };
    Source source;
    // tick(): did this tier fire THIS sample? (cheap — edge/counter/compare only)
    bool tick = false;
    uint64_t generation = 0;   // bumped each tick — consumers detect "new value available"
};

// A mode declares its CHAIN: the ordered tiers it uses + each tier's source. ONE per mode.
struct RateChain {
    // CLOCK: {Sample:AudioClock} {PPQN:Periodic} {Step:TickCount(N·PPQN)} {Phrase:TickCount(N·Step)}
    // PHASE: {Sample:AudioClock} {PPQN:PhaseMap}  {Step:TickCount} {Phrase:TickCount}
    // GATE:  {Sample:AudioClock} {Step:GateEdge}  {Phrase:TickCount}      // NO PPQN tier
    // Frame/Control are ALWAYS present (independent clocks), never driven by the others.
    std::vector<RateTier> tiers;   // only the tiers THIS mode uses; PPQN absent in gate
    bool has(Rate r) const;        // "is this tier in the active chain" — structural, not a flag
};
```

## THE MANAGER API (what producers/consumers use — NO raw dividers anywhere)
```
class RateManager {
  // per block: advance all active-chain tiers' tick state from their sources (cheap edge/counter)
  void beginSample(const ProcessArgs&, Mode activeMode, /*clock/phase edges*/ ...);

  bool ticked(Rate r) const;              // did tier r fire this sample? (gate dispatch)
  uint64_t generation(Rate r) const;      // for slot-freshness

  // PRODUCER: register a quantity computed at a tier. The manager calls produce() only on that tier's
  // tick, ONCE, batched. The result goes to a published SLOT.
  template<class T> SlotHandle<T> declareProducer(Rate at, std::function<void(T&)> produce);

  // CONSUMER: read the latest published slot value. A consumer at a SLOWER rate reads the most-recent
  // faster-produced value (independent clocks + published slot = always fresh). A consumer CANNOT
  // request a tier absent from the active chain (compile/assert), CANNOT force a faster compute.
  template<class T> const T& read(SlotHandle<T>);
};
```
ENFORCEMENT: there is NO public "compute now" and NO raw divider. A producer only DECLARES (Rate, fn);
the manager owns WHEN it runs. "Step work per sample" is UN-WRITABLE — you can't call the heavy fn
yourself; you declare it at Step and the manager ticks it at Step.

## PER-MODE CHAINS (the single timing authority — ONE place)
Define the three chains ONCE (a function Mode→RateChain). This is the ONLY place timing topology lives:
- Step/Phrase tiers are MODE-INDEPENDENT (TickCount of the tier below); only the PPQN/Step SOURCE differs.
- PPQN tier PRESENT (clock/phase) / ABSENT (gate) — structural (not in the gate chain's vector).
- Frame (VideoClock) and Control (WallClockDiv) are independent, always present, never divide others.
Mode switch = swap the active chain. Producers/consumers unchanged.

## SAMPLE-RATE + FRAME-RATE INVARIANTS (tests, per the design)
- SR-UP: non-Sample tiers fire the same times/second → their per-sample cost FALLS. TEST: raise SR,
  assert non-Sample producer call-COUNT per second is unchanged (only Sample-tier work scales).
- FRAME-RATE: changing Frame rate changes ONLY Frame-tier producer cadence; PPQN/Step/Phrase/Control
  unchanged. TEST: vary frame rate, assert Step/PPQN tick cadence invariant.
- Gate/Phase Step is TRIGGERED by a cheap Sample-tier detector (GateEdge/PhaseMap) that is SEPARATE
  from the heavy Step producer. TEST: the heavy Step producer runs at Step count, not per sample.

## MIGRATION PATH (incremental — do NOT big-bang)
1. Introduce RateManager + the three chains; drive its Sample/Control/Frame/PPQN/Step/Phrase ticks from
   the EXISTING clock/phase/divider edges (so behaviour is identical — a pure wrapper first).
2. Convert controlDivider consumers → declareProducer(Control, ...). Then lightDivider → Frame. Then the
   gridPulse/step blocks → Step producers. One divider-site at a time, each a no-op-behaviour commit.
3. The heavy RESOLVER (post UI-to-resolver) → declareProducer(Step, ...) batched over all (voice,lane);
   bars read the Step slot + thin Frame overlay; outputs read the Step slot latched. (This is where the
   per-sample resolver cost dies.)
4. CROSS-MODULE: Macro/expanders get the SAME RateManager (injected), read the SAME slots — no module
   recomputes (kills the Macro-124% cross-module recompute class).
5. DELETE the raw dividers + scattered "if (divider.process())" once all consumers are declarations.
Each step: run_all.sh green + Rack CPU-meter check (CPU should only ever go DOWN).

## POPULATE-FROM-MEASUREMENT (the deferred part — fill AFTER the profile)
The REGISTRATION TABLE (quantity → rate → consumers) is built from the section-profile inventory +
the post-resolver graph. Leave a TODO table in the code; populate it with the measured quantities.
Do NOT guess the table now.

## DONE (framework) =
- RateManager + Rate/RateTier/RateChain types; the three per-mode chains in ONE place; declare
  Producer/read API; enforcement (no raw divider, no public compute-now).
- Migration steps 1-2 landed as behaviour-identical wrappers (existing edges drive the manager; one
  divider converted). SR + frame-rate invariant tests green. run_all.sh green.
- Registration table stubbed with a populate-from-profile TODO.
- PR: "framework only; populate + steps 3-5 after the profile + UI-to-resolver; CPU unchanged-or-lower."

## NOT THIS SPEC
- Populating the quantity table (needs the profile).
- The resolver migration (its own spec; prerequisite for step 3).
