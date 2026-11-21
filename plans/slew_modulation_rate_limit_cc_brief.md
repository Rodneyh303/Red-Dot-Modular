# CC BRIEF — rate-limit modulated slew/probability recompute (MUST-FIX perf)

## The problem (measured)
Modulating the 3 slews is catastrophically expensive:
- 8 voices, 3 slews modulated: ~40% CPU.  16 voices: ~80%+ CPU. (Linear in voices -> per-voice recompute.)
Steady state (fixed slew) is ~1% — fine. The cost is CONTINUOUS MODULATION.

## Root cause
recomputeEffective{Rhythm,Melody,Qmix} (PatternEngine.cpp:244/328/386) re-derive the slew MA window
(applyZ over K=64, per voice, per lane). They're gated by rhythmSlewApplied (recompute only when r
CHANGES) — so FIXED slew is cached (cheap). But MODULATING slew changes r EVERY FRAME -> full recompute
EVERY FRAME, per voice. That's the 40%/80%.

## This is the SAME fix the COPULA crisis needed
Same class: expensive control-domain computation running at audio/frame rate. The copula pass fixed the
STEADY-STATE/spread cost (why steady state is ~1% now) but left the MODULATED recompute at full rate —
because it's gated off for fixed slew, so the earlier pass (fixed-slew cases) never exercised it. The
differential method (sweep slew -> meter spikes) surfaced it. The proven fix: DECIMATE to natural rate.

## The fix — run each computation at its NATURAL rate (three rates)
1. **Probability / slew recompute -> CONTROL RATE** (not audio/frame rate). Gate recomputeEffective* with
   a rack::dsp::ClockDivider (e.g. every 16-64 samples, ~700Hz-2.7kHz). Slew is a slow CHARACTER control;
   recomputing its window at control rate is perceptually identical to audio rate. ~44x reduction on the
   dominant cost -> should take 80% -> low single digits. THE main fix.
2. **Visuals -> VIDEO RATE (~60Hz)** (Sands bars, light ring, any display recompute). If any visual
   recomputes per-sample/per-frame-at-audio-rate, drop to 60Hz. Additional saving + correct (nobody sees
   faster). Separate ClockDivider.
3. Audio-rate output stays audio-rate (the final CV per sample; interpolate/hold between control-rate
   slew updates so output stays smooth).

## Verify (determinism — critical)
- FIXED slew: BIT-IDENTICAL before/after (no recompute path changes -> cached, unchanged).
- MODULATED slew: the control-rate recompute must still be DETERMINISTIC + preserve the reversible
  round-trip at the recompute points ("r enters only at readout", position-indexed — so WHEN you recompute
  shouldn't change WHAT, but CONFIRM the 1e-15 round-trip / reversibility test still holds with control-rate
  recompute). Bit-compare + the reversibility test on all 3 CI platforms.
- MEASURE: modulate 3 slews at 16 voices, watch the VCV CPU meter -> confirm 80% -> low single digits.

## Method
ClockDivider on recomputeEffective* (control rate) first — measure. Then visuals->60Hz. One at a time,
build + bit-compare + reversibility test + commit + PUSH. Watch CI (3 platforms). Report the CPU drop.

## Do NOT kill slew
Slew is a core expressive feature (correlation character). Fixed slew is already cheap; the fix makes
MODULATED slew cheap too (control-rate recompute). Rate-limiting preserves the feature fully.
