# How the countdown timers react to BPM modulation

Two independent timer mechanisms close gates, and they react DIFFERENTLY to a
changing tempo. Summary up front, then the trace, then the one real artifact.

## The two timers
1. STEP timer — GateState.holdRemain, decremented by 1 per 1/16 edge in tick().
   Governs WHOLE-step note lengths (1/16,1/8,1/4,1/2,1/1 = 1,2,4,8,16 steps).
   Closes the gate on a step EDGE.
2. SECONDS timer — GateState.gateSecRemain, decremented by sampleTime each sample
   in process(). Governs only SUB-STEP / fractional tails (1/32 = 0.5 steps,
   triplets 1.333/2.667). Armed by armGate() = durSteps * curStepSec.

armGate deliberately arms the seconds timer ONLY for fractional tails; whole steps
are left to the edge mechanism (a prior fix — the seconds timer raced the edge and
clipped whole notes early).

## STEP timer under BPM modulation → ROBUST (self-correcting)
holdRemain counts EDGES, not time. A note of "4 steps" closes after 4 edges no
matter how fast/slow those edges arrive. So:
- Internal clock BPM modulated: the accumulator compares timeAcc >= sixteenthSec
  every sample against the LIVE sixteenthSec (refreshed continuously from bpm). Speed
  up mid-note → edges arrive faster → the 4-step note simply ends sooner in
  wall-clock time, but still lasts exactly 4 steps musically. Correct and seamless.
- External clock BPM modulated (i.e. the incoming clock speeds/slows): edges are
  the incoming pulses themselves, so again holdRemain just counts them. Tempo-
  correct by construction.
Verdict: whole-step notes track tempo modulation perfectly, both sources. This is
the majority of notes. No artifact.

## SECONDS timer under BPM modulation → has a FREEZE artifact (sub-step only)
gateSecRemain is armed ONCE at note-start: durSteps * curStepSec, where curStepSec
is sixteenthSec AT THAT MOMENT. It then counts down real seconds, blind to any
later BPM change. So a fractional tail's gate length is FROZEN at the tempo that
was current when the note triggered.
- If BPM rises during a sub-step note: the gate stays open its original
  (now-too-long) seconds → the fractional note runs slightly LONG relative to the
  new tempo (the following edge may even arrive before the seconds timer expires).
- If BPM falls: the gate closes its original (now-too-short) seconds → the
  fractional note ends slightly EARLY relative to the new tempo.
Magnitude: only the FRACTIONAL PART is seconds-governed and only for sub-step
note values, and only matters if BPM changes meaningfully WITHIN one step's
duration. For slow tempo automation it's negligible; for audio-rate / fast BPM
modulation on 1/32 or triplet notes it's audible as slight timing slop on those
notes specifically.

## Why curStepSec being refreshed each tick() helps (but doesn't fully fix it)
tick() refreshes curStepSec from the live sixteenthSec every step edge, so the
NEXT note armed will use the updated tempo. The freeze only affects the note
CURRENTLY sounding across the tempo change — once it ends, all subsequent notes
are correct. So the artifact is transient (one note), not cumulative, and never
drifts the sequence.

## External-clock subtlety
Under external clock, sixteenthSec is updated only when a 1/16 pulse is MEASURED
(derivedBpm from clockTimeAcc). So curStepSec lags by up to one measured interval
after an external tempo change — a sub-step note armed in that lag window uses the
slightly-stale tempo. Same transient-freeze character, bounded to one step.

## Net assessment
- Whole-step notes (the bulk): tempo-modulation-correct, both clock sources. No
  action needed.
- Sub-step / fractional notes: one-note transient timing slop when BPM changes
  mid-note, because gateSecRemain freezes the tempo at trigger. Bounded, non-
  cumulative, no drift.

## If you wanted to remove the sub-step artifact (optional)
Make the seconds timer track tempo instead of freezing: store the note's remaining
duration in STEPS (a fraction), and each process() decrement by
sampleTime / curStepSec(live) rather than counting fixed seconds. i.e. integrate
"steps elapsed" against the live step-duration, closing when remaining-steps hits 0.
That makes even fractional tails breathe with tempo (and, notably, with a WARPING
PHASE ramp — this is the SAME refinement flagged in the phase audit's BPM section:
gate length should track current rate, not rate-at-trigger). One small change to
armGate (store frac-steps) + process() (decrement by sampleTime/curStepSec), with
curStepSec kept live. Low risk, and it unifies the clock-modulation and phase-warp
cases under one fix.

## Connection to phase mode
This is the clock-domain version of the warping-ramp refinement in
PHASE_ENGINE_AUDIT.md. Same root cause (duration frozen in seconds at trigger),
same fix (track live rate). Worth doing the steps-vs-seconds refactor ONCE and
having both clock-BPM-modulation and phase-warp benefit.
