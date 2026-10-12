# WHY MIX STILL SPIKES — the cache landed but it's not enough. Next: part 3 (decomposition).

## Diagnosis (checked the code)
B1 (z0/z1 cache) DID land and is correct: rhythmZCache_.hit(N,f,r), MAX_FIELDS=64 (= 4 + 4*15) covers
16 steps x (4 + 15 voices x 3) combos, so sumZ IS cached out on a hit. Good — the sumZ cost is gone.

BUT mix still spikes, because the cache only cut the PER-BLEND cost (no more sumZ). It did NOT cut the
ITERATION, and there's NO RATE LIMIT:
- latchMix: `if (rhythmMixLatched != rhythmMixApplied) recomputeEffectiveRhythm()` — fires EVERY sample
  while the mix knob/CV sweeps (mix changes every sample).
- recomputeEffectiveRhythm loops ALL 16 steps x (4 + numPoly*3) = up to 832 blend() calls, each doing a
  blend + PhiFast. So sweeping mix = ~832 PhiFast x 44.1kHz per stream x 3 streams ~ ~100M PhiFast/sec.
  THAT is the remaining spike — not sumZ (cached), but 832 blends EVERY SAMPLE.

## The missing piece = PART 3 of mix_scrub_fix_cc_brief.md (engine per-step + display video-rate)
The cache was necessary, not sufficient. Now do the DECOMPOSITION:

### Engine: apply mix at the STEP boundary, for the READ step only
The audio output consumes ONE step per step. So do NOT recompute all 16 steps x all voices on every
mix-change. Instead, at each STEP boundary compute only the step being READ (its voices) at the current
frac. -> ~16x fewer blends AND at step rate (~10-50Hz) not sample rate (44.1kHz). The cached z0/z1 feed it.
(Each step's blend is independent — pure fn of that step's cached z0/z1 + a/b/v(frac) [+ xi for the
OU-bridge later].)

### Display (Sands bars): video rate, DISPLAYED VOICE only
Recompute only the displayed voice's step bars at ~60Hz (a ClockDivider or the module draw cadence) — NOT
all 15 voices, NOT at audio rate. Truthful (mix is consumed continuously so live bars match output) +
cheap. (See the display-truthfulness note: mix bars video-rate — unlike slew bars which are phrase-locked.)

### Net
mix modulation = read-step blend at STEP rate (audio) + displayed-voice bars at 60Hz (visual). NOT
full-pattern-every-sample. The 832-blends-every-sample iteration is what spikes; this removes it.

## Verify
Modulate 3 mixes @ 16 voices -> CPU FLAT (no full-pattern recompute per sample). Audio still scrubs
correctly (read step reflects current frac at each step). Sands bars live + match output (video-rate
displayed voice). Then the OU-bridge (part 2) swaps the blend formula (correctness) on top.
