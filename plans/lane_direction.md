# Plan: per-lane direction (reverse a lane's read, independently)

Give each LOR lane its own direction so it can read its window forward or reverse
independently of global playback (retrograde melody over a forward rhythm, etc.).
See `docs/design/LANE_DIRECTION_REVERSE.md` for the reverse-traversal background.

## Design decisions (settle these first)

### 1. Semantics — opposite-to-global (relative), not absolute
A reverse lane runs *opposite to the global direction*: it retrogrades when playback
is forward, and runs forward under Mode E reverse (double-negative). This composes with
the existing global direction for free and matches the musical meaning of "retrograde
this lane." Absolute retrograde (always backward regardless of global) is a possible v2
but needs the lane to ignore global sign — skip for v1.

### 2. Read model — per-lane accumulated tick (NOT negate-tick)
`getStrandIdx(tick, len, off, rot) = ((tick + rot) mod len + off) mod 16`, and it already
wraps negative ticks. Two ways to reverse a lane:

- **Negate-tick (stateless):** `getStrandIdx(-totalStepsElapsed, …)`. Simple, but flipping
  the sign *jumps* the window position (mirror around `rot`). Only clean at a phrase
  boundary where the lane restarts anyway.
- **Accumulated per-lane tick (chosen):** each lane owns `laneTick_[l]`, advanced each step
  by its effective direction. `cell_l = getStrandIdx(laneTick_[l], len, off, rot)`. Flipping
  direction turns the lane around **from its current position** — a smooth pendulum turn,
  no jump — so it can flip cleanly on a step edge, not just at a phrase boundary. This is
  the same walk model `advancePlayhead` already uses globally, and it **reduces exactly to
  today's behaviour** when every lane is forward (`laneTick_[l] == totalStepsElapsed`).

Per-step advance: `laneTick_[l] += globalDir * laneSign_[l]` (mod `DNA_LCM`), where
`globalDir` is the sign passed to `advancePlayhead` and `laneSign_[l] ∈ {+1,-1}`
(+1 = follow global, default; -1 = reverse). All the DNA drift / cross-lane phasing is
preserved (it comes from differing `len`, untouched).

State cost: 6 ints for mono; +90 (15×6) if the per-voice extension (step 6) is built.

### 3. Flip quantization — WHEN a direction change takes effect
Never mid-step (that glitches the value read). A UI direction change sets
`laneSignPending_[l]`; the engine promotes it to `laneSign_[l]` at a boundary:

- **Step edge (floor):** apply pending at the top of each step, before advancing
  `laneTick_`. Smooth turnaround (because accumulated). This is the minimum granularity.
- **Phrase boundary (option):** apply pending only when `advancePlayhead` reports a wrap,
  so the next phrase plays retrograde from its start.

Provide both, as a mode (`laneFlipQuant ∈ {StepEdge, PhraseBoundary}`), global first;
per-lane later if wanted. **Recommended default: PhraseBoundary** (most musical / least
surprising), with StepEdge available for live turnarounds. The phrase boundary is the
global window wrap (`advancePlayhead`'s return), i.e. the same boundary
`boundaryInterrupt` keys off.

### 4. Scope
Mono's 6 lanes first (rest/mel/oct/acc/var/leg). Value lanes are pure read-flips; the
legato lane is mechanically identical (its reversed *draw* just changes when slurs fire)
and needs **no** new predecessor logic — mono keeps one global articulation stream, so
`prevPlayedDec` stays correct. Poly per-voice is a later, heavier extension (step 6).

## Steps

1. **Engine — accumulated ticks + effective direction.**
   - Add `int laneTick_[6]` and `int8_t laneSign_[6]` (default +1), `laneSignPending_[6]`,
     and `laneFlipQuant`. Reset `laneTick_[l] = 0` in `reset()`.
   - In `advancePlayhead(dir)` (or right beside it, per step): if `laneFlipQuant==StepEdge`
     promote pending→current for all lanes; then `laneTick_[l] = (laneTick_[l] +
     dir*laneSign_[l] + DNA_LCM) % DNA_LCM`. If the call reports a wrap and
     `laneFlipQuant==PhraseBoundary`, promote pending→current there.
   - Route each mono lane read through its own tick: the `getRestStep()/getMelodyStep()/
     getOctaveStep()/getAccentStep()/getVariationStep()/getLegatoStep()` accessors switch
     from `totalStepsElapsed` to `laneTick_[l]`. One edit per accessor; the 31 raw call
     sites mostly go through these.

2. **Standalone test first** (`g++ -std=c++17`, like the Rule 2 replicas).
   - Pin the pure math: a lane walking `+1` vs one walking `-1` visits the same window
     cells in reversed order; `off`/`rot` still land the window; the all-forward case is
     bit-identical to `getStrandIdx(totalStepsElapsed,…)`.
   - Pin the flip: after a mid-window sign flip, the next cells are the immediate neighbours
     in the new direction (turnaround), NOT a jump to the mirror position.

3. **Persistence.** Save/load `laneSign_[6]` + `laneFlipQuant` in
   `MonsoonPersistenceManager` next to `restBeatsLegato`/`boundaryInterrupt`.
   (`laneTick_`/pending are runtime, not persisted.)

4. **UI + cue.** Context-menu toggles first ("Reverse melody lane", …) + a flip-quant
   submenu — no panel change, fast to audition. If it earns permanent controls: a lane-end
   arrow cell in the visual editor (same pattern as the delegation `OwnerCell`), an arrow
   glyph on reversed lanes, and a per-lane playhead that travels the lane's own direction
   (extend the existing `setPlayDir`).

5. **Audition, legato last.** Value lanes → retrograde textures, low risk. Then the legato
   lane on its own: confirm no isolated-teal (there shouldn't be — classification stays
   global) and that reversed slur timing feels right. Sweep both flip-quant modes live.

6. **Per-voice extension (optional, later).** Mirror Rule 2's delegation exactly: per-voice
   per-lane `laneSign` (default follow mono's lane dir, opt-in own), `laneTick_[15][6]`,
   pushed by the manager, read at the poly `getStrandIdx` sites (555–557, 577, …). Only if
   the mono version proves worth it.

## Invariants to hold

- All-forward must stay bit-identical to current output (`laneTick_[l] == totalStepsElapsed`).
- Direction promotion happens only at a boundary (step edge or phrase), never mid-step.
- A reversed lane turns around from its current position (no jump) — the pulse-clean flip
  is the whole reason for the accumulated model.
- Legato reverse adds no predecessor logic; mono articulation stays one global stream.
- `laneTick_` advances with `dir*laneSign_` so it composes correctly with Mode E.

## Open questions

- Per-lane vs global `laneFlipQuant`? Global is simpler; per-lane lets one lane snap on
  phrase while another turns on the step. Start global.
- Absolute retrograde (v2): a lane that always retrogrades regardless of global — needs a
  separate model (ignore `globalDir` for that lane). Defer.
- Pendulum/ping-pong as a *lane* mode (auto-flip `laneSign_[l]` at each phrase boundary),
  reusing this exact machinery — natural follow-on once flip-quant exists.
