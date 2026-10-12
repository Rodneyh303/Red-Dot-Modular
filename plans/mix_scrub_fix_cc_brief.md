# CC BRIEF — mix/scrub: cache phrase-stable sumZ + cheap per-frac blend; engine at STEP boundary; video-rate displayed-voice bars

## Problem (measured)
Even with slew fixed at phrase boundary, modulating MIX still spikes CPU. Root: on every mix change,
recomputeEffectiveRhythm/Melody/Qmix redoes the FULL recompute — for ALL 16 steps x lanes x N voices it
runs blend(), and each blend() does TWO sumZ over K=64 (z0 = window@N-f, z1 = window@N-f-1) + PhiFast
(PatternEngine.cpp ~254-283). Modulating mix fires this continuously -> the spike.

## The fix — three compounding parts
### (1) CACHE the frac-independent, now PHRASE-STABLE window sumZ (z0, z1)
z0 = sumZ(window@N-f, r), z1 = sumZ(window@N-f-1, r). These depend on POSITION and slew r — NOT on frac.
AND slew r is now FIXED across a phrase (the slew-phrase fix landed). So within a phrase z0, z1 depend ONLY
on position -> compute ONCE per position per phrase and CACHE (z0[i][lane][v], z1[i][lane][v]).
- Reuse on every FRAC change (scrub within a position-pair) — the expensive sumZ is skipped.
- Invalidate/recompute z0,z1 only on: PHRASE BOUNDARY (slew may change -> new r) OR POSITION-CROSS (f
  changes -> new windows). Both rare vs per-tick scrub.
Big win: scrubbing mix no longer redoes the K=64 sumZ every tick (~64x off the scrub path). The slew-phrase
fix is what makes this cacheable (slew-fixed-in-phrase -> windows are position-only).

### (2) Cheap per-frac blend = the OU/AR(1) BRIDGE (fixes the r==0 / marginal bug too)
On a frac change recompute ONLY the blend from cached z0, z1 — no sumZ:
  X = a(r,f)*z0 + b(r,f)*z1 + sqrt(v(r,f))*xi   then PhiFast(X)
a,b,v closed-form in (r, frac); xi = ONE Philox draw (SEPARATE key domain from the base draws),
position+frac keyed -> reversible. See AB_MIX_COPULA_FIX.md (a/b/v forms, sqrt(v) range, why it preserves
the uniform marginal at every frac; current linear blend distorts, r==0 worst = a bug). Cheap (a few muls +
one Philox + PhiFast). a,b,v can be a small LUT in quantised (r, frac). (r<0 hocket: TBD, no OU embedding —
keep current behaviour there for now.)

### (3) RATE — engine at STEP boundary; display at VIDEO rate (Rodney)
- ENGINE applies the mix modulation at the STEP boundary — the output consumes one step per step, so apply
  the current frac to the step being READ at each step boundary (not per-sample, not full-pattern). With
  cached z0/z1 this is the cheap bridge blend for the read step.
- DISPLAY: show the mix probability-bar modulation on SANDS at VIDEO rate (~60Hz), DISPLAYED VOICE ONLY.
  TRUTHFUL and REQUIRED: unlike slew (consumed at phrase -> bars phrase-locked else they'd lie), MIX IS
  CONSUMED CONTINUOUSLY (scrub changes output live) -> live bars MATCH output -> it's real, show it. Video
  rate, displayed voice only (cheap; don't recompute all 15).
  PRINCIPLE: the display updates at the rate the parameter is CONSUMED — phrase for slew, video/continuous
  for mix. Same truthfulness rule, opposite answer, because consumption rates differ.

## Verify
- Modulate mix: CPU FLAT (cached z0/z1 -> scrub does only the cheap bridge blend; no K-sumZ per tick).
- Scrub output marginal-uniform at every frac (OU-bridge) incl. r==0 (fresh N(0,1) mid-interval) — r==0
  distortion GONE. Monte-Carlo: variance=1 + endpoint corrs r^f/r^(1-f) at several r,f.
- Sands bars track the scrub LIVE (video rate, displayed voice) and MATCH the audio output.
- Integer positions f=0 stay EXACT (= z0). Fixed mix: bit-identical for the cache step; the OU-bridge SHOULD
  change mid-scrub values (old blend was wrong) -> re-baseline/document (not a regression).
- Build + commit + PUSH each part; watch CI (3 platforms).

## Order (separate commits)
(1) cache z0/z1 (big perf win, KEEP current linear blend) -> measure CPU flat. THEN (2) swap blend to
OU-bridge (correctness). THEN (3) wire video-rate displayed-voice bar update.
