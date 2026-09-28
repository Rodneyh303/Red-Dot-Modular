# dot.modular — vision

*The headline document. Everything else in docs/design/ is detail beneath this. The `00_` prefix keeps
it first in the listing. If you read one doc, read this one.*

## One line
**New ways of combining and exploring order and chaos.**

## What it is
A **generative sequencer-quantiser**: a generative sequencer and a quantiser that are the same
instrument at opposite ends of one continuous, per-voice axis. Reading it one way, it is a quantiser
that may choose a generated pitch instead; the other way, a sequencer that may choose a quantised one.
The interesting music is in between, where different voices sit at different points.

This is a new way to INTERACT with sequencers — not sequencers sequencing sequencers, but an instrument
that takes as much or as little of your timing and pitch as you hand it and generates the rest. Patch a
plain SEQ-8 in and it becomes an ensemble.

## The idea, and why the parts are one idea (not a feature pile)
Everything is the same thing surfaced at a different layer: **navigable, reversible, correlated
generative state.** Two independent axes carry the musical stakes:
- **Probability** — order↔chaos in TIME and PITCH: how predictable each voice's material is.
- **Correlation** — order↔chaos in the ENSEMBLE: how much the voices agree.
Most generative modules give one knob for "amount of randomness". This gives a FIELD, because those two
axes are independent — a rigid rhythm under chaotically independent voices, or tightly correlated voices
playing an unpredictable line.

And it is NAVIGABLE: scrub, reverse, seed offset and the seeds let you move through that field and come
BACK. Chaos you cannot return from is noise; chaos you can reverse out of is exploration. Most
generative modules are fire-and-forget — you seed them and watch. This one you PERFORM.

## The derivation that gives the project its spine
The instrument was not assembled from features; one decision entailed the next.
1. **Make correlation first-class.** Once voices can be grouped, led, mirrored and graded, you are no
   longer describing a random source — you are describing how PLAYERS relate: heterophony, hocket,
   stratification, antiphony.
2. **Those grammars are largely non-Western.** Heterophony is the default texture of maqam, of gamelan,
   of much of the world's music; independent voices and rule-governed counterpoint are the Western
   outlier. So the moment correlation became expressive, the instrument leaned toward the traditions
   that think in those terms — by its structure, not by decoration.
3. **Those traditions are not 12-TET.** A heterophonic maqam line in equal temperament is not a maqam
   line — the microtones ARE the idiom. So microtonality was not a feature to take or leave; it became
   NECESSARY, almost by entailment, to make the textures true rather than approximate.
A technical decision (correlation first-class) had a musical consequence (non-Western ensemble textures
become natural) that forced a second technical decision (the tuning system must be open). Stated
honestly: this builds a SPACE where those musics are expressible and natural — not a reproduction or a
museum of them.

## The feature set (each an expression of the one idea)
- **Sequencer-quantiser axis** — the reframe; the continuous per-voice pitch origin. [three timing
  origins — clock/gate/phase — × the q-mix pitch axis]
- **Probability + correlation, controllable / visualisable / modulatable / REVERSIBLE** — the spine.
  Probability is the what, correlation is the who-with, reverse is what almost nothing else has because
  almost nothing else is built on a counter-addressed spine that makes it free.
- **MPE out and microtonal generation** — the expressive output side, in tunings most of the ecosystem
  cannot reach; where the derivation above becomes audible.
- **Lantern** — the observability layer; a generative system you can see inside is one you can trust and
  steer.
- **Combined arranger / sequential switch (Intertropical)** — the structural layer, arranging voices and
  scenes and now carrying correlated expression through the SAME mapping as the notes.

## The public one-liner
> dot.modular — new ways of combining and exploring order and chaos. A generative sequencer-quantiser
> where probability and correlation are independent, visible, modulatable and reversible — so you
> perform the space between order and chaos rather than dialling in an amount of randomness.

## Where the detail lives (predecessor docs)
- **Modes / the two axes** — MODE_COLLAPSE_6_TO_3.md, QUANTISER modes notes.
- **Correlation model** — SPREAD_TARGET_MODES.md (graded correlation, follow-CA), CA_* docs,
  CONNECTION_UI_MODEL.md / CONNECTION_MODEL_SPEC.md.
- **Reversibility / navigation** — DICE_SCRUB_SLEW_B2.md, SLEW_COPULA_PLAN.md,
  CA_DICE_COUNTER_MODEL.md (true reverse), CRAB_CANON_RECIPE.md (seed offset).
- **Distribution fidelity** — UNIFORM_MARGINALS_COPULA_PLAN.md (why order↔chaos changes RELATIONSHIPS,
  never each voice's distribution).
- **Expression / MPE / microtonal** — MPE_UTILITY_BUILD_SPEC.md (Keppel), CA_EXPRESSION_CV_CORRELATION.md,
  CORRELATED_POLY_MODULATION.md, the tuning/Colonnades/Shophouse docs.
- **Arranger** — INTERTROPICAL_SPEC and related.
- **Craft & consistency (the finish, not the vision)** — PANEL_CRAFT_AUDIT.md,
  PANEL_KIT_CONSISTENCY_AUDIT.md.

## Status (2026-09)
Feature vision essentially COMPLETE — the last new idea (collapse 6 modes to 3) was a simplification, not
an addition, which is the signal of completeness. Remaining: connection rework (the one bug users feel),
the mode collapse, the seed-offset knobs, then the CRAFT pass (panel framework, one visual system) and
release materials (docs, sample patches incl. the SEQ-8 demo, DAW setups, .dmtune files). Target: VCV
Library, H1 2027.
