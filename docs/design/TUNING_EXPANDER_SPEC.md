# Sikit -- the Tuning Expander (Phase 1)

**Name: Sikit** (Malay, "a little" / "a small amount"). Semantically precise for a cents-adjustment
module; culturally coherent with Shophouse/Change Alley/Peranakan (established Malay/Singaporean
vocabulary); short single-word name matching the collection's panel rhythm; teaches the user immediately
what kind of module it is. Availability confirmed clear in the VCV Library.

A LIGHTWEIGHT tuning expander distinct from the Micro-12/24 microtonal work. Purpose: let the user
retune Monsoon's existing 12-degree system (cents-per-degree) WITHOUT touching the scale mask.
Referenced by MICROTONAL_MASTER as Phase 1 of the microtonal roadmap.

## The scenario it solves
User wants slightly-retuned 12-TET -- well-tempered, meantone, Pythagorean, kirnberger, stretched piano
tuning, personal expressive detune -- while STILL using their usual 12-tone scale library (major, minor,
Dorian, harmonic minor, etc.) from Monsoon's existing scale system. This is the most common real-world
microtonal use case; most users want the tuning flexibility without abandoning their scale library.

## Distinct from Micro-12 / Micro-24
The Micros (MONSOON_MICRO_SPEC) do the FULL microtonal work: .scl defines BOTH tuning and scale, custom
degree counts (12 or 24), authored mask on the expander. That's the ambitious "true microtonal / non-12
systems" case.
The Tuning Expander is the SIMPLE case: only cents adjustment, no scale-mask work, 12 degrees only. It's
a modifier on the existing system, not a replacement of it. Different job, different module.

Compositional principle at the module-boundary layer: ONE JOB PER MODULE. Tuning is one job; microtonal
tuning-AND-scale is a different job. Don't force both into one control surface.

## Panel (Sikit)
- Panel wordmark: "Sikit" in Barlow Black, dot.modular brand palette (Singapore red #d4001a for the
  dot glyph, gold #c8960c for accent, dark #070707 background). Compact single-word lockup.
- 12 CENTS knobs, one per degree (C, C#, D, ..., B). Equal-division default (0/100/200/.../1100 cents,
  = 12-TET), drag to detune. Same defaultCentsFor(i, 12) = i*100.
- Root (C) locked at 0 cents (Scalar rule).
- LABELS: standard note names (C, C#, D#, ..., B) -- meaningful because we're near 12-TET always.
- .scl READ from context menu (12-note .scl only; reject or warn on non-12-note files -- clear message
  pointing at Micro-24 for those). No .scl WRITE for v1 (add later if wanted).
- Standard ConnectMark (src/ui/ConnectMark.hpp) showing claim state.
- Small module -- ~8-12HP likely, fits alongside Monsoon.

## Delegation
- ONLY cents delegates. The scale mask stays with Monsoon's own scale faders (or Shophouse when
  attached). Monsoon's scale faders DO NOT blank when a Tuning Expander attaches -- they're doing a
  DIFFERENT job (scale enable/weight) than the Tuning Expander (cents).
- One tuning-authoring expander per Monsoon: Tuning Expander OR Micro-12 OR Micro-24, never more than
  one. Same single-owner discipline (which-Raffles, WriteLedger, ConnectMark greys the losers).
- Standalone (no Monsoon): ConnectMark greys, panel inert. Same idiom as everywhere.

## Engine integration -- reuses the TuningTable infrastructure from MICRO_TUNING_INTEGRATION_PLAN
The engine's shared TuningTable{N, cents[MAXN], weight[MAXN]} is what this expander populates:
- N = 12 (fixed).
- cents[0..11] = the 12 cents knob values.
- weight[0..11] = ignored / read from Monsoon's scale system as usual (the scale MASK stays with
  Monsoon; only cents come from this expander).
So this expander is a partial TuningTable writer -- it writes cents[], leaves weight[] to Monsoon. That
means the TuningTable writers need a small "which fields does this owner write?" discipline (WriteLedger
territory): Tuning Expander writes cents only; Micro-12/24 writes both. If the owner rules are clear,
there's no conflict.

## BUILD ORDER: this is Phase 1 of the microtonal roadmap (Rodney's insight)
The Tuning Expander is the NATURAL FIRST STEP for the microtonal work, because:
- It exercises the tuning-table infrastructure (cents[] array replacing hardcoded 12-TET voltages) with
  N still fixed at 12. NO pervasive-widening pain -- semiWeights[12], %12/*12 audits, sem->degree
  rename, all stay 12 for this phase.
- It ships REAL VALUE alone -- well-tempered, historical temperaments, expressive detuning, stretch --
  the majority of what most users actually want from microtonal.
- It de-risks the tuning-table plumbing at N=12 before tackling N=24. If Phase 1 ships clean, the
  engine-side machinery is already proven when Phase 2/3 land.

Refactored microtonal roadmap:
- PHASE 1 (this doc): Tuning Expander -- 12-note cents, .scl read, engine tuning-table wired in, N=12.
  Ships "retune 12-TET, keep your scale". Small module, small change.
- PHASE 2 (MONSOON_MICRO_SPEC Micro-12): adds mask-authoring on top of the same tuning-table (still
  N=12). Full 12-tone microtonal + Interchange modulation via pairing.
- PHASE 3 (MONSOON_MICRO_SPEC Micro-24 + MICRO_TUNING_INTEGRATION_PLAN engine widening): the
  pervasive N=24 rework, .scl for non-12, arbitrary Scala. Big scope; builds on Phases 1-2.

Test invariant across phases: Phase 2 with equal-division cents + all-faders-up = identical to Phase 1
with default cents. Phase 3 with N=12 = identical to Phase 2. Each phase reproduces the previous as a
special case; the engine layers on cleanly.

## Cross-refs
- MICROTONAL_MASTER.md -- entry point; add Tuning Expander as Phase 1.
- MONSOON_MICRO_SPEC.md -- Micro-12/24, Phases 2 + 3.
- MICRO_TUNING_INTEGRATION_PLAN.md -- shared TuningTable, engine seams.
- src/ui/ConnectMark.hpp -- claim/reject indicator.
