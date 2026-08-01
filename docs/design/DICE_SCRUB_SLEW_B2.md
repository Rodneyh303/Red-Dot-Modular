# Slew under scrub: B2 truncated-FIR smoothing (K=6, geometric)

## Decision (Rodney)
Slew survives (option B: correlated walk = "variations on a theme"), but implemented as a FINITE-
WINDOW smoothing so it stays counter-addressable and reversible (low state). Chose B2: a truncated
FIR (moving average with geometric weights) over the raw draws, NOT a recursive chain (naive B has
infinite memory -> not addressable).

## The model
rawDrawAt(M)[i] = pure independent Philox draw at counter M (the current drawRhythm/MelodyPatternAt).
patternAt(M)[i] = SUM_{j=0..K} w_j * rawDrawAt(M-j)[i]      (K = 6, ties to the scrub window)
  weights geometric in slew:  w_j = (1-slew)^j , normalized so SUM w_j = 1.
  slew high  -> w concentrates on j=0 (raw(M) dominates)   -> sharp, near-independent draws.
  slew low   -> weight spreads across M..M-6                -> smooth, correlated (variations theme).
Then the SCRUB blends adjacent smoothed patterns as before:
  effective[i] = blend( patternAt(N-f)[i], patternAt(N-f-1)[i], frac )

## Why this satisfies the constraints
- Correlated walk (option B feel): consecutive patternAt share K-1 of K source draws -> scrubbing/
  rolling moves through variations on a theme, not unrelated rolls.
- Counter-addressable / reversible (low state): patternAt(M) reads raw draws M..M-K, all pure Philox.
  No chain, no stored walk state. Reverse uses the same formula (symmetric). Undo stays the counter
  scalar. This is the whole point of truncated-FIR vs recursive IIR.
- Slew keeps its musical role (bounded-walk tightness) as smoothing width.

## Cost
Each patternAt reads K+1=7 raw draws; scrub blends 2 patternAt sharing K overlap -> ~ (K+2)=8 raw
draws per slot (the two windows overlap in all but the extreme draw). ~12 Philox evals/slot/frame
worst case. Manageable; buffer fallback if it profiles badly. K bumpable if changes feel too abrupt
(Rodney: "if too abrupt we can increase k").

## First cut
Geometric weights (classic exponential slew feel). Linear ramp is the alt if geometric feels wrong
-- decide by ear. slew source = in.rhythmSlew / in.melodySlew (the existing slew param, repurposed
from roll-time B-shaping to window smoothing width).

## Reverse note
Under reverse, N decrements and the window M..M-K still reads the same addressable draws (Philox is
symmetric over signed counter). patternAt is direction-agnostic -- it's a function of absolute M. So
reverse "just works": the effective pattern at counter M is identical whether reached forward or
backward. This is the reversibility payoff.
