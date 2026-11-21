# MIX display — RATE is right (90Hz), SCOPE is wrong (all voices). Fix: displayed voice only.

## What CC has (checked)
- refreshVisualCache is ALREADY gated to lightDivider (~90Hz, Monsoon.cpp:1147 block) = VIDEO rate.
  CORRECT rate. (lightDivider = sampleRate/90; controlDivider = sampleRate/1500 — don't use control here.)
- Slew is phrase-boundary (done). z0/z1 cache landed (B1). SIMD sumZ landed (A2).

## The remaining waste = SCOPE, not rate
refreshVisualCache (PatternEngine.cpp) calls recomputeEffectiveRhythm/Melody/Qmix() when mix/ctr changed.
Those recompute the ENTIRE pattern: ALL 16 steps x ALL fields x ALL voices (~832 blends x 3 streams).
But the DISPLAY only shows the DISPLAYED voice (the function already receives `const PatternInput& in` /
currentPatternInput -> it KNOWS which voice). So at 90Hz it is recomputing ~16x more than the display needs.
That ~16x over-compute is the "still wasting CPU" you see — the RATE is right, the SCOPE is all-voices.

## Fix — recompute ONLY the displayed voice on the display path
Add a voice-scoped variant, e.g. recomputeEffectiveRhythmForVoice(v) (+ Melody/Qmix), that recomputes just
the displayed voice's 16 steps (using the cached z0/z1 + the per-frac blend). refreshVisualCache calls the
FOR-VOICE variant for currentPatternInput's displayed voice — NOT the full all-voices recomputeEffective*.
- Display path (refreshVisualCache @ 90Hz): displayed voice only -> ~16x less than all-voices.
- Engine/roll path: keep the full recomputeEffective* (all voices) BUT only at the roll/phrase boundary
  (not per-sample) — the audio needs all voices' patterns; the display needs one. Different scopes for the
  two consumers.
Net: mix modulation display = 1 voice x 16 steps x 3 streams @ 90Hz (tiny), engine = all voices only at
roll. The all-voices-@-90Hz over-compute is gone.

## Also confirm the ENGINE (audio) path isn't separately over-computing on mix
When mix modulates, does the AUDIO path recompute all 16 steps every sample, or read the step being played
from the cache? The output consumes ONE step per step. If the audio still full-recomputes on mix change
(outside the dividers), gate it: apply mix at the STEP boundary for the READ step only (cached z0/z1 feed
it). (If audio already reads the cached pattern and only the display recompute was the cost, this is moot —
check.)

## Verify
Modulate mix @ 16 voices: CPU FLAT. Display shows the displayed voice scrubbing live @ ~90Hz (truthful).
Switching the displayed voice recomputes the newly-displayed one (cheap). Audio scrubs all voices correctly
(step-boundary). Build + bit-compare (integer scrub positions exact) + CI.
