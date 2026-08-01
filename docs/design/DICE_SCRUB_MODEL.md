# Dice scrub model -- settled build spec (consolidates DiscussionDiceAndCA.md + Rodney's 4 points)

Supersedes the three-way "audition / reversible / reversible-A/B" distinction in
DiscussionDiceAndCA.md. That doc reasoned toward keeping all three; Rodney's 6-draw scrub window
collapses them into ONE model. This is the decided direction for build (main dice mode first).

## The four decisions (Rodney)
1. DITCH trial/audition mode entirely. The scrub window (below) covers what audition was for
   (explore recent candidates) without the frozen-A bookkeeping that fights reversibility.
2. A/B MIX stays, becomes the SCRUB control. The mix knob IS the scrub position -- fractional
   counter position is the blend (the doc's "reversible A/B mix" / "phase gains A/B blend for free").
3. SCRUB over the last 6 draws (dice has 6 sides). Compromise between old audition (explore forward
   from frozen A) and pure reversible-only (navigate full history): a bounded 6-deep blend window.
4. NO mode distinction. One model (signed counter + 6-draw scrub) works IDENTICALLY across clock,
   gate, and phase drive -- including phase running backward. Kills the NORMAL/REVERSIBLE per-stream
   flag.

## Scrub window semantics (Rodney, confirmed)
- The mix/scrub knob sweeps CONTINUOUSLY across the last 6 draws: positions N, N-1, ..., N-6 (7
  points). Blend between adjacent draws = fractional knob position (option-1 blend).
- DETENTS at each integer draw (N, N-1, ... N-6): the knob can ride the blend OR snap cleanly onto a
  specific recent draw. Continuous morph + pick-a-draw, both.
- The window FOLLOWS THE COUNTER: it's always "the last 6 from wherever the counter is now," a
  rolling lens, not a fixed absolute buffer. A reverse roll moves the counter back and the window
  slides back with it.

## Counter direction (signed; reverse inverts roll->counter mapping)
- The draw index is a SIGNED counter (already in engine: PatternEngine reverseActive sign-flip
  `(reverseActive && reversible) ? -1 : +1` -- reversible flag goes away, the sign-flip stays as the
  reverse-mode behaviour).
- FORWARD roll normally increments; in REVERSE mode a forward-drawn roll DECREMENTS the counter and
  a backward one INCREMENTS. Roll-direction and counter-direction invert under reverse. Philox is a
  keyed bijection over the full signed counter space -- negative indices are fine, fully
  reproducible.

## Data model: DERIVE, don't store (Rodney: ideal; buffer only if inefficient)
- The 6-draw window is a COMPUTATION over the counter, not stored state: evaluate at(N), at(N-1),
  ... at(N-6) from Philox on demand (7 addressable draws per active stream per frame). No buffer.
  Scrub is pure counter math. This is the whole elegance -- Philox addressability means the window
  is free.
- FALLBACK (only if per-frame 7-draw cost profiles badly): capture the 6 draws into a tiny rolling
  buffer. Not expected to be needed (7 Philox evals = a few multiplies each).

## Undo under the scrub model (from the doc -- collapses to a scalar)
- A dice roll's undo state is just the counter position: (before_counter, after_counter), 8-16
  bytes, handled directly by StoreEditAction. No LockedA/CandB array snapshots (those disappear --
  the counter re-derives everything). Scrub-knob drags coalesce like any knob.

## What this unifies / retires
- Retires: trial/audition mode, NORMAL vs REVERSIBLE per-stream mode flag, LockedA/CandB float-array
  buffers as source of truth (become at-most a cache), array-snapshot dice undo.
- Unifies: audition + reversibility + phase A/B blend into the one scrub gesture; dicing identical
  across clock/gate/phase drive.

## Build order (main dice mode FIRST, per Rodney)
1. Main dice: signed counter + 6-draw scrub-with-detents recompute + mix knob as scrub. Remove
   audition/trial + mode flag. Wire across clock/gate/phase.
2. THEN Change Alley reverse (separate mechanism -- snapshot/buffer, NOT counter-rewind; see
   DiscussionDiceAndCA.md -- CA state isn't counter-derivable, needs the phrase-boundary snapshot
   buffer). Raffles dice-gate QUEUE rides here.
3. THEN Intertropical finish + Lantern-intertropical mode.

## Note: CA is NOT this model
Dice scrub = counter-derivable (pure Philox). CA scatter state = accumulated event history, NOT
counter-derivable -> CA reverse uses the phrase-boundary-indexed snapshot buffer (28-byte entries),
a DIFFERENT mechanism. Don't conflate. (DiscussionDiceAndCA.md covers CA in full.)
