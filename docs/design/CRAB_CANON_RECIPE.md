# Crab canon: buildable-today (dice-scrub) vs the ideal (counter-address) -- recipe + feature gap

## The goal
Two Monsoons, shared Philox key (seed) + shared Change Alley (correlation), one playing FORWARD and one
BACKWARD through the SAME material, crossing in the middle = crab canon (canon cancrizans, the Musical
Offering device) generated live from one stochastic source.

## The core requirement + the constraint
For a TRUE crab: both voices must traverse the SAME counter RANGE in opposite directions, so at time t
one is at position p and the other at N-p, coinciding at the midpoint (shared seed => same draw =>
audible coincidence at the crossing).

CONSTRAINT (why the elegant version isn't buildable yet): there is no user-facing COUNTER-ADDRESS
primitive. The Philox counter is addressable INTERNALLY (the whole navigable-probability claim), but the
only control that MOVES it is the dice (forward) / dice-scrub (reversible). You cannot "set counter=10";
you'd have to advance/draw 10 times to get there. So "fwd 0->N while back N->0" can't be dialled in
directly today.

## BUILDABLE TODAY: crab via dice-scrub crossfade
Dice-scrub (reversible mode) moves the counter back and forth. Drive two Monsoons with COMPLEMENTARY
scrub positions:
- Monsoon A: scrub 100% -> 0% (counter retreats through accumulated positions N..0)
- Monsoon B: scrub 0% -> 100% (counter advances 0..N)
- One control cross-wired: B_scrub = 1 - A_scrub (complementary drive, same idea as phase 1-phi).
- Shared seed + shared CA.
As A retreats and B advances they PASS THROUGH each other; at the crossover both are at the same counter
position drawing the same material = the crab crossing. Realized as a CROSSFADE gesture, not a phase
mirror.

WHY THIS IS ACTUALLY GOOD (not just a workaround):
- PERFORMABLE as a single gesture -- you sweep the crab in real time, control its speed, reverse it.
- Sidesteps the addressing problem -- you don't SET a counter, you SCRUB THROUGH to the crossing.
- The crossing POINT is a live parameter (the crossfade balance: A@70%/B@30% = not yet met).

HONEST BOUNDS:
- Quantized to draw-count steps, not continuous phase -- crossing lands on counter positions (fine for
  a canon; note-events anyway), not a smooth sweep.
- REVERSIBLE-MODE dependent + bounded by scrub-history DEPTH: the crab length is limited by how far back
  the reversible counter can scrub (see UNDO/reversible-mode bounds). Not unbounded.
- Needs complementary drive (B = 1 - A) to cross at a stable, controllable point.

## THE IDEAL (needs a new feature -- NOW A MUST-HAVE): counter-address / seed-offset primitive
What's missing is a way to JUMP a Monsoon to a specific counter position (or sync two counters at a
known offset) without advancing there manually. This is the difference between "the counter IS
addressable" (true internally) and "YOU can address the counter" (not exposed).

STATUS: MUST-HAVE, not nice-to-have. Rationale: we ALREADY allow forward AND backward navigation
(dice-scrub, reversible mode, phase reverse). Given bidirectional navigation is a first-class feature,
NOT being able to set/offset the position is an obvious gap -- you can move but not GO TO. The seed
offset is the natural completion of the navigation feature set.

PROPOSED: a SEED OFFSET input on Monsoon --
- A CV/param that offsets this Monsoon's Philox counter by a settable amount relative to its base.
- Set both Monsoons' counter from one CV; offset one by a fixed amount => arbitrary canon alignment.
- Makes the phase-mirror crab trivial: fwd Monsoon at phase phi -> counter t; back Monsoon at 1-phi
  -> counter N-t; both addressed from one ramp. Smooth, unbounded, exact midpoint crossing.
- Generally unlocks navigable-probability-space as a USER capability, not just an internal property --
  this is the control surface the headline "navigable probability space" claim has been missing.

## Selectable SCRUB DISTANCE (make more of the scrub feature)
Add a selectable scrub distance / span -- e.g. 6 / 8 / 10 / 12 draws -- so the dice-scrub crossfade
(and the crab built from it) can target a chosen phrase length:
- Sets how many counter positions a full 0->100% scrub traverses.
- Lets the crab canon be built at a chosen length (6-step crab vs 12-step crab) without depending on
  whatever the accumulated history happens to be.
- Musically: the scrub distance = the crab's period, so it becomes a compositional choice (short tight
  crossings vs long arcing ones).
- Pairs naturally with the seed offset: offset sets WHERE, scrub distance sets HOW FAR.
Values 6/8/10/12 are a sensible starting set (even lengths, musical phrase sizes). Could be a param or
a context-menu selection.

Together, seed-offset + selectable-scrub-distance turn the crab from "a lucky crossing wherever the
history is" into a PRECISELY PLACEABLE, CHOSEN-LENGTH device -- and more broadly make the whole
navigable-probability-space feature genuinely performable.

## The phase-mirror version (once counter-address exists)
- Same key, same CA, same counter RANGE (both cover the phrase 0..N).
- One master phase ramp -> forward Monsoon directly.
- Backward Monsoon driven by (1 - phase) -- the complemented ramp -> guaranteed crossing at N/2 every
  cycle, stable, smooth, unbounded.

## Crossing cleanness is itself a parameter (applies to both versions)
At the crossover both voices draw from the same counter+seed => SAME draw. If their lane settings
(spread, spread-sign inversion, big-5) are IDENTICAL, the crossing is a clean UNISON coincidence. If
they differ (e.g. different spread-inversions per point 10/11), the crossing is "same underlying draw,
differently shaped" -- a blurred, heterophonic crossing. So HOW CLEANLY the crab crosses is a
controllable feature: identical settings = clean unison; divergent = heterophonic blur.

## NOT the crab: the "boundary mirror" (different ranges)
"back 12->6 while fwd 0->6" is a DIFFERENT device: the voices read DIFFERENT regions (fwd 0..6, back
6..12), sharing only the seam at 6. Two adjacent phrases hinged/reflected at a shared point -- a mirror
at the boundary, more like a mirror-fugue subject/answer than a crab. Valid and worth having, but it is
NOT the same-material crossing of a true crab. Keep the distinction clear in any demo/pitch.
