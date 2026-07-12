# Lane direction & reverse traversal

Notes captured from the Rule 2 legato work, where reverse playback (Mode E) forced
us to reason carefully about *direction* — how a lane is read, and what "the previous
note" means when the playhead can move either way. Recording the ideas and invariants
here so they survive, plus the open thoughts we didn't build.

## Why this matters

Legato and tie are **relational**: a note is a tie/legato because it connects to *the
note played just before it*. That "just before" is a **play-order (temporal)** fact,
not a step-number one. Forward, play order and ascending step number coincide, so the
distinction is invisible. In **reverse**, they diverge — and any logic that leans on
step number or on a raw gate read instead of play order produces wrong articulation.

The whole digression is really one rule stated three ways: **classify articulation by
the temporally-adjacent predecessor, never by a gate snapshot or a step index.**

## Reverse traversal mechanics (Mode E)

`advancePlayhead(int dir)` (`SequencerEngine.cpp`) drives both directions; `dir < 0`
is reverse.

- **DNA tick tracks direction.** `totalStepsElapsed` is stepped `-1` in reverse, `+1`
  forward (mod `DNA_LCM`). This index maps physical positions to the drifting DNA
  content (strand indices) and the ring lights. Counting *up* while playing backward
  desyncs it from `stepIndex`, and both the strand drift and the lights go the wrong
  way (the "ring lights up all around" symptom). So direction must be applied to the
  tick, not just the visual.
- **Reverse seeding is the mirror of forward.** Forward seeds *before* `startStep` and
  steps onto `startStep`; reverse seeds just *after* `endStep` (`(endStep + 1) & 0x0F`)
  so the first reverse step lands on `endStep`. Then it steps left (`-1`), skipping
  out-of-window steps, with the same 16-step safety bound as forward.
- **Boundary detection is symmetric.** Forward reports a phrase boundary when it wraps
  to `startStep`; reverse when it wraps back to `endStep`. `advancePlayhead` returns
  that boolean so the boundary-interrupt / continue behaviour is direction-correct.
- **`lastPlayDir`** (`+1` / `-1`) records the last direction taken, for the engine and
  the UI to read after the fact.

## The direction-correct predecessor test (mono)

In `executeStep`, whether *this* note connects back is governed (leading-edge model) by
the previous starting note's own onset commitment, `prevSlur = gs.slurForward`. But the
connection is only *real* if there was actually a sounding note immediately before in
play order to connect to. Two signals are involved and only one is direction-safe:

- **`wasHeld` is a gate READ.** It answers "is a gate currently held?" — not "was the
  play-order-adjacent predecessor sounding?" In reverse, `wasHeld` can be true from a
  note that is *not* the temporal predecessor, so a legato/tie could be emitted with a
  **rest immediately before it in play order** → an isolated teal cell connected to
  nothing. This is the **reverse isolated-teal bug**.
- **`prevPlayedDec = lastStepResult.decision` is play-order-correct.** `lastStepResult`
  is written at the end of every `executeStep`, so at the top of the next call it holds
  *the step played immediately before this one*, temporally — correct in both directions
  because it is temporal, not step-numbered. `prevPlayedSounded` is true when that
  decision was any sounding kind (NewNote / Legato / LegatoMax / Tie / MidNote).

The connect branch therefore requires
`legatoConnects && (wasHeld || hadTail) && prevPlayedSounded`: intent to connect, a held
gate/tail to slide on, **and** a real sounding predecessor in play order. Fail the last
and it falls through to `NewNote` (a fresh note), never an isolated teal.

## The poly analogue

`executePolyVoice` has the same trap on the per-voice lane. When mono slurs/ties into a
new gate but *this* poly voice had **no held gate** on its previous played step (it was
resting/silent), sliding would produce an isolated Legato cell — teal with no predecessor
*on that lane*. So the code triggers a fresh note instead of sliding:

```
if (decision == NewNote)            triggerNote(...)          // mono retrigger
else if (wasHeldPoly || hadPolyTail) slideNote(..., wasHeld=true)  // real slide
else                                 triggerNote(...)          // no predecessor → fresh
```

This is **direction-independent** — it can happen forward too — but reverse is what
surfaced it. It is the poly mirror of the mono isolated-teal fix, and it composes with
Rule 2's later per-voice consume (a slur can only connect to a still-sounding predecessor).

## UI direction cues

`lastPlayDir` feeds the visuals so the display reads the same direction the engine plays:

- **Trail LED** = the step the playhead just *left* = `stepIndex - lastPlayDir`
  (`((stepIndex - lastPlayDir) % 16 + 16) % 16`). Using `-lastPlayDir` makes the trailing
  light sit behind the playhead in whichever direction it is moving.
- **Editors / lantern** get `setPlayDir(engine.lastPlayDir)` (StraitsEastSandsVisual,
  StraitsSandsMacroVisual, MonsoonSandsVisualExpander) so the playhead cue points the
  right way.

## Invariants to preserve

1. Apply direction to `totalStepsElapsed`, not only to `stepIndex` — strand drift and
   ring lights depend on it.
2. Classify legato/tie by the **temporal predecessor** (`prevPlayedDec` /
   `prevPlayedSounded`), never by `wasHeld` alone or by step number.
3. A connection needs *both* a held gate to slide on **and** a sounding play-order
   predecessor; otherwise emit a fresh note.
4. Reverse seeding/boundary is the exact mirror of forward (seed past `endStep`, wrap to
   `endStep`).
5. The poly per-lane path obeys the same "no predecessor → trigger, don't slide" rule.

## Open ideas (not built)

- **Per-lane / per-strand direction.** Today direction is global (Mode E). A natural
  extension is a per-lane direction bool so each LOR lane (rest / melody / octave /
  accent / variation / legato) can read its window forward or reverse independently —
  retrograde melody over a forward rhythm, palindrome textures, etc. Same delegation
  shape as VARIATION/LEGATO (default = follow global, opt-in = own direction). The
  articulation classification above already keys off temporal order, so a per-lane
  reverse would need its *own* per-lane play-order predecessor, not the global one —
  that's the main design cost to think through before building.
- **Pendulum / ping-pong traversal.** A mode that alternates direction at each phrase
  boundary. Reuses the symmetric seeding; the boundary return value is the flip trigger.
  Watch the endpoints so the turn-around note isn't double-played or dropped.
- **Reverse × Rule 2 per-voice legato.** The per-voice consume ("connect only to a
  still-sounding predecessor") should already be direction-safe because it reads gate
  state, not step order — but it hasn't been auditioned in reverse. Worth a pass to
  confirm no per-voice isolated-teal appears when the global direction is reversed.
