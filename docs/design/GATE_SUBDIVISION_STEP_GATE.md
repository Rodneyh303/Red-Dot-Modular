# Gate subdivision via STEP_GATE — the "make a deterministic rhythm stochastic" completion (Rodney)

STATUS: mostly ALREADY BUILT (step-legato exists); this records the framing and the one open question.

## The gap this closes
Gate mode can SKIP gates (rest) and JOIN them (legato), but not REPLACE a long gate with several short
ones — rest/legato are subtractive/combinative, never subdividing, because they only act on the events
you sent. Adding density needs a finer grid to articulate against.

## The realisation (Rodney)
Subdivision is NOT a new rhythm stream (rhythm q-mix / a 4th Philox stream — considered and rejected as
over-scoped). It is the rhythm-shaping lanes evaluated at a finer, EXTERNALLY-CLOCKED grid.

**STEP_GATE is an INPUT, not the existing generated step-legato — do not conflate them (Rodney).**
- *step-legato* is GENERATED: the engine decides legato at step boundaries; it is an OUTPUT behaviour.
- *STEP_GATE* is an external clock sent IN that sets the RESOLUTION at which the rhythm lanes are
  evaluated. Opposite direction of signal flow. The decision LOGIC exists (step-legato); the missing
  piece is the INPUT that drives it — a jack, param/enum entry, the plumbing to clock lane evaluation
  off it, and a kit anchor. Bounded, but a real build, not just config.
- **Name the input to avoid the near-homonym** (STEP_GATE vs step-legato read as the same feature on the
  panel). Prefer something like SUBDIV_IN / GRID_IN — "a resolution clock", not "the legato thing again".

**It drives REST and LEGATO both (Rodney), not legato alone** — they are the two lanes that shape the
rhythm of incoming events, so a resolution clock must govern both or it is only half a control:
- LEGATO at the fine grid: tie adjacent sub-slots or retrigger — density UP.
- REST at the fine grid: sound or drop a sub-slot — density DOWN (e.g. subdivide an 1/8 into two 1/16s,
  then rest silences one -> a 16th + 16th-rest figure the input never contained).
Possibly ACCENT too (so ratcheted sub-hits carry their own accent pattern). The main gate marks note
EVENTS; the input sets the RESOLUTION at which rest/legato(/accent) may subdivide and re-articulate.
Unpatched = step resolution, as today.

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

## Design fork: pitch per sub-hit (ratchet vs roll)
Rest/legato/accent clearly follow the fine grid. PITCH is the fork: when a gate subdivides, do the
sub-hits SHARE the parent note's pitch (a RATCHET — same note repeated) or draw FRESH pitch each sub-hit
(a ROLL/TRILL — different notes)? Default should be ratchet (subdividing should not silently re-roll
pitch every sub-hit); fresh-pitch is the opt-in. Likely a per-voice or per-lane choice, not global.
Pitch/octave/q-mix otherwise stay at the note-event grain.

## Open question (the other real build item)
Two parts: (1) does patching the input AUTOMATICALLY raise the evaluation resolution, or only within an
explicit legato span? Auto is more discoverable (patch a faster clock -> subdivision just happens). (2)
WHICH lanes evaluate at the fine grid — rest + legato at least, accent probably, pitch NOT (see fork
above). Confirm what the engine reads today: step-legato currently supplies inner boundaries WITHIN a
legato run; the extension is to let an external clock govern rest/legato(/accent) re-articulation of ANY
gate, not only inside a committed slur. Verify before building.

## Deferred / rejected alternatives (recorded so they are not re-proposed)
- **Rhythm q-mix / 4th stream** — over-scoped; doubles the Sands/panel/CA surface for a capability this
  covers, and muddies the clean "timing origin (mode) x pitch origin (q-mix)" two-axis story.
- **Internally-generated subdivision grid** (invent N even divisions within a gate with nothing patched)
  — the only genuinely new build; leave until a musical case needs subdivision with NOTHING clocked in.
- **Patched burst/ratchet generator in front of the gate input** — works today for UNcorrelated,
  non-reversible subdivision; on-brand (Monsoon perturbs what arrives). The internal route exists only
  for the correlated/reversible version.
