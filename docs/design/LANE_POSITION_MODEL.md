# Lane position: walker vs read head

How a lane knows which cell it is on. Settled after three wrong turns and two live bugs, so
the reasoning is recorded here rather than the conclusion alone — the conclusion is one line
and the reasoning is the part that transfers.

## The conclusion

> A lane's position is a **pure function of the master clock**:
> `position = laneTickFor(dir, totalStepsElapsed, len)`, then `getStrandIdx` applies rot/off.
> No lane carries an accumulator.

`laneTick_` / `laneTickV_` / `macroLaneTick_` still exist, but as a **cache** of that function,
recomputed each step, so readers need no change. `laneSign_*` are **derived** (step-to-step
delta) for the UI cue only; nothing reads them to decide position.

## Why `totalStepsElapsed` exists at all

It is the DNA/polymeter master position, and everything below follows from it.

`stepIndex` cycles 0–15 and resets every bar. A lane reading from it would restart identically
each bar — a len-3 lane would play `0,1,2,0,1,2…` forever and never move against the bar.
`totalStepsElapsed` is free-running: a len-3 lane reads `t mod 3`, and since `16 mod 3 = 1`,
each bar starts one cell later. **That precession is the DNA idea.** Lanes of coprime lengths
precess against each other; hard reset zeroes the counter ("sync polymeters to Beat 1") and
snaps them back into alignment at once.

`DNA_LCM = LCM(1..16) = 2⁴·3²·5·7·11·13 = 720720`, so `720720 % len == 0` for every valid
length: at the wrap, every lane is exactly where it was at `t = 0`. The wrap point is chosen so
that it coincides with the moment the whole system has already realigned — that is what makes
it inaudible.

**One clock. Lanes differ only by their window.** That is the invariant.

## What went wrong

Per-lane direction gave each lane its own accumulator (`laneTick_ += dir * sign`). That forked
every lane off the shared clock into private history — and broke the invariant. Three
consequences, all the same bug, two of them silent:

1. **Drift** (reported). Reverse a lane for *k* steps and it is `2k` behind the master
   **forever** — nothing ever re-aligned it. Lanes flipped at different times each carried a
   private offset and never resynced.
2. **Prob-out disagreement** (found in review). `masterLaneStep()`, `polyLaneProbability()` and
   `polyLaneStep()` still read the raw `totalStepsElapsed` while the notes read `laneTick_`.
   Reversing a lane reversed its notes but **not** its probability CV or S&H step detection.
3. **RESET stopped working** (found in review). `handleRestart()` zeroed `totalStepsElapsed`
   but nothing cleared `laneTick_*`, so lanes walked straight on and polymeters silently
   stopped syncing to Beat 1.

The pattern: *anything that forks off the master clock needs its own copy of every rule that
applies to the master clock, and it will be forgotten.* Two of these were invisible.

## The decisive argument: Reversible mode

The instrument had **already answered this question**, one layer down.

`setRhythmReversible()` switches the RNG to Philox — *"pure dice, signed index, forward/back"*.
Philox is counter-based: `value = f(key, index)`. To run the sequence backward you decrement the
index. There is no state to unwind. **Reversibility was already defined here as "make it a pure
function of a signed index."**

A walker cannot meet that bar, and not by a little. It is an integrator whose update rule is
time-asymmetric (the bounce test fires on one side), so running it backward does not retrace —
it runs a *different system*. Measured, len=4 pendulum:

```
closed form  fwd : 0 1 2 3 2 1 0
closed form  back: 0 1 2 3 2 1 0     <- exact retrace
walker       fwd : 0 1 2 3 2 1 0
walker       back: 3 0 3 0 3 0       <- garbage
```

Under Mode E with a reversing phase ramp, the draws retrace exactly (Philox) while a walked
lane does not — so the lane desyncs **from its own random data**, which is the one thing
Reversible mode exists to prevent. Modes A–D never reverse, so this only bites in Mode E; but
the model cannot be chosen per-mode without the switch itself jumping.

## The teleport objection, and why it doesn't hold

The real objection to closed form: `len` is CV-modulated at control rate, and Pendulum/PingPong
have period `2(len-1)` / `2·len`, so the *shape* of the function depends on `len` — change it and
the phase recomputes and the lane jumps. A walker would just turn around at the new endpoint.

This looked decisive and isn't, because **a plain Forward lane already teleports on `len`
modulation, and always has**:

```
FORWARD lane, len 4 -> 5 at t=104:   cell 3 -> 4 -> 0   (jump)
```

That is `getStrandIdx(totalStepsElapsed, len, off, rot)` — the shipping DNA design, identical
under either model since `laneTick_ == t` for Forward. `len` is part of the **address**, exactly
like `rot`; changing the address means you are somewhere else. Modulating `rot` teleports today,
deliberately.

So the walker does not *prevent a bug* — it makes bouncers **uniquely immune to a behaviour
every other lane has**. That is an inconsistency, not a feature. The jump is also bounded by
`len` (within the window) and motion resumes normally after; and `len` is `lround`ed, so a slow
LFO changes it only at quantisation crossings.

## The trilemma (there is no free corner)

| want | pay |
|---|---|
| smooth engage / smooth `len` modulation | position depends on history → drift, no retrace, no scrub |
| deterministic phase, retrace, scrub | jumps when `len`/`rot`/mode changes |
| both | — does not exist |

Ranked against what this instrument actually does:

| | `len` modulation | Mode E reverse | scrub / evaluate at arbitrary t |
|---|---|---|---|
| walker | smooth ✓ | **broken** ✗ | impossible ✗ |
| closed form | jumps (as every lane already does) | exact ✓ | exact ✓ |

The walker wins on the one axis where the rest of the instrument doesn't even try.

## Consequences worth remembering

- **Computable vs replayable.** A closed-form lane's position is a *fact about now* — ask for
  `t = 100000` and get it instantly. A walker's is a *summary of everything that happened*;
  there is no answer without simulating every step. That closes off scrub, transport jumps and
  offline evaluation.
- **A cache cannot be stale if it is recomputed from the clock every step**, which is why the
  prob-out bug becomes structurally impossible rather than merely fixed.
- **`resetLaneWalk()` is now redundant** (zeroing the clock syncs everything). Kept because it
  is free and correct again the instant a walker returns.
- **The len-16 PingPong wrap edge does not arise** for the walker (phase in state) but **does**
  for closed form: PingPong's period is `2·len`, and `2·16 = 32` does not divide `DNA_LCM`
  (`2⁴·…`), so a len-16 PingPong lane jumps phase once per DNA wrap ≈ 25 h of continuous play.
  Fix if it ever matters: wrap at `LCM(1..16)·2 = 1441440` — divisible by 32 and still by every
  `len`, so DNA drift semantics are preserved.

## History of wrong turns (the useful part)

1. **"Direction has no store — add 96 East params + a manager push."** False: it round-tripped
   in Monsoon's JSON. Would have created a *second persisted home* — the exact bug it claimed to
   prevent. Reverted.
2. **"So the engine is the right home; just change-detect the push."** Half true — right for a
   datum Monsoon authors, but the Monsoon menu was dev scaffolding, so nothing Monsoon-side
   authored direction. Once that fell, the engine had no claim.
3. **"Accumulate the tick so flips turn around smoothly."** Bought a smooth turnaround and paid
   with permanent drift, a broken RESET, a silent prob-out disagreement, and a pendulum that
   cannot run backward under the mode built for running backward.
4. **"The teleport kills closed form."** Overstated — Forward lanes already teleport on `len`.

The recurring lesson, in three forms: *check where a datum is persisted before declaring it
homeless; check who authors it before choosing its home; and check whether the "new" cost is
one the system already pays everywhere else.*
