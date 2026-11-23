# Tie semantics under microtonality — the cents question (Rodney 2026-11-23)

## STATUS — BOTH ITEMS IMPLEMENTED (2026-11-23, commit 8e4ab7b5)
Rodney approved both; this doc is now the RATIONALE, not a proposal.
1. **The contradictory comment is fixed.** The tie law MOVED to `GateState` (it now sits with the
   action it selects), and the corrected text states the epsilon's bound cuts BOTH ways: it admits
   numerical dust and NOTHING a user can author, so a one-cent nudge SLIDES. Note why the wrong
   comment survived its own test: `test_poly_tie_emergent` M3 pins a **+8.3e-6 V** retune — one
   HUNDREDTH of a cent — which genuinely does stay a tie. The test was right; the comment
   generalised from it. (So the line references below reading `SequencerEngine.hpp:22xx` are
   historical — the law is `GateState::kPitchEps` / `isSamePitch` / `isSameAsHeld` now.)
2. **The tie/legato collapse is done.** One `GateState::continueNote` replaces the pair at all four
   join sites; it always writes the drawn pitch and degree, derives the label from the same predicate
   the action uses, and returns it. Byte-identical on every pinned digest.

### Two corrections the implementation forced on the analysis below
- **The `markSemi` delta is a RULE, not an omission.** The analysis called `holdRemain`-vs-`dur` "the
  ONE thing to decide" and leaned on it being visual-only. It IS visual-only, but it is also
  genuinely right in both legs and was KEPT, derived from the same predicate: a tie is one sustained
  note so its fader stays lit for the whole remaining hold, while a slide lights the new degree for
  that note's own length. Collapsing to `dur` would cut ties' flash short; collapsing to `holdRemain`
  would leave EVERY fader in a legato run lit. Neither "decision" was correct — both legs were.
- **"Writing pitch/semitone on a tie are no-ops" is true of SEMITONE only.** The degree write is a
  provable no-op (the predicate contains `sem == heldSem`); the PITCH write is not, because the
  predicate admits a sub-epsilon delta. It moves the stored value by at most ~0.12 cents, towards the
  value actually drawn. No pinned march exercises it, but it is a real (intended) widening, not a
  no-op — see `test/test_continue_note.cpp` G1a, which pins it bit-exactly.


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


## THE DEEPER SIMPLIFICATION — DO WE NEED TIE vs LEGATO AT ALL? (Rodney 2026-11-23)
Rodney: "if we draw legato then we keep gate high whether pitch changes or not. That doesn't need to
split strict tie from non-strict tie." Verified in `GateState.cpp` — he is right, the split is nearly
vestigial.

### What the two operations actually do (wasHeld case)
- `slideNote(pitchV, sem, nv, wasHeld, isQuant)`: `currentPitchV = pitchV; lastSemitone = sem;`
  `holdRemain += dur; gateHeld = true; armGatePulses(...); markSemi(sem, dur, isQuant);`
  label Legato.
- `extendHold(sem, nv, isQuant)`: `holdRemain += add; gateHeld = true; armGatePulses(...);`
  `markSemi(sem, holdRemain, isQuant);` label Tie.
IDENTICAL gate handling. **No portamento difference** — `slideNote` does NOT glide, it sets the pitch
instantly. The ONLY real differences are three OMISSIONS in `extendHold`:
  1. `currentPitchV` is NOT written  (this IS the octave-discard bug the tie work fixed around)
  2. `lastSemitone` is NOT written
  3. `markSemi`'s duration arg: `holdRemain` (accumulated) vs `dur` (this note's length)

### The load-bearing accident
Omissions 1 and 2 are benign ONLY BECAUSE `isSamePitch` is STRICT (a tie requires same degree AND same
pitch, so not writing them is harmless). LOOSEN the tie rule — either cents option above — and both
omissions become LIVE BUGS immediately. The strictness is silently load-bearing for a correctness
reason that was never written down. That alone argues for unifying.

### Recommendation: ONE operation, emergent label
Replace the `if (connect && isSamePitch) extendHold else slideNote` branch with a single
`continueNote(pitchV, sem, nv, isQuant)` that ALWAYS writes pitch and semitone, extends `holdRemain`,
holds the gate, re-arms the pulses and marks the semi. Compute the Tie/Legato LABEL emergently from
whether the pitch moved. Benefits:
- **Eliminates the bug class**: the octave-discard bug existed because a branch could skip writing the
  pitch. With one always-writing operation it is UNREPRESENTABLE (writing an unchanged pitch is a no-op).
- **Makes the cents/tolerance question mechanically moot**: any tie rule is safe because the state is
  always written correctly regardless of which branch "would" have been taken.
- **Removes the branch** the emergent-tie work had to thread `isQuant` through on every arm.
- **Finishes the direction**: tie stopped being a DECISION (emergent-tie work); this stops it being a
  CODE PATH. The label becomes a readout — which is all `tieHold`, the Lantern colours and the
  (callerless) `setTieGateOutput` ever needed.

### The ONE thing to decide (the only behavioural delta)
`markSemi`'s duration argument — `dur` vs `holdRemain`. Writing pitch/semitone on a tie are no-ops, so
the unification is otherwise BYTE-IDENTICAL; the marked span is the single real difference and the
unified call must pick one. Testable against the goldens, not arguable.

### Discipline
`GateState` is the audio path and the STEPS_32 goldens cover it. Do the unification byte-identical
FIRST (pick the markSemi arg that reproduces current output, prove it against the full battery), and
only then consider changing the marked span if it is wrong.


## ANSWERED: what the markSemi duration delta REALLY means (2026-11-23)
`markSemi(sem, dur, isQuant)` writes `semiPlayRemain[sem] = max(existing, dur)` (or
`semiQuantPlayRemain` when quantised) — a per-DEGREE countdown in STEP units. Its ONLY consumer is the
UI: `MonsoonWidget.cpp:27` states the tuning-fader handle FLASH light is play-driven,
`semiLedBrightness <- semiPlayRemain`. **It is purely VISUAL — no audio path reads it.**
THE DELTA: on a slide FROM A HELD NOTE, `slideNote` marks the new degree with `dur` (that note's
nominal length) while the note actually sounds for the accumulated `holdRemain`. So the degree's LED
can go DARK WHILE THE NOTE IS STILL SOUNDING — an under-report. `extendHold` passes `holdRemain` and
is correct. Because `markSemi` takes `max()` it never shortens an existing mark, only extends; and in
the common `!wasHeld` case `holdRemain == dur`, which is why this was never noticed.
=> `holdRemain` is the CORRECT argument for the unified `continueNote`; `slideNote`'s `dur` is a
latent VISUAL under-report, not a deliberate choice.
=> **THE UNIFICATION IS AUDIO-BYTE-IDENTICAL EITHER WAY.** Nothing audible reads these arrays, so the
STEPS_32 goldens CANNOT move. That makes the tie/legato collapse a far lower-risk refactor than the
earlier "prove byte-identity first" framing implied — the only observable change is LED flash timing,
and choosing `holdRemain` FIXES it.

## CENTS CADENCE — RESOLVED (Rodney 2026-11-23: phrase boundary) + the finding that reframes it
Rodney's inclination: PHRASE BOUNDARY for a cents change ("or even rest") — it changes a TUNING
SYSTEM, not a live performance gesture.
**FINDING: there is NO mid-note pitch drift to prevent.** The output CV is the STORED
`currentPitchV` (`MonsoonOutputGenerator.cpp:159` `cvOut.setVoltage(..., currentPitchV, 0)`;
`:119` `polyCV[i] = engine.voices[vs].gs.currentPitchV`) — it is NOT re-derived from the tuning table
each frame. So a cents change does NOT affect a SOUNDING note at all; the held note keeps its stored
pitch and the retune is heard naturally on the NEXT note event (where the new pitch is computed,
compared, found different, and slid to).
So latching is still worth doing, but for a SMALLER reason: preventing a TORN READ WITHIN A STEP
(some voices computing pitches with the old cents, others with the new).
**PHRASE BOUNDARY beats "at a rest", for a reason beyond the performance-gesture one:** the tuning
table is GLOBAL (one tuning authority per Monsoon — Colonnades | Duo | Sikit are mutually exclusive
and write a shared table), so a PER-VOICE "apply at this voice's rest" cadence would let voices switch
at DIFFERENT times and split the ensemble across TWO TUNING SYSTEMS mid-chord. A phrase boundary
switches every voice at the same instant. "At a rest" would have to mean ALL voices silent, which a
dense patch may never offer (unbounded deferral).
DECISION — REVISED (Rodney 2026-11-23): latch cents to the **STEP BOUNDARY**, globally. NOT the
phrase boundary — that is TOO SLOW for the common use. The dominant case is a user CREATING a custom
tuning by editing Sikit / Colonnades faders, who needs to hear the change immediately; a phrase
boundary can be 16-32 steps away, hopeless for tuning by ear. A STEP boundary is a fraction of a
second at normal tempos while still being quantised enough to prevent a torn read within a step.
(Phrase cadence would only suit a SETTLED tuning system, which is rarely changed anyway.)
KEY CONSTRAINT THAT MAKES THIS SAFE (Rodney): cents are a USER SETTING ONLY — there is NO intention to
modulate tuning cents mid-phrase. So the earlier worry that continuous cents modulation would make
ties never occur (always slides) does NOT apply: a fader drag is a handful of DISCRETE changes, each
producing a slide at the next step, which IS the desired feedback. Strict ties stay fine.
POLYPHONY still consistent: the transport tick is global, so every voice evaluated within a tick sees
the same table.
Same latched-at-boundary shape as slew / MA depth / the reverse toggle (different boundary, same
lifecycle) => ride the CA1 skeleton rather than adding a bespoke rule.

### Ergonomic limit — the cadence is NOT the only latency
Because the output is the STORED `currentPitchV`, a retuned degree is heard WHEN THAT DEGREE NEXT
PLAYS. Editing a degree the pattern is not currently visiting produces silence however fast the latch
is. If the goal is genuinely tuning BY EAR, the real answer is likely a PREVIEW ON FADER TOUCH
(audition the degree directly) rather than waiting for the pattern to reach it. Step-boundary latching
is right, but it is not sufficient on its own for the tuning-creation workflow.
