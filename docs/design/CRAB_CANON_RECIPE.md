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

## The Monsoon key/seed-OFFSET knob (Rodney's reminder) -- = the counter-address primitive; MUST-HAVE, NOT built
Rodney recalled "a knob on Monsoon for setting dice Philox key offsets". Confirmed: this IS the counter-
address / seed-offset primitive flagged above (:42-48) as "NOW A MUST-HAVE". Status: DOCUMENTED, flagged
must-have, NOT built. Today's code has the SEED/reseed system (share a seed -> identical draws) but NOT the
OFFSET -- so today only the degenerate canon (two Monsoons in unison, offset 0) is possible; the buildable-
today workaround is the dice-scrub crossfade (:24-27), and the offset primitive is the ideal/gap.

### What it is + what it's for
The addressability of Philox exposed as a CONTROL: a knob/CV that sets WHERE in the stream a Monsoon is
(counter-address), or an OFFSET from another Monsoon's position. Purpose = MULTI-MONSOON CANONIC
relationships: two Monsoons on the SAME seed but OFFSET positions -> canons, delays, crab/retrograde, for
FREE (they read the same deterministic material at different offsets). The offset knob turns "same seed"
from "identical output" into "CANONICALLY-RELATED output" (same material, shifted). The crab canon (:4-10)
is the headline: one at p, one at N-p, shared seed => same draw => they meet at the midpoint.

### Connects to everything
- Addressability MADE A CONTROL: same signed-addressable Philox counter behind dice-reverse + true-reverse.
  The offset knob = "set the counter-address directly" vs "step to it". Same bijection property, exposed as
  a PLACEMENT control.
- Multi-Monsoon complement to the shareable sources: shared DRAWN material (Esplanade/Zouk editors) +
  shared SEED AT OFFSETS (this knob) = a canonic/heterophonic multi-Monsoon toolkit.
- Reversible-automation class: offset is just another way to address the bijection -> inherits
  reproducibility for free.

### Design note: RELATIVE offset (CV-able), not just absolute position
Is the knob ABSOLUTE (set position N directly) or RELATIVE (offset from a reference Monsoon's live
position)? Canons want RELATIVE (B = A + offset, or B = N - A for crab; the crab needs B = 1 - A
complementary drive, :24 -- that's relative). So lean RELATIVE offset from a shared/reference position,
CV-able (so the offset itself can move = a time-varying canon). Absolute-position is a different, more
static gesture. LEAN: relative + CV-able.

### Status / next
DOCUMENTED MUST-HAVE, unbuilt. The knob = expose Philox counter-addressing as a Monsoon control (relative
offset from a shared position, CV-able). Enables canons/crab/retrograde across Monsoons sharing a seed.
Pre-release: free to add the param. A V1-candidate given it's flagged must-have + it's the addressability
already in the engine, just needing a control surface.

Cross-ref: CRAB_CANON_RECIPE:42-48 (the counter-address/seed-offset ideal = this knob), PHILOX_KEY_
DERIVATION (the addressable bijection this exposes), UNDO_AND_REVERSIBLE_AUTOMATION_PRINCIPLE (offset =
addressing the bijection = reversible-automation class), the shareable-editors sections (shared material +
this = canonic multi-Monsoon toolkit), Monsoon.cpp seed/reseed system (has SEED, lacks OFFSET -- the gap)." 

## Offset -- WHEN is it applied? Two options, best design considered LATER (Rodney)
Parked design fork on the offset primitive: when does the offset take effect?

1. LIVE / CONTINUOUS offset (knob/CV, always active): offset is a running control, settable/modulatable
   ANY TIME -> relative position can shift DURING playback -> TIME-VARYING canons (offset moves, crossings
   you slide/perform). More expressive; more to build (a live counter-address control always active).
2. OFFSET-AT-RESEED (Rodney's alternative): the offset is set ONLY at reseed time -- reseed also specifies
   the offset, baked in until the next reseed. Static between reseeds. SIMPLER -- piggybacks on the reseed
   machinery that ALREADY exists (reseedCorrKeys, setPendingRhythmSeed, deferred phrase-boundary reseed),
   just adding an offset parameter to that moment.

### Why offset-at-reseed is attractive
- REUSES existing machinery (reseed system already built: seed CV, deferred phrase-boundary reseed, per-
  axis, lock-aware). Adding "...and apply this offset" is a small extension of a working path vs building a
  whole live counter-address control. (Pattern-reuse de-risking.)
- ENOUGH for the headline use case: a crab/fixed-offset canon doesn't need the offset to move mid-
  performance -- set up (reseed both, one at offset N-p), let it run. Delivers the canon purpose with far
  less build.
- Fits the reseed MENTAL MODEL: reseed is already "reset generative state to a fresh/specified config";
  "reseed to this seed AT this offset" is a natural extension, not a new concept.

### The trade
Offset-at-reseed gives up TIME-VARYING offset -- can't slide the canon relationship during playback (fixed
between reseeds). Fine for a static canon; for a MORPHING canon (offset sweeping, live crossings) you want
option 1. Usual static-but-simple (reseed-time) vs live-but-more-work (continuous knob/CV) fork.

### Hybrid staging (for when this is picked up)
Offset-at-reseed as the MVP (cheap, reuses machinery, covers static canons), live/continuous as a LATER
enhancement IF time-varying canons prove wanted. Same MVP-then-enhance staging as q-mix.

### Deciding question (LATER)
"Do you want to PERFORM/MORPH the canon offset live, or just SET-AND-RUN?" Set-and-run -> reseed-time wins
on cost. Live morphing matters -> need the continuous control. Best design considered later; parked.

Cross-ref: the key/seed-OFFSET knob section above (the primitive; this is WHEN it applies), Monsoon.cpp
seed/reseed system (offset-at-reseed piggybacks on this existing path), the shareable-editors + reversible-
automation notes (the canonic multi-Monsoon toolkit this serves)." 

## RESOLVED (Rodney): offset is MODULATABLE (live), user tweaks undo, modulation doesn't
"Modulatable one of course; modulations not undo, user tweaks undo." This resolves the timing fork AND
applies both undo principles at once:

### Picks the LIVE/continuous offset (option 1)
Modulatable is the point -> the live knob/CV, giving time-varying canons (offset sweeping, live crossings)
that reseed-time-only can't. The fork resolves toward the live control because it must be modulatable.
(Offset-at-reseed remains a cheaper fallback if the live build is ever deferred, but the chosen design is
the modulatable live offset.)

### Applies the undo principles to the offset param (no new rules)
The SAME knob carries both classes, sorted by WHAT MOVED IT:
- USER tweaks the offset knob by hand -> user edit -> UNDOABLE (on the undo stack).
- MODULATION (CV/automation) moves the offset -> NOT on the undo stack (Principle 1: modulation isn't
  undoable; you undo YOUR knob-turn, not automation moving the knob).
Standard plugin behaviour for a modulatable param: undo captures the user's changes, not the automation's.

### Both principles apply, cleanly, no conflict
- Principle 1 (user-only undo): modulated offset movement stays OUT of undo; user tweaks go IN.
- Principle 2 (reversible automation): the offset IS an addressing of the Philox bijection, so even
  MODULATED offset movement is reversible-by-construction (the position is never lost, always re-
  derivable). So modulation of the offset is not-undoable (P1) BUT inherently reversible (P2) -- no
  conflict: user tweaks undoable; modulation not-undoable-but-reversible.

### Net
Offset knob = MODULATABLE live/continuous (time-varying canons), following the house undo rules: user
tweaks undoable, modulation not (P1), and modulation reversible-by-construction anyway (P2, it's a Philox
address). No special-casing -- the general principles applied to a modulatable param.

Cross-ref: the offset-knob + offset-timing sections above (this picks live/modulatable + resolves when),
UNDO_AND_REVERSIBLE_AUTOMATION_PRINCIPLE (P1 user-only-undo + P2 reversible-automation, both applied here),
the reversible-automation class (the offset as a Philox address = reversible modulation)." 
