# Gate subdivision via STEP_GATE — the "make a deterministic rhythm stochastic" completion (Rodney)

STATUS: mostly ALREADY BUILT (step-legato exists); this records the framing and the one open question.

## The gap this closes
Gate mode can SKIP gates (rest) and JOIN them (legato), but not REPLACE a long gate with several short
ones — rest/legato are subtractive/combinative, never subdividing, because they only act on the events
you sent. Adding density needs a finer grid to articulate against.

## The realisation (Rodney)
Subdivision is NOT a new rhythm stream (rhythm q-mix / a 4th Philox stream — considered and rejected as
over-scoped). It is STEP-LEGATO applied within one incoming gate: **if STEP_GATE is patched, legato/tie
decisions are made at the STEP_GATE resolution, not the main gate's.** The main gate marks note EVENTS;
STEP_GATE sets the RESOLUTION at which those events may be subdivided and re-articulated. Unpatched =
step resolution, as today.

## Worked example
External sequencer plays a mix of 1/8 and 1/16 notes; send its 1/16 clock to STEP_GATE. The engine now
evaluates legato at 1/16, so an 1/8 note is "two 1/16 slots, tied", and the legato lane decides PER
VOICE whether they stay tied (sounds as the 1/8) or retrigger (becomes two 1/16s). A fixed 1/8 becomes a
probabilistic 1/8-or-two-16ths.

## Why it is the right form
- **Join OR split, both directions**, at the finer grain: tie 1/16s up into longer notes, or break a
  held note into 1/16 retriggers. The "can't turn one long gate into several short ones" limit dissolves
  once there is a finer grid — HONESTLY (via a real clock) rather than by inventing subdivisions.
- **No internal grid generation needed** in the patched case — the external stream IS the grid, so the
  sub-step-timing worry (could the engine schedule between step edges?) is already solved by step-legato.
- **Per-voice + correlated + reversible** is the part patching cannot touch: an external ratcheter
  subdivides every voice identically; here WHICH voices split moves with the correlation structure and
  reverses, because the decision is a seeded per-voice lane value. This is why it is an internal feature
  and not just "patch a burst generator" (which remains the answer for uncorrelated subdivision).

## Open question (the only real build item)
Does patching STEP_GATE AUTOMATICALLY raise the legato-evaluation resolution, or only within an explicit
legato span? Auto is more discoverable (patch a faster clock -> subdivision just happens). Confirm what
the engine reads today: step-legato currently supplies inner boundaries WITHIN a legato run; the small
extension is to let those boundaries govern re-articulation of ANY gate, not only inside a committed
slur. Verify before building.

## Deferred / rejected alternatives (recorded so they are not re-proposed)
- **Rhythm q-mix / 4th stream** — over-scoped; doubles the Sands/panel/CA surface for a capability this
  covers, and muddies the clean "timing origin (mode) x pitch origin (q-mix)" two-axis story.
- **Internally-generated subdivision grid** (invent N even divisions within a gate with nothing patched)
  — the only genuinely new build; leave until a musical case needs subdivision with NOTHING clocked in.
- **Patched burst/ratchet generator in front of the gate input** — works today for UNcorrelated,
  non-reversible subdivision; on-brand (Monsoon perturbs what arrives). The internal route exists only
  for the correlated/reversible version.
