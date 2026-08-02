# Sands REST/ACCENT flicker on CA scatter -- diagnosis

## Symptom
Screen flicker on Sands MONO and MACRO, REST + ACCENT lanes only, when a CA scatter transform is
run. Never on Sands EAST, never on MEL/OCT. Crept in recently.

## Root cause: audio-thread in-place rewrite of buffers the UI reads (no double-buffer)
MonsoonSandsManager (AUDIO thread, every process block) does, whenever the CA pins are non-identity:
    if (!identity) { pe.forceRecomputeSlewed(); pe.remapSlewedByPins(); }
Since a scatter leaves the pins non-identity PERSISTENTLY, this runs EVERY BLOCK after a scatter --
continuously tearing down and rewriting slewedRhythm[]/slewedAccent[]/... IN PLACE
(PatternEngine::remapSlewedByPins, ~line 160-166).

Mono + Macro display read those SAME buffers directly (macroOwnProbability reads pe.slewedRhythm /
slewedAccent, pre-spread). UI thread reads mid-rewrite -> catches partially-updated buffers -> flicker.

EAST does NOT flicker because it reads a DIFFERENT path: resolver.laneProbabilityAtStep ->
polyLaneProbabilityAtStep -> polyRandomSrc (the CA remap applied at read time), not the in-place
slewed buffers.

Why REST + ACCENT only (not MEL/OCT): the identity guard keys on caRhythmSrc/caMelodySrc. ACCENT
follows the RHYTHM strand's CA source, so REST(rhythm) + ACCENT are the strands rewritten whenever
the rhythm-side pins move; they're also written FIRST in the remap write-back loop. MEL/OCT are on the
melody-side / less churned.

Introduced by: c9beab9 "remap PRE-SPREAD (slewed buffers) -- the agreed design, finally
[NEEDS RACK BUILD-VERIFY]" -- which moved the remap INTO the slewed buffers (in place).

## Fix options (all avoid the tear; pick per Rack verification)
1. DOUBLE-BUFFER the slewed buffers: remap writes into a back buffer, then atomically publishes a
   pointer/index the UI reads. No tear. Most robust; a bit more memory + an index swap.
2. GUARD by change, not every-block: only run forceRecomputeSlewed()+remapSlewedByPins() when the CA
   pins ACTUALLY CHANGED (compare caRhythmSrc/caMelodySrc to last applied), not every block while
   non-identity. Cuts the continuous churn to a single remap per pin change -> the UI only races a
   one-frame window on the scatter itself, not continuously. Cheapest; may leave a 1-frame blip.
3. Combine 1+2: guard on change AND double-buffer -> no churn and no tear.

Recommendation: (2) first (cheap, likely kills the SUSTAINED flicker which is the complaint), then (1)
if any residual single-frame blip remains. (2) is also just correct -- recomputing every block when
nothing changed is wasted work.

## NOTE
This is independent of the CA counter/RNG work and the CA-reverse scope question. It's a pre-existing
Sands remap regression exposed by scatter. Fix it on its own.
