# How the playhead moves

Canonical statement of position, for documentation and for reviewing the core algorithms.
Design rationale lives in `LANE_POSITION_MODEL.md`; this is the *what*, not the *why*.

There are **two playheads** and they are not the same thing. Most confusion in this area
comes from conflating them.

---

## 1. The physical playhead — `stepIndex`

Where the sequencer is *in the bar*. Walks the 16-step window, `startStep..endStep`.

`advancePlayhead(dir)`:

```
prevStep  = stepIndex
lastPlayDir = sign(dir)                       // +1 fwd, -1 rev (Mode E)

forward (dir > 0):
    seed if unstarted: stepIndex = startStep - 1
    stepIndex = (stepIndex + 1) & 15
    skip out-of-window steps (bounded to 16 tries)
    wrapped = (stepIndex == startStep)

reverse (dir < 0):                            // exact mirror
    seed if unstarted: stepIndex = endStep + 1
    stepIndex = (stepIndex - 1) & 15
    skip out-of-window steps
    wrapped = (stepIndex == endStep)
```

`wrapped` is the **phrase boundary**, returned to the caller. It drives boundary-interrupt
and the flip quantization below.

## 2. The DNA clock — `totalStepsElapsed`

Where the sequencer is *in absolute time*. Free-running, **never resets at the bar**:

```
totalStepsElapsed = (totalStepsElapsed ± 1) mod DNA_LCM
```

This is the whole polymeter mechanism. `stepIndex` resets every bar, so a lane reading from
it could never drift. `totalStepsElapsed` doesn't, so a `len`-3 lane reads `t mod 3`, and
since `16 mod 3 = 1` each bar starts one cell later than the last. Lanes of coprime lengths
precess against each other. **That precession is the DNA idea.**

Hard reset (`handleRestart`) sets it to 0 — "sync polymeters to Beat 1" — which realigns
every lane at once, because every lane is a function of it.

### Why `DNA_LCM = 1441440`

The wrap must be **inaudible**: at `t = DNA_LCM` every lane must be exactly where it was at
`t = 0`. So the constant must be a multiple of every lane *period*, not merely every `len`:

| mode | period | needs |
|---|---|---|
| Forward / Reverse | `len` | 1..16 |
| Pendulum | `2(len−1)` | ≤ 30, worst case `2·8 = 16 = 2⁴` |
| PingPong | `2·len` | ≤ **32 = 2⁵** |

`LCM(1..16) = 2⁴·3²·5·7·11·13 = 720720` covers all but the last: 32 needs `2⁵`. So
`DNA_LCM = LCM(1..16)·2 = 1441440`. Divisible by every period; drift pattern unchanged (it
already repeated at 720720 — the counter now passes through without resetting).

Guarded by `test_lane_direction`: `DNA_LCM % period == 0` for all three modes across
`len` 1..16.

---

## 3. Lane position — the part that moves the read head

**Every lane is a pure function of the DNA clock.** No lane carries an accumulator.

```
cell(lane) = getStrandIdx( laneTickFor(dir, totalStepsElapsed, len), len, off, rot )
```

### `laneTickFor(dir, t, len)` — direction

```
L = max(1, len)

Forward:   t
Reverse:  -t

Pendulum:                       // triangle; bounces at the window ends, no repeat
    if L < 2: 0
    P = 2(L-1)
    u = ((t + L-1) mod P + P) mod P
    |u - (L-1)|                 // -> 0,1,..,L-1,L-2,..,1,0,1,..

PingPong:                       // triangle with a HOLD; the endpoint plays twice
    P = 2L
    u = (t mod P + P) mod P
    u < L ? u : P-1-u           // -> 0,1,..,L-1,L-1,..,1,0,0,1,..
```

Verified against the old accumulator: identical sequences for a static `len`.

### `getStrandIdx(tick, len, off, rot)` — window

```
safeLen     = max(1, len)
timelineIdx = ((tick + rot) mod safeLen + safeLen) mod safeLen   // drift inside the loop
return        (timelineIdx + off) mod 16                          // slide the window
```

`rot` phase-shifts *within* the lane; `off` slides *which source cells* the window sees.
Negative ticks wrap correctly — required, since `Reverse` returns `-t`.

### Direction changes

A UI/CV change sets `laneDirPending_`; the engine promotes it to `laneDir_` at a boundary:

- **StepEdge** — top of every step (live turnarounds)
- **Phrase** — only when `advancePlayhead` reports `wrapped` (default; musical)

Position is recomputed from `t` immediately after `totalStepsElapsed` advances.
`laneTick_*` / `laneSign_*` still exist but are a **cache of the function**, not
accumulators — recomputed each step so existing readers (displays, prob-outs, cues) need no
change. `laneSign_` is *derived* from the step-to-step delta, for the UI cue only; nothing
reads it to decide position.

---

## 4. Jumps: when the playhead moves discontinuously

**Jumps are unavoidable and are not a defect.** `len`, `off` and `rot` are the lane's
*address*, and all three are CV-modulated at control rate. Changing an address means being
somewhere else — there is no continuous path from a `len`-4 window to a `len`-16 window. A
square or saw LFO on length is a discontinuity *in the input*; no position model can smooth
it without lying about the parameter.

This is not new with per-lane direction. A plain **Forward** lane has always jumped on a
`len` change — `getStrandIdx(t, 4, …)` → `getStrandIdx(t, 5, …)` relocates the cell — and
that is the shipping DNA behaviour. Modulating `rot` is a phase jump *by construction*.

What the stateless model guarantees instead:

- **No drift.** A jump is momentary; position is always `f(t, len, off, rot, dir)`. There is
  no residue. Sweep `len` 8 → 3 → 8 and the lane lands exactly where it would have been had
  you never touched it. An accumulator cannot do this: the excursion is baked into its walk,
  permanently.
- **No compounding.** With 16 voices × 6 lanes, an accumulator gives 96 independent private
  offsets — the "polyphonic mess". Stateless has none, by construction. (Kodiak shows the
  same history-dependence under range changes, but it is mono, so it cannot compound.)
- **Computable, not replayable.** Position at any `t` is answerable directly. A walker's
  position only exists as the outcome of simulating every step — which forecloses scrub,
  transport jumps and offline evaluation.
- **Reversible.** This is the instrument's own standard: `setRhythmReversible` makes the RNG
  Philox — a pure function of a *signed index*, so decrementing runs the sequence backward
  exactly. A walker is an integrator with a time-asymmetric update, so reversing it does not
  retrace; it runs a *different system*. Measured, len-4 Pendulum:

  ```
  closed form  fwd: 0 1 2 3 2 1 0     back: 0 1 2 3 2 1 0     exact retrace
  walker       fwd: 0 1 2 3 2 1 0     back: 3 0 3 0 3 0       garbage
  ```

  Under Mode E with a reversing ramp the draws retrace while a walked lane would not — the
  lane would desync from its own random data, which is precisely what Reversible exists to
  prevent.

**Placing the jumps** is the available control, not avoiding them:

- `laneFlipQuant = Phrase` puts *direction* changes on the wrap.
- Beat-sync the modulation source (e.g. S&H at phrase rate into a rest/LOR CV) and the
  address changes land on the grid — one lap differs, then it heals.

That self-healing is the property to lean on: because nothing accumulates, a bold excursion
costs exactly one lap and never the patch.
