# Tie semantics under microtonality — the cents question (Rodney 2026-11-23)

## The question
With microtonal tuning (Colonnades Duo's 24 degrees, the Shophouse / Shophouse-Micro pair) and NO
restriction yet on the cents-adjustment cadence, what IS a tie? Rodney's options were: keep it strict,
an arbitrary tolerance, or base it on the faders (same fader/degree = tie even if the cents changed).

## Facts established (verified in code 2026-11-23)
- `isSamePitch(pitchV, heldPitchV, sem, heldSem) = (sem == heldSem) && (|dV| < kPitchEps)`,
  `kPitchEps = 1e-4f` (SequencerEngine.hpp:2216).
- A CENT is 8.33e-4 V (1/12 V per semitone / 100). So `kPitchEps` ~ 0.12 cents — a 1-cent nudge is
  ~8x LARGER than the epsilon.
- **CONTRADICTION (must be resolved):** the comment immediately above (`:2212-2213`) claims "a
  mid-phrase cents nudge of the same degree STAYS A TIE; an octave move never does". The code does the
  OPPOSITE: any nudge >= ~0.12 cents breaks `isSamePitch`. Comment and implementation disagree.
  Whatever is decided, FIX THE COMMENT — otherwise someone later "fixes" the code to match it.
- **NO cents latch exists** — cents are live per-frame (grep: no centsLatch/latchCents/centsApplied).

## The mechanical fact that decides it
A failed `isSamePitch` on a JOINED pair does NOT re-strike. The arm is
`if (connect && isSamePitch(...)) extendHold(...) else slideNote(...)`, and the two differ in exactly
one way:
- **tie / `extendHold`** — gate continues, pitch FROZEN.
- **slide / `slideNote`** — gate continues, pitch MOVES (no re-gate).
So TODAY a retuned held note SUSTAINS AND FOLLOWS the new tuning. For a microtonal instrument that is
the desired behaviour: you hear the cents fader move on the held note.

## Why that inverts the options
- **Option 3 (fader/degree-based tie)** would make a cents change a TIE -> `extendHold` -> the pitch
  FREEZES -> **retuning becomes INAUDIBLE on held notes**, defeating a live cents control. It also
  breaks the invariant the tie work strengthened for Intertropical's `tieHold` ("a Tie guarantees no
  pitch move"); you would have to redefine tie as "may move pitch", collapsing the tie/slide
  distinction entirely.
- **Option 2 (tolerance)** has the same freezing problem inside the tolerance band, plus a magic
  number and a boundary where behaviour flips.
- **Option 1 (strict)** keeps the audible behaviour correct. Its only cost is COSMETIC: under live
  cents wobble the LABEL is almost always "slide" rather than "tie". A 0.5-cent slide is audibly
  indistinguishable from a tie, so this affects only the Lantern colour and `tieHold`.

## Recommendation
**KEEP IT STRICT (option 1), and fix the contradictory comment.**

## The better lever — CADENCE, not tolerance
The root cause is that cents are live per-frame, so a held note's pitch can drift mid-note. LATCHING
CENTS TO A BOUNDARY (as slew, the MA depth and the reverse toggle are / will be) removes the drift
entirely: pitch is stable within a step, and changes land where a new note is being decided anyway.
Then strict ties work naturally and the tolerance question evaporates. This is the SAME
latched-at-boundary shape as CONCEPT_ODR_AUDIT CA1, so it should ride that skeleton rather than become
a bespoke rule. RECOMMENDED as the real fix; the tie predicate then needs no microtonality clause.

## Why emergence was the right call
A non-emergent tie would have needed a special microtonality clause. The emergent one asks only "did
the pitch move?" and gives a defensible answer under ANY tuning system — 12-TET, 24-tone, or arbitrary
.dmtune fronts. The question above is about the CADENCE of the inputs, not about the tie rule.

## Open
- Confirm the cents cadence should be latched (and at which boundary — step, draw, or phrase).
- Until then, strict stands and the comment must be corrected to describe the actual behaviour.
