# CC PROMPT — advance-on-tie-into-rest becomes PROBABILISTIC (option 2) — unify with step-advance

CONTEXT (Rodney): step-advance made the RISE advance probabilistic (Variation draw). advanceOnTieIntoRest is
a SECOND advance site (on the gate FALL, into a rest, when tieAcrossRests && advanceOnTieIntoRest). It is
currently DETERMINISTIC (always advances when the flag conditions hold). DECISION: OPTION 2 — unify it with
the probabilistic advance. (Option 3, "lose the flag, always probabilistic," is possible later if Rodney
decides the flag isn't worth keeping — leave a comment to that effect.)

## WHAT advanceOnTieIntoRest DOES (so the change is clear)
SequencerEngine::legatoCheckpointOnFall() (SequencerEngine.cpp ~:890): on the gate FALL with a pending slur
(flags on), it UNCONDITIONALLY advancePlayhead() one step INTO the rest position and re-draws there — the
legato draw at the rested step decides whether the slur SURVIVES across the rest or ENDS. Advancing lets the
rest step speak (re-draw legato → may end the chain); NOT advancing keeps the rest passive (chain carries).
Called from Monsoon.cpp ~:1215 on the fall transition.

## OPTION 2 — the flag stays as the ENABLE; the advance becomes PROBABILISTIC (Variation draw)
- KEEP the flag advanceOnTieIntoRest as the ENABLE (preserves the OFF case: never fall-advance). When OFF →
  never advance on the fall (unchanged).
- When ON → the fall-advance is now PROBABILISTIC, governed by the SAME Variation-draw mechanism as the
  rise advance: decide advance-or-not from the rest step's r_vary vs the resolved Variation, exactly like
  decideStepAdvance_ (SequencerEngine.cpp:348) does for the rise.
  - DRAW: use r_vary at the STEP BEING ADVANCED INTO (the rest step the fall lands on) — the step-addressed
    variation draw at that target step, consistent with "the step-addressed draw at the target" the rise
    uses. (The rise reads getVariationStep() at its target; the fall reads the rest step's variation draw.)
  - DECISION: BIPOLAR — the FULL decideStepAdvance_ decision {BACKWARD, HOLD, FORWARD}, NOT unipolar.
    (Rodney corrected an earlier unipolar draft.) The Variation value is BIPOLAR; the fall-advance draws
    against it and gets BACKWARD/HOLD/FORWARD just like the rise. WHY bipolar is REQUIRED (not optional):
    it is the SAME shared playhead. If the rise-advance is wandering BACKWARD (negative Variation,
    retrograde) and a tie-into-rest fall advanced FORWARD, the playhead would LURCH forward mid-backward-
    passage = incoherent. The fall direction MUST follow the same Variation-governed motion as the rise.
    So: p>0 → forward fall-advance (rest re-draws at the NEXT step); p<0 → backward fall-advance (rest
    re-draws at the PREVIOUS step, advancePlayhead(-1), 0->15 wrap); near 0 → HOLD (no advance, rest
    passive — the old behaviour). Pass the decided direction to advancePlayhead(), exactly as the rise
    sites do. The pendingCheckpointArrival accounting is direction-agnostic (the next rise skips its
    advance either way) — bipolar does not break "one advance, one arrival".
- EFFECT: the chain-ending-at-rest becomes a MODULATABLE PROBABILITY — high Variation at the rest step →
  the rest likely re-draws and may end the chain (more interruption); low → the rest likely stays passive
  (longer chains). Same Variation knob/draw as all advance. Consistent + expressive.

## CODE COMMENT (required — Rodney)
At legatoCheckpointOnFall (and/or the flag's menu), add a comment stating:
  "advanceOnTieIntoRest ON = PROBABILISTIC fall-advance (option 2): the fall into a tie-rest advances the
   playhead with probability = the rest step's Variation draw (same mechanism as rise step-advance), so the
   rest re-draws (and may end the slur chain) probabilistically. The flag is the ENABLE; OFF = never
   fall-advance. OPTION 3 (future, if Rodney decides the flag isn't worth keeping): drop the flag, always
   probabilistically fall-advance."

## GOLDEN PINS
- The fall-advance was deterministic → making it probabilistic MOVES gate-mode tie-into-rest behaviour when
  the flag is ON. Re-pin the affected gate-mode digests DELIBERATELY (documented). GOLDEN_A untouched.
- Flag OFF path byte-identical to today (never advances). advanceOnTieIntoRest=ON with Variation=1 (p=1 at
  the rest step) → always advances → byte-identical to today's deterministic always-advance (the identity
  point, pin it — same discipline as decideStepAdvance_ p=1).
- Reuse decideStepAdvance_'s draw/compare so there is ONE advance mechanism, not a parallel fall-advance
  dice roll.

## DONE =
- legatoCheckpointOnFall advances PROBABILISTICALLY (rest step's r_vary vs resolved Variation, BIPOLAR
  BACKWARD/HOLD/FORWARD, direction passed to advancePlayhead) when the flag is ON; never when OFF. Reuses the step-advance decision mechanism (one
  authority, not a second dice path).
- Required code comment present (option 2 explained + option 3 noted as future).
- Pins: flag-OFF byte-identical; flag-ON Variation=1 = deterministic-always identity; mid-range re-pinned;
  GOLDEN_A untouched. run_all + strict flags green.
