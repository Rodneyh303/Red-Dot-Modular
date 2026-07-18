# Feasibility Audit — Phase-Driven Mode (idea 2)

Read-only investigation. No code written. Verdict up front, then evidence, then a
phased plan and the one genuine design knot.

## Verdict: FEASIBLE, medium effort — and easier than I feared

The architecture is more favourable than a typical stochastic sequencer because of
one design choice you already made: **the randomness is precomputed per-step, not
rolled live on the clock edge.** That makes the engine latently position-
addressable, which is exactly what phase mode needs. The clock is also cleanly
isolated behind a small ClockEngine that produces edge pulses the sequencer
consumes — a clean seam to splice a phase engine into.

It is NOT a drop-in "swap ClockEngine for PhaseEngine" though: step advance and the
note-hold countdown are coupled to the forward edge. Reverse/scrub need those
reworked. So: a focused feature with one real design decision, not a rewrite.

## What I found (the evidence)

### 1. The clock is cleanly isolated ✓
`src/dsp/engines/ClockEngine.{hpp,cpp}` is a self-contained unit. It takes
clockVoltage + sampleTime and produces three outputs the sequencer reads:
  - `sixteenthEdge` (bool, one-sample pulse — "advance a step now")
  - `quarterEdge`  (bool)
  - `bpm` / `sixteenthSec` (timing)
The sequencer only ever reads `clock.sixteenthEdge` / `.quarterEdge` / `.sixteenthSec`.
That's the splice point: a PhaseEngine would produce the same "edge" signal when
phase crosses a step boundary, plus expose fractional position.

### 2. Randomness is per-step deterministic, NOT live-rolled ✓✓ (the key win)
executeModeA reads dice from precomputed arrays indexed by step:
    pe.variationRandom[getVariationStep()]
    pe.rhythmRandom[getRhythmStep()]
    pe.legatoRandom[getLegatoStep()]
    pe.accentRandom[getAccentStep()]
NOT random::uniform() on the edge. So step N always yields the same roll until the
pattern is regenerated. This means a phase engine landing on step N — forwards,
backwards, or by scrubbing — gets the SAME values. Phase-addressable determinism
is already latent. This is the single biggest reason this is feasible; in most
stochastic sequencers it's the dealbreaker.

### 3. What IS coupled to the forward edge (the work)
  a. `advancePlayhead()` (SequencerEngine.cpp:73) — increments stepIndex by +1 and
     skips to in-window steps. Assumes monotonic forward. Phase mode needs
     "position = floor(phase * len) mapped through the window" instead of ++.
     advancePlayhead is a clean standalone fn, so this is replaceable, but the
     window-skip logic (start/len/playable-step) must be re-expressed as a
     position map.
  b. `GateState::tick()` (gates/GateState.cpp) — decrements holdRemain by 1 PER
     EDGE (step units). This is the multi-step note-spanning "secret sauce": a
     note spanning 3 steps sets holdRemain=3 and counts down each edge. This is
     the most forward-assuming piece. Under phase reverse/scrub it would
     mis-count. Options below.
  c. executeModeA fires the whole step (advance + gate + hold tick + executeStep)
     as ONE edge event. Under phase, crossing a boundary mid-buffer is fine
     (treat as one edge), but sitting still or going backward must NOT re-fire.

### 4. Modes already vary the trigger source — there's precedent ✓
The engine already has executeModeA (internal clock edge), executeModeB
(external gate rise — gate1Rise/gate1High), executeModeC (CV quantize). So
"different things advance the sequencer" is an established pattern. A phase mode is
philosophically a 4th trigger source — it slots into the same dispatch.

## The one real design knot: note-hold under non-forward phase

Everything else is mechanical. The genuine decision is what multi-step held notes
(holdRemain, the phrase-feel secret sauce) mean when phase can reverse or scrub:

  - Option A (simplest, "phase as clock"): phase only ever moves FORWARD (you feed
    it a rising ramp). Detect boundary crossings, fire one edge per crossing,
    holdRemain works UNCHANGED. You get tempo-ramping, swing-via-phase-warp,
    sync-to-external-phase — but NOT reverse/scrub. ~80% of the Phaseque appeal
    (continuous position, warpable timing) for ~30% of the effort. RECOMMENDED
    first cut.
  - Option B (full Phaseque-style): phase addressable anywhere, reverse + scrub.
    Requires note-hold to become POSITION-derived, not countdown-derived: "is this
    step within the span of a note that started ≤N steps earlier" computed from
    position, not a decrementing counter. This is a real rework of GateState's
    hold model and the note-spanning logic, and it forces a decision on stochastic
    re-entry (landing mid-held-note while scrubbing — recompute the span
    deterministically from the owning step's precomputed roll). Doable BECAUSE the
    rolls are deterministic per step, but it's the bulk of the effort.

My recommendation: ship Option A as "Phase mode" (forward phase clock), and treat
Option B (bidirectional/scrub) as a later, separately-scoped enhancement. Option A
alone is genuinely useful and novel (a stochastic sequencer driven by a warpable
phase ramp) and de-risks the concept before committing to the hold-model rework.

## Phased plan (when you want to build it)
1. PhaseEngine.{hpp,cpp} mirroring ClockEngine's output contract: input a 0–10V
   phase ramp, output sixteenthEdge on forward boundary crossing + expose
   fractional position. Internal fallback ramp when unpatched (so it free-runs).
2. A mode/menu toggle (clock vs phase) — reuse the modeSelect dispatch; phase
   feeds the same executeModeA path via synthesized edges (Option A).
3. Position map: replace advancePlayhead's ++ with floor(phase*len)→window step
   for phase mode (keep ++ for clock mode).
4. (Later, Option B) reverse/scrub: rework GateState hold to position-derived
   spans; define deterministic re-entry.

## Effort estimate
  Option A: medium — a new engine unit + a mode toggle + a position map. The
  deterministic-roll design means no stochastic re-architecture. A few focused
  sessions.
  Option B: significant — the hold-model rework is the real cost, plus careful
  reasoning about the note-spanning logic under reverse. A proper project.

## Bottom line
Your instinct ("swap the clock engine for a phase engine") is right for Option A,
because ClockEngine is a clean isolated seam AND the rolls are already
per-step-deterministic. The thing that's NOT a swap is the note-hold countdown
(secret sauce), which is forward-coupled — fine for forward-phase Option A,
the core rework for bidirectional Option B.

════════════════════════════════════════════════════════════════════════════════
# DEEP DIVE — Option B (bidirectional / scrubbable phase)

Option A treats phase as a forward clock: detect boundary crossings, fire one edge
each, everything downstream is unchanged. Option B lets phase go anywhere —
reverse, scrub, jump, sit still — and asks the sequencer to render the correct
musical state for ANY position at ANY time. That is a different and much deeper
problem. Below are the specific engine realities that make it hard, each with what
it would take.

## The core mismatch: the engine is EVENT-DRIVEN MUTATION, not POSITION→STATE

Phase mode wants a pure function: position → (gate, pitch, accent). The engine
instead evolves running state through a sequence of mutating events. Option B's
real cost is converting enough of that mutation into position-derivable functions
WITHOUT losing the musical behaviour the mutation produces. Five concrete obstacles:

### Obstacle 1 — Hold counter is accumulative, not positional  (HARD)
GateState.holdRemain is mutated incrementally:
  triggerNote: holdRemain = dur
  slideNote  : holdRemain += dur        (legato extends)
  extendHold : holdRemain += dur        (tie extends)
  tick()     : holdRemain -= 1 per edge
A note's actual sounding span is therefore a function of the HISTORY of slides/ties
that extended it, accumulated forward. There is no stored "note at step N spans
[N, N+k]" you can query at an arbitrary position. To scrub, you'd have to
reconstruct: "for the step that owns position P, walk forward from its start
applying the same legato/tie/dur decisions to know if P is still within its span."
Doable because the decisions are deterministic (precomputed rolls), but it means
replacing the counter with a positional span computation — a real rewrite of
GateState's contract, and re-deriving it touches triggerNote/slideNote/extendHold/
slideMax/rest/armGate.

### Obstacle 2 — Sub-step precise timing is wall-clock seconds  (MEDIUM-HARD)
gateSecRemain + curStepSec implement exact sub-step note lengths (triplets, 1/32)
by counting DOWN real seconds per sample (armGate sets durSteps*curStepSec). This
is inherently a forward, real-time countdown — it has no meaning at a scrubbed
position (what does "0.5 steps of gate remaining in seconds" mean when you jump
backwards?). Under phase, sub-step gate length would have to be re-expressed as
"gate is high while fractional-position-within-note < noteLenFraction", i.e.
positional, not a seconds timer. The good news: armGate's own comment says
whole-step lengths are already closed by the edge mechanism (not the seconds
timer); only fractional tails use gateSecRemain. So the positional rewrite is
bounded to the sub-step tail case.

### Obstacle 3 — Slew is "consumed at roll time" (stateful across steps)  (HARD)
PatternEngine: "SLEW is consumed here" — the slewed* buffers (slewedRhythm[16],
slewedPolyRhythm[15][16], etc.) carry a value that slews from the previous draw
toward the new target ACROSS steps, latched at re-draw (latchMix /
recomputeEffectiveRhythm: public = A + mixLatched*(B-A)). This is explicitly a
running, order-dependent process: the value at step N depends on the slew state
arriving from N-1. Scrubbing backward into N would need the slew state AS IT WAS
when playing forward — which isn't stored per-position. Either (a) make slew
positional/deterministic too (recompute the slew trajectory from the phrase start
to position P — expensive but deterministic), or (b) declare slew a "forward-only
expressive feature" that freezes/disables under scrub. (b) is the pragmatic call.

### Obstacle 4 — Phrase boundaries MUTATE the pattern (reseed/redraw)  (HARD/CONCEPTUAL)
At phrase boundaries the engine applies pending seeds and REDRAWS patterns — the
pattern arrays themselves change (new dice draw for the next phrase). So "position
→ state" is only deterministic WITHIN a phrase; across a phrase boundary the
content is regenerated. Under forward play that's invisible (you only cross
boundaries going forward). Under scrub it's a genuine conceptual problem: if you
scrub back across a phrase boundary, do you (a) restore the previous phrase's
pattern (requires storing phrase history — unbounded), (b) keep the current
pattern (scrub is "wrong" across boundaries but stable), or (c) forbid scrubbing
past the current phrase? This is a DESIGN decision with no purely-correct answer —
Phaseque sidesteps it by not being a re-seeding stochastic engine. This is the
deepest issue and it's conceptual, not code.

### Obstacle 5 — Poly mirrors all of the above ×15  (MULTIPLIER)
Every voice has its own GateState (voices[i].gs) with the same accumulative hold,
plus poly-specific rules (MidNote/Rest/Tie interplay with the mono voice — the
poly voices' behaviour DEPENDS on what the mono voice did this step). So poly
position→state is even more history-coupled: a poly voice's note can be a function
of the mono voice's tie/rest decision at the same step. Reconstructing that at an
arbitrary scrub position means reconstructing the mono decision first, then each
poly voice. Deterministic (rolls are fixed) but heavy.

## What Option B would actually require (honest scope)
1. Replace GateState's accumulative hold with a POSITIONAL span model: for any
   position P, compute "which note-start owns P and does its (deterministically
   derived) span cover P" — for mono AND 15 poly voices. New core abstraction.
2. Re-express sub-step gate length positionally (bounded to fractional tails).
3. Decide slew under scrub: recompute-from-phrase-start (pure but costly) vs
   freeze-under-scrub (pragmatic). Recommend freeze.
4. Decide phrase-boundary scrub semantics (Obstacle 4): recommend "scrub clamped
   to current phrase" for v1 — simplest coherent rule, avoids phrase-history
   storage.
5. Poly: apply the positional model per voice, preserving the mono-dependency
   rules. The multiplier on all the above.
6. A reverse-aware edge concept: "crossed boundary downward" must NOT re-trigger
   envelopes the way a forward cross does (or it should — another design choice:
   does scrubbing backward retrigger? Phaseque-style usually retriggers on
   crossing in either direction).

## Effort + risk
SIGNIFICANT. Not infinite — the deterministic precomputed rolls are what keep it
in the realm of possible — but it's a re-architecture of the hold/gate model plus
~4 genuine design decisions (Obstacles 3,4,6 + retrigger-on-reverse) that change
musical behaviour. This is a multi-week project with real regression risk to the
note-spanning "secret sauce" that's central to the instrument's feel.

## Strong recommendation
Ship Option A first (forward phase clock) — it's the clean swap, preserves all the
secret-sauce behaviour untouched, and delivers the headline phase features
(continuous position, warpable/rampable timing, external phase sync). LIVE WITH IT,
play it, and let real use tell you whether bidirectional scrub is worth the
Obstacle-1-through-5 rework. Much of Phaseque's magic (phase as a modulatable,
warpable position source) is already in Option A; true scrub/reverse is the
expensive 20%. If after living with A you want B, the four design decisions above
are the place to start — and several (slew-freeze, phrase-clamp) have pragmatic
answers that shrink the scope considerably.

A middle path also exists ("Option A+"): forward phase BUT allow position JUMPS to
phrase-aligned points (no mid-note scrub, no reverse). That dodges Obstacles 1,2,3
(notes only ever start at the jumped-to step) while adding live re-cueing — a
fraction of B's cost. Worth considering if jump-but-not-scrub covers your musical
intent.

════════════════════════════════════════════════════════════════════════════════
# BPM under phase — demoted, not deleted

Q: is BPM irrelevant under phase? Short answer: as a RATE control, yes; but it
doesn't fully disappear — two engine dependencies still need a "seconds-per-step"
value, and the unpatched free-run case still needs a rate.

## As a rate control: redundant under external phase
Under a clock, BPM/clock-rate IS what decides when steps happen — the engine
generates timing. Under external phase, the phase ramp's SLOPE already encodes the
rate: whoever generates the phase (LFO, clock→phase converter, Phaseque) owns the
tempo, and the engine just reads "where am I in the cycle." Setting BPM would do
nothing. → In phase mode, grey out / relabel the BPM control ("ext phase") so it
doesn't mislead.

## But BPM still leaks in via two real dependencies
1. Sub-step gate timing. armGate() uses curStepSec (= sixteenthSec = 15/bpm) to
   compute precise gate-open durations for fractional notes (triplets, 1/32) — a
   SECONDS quantity. Under phase, "seconds per step" isn't fixed; it depends on the
   instantaneous phase slope. Replace BPM here with a MEASURED phase velocity
   (dφ/dt per sample), analogous to how the clock already derives BPM from measured
   pulse period (derivedBpm = (60*4)/clockTimeAcc). So BPM isn't needed — but
   SOMETHING must still provide seconds-per-step, sourced from measured phase rate.
2. Internal free-run fallback. When NO phase cable is patched, the sequencer should
   still run (as the clock's internal-BPM fallback does today). An internal phase
   ramp has to advance at some rate, naturally expressed as BPM. → BPM remains the
   meaningful rate for the unpatched free-run case.

## Precise picture
  External phase patched, as a RATE control        → BPM irrelevant (ramp owns rate)
  External phase, for SUB-STEP GATE LENGTH          → replaced by measured phase velocity
  No phase patched (internal free-run)              → BPM relevant (internal ramp rate)
Framing: BPM is DEMOTED, not deleted. Under external phase it stops being a rate
control, but the engine still needs seconds-per-step (from measured phase velocity)
and BPM still rates the unpatched free-run ramp.

## Subtle refinement under a WARPING phase ramp
With a non-constant-slope ramp (swing, accel/decel — Phaseque's whole appeal),
seconds-per-step CHANGES within a cycle. Sub-step gate lengths would then naturally
stretch/compress with the warp — musically lovely (gates breathe with the phase).
But it means armGate's "lock in a seconds duration at note-start" model is slightly
wrong under warp: the gate duration should track the CURRENT phase velocity, not the
velocity frozen at trigger time. Small but real refinement — only matters once
warping ramps are in play, and only affects fractional/sub-step gate tails (whole
steps close on the positional boundary anyway, so they breathe correctly for free).
This is an Option-A-relevant detail too (forward warping phase already exposes it),
not just Option B.

---

# Mode E phase from a host (DAW automation) — direction & scrub

PHASE_PARAM (added feat 7d48661) is the Mode E phase source when CV1 is unpatched:
`params[PHASE_PARAM] * phaseInMax` fed to `phase.process(..., connected=true)`, identical
to the CV path. Purpose is host automation — a `configParam` is in the DAW's parameter
list whether or not a widget draws it, so e.g. Bitwig can drive Mode E from a Grid phase
modulator out with no CV cable.

## All three motions already work (the engine reads MOTION, not rising)
PhaseEngine derives everything from the shortest-path per-sample delta `d`, so nothing is
special-cased to "rising". The knob path inherits the full CV semantics:

- **Forward** — phase rising: `d>0`, edges forward, playhead forward.
- **Reverse** — phase FALLING (automate the value downward, or a descending ramp): `d<0`,
  `reverse=true`, edges fire backward, SequencerEngine steps the playhead backward via
  `setPhaseReverse`/`setReverseActive`. Reverse is NOT limited to the CV input.
- **Scrub/jump** — a move >1/16 in one sample: caught as a jump, position resyncs and
  `jumpSixteenths` reports the crossed steps to replay. Loop wraps and playhead drags land
  here.

So from Bitwig: a rising Grid-phase ramp = forward; automate it downward = reverse; jump/
loop it = scrub. No new code — it's the same `process()`.

## The one inherent caveat: the wrap seam (shortest-path ambiguity)
PhaseEngine uses SHORTEST-PATH delta, so `0.98 -> 0.02` reads as a small FORWARD step across
the bar line, not a huge reverse. This is CORRECT for a LOOPING phase ramp (Bitwig's phase
out wraps 1->0 each cycle and you want it to keep going forward) — and that is exactly the
intended use.

But it means a DELIBERATE backward scrub that CROSSES the 0/1 seam (`0.02 -> 0.98` meaning
"go back one step") is read as forward-by-one instead of back-by-fifteen. Shortest-path
cannot distinguish "wrapped forward" from "jumped backward across the seam" — same delta.
This is a fundamental property of ANY wrapped phase signal (tape, Phonogene, Rings' clock
in, etc.), not a bug. Backward moves that stay WITHIN the bar reverse correctly; only
seam-crossing backward moves alias to forward.

## Optional future mode: unbounded (non-wrapping) phase for unambiguous scrub
If unambiguous arbitrary scrub is ever wanted, feed an UNBOUNDED phase — a ramp that climbs
0,1,2,3,… bars instead of wrapping 0->1. With no seam, direction is never ambiguous. The
engine ALREADY tracks `prevContPhase` (continuous/unwrapped) internally, so this is a small
addition: a "continuous phase" option that skips `normPhase`'s wrap and the shortest-path
clamp, integrating the raw delta directly. NOT needed for Bitwig Grid phase out (a wrapping
ramp, works as-is); only for exotic full-range scrubbing. Deferred.

## Build check (the only real open question)
The knob path feeds phaseVolt/connected into `phase.process` identically to CV, so
`phase.reverse` SHOULD propagate through the same `setPhaseReverse`/`setReverseActive`.
Confirm on a scope in Rack: automate PHASE_PARAM downward and verify the sequence runs
backward (not just that it stops). Also confirm one Bitwig phase cycle == one Monsoon bar
(phaseInMax scaling) so the mapping is 1:1; if the host ramps per-beat, add a multiplier.
