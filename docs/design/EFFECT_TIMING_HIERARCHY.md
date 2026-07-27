# Effect-timing hierarchy — when a control's change reaches the engine

Status: PRINCIPLE (engine-wide, cross-cutting). Not module-specific; guides lock
implementation and every pattern-shaping control.

## The core principle
**Nothing that shapes the pattern should be sampled faster than the pattern is READ.**

The pattern advances at 1/16-step boundaries — that is the rate the engine CONSUMES its
shaping inputs. Reading a pattern-shaping control (LOR, A/B mix, spread, Change Alley pins) at
CONTROL RATE (audio-block rate, ~kHz) computes thousands of intermediate values per step that
the engine reads only ONCE per step. Every intermediate sample is either:
- wasted (the extra precision is discarded at the next step read), or
- WRONG (the playhead/display reacts to a value the sequencer has not acted on yet — this is
  the Sands lane-playhead JITTER: it reacts to realtime LOR changes mid-step).

So control-rate sampling of pattern-shaping controls is strictly not-better: at best wasted, at
worst jitter. **Step-boundary sampling is the CORRECT rate, not a compromise.** The playhead
"jumping" at the step boundary is the HONEST behaviour — it shows exactly when the change took
effect (when the engine read it), instead of smearing a not-yet-consumed value across the step.

The jitter is a SYMPTOM of a rate mismatch. The fix is not to smooth it (that papers over the
cause) but to sample at the rate the value is actually used (removes the cause).

## The read-vs-map test (what quantizes to the step, what doesn't)
Same test as the lock classification (LOCK_SEMANTICS.md 9):
- Does the value feed the **pattern READ**? -> quantize to the 1/16 step boundary.
- Does it feed the **continuous OUTPUT** (audio, true CV passthrough, a value meant to sweep
  audibly within a step)? -> leave at control rate.

Members that feed the pattern read, so QUANTIZE to 1/16 step:
- LOR mod (len/off/rot) — the case that surfaced this (Sands playhead jitter).
- A/B mix probability.
- Spread probability.
- Manual Change Alley pin changes.
- (Any future pattern-shaping control: apply the test, don't assume.)

State it as "controls feeding the pattern read", NOT "all controls" — a control genuinely meant
to sweep within a step would be wrong to quantize. No such counterexample in the shaping set
today, but the principle must not over-apply.

## The effect-timing ladder (how this composes with lock + queue)
This insight names a rung in a hierarchy that was implicit across the lock work. From finest to
coarsest, WHEN a change reaches the engine:

| Rate | What | Why |
|---|---|---|
| control rate (~kHz) | genuinely continuous signals: audio, CV-to-output | must be smooth |
| **1/16 step boundary** | **pattern-shaping controls: LOR, A/B, spread, manual pins** | **read rate of the pattern (this doc)** |
| phrase boundary | queued EVENTS: dice, scatter/trigger gates, scene advance | can't hold a value; arm-and-fire |
| unlock | latched CONFIG under lock mode | DJ-cue: silent until release |

The rungs COMPOSE, they don't conflict:
- A pattern-shaping control reads at STEP rate normally; the SAME control, under LOCK, defers to
  UNLOCK (lock is a coarser gate layered on top).
- An event always defers to the PHRASE boundary (queue), whether or not lock is engaged.
- Lock/queue (LOCK_SEMANTICS.md) answer "when does an EDIT reach the engine"; this doc answers
  "when is a LIVE value SAMPLED". Same shape (quantization of effect), different granularity.

## Relation to de-param
Not caused by de-param — control-rate reads always happened. But de-param made it VISIBLE
(explicit single-point store reads) and makes the FIX clean: the "sample at step boundary" gate
belongs exactly at the single store->engine read the de-param created. Implement the gate there.

## Implementation note (for when built)
The step-boundary sample point is the same clock edge the engine already uses to advance the
pattern (stepIndex change / totalStepsElapsed increment). Latch the shaping-control values into
the engine's working copy on that edge; read the working copy, not the live control, during the
step. One gate, at the store->engine boundary, keyed on the step-advance edge.
