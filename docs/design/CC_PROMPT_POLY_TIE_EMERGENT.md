# CC prompt — Tie is EMERGENT: joined notes, tie/legato derived from the drawn pitch

Branch: `fix/poly-legato-quantiser` (continue; same branch as the quantiser work, Rodney's call
2026-11-22: "I don't care about existing patches"). Builds on `CC_PROMPT_POLY_LEGATO_QUANTISER.md`
and the one pitch-draw law (`polyPitchDraw`). Rack behaviour is Rodney's feel-test.

## Rulings (Rodney)
1. TIE IS EMERGENT. There is no separate tie decision and NO second tie control. The legato
   knob decides whether notes JOIN (the held/slurred gate, the forward commitment `slurForward`).
   Whether a join is a strict tie is just whether the pitch drawn for the next note equals the
   held pitch, which falls out of the melody and octave knob settings and their correlations.
   (Matches `LEGATO_TIE_REDESIGN.md` / `LEGATO_TIE_MODEL_NOTE.md`: tie and legato are ONE
   concept, the label is derived from pitch.)
2. A TIE = SAME DEGREE **AND** SAME OCTAVE (the same pitch). Different degree OR different octave
   = a slide (Legato).
3. Poly voices DO NOT use mono's tie decision. Each voice draws its OWN degree and octave
   (independently, via `polyPitchDraw`, which already honours the quantiser per axis) and derives
   tie-vs-slide from ITS OWN held pitch.

## The bugs this fixes (read the code, verified 2026-11-22)
- MONO (`SequencerEngine.cpp` ~2037): `if (sem == gs.lastSemitone) { extendHold(...); Tie }`
  compares the DEGREE only (`sem` is the degree index; the octave is a separate term in
  `genPitchSplit` / `voicePitch`). A same-degree / different-octave draw is classed as a Tie, and
  `GateState::extendHold` never touches `currentPitchV`, so the NEW OCTAVE IS SILENTLY DISCARDED
  (the note keeps sounding at the old octave). In quantiser mode a CV jump of an octave is ignored.
- POLY S1 (`connect && isTie`) and S2 (`rd.gateType == MonoDecision::Tie`): hold the voice's own
  pitch with NO redraw because MONO tied. A mono tie only happens when mono's own draw coincided
  with its last degree: poly is coupled to a coincidence in another voice.
- POLY S1/S2 slide branches never compare pitch at all, so with a held quantiser CV a poly "slide"
  lands on the same pitch and is labelled Legato where mono would say Tie.

## The change
Define ONE pure predicate (in `SequencerEngine` or a small core; Rack-free), e.g.
`isSamePitch(pitchV, heldPitchV, sem, heldSem)`:
- same degree (`sem == lastSemitone`) AND same octave. The robust test is on the drawn VOLTAGE:
  `std::fabs(pitchV - gs.currentPitchV) < kPitchEps` (a named constant, ~1e-4 V, far below the
  finest tuning step) PLUS the degree equality, so a microtonal/Micro-N tuning and the
  quantiser's `quantize()` output compare consistently. Decide the exact form, justify it in
  the test, and use it at EVERY site below.
Apply at:
- MONO join (~2037): `sem == lastSemitone` -> `isSamePitch(...)`. Tie -> `extendHold` (unchanged
  mechanism); otherwise `slideNote`. The mono decision labels (Tie/Legato, the tie-gate and
  legato-gate OUTPUT JACKS, Lantern colours) stay: they are now derived from the full pitch.
- POLY S1 (the landing): remove the `connect && isTie` freeze. On a CONNECT, always draw this
  voice's own pitch with `polyPitchDraw`, then `isSamePitch` against the voice's held pitch ->
  `extendHold` + verdict Tie, else `slideNote` + verdict Legato. The re-articulate (not connect)
  branch is unchanged: a fresh `triggerNote` + its own accent draw.
- POLY S2 (mid-hold follow, `rd.holdFollow && wasHeldPoly`): the Tie and Legato/LegatoMax arms
  merge into ONE "held" arm: draw own pitch via `polyPitchDraw`, then the same derive. The
  MidNote arm (mono is sustaining, nothing chosen; follow the hold only while this voice's own
  gate window is open) is UNCHANGED.
- `isQuant` is passed to `extendHold`/`slideNote`/`triggerNote` (the green flash) on every arm.
- The forward commitment (`slurForward` re-roll, the legato knob) is UNCHANGED: it is the JOIN
  decision. Only the tie/legato label and the poly freeze are changed.
- Update the comments at S1/S2 ("poly tracks mono's articulation shape, never mono's actual
  notes" stays true for the JOIN; it no longer holds for the tie freeze) and
  `LEGATO_TIE_REDESIGN.md` (add a short "implemented: tie is emergent, 2026-11-22" note).

## This CHANGES existing behaviour (accepted)
Poly voices that used to freeze on a mono tie now redraw. Existing tests/goldens that encode the
old behaviour will fail. Do NOT blanket re-baseline: for each failing test, say which old
behaviour it pinned and confirm it is the accepted change (poly freeze on mono tie / degree-only
tie), then update it with a one-line reason. Any failure you cannot explain this way is a REAL
regression: stop and report.

## Tests (failing first; use the `forestDrive` rig, registered; the unreachable-path lesson applies)
Independent oracle: hold-vs-slide expected from the stated rule (same degree AND same octave) in
the test body, not from `isSamePitch`.
1. MONO octave bug: same degree, different octave drawn on a join -> slide to the new octave
   (pitch voltage == the new draw), verdict Legato. Fails today (tie, old octave kept).
2. MONO same pitch -> Tie, `extendHold`, pitch unchanged, tie-gate jack output fires.
3. POLY independence: mono ties but the poly voice draws a different pitch -> poly SLIDES; mono
   slides but the poly voice draws the same pitch -> poly TIES. Fails today (follows mono).
4. POLY octave: same degree, different octave -> slide; same pitch -> tie.
5. QUANTISER mode with a HELD CV: poly joined notes TIE (no zero-interval Legato), verdict Tie;
   a CV octave jump with the same degree -> slide to the new octave.
6. Per-voice 16ch CV: each voice derives from its own CV.
7. The mid-hold MidNote arm is unchanged (a voice whose gate window closed is not re-opened).
8. Coverage counters on the rig: every arm (mono tie, mono slide, poly S1 tie/slide, poly S2
   tie/slide, re-articulate) is REACHED; the test fails if a counter is zero (the dip-guard lesson).
Mutation-check: (a) degree-only compare restored -> 1/4 fail; (b) poly follows the mono verdict ->
3 fails; (c) drop `isQuant` on one arm -> the flash assertion fails; (d) pitch epsilon 0 or huge ->
a rounding/near-equal case fails. Show red first (against `7fe4a7ec`), then green, then register
the quantiser suite too and run the full suite.

## Report back
Red/green, the predicate form and epsilon rationale, the list of old tests changed and why, the
coverage counters, and anything in the tie-gate / legato-gate output jack path or Lantern that
read the old label and needs a look.
