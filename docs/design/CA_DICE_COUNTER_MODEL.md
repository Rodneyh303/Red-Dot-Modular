# Change Alley scatter counters -> true dice Philox model (8 streams, addressable position)

## What was wrong before
- Collapse 8->2 (commit bfdb586, REVERTED a521734): wrong. The 8 counters are real -- the panel
  exposes separate jacks for Intra/Inter x rhythm/melody x domain/codomain, each an independent
  scatter control. Forward/back is the SIGN of scatterDelta (+1/-1), not a separate counter.
  8 = SIDES(Intra/Inter=2) * TYPES(rhythm/melody=2) * (domain/codomain=2).

## The correct reform (Rodney): keep 8, make them DICE-STYLE
Not fewer counters -- BETTER counters. Adopt the main-dice Philox model for each of the 8:
- Counter type = signed int64 (same as dice rhythmDrawCtr/melodyDrawCtr). Negative allowed.
- The counter IS the addressable POSITION in that stream's Philox sequence -- NOT a seed/key.
  Draw via rng.at(position), so at(N) and at(N-1) are stable neighbouring draws.
- Back-jack does counter-- -> at(N-1) returns EXACTLY the previous draw. NO reseeding. Forward
  counter++ -> at(N+1). Pure addressable rewind, exactly like dice reverse.
- Each of the 8 is domain-separated (its own fixed key) so the 8 streams don't correlate and each
  rewinds independently. The FIXED key encodes which of the 8 (ci); the COUNTER is pure position.

### Before (seed-keying -- what we're replacing)
applyCorrelation extracted seed = counter & 0xFFFFFFFF and each transform built
correlationRng(seed) then drew rng.at(voice). => counter was the KEY (a new random sequence per
count), voice was the position. Decrementing the counter jumped to a DIFFERENT permutation, not a
smooth addressable step back.

### After (addressable position -- dice model)
The correlation RNG is keyed by a FIXED per-stream domain (ci in 0..7, mixed with the correlation
nonce). The counter is the position page: voice v of counter N draws rng.at(N*16 + v) (a stable
16-wide page per counter). counter-- returns to the prior page exactly. No reseed.

## Deferred (separate concern -- Rodney): pre-scatter PIN / fan-in undo
scatter() has fan-in (multiple voices can source the same slot) => NO inverse TRANSFORM. Today undo
is a pre-scatter pin-state snapshot (store snapshot). The addressable-position counter makes the
DRAW reversible, but the fan-in means re-deriving the board by stepping the counter back is not the
same as restoring the pins. Reconcile "counter rewind" vs "pin snapshot" undo as its own step AFTER
this counter-model change lands. Do not touch the pin-snapshot undo in this change.
