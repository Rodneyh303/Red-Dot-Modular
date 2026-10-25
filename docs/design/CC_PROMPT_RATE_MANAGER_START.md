# CC PROMPT — Compute Rate Manager (FRAMEWORK) + first producer = the Macro-only CPU fix

Implements RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md. Prereqs MET (verified, trunk bb490a6): resolver
merged (effectiveProbability), strong types merged (VoiceSlot/LaneId), SweepSig gate merged. Build the
rate-manager FRAMEWORK, and land the Macro-only CPU fix AS ITS FIRST PRODUCER (not a throwaway gate).
Branch off lane-expander-refactor.

## THE CONCRETE TARGET (producer #1) — the MEASURED Macro-only 3%
Profiled (MONSOON_PROFILE, both configs): Macro-only 3% vs East+Macro 1% is ENTIRELY the "sync" section
(controlDiv sync 515-578ns Macro-only vs 33ns East+Macro). Root cause: MonsoonExpanderManager::sync, the
Macro-drives-poly population block (~line 380-520: polyLenERef/polyOffERef/polyRotERef + spread taps,
for v=1..effPolyVoices × 7 lanes) runs EVERY control cycle UNCONDITIONALLY when Macro is present. East-
absent → Macro owns the whole grid → full 15×7 re-population every cycle, recomputing IDENTICAL values
~1500×/sec = the ~500ns. The values change only when inputs change (macroBase knobs/CV, effPolyVoices,
spread params, presence). This is EXACTLY what the rate manager exists to own: control-rate work that
must be event-driven, not every-cycle.

## FRAMEWORK (the types + API — RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md is the authority)
- enum class Rate { Sample, Frame, Control, PPQN, Step, Phrase };  // identity, NOT an ordering
- RateTier { Rate; Source (AudioClock/VideoClock/WallClockDiv/Periodic/PhaseMap/GateEdge/TickCount);
  tick; generation }.  Per-MODE RateChain (clock/phase have PPQN, gate does NOT — structural).
- RateManager API: declareProducer(Rate at, fn, fingerprint) → SlotHandle; read(SlotHandle). The manager
  owns WHEN producers run; NO raw divider, NO public compute-now. Producer runs at its tier's tick AND
  only when its input FINGERPRINT changed (skip-on-unchanged — the SweepSig pattern, generalized).
- GENERALIZE publishGlobal (MonsoonSandsManager.cpp, 6 refs) and the SweepSig gate INTO this one
  producer/slot mechanism — do NOT leave two parallel change-detection patterns. publishGlobal and
  SweepSig are proto-producers; fold them in.
- Rate primitives to REPLACE incrementally (verified present): lightDivider ~90Hz (Monsoon.cpp:80),
  controlDivider ~1500Hz (:81), clock/phase→gridPulse (:949). controlDivider is already SR-aware (fixed
  Hz) — the manager generalizes that correct pattern. Frame = VideoClock, NEVER drives other tiers.

## MIGRATION — incremental, behaviour-identical, CPU only goes down
STAGE 1 (framework + first producer, THIS prompt's core):
- Introduce RateManager + the Control tier (driven by the existing controlDivider edge — a pure wrapper
  first, behaviour-identical).
- Make the Macro sync-population block a CONTROL-RATE PRODUCER with an input fingerprint: shadow
  macroBase[7][4] (len/off/rot/spread-relevant), effPolyVoices, the spread params the taps read, and the
  macro/east presence flags. memcmp each control cycle; SKIP the 15×7 population when unchanged. Only on
  fingerprint change → re-populate + update shadow. (Same discipline as the merged SweepSig gate, now
  expressed as declareProducer(Control, populate, fingerprint).)
- This makes sync Macro-only == East+Macro (idle ≈ 0 population) → the 3% → ~1%.
STAGE 2+ (follow-up, note but don't do now): convert lightDivider→Frame, gridPulse→Step producer, the
resolver→Step producer; cross-module shared manager; delete raw dividers. Per-mode chains. Leave these
staged — Stage 1 proves the framework AND fixes the measured 3%.

## CONTRACT / VERIFY
- MONSOON_PROFILE "sync": Macro-only == East+Macro (idle ≈ few ns, not 500). Build -DMONSOON_PROFILE,
  read both configs.
- Rack: Monsoon ~1% in BOTH configs; Macro-only no longer 3%.
- Populated engine arrays (polyLen/Off/Rot, spread-applied) BIT-IDENTICAL to ungated — a moving macroBase
  CV still re-populates (test: perturb a fingerprinted input → re-runs; static → skips). This is the
  invariant that stops a stale-population bug.
- run_all.sh green (incl the new producer skip-on-unchanged test); MONSOON_PROFILE OFF for release.

## DONE (Stage 1) =
- RateManager + Rate/RateTier/RateChain types + declareProducer/read API (framework).
- The Macro sync-population is a Control-rate producer gated on an input fingerprint; publishGlobal +
  SweepSig folded into the SAME producer/slot mechanism (not parallel).
- MONSOON_PROFILE: sync Macro-only == East+Macro; Rack both ~1%. Values bit-identical; skip test green.
- run_all.sh green. PR: sync ns before/after both configs + Rack %, and which existing gates were folded in.

## NOT THIS PROMPT
- Stage 2+ tier migrations (Frame/Step/PPQN producers, per-mode chains, raw-divider deletion) — staged.
- The slew re-test / the LaneId→named-enum refinement (separate follow-ups).
