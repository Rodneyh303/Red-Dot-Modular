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

## Initial state & reverse: INFINITE LINE (no warmup, no origin floor) -- Rodney

DECISION: the counter is an infinite bidirectional line. There is NO special-casing of the first
few positions and NO clamp on reverse. Rationale (Rodney): the OLD model needed a warmup (initial
A=0, B=1, or first-draw A:=B:=draw) because it carried accumulated A/B state that had to be primed.
The scrub/B2 model has NO accumulated state, so there is nothing to prime -- it is "warm" at every
position including N=0, because each position is a self-contained pure function of the counter.
Adding an origin floor or first-draw branch would RE-ADD exactly the boundary state B2 was designed
to remove.

Consequences:
- patternAt(0) is as well-formed as patternAt(1000). The FIR ALWAYS reads the full window
  pos..pos-SCRUB_K, into NEGATIVE positions near the origin. Philox is a keyed bijection over the
  whole signed counter space, so negative-index draws ("phantom" predecessors below the seed) are
  valid and fully reproducible -- they are just more draws, not a warmup gap.
- Truncating the FIR near the origin is FORBIDDEN: it would break the pure-function-of-position
  property (patternAt(2) computed at seed-time would differ from patternAt(2) reached by reversing
  from N=8), destroying reversibility symmetry. Always read full K.
- REVERSE is UNBOUNDED. N may go negative; reverse past the seed origin is fine and reproducible.
  No clamp on advanceDraw.

Seed semantics (keep current behaviour): seedRhythm/MelodyPhilox re-keys AND zeros the counter
(rhythmDrawCtr=0). So "seed X" reproducibly means "the window at position 0 under key X" -- the
reproducible-seed mental model is preserved WITHOUT any first-draw special case. (If seeding left N
where it was, "seed X" would give different patterns depending on N; zeroing N fixes that.)

"Fresh seed should equal pure draw 0" expectation: satisfied via SLEW, not special-casing. At
slew=1 the FIR collapses to raw(N), so a fresh seed at slew=1 gives exactly draw 0. At low slew the
pattern is draw 0 blended with phantom predecessors -- which is precisely "variations on a theme,"
the intent at low slew. So the expectation is a slew setting, not a boundary case.

Net: LESS code than the old model (no A=0/B=1 priming, no first-draw branch, no reverse clamp) --
a sign the infinite-line choice is consistent with the model's core property.
