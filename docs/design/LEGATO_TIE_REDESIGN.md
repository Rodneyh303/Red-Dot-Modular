# LEGATO_TIE_REDESIGN — tie and legato are ONE concept (the label derives from pitch)

[The original design note lived in the pre-repository Melodicer tree. This file was
recreated 2026-11-22 to resolve the dangling references (RHYTHM_BEHAVIOUR_POLICIES.md,
SHOPHOUSE_SPEC.md, STEP_GATE_IMPLEMENTATION.md, the leading-edge comments in
SequencerEngine) and to record the implementation state.]

## IMPLEMENTED: tie is emergent (2026-11-22, CC_PROMPT_POLY_TIE_EMERGENT)

The ruling (Rodney): there is no separate tie decision and NO second tie control.
The legato knob decides whether notes JOIN (the held/slurred gate, the forward
commitment `slurForward`). Whether a join is a strict tie is just whether the pitch
drawn for the next note equals the held pitch:

- A TIE = the same degree AND the same octave — `SequencerEngine::isSamePitch`
  (the degree equality `sem == heldSem` PLUS the drawn-voltage test
  `|pitchV − heldPitchV| < kPitchEps`, `kPitchEps = 1e-4 V` ≈ 0.12 cents — far
  below the finest tuning step, far above float/table ULP dust, so microtonal
  tables and the quantiser's `quantize()` output compare consistently).
- Anything else is a slide (Legato).

Applied at every join: the mono join (`executeStep`), the poly S1 landing, the S2
mid-hold arm (merged into one held arm), and the onset-path slide. Poly voices
derive against their OWN held pitch — they no longer follow mono's tie verdict
(the old `connect && isTie` freeze is gone), which also removes the zero-interval
"Legato" a held quantiser CV used to produce.

The consequences that follow from the one rule: a same-degree/different-octave
join SLIDES to the new octave (the old degree-only mono compare silently discarded
it — `extendHold` never touches `currentPitchV`); a microtonal cents nudge of the
same degree across a mid-phrase retune stays a TIE; duplicate-pitch degrees (a
user scale with doubled notes) stay SLIDES (the degree term).

See `CC_PROMPT_POLY_TIE_EMERGENT.md` for the full change, and
`test/test_poly_tie_emergent.cpp` for the law's test (the schedule-oracle marches,
the coverage counters, and the mutation checks).
