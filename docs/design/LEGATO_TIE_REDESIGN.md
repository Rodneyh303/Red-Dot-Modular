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

- A TIE = the same degree AND the same octave — `GateState::isSamePitch`
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
it — the old `extendHold` never touched `currentPitchV`); duplicate-pitch degrees
(a user scale with doubled notes) stay SLIDES (the degree term); and a RETUNED
degree SLIDES — a cent is 8.3e-4 V against an epsilon of 1e-4 V, so a one-cent
nudge is 8x the epsilon and breaks the tie, with the gate continuing and the pitch
following the retuning. (An earlier version of this note claimed the opposite. The
epsilon is deliberately BELOW anything a user can author: it admits numerical dust
only. See TIE_UNDER_MICROTONALITY.md, which also latches the cents cadence to the
STEP boundary so that slide lands on a quantised edge.)

See `CC_PROMPT_POLY_TIE_EMERGENT.md` for the full change, and
`test/test_poly_tie_emergent.cpp` for the law's test (the schedule-oracle marches,
the coverage counters, and the mutation checks).

## IMPLEMENTED: the join is ONE code path (2026-11-23, commit 8e4ab7b5)

Tie stopped being a decision above; this stops it being a code path. The
`extendHold`/`slideNote` pair — picked by each caller from its own copy of the
predicate — is now one `GateState::continueNote`. It writes the drawn pitch and
degree on EVERY path, derives the Tie/Legato label from the same predicate the
action uses, and RETURNS it so no caller repeats the test. The tie law
(`kPitchEps`, `isSamePitch`/`isSameAsHeld`, `continueNote`) lives in `GateState` as
one unit, so a join can no longer pair the wrong test with the wrong action, and
the octave-discard class is unrepresentable rather than merely unreached.

Byte-identical on every pinned digest (`test_poly_tie_emergent`, the quantiser
fixtures, `GOLDEN_A/B/BS`, `test_stage5_combined`): in practice a tie draws the
bit-identical pitch, so storing it changes nothing. See
`test/test_continue_note.cpp` for the invariant and the mutation log (two
survivals recorded as structural, with the reasons).
