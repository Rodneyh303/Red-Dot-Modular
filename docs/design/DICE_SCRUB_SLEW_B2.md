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

## CORRECTION: reversibility holds ONLY at constant slew (Rodney)

The "pure function of position -> reversible" framing above is IMPRECISE. patternAt(M, slew) is a
pure function of TWO args (M AND slew). The COUNTER is exactly reversible; the AUDIBLE PATTERN is
reversible iff slew is held constant (or its value is captured with the counter).

Why: if slew changes between two visits to position M, patternAt(M, slew_a) != patternAt(M, slew_b).
Scrub back to M under a different slew gives a different pattern. So the counter alone does NOT
reconstruct the pattern -- you need (counter, slew) together.

Consequences (corrections to earlier claims):
- UNDO is NOT purely the counter scalar. Dice undo state = (counter, slew) pair (still ~12 bytes,
  still no array). Restoring only the counter under a changed slew won't reproduce the pre-roll
  pattern. Capture slew alongside. (Amends UNDO_PLAN / DICE_SCRUB_MODEL "undo = counter scalar".)
- The seed reproducibility guarantee is (seed, position, slew), NOT (seed, position). Seed X at
  slew=0.2 and slew=0.8 give different position-0 patterns. Expected (slew is a live control), but
  state the full tuple.
- This is INHERENT and acceptable: slew is a live/CV-modulatable performance axis, and a live-
  modulated control is by nature not part of the reproducible-from-counter state -- that IS the
  tradeoff for playability. Still strictly better than the OLD model, where slew was consumed
  DESTRUCTIVELY at roll time (not recoverable at all); here it is recoverable if its value is known.

Precise statement: COUNTER exactly reversible; PATTERN reversible iff slew constant (or slew captured
with the counter). Scrub with slew parked = fully reversible. Scrub while modulating slew = not
bit-reversible, BY DESIGN (slew is a live axis).

OPEN (lock interaction): should slew LATCH under lock? If slew is frozen under lock, locked playback
is FULLY reversible (clean guarantee). If slew stays LIVE under lock, locked playback is not bit-
reversible while slew moves. Given slew now shapes pattern STRUCTURE (via the FIR window), there is a
real argument for LATCH under lock -- same reasoning as the Intertropical-transpose LATCH call.
Decide when wiring lock + dice-scrub together.

## Reversibility DEGRADES GRACEFULLY (Rodney) -- it's a gradient, not a cliff

"Reversible iff constant slew" undersells it -- it sounds binary/fragile when it's actually a smooth
gradient. patternAt(M, slew) is CONTINUOUS in slew, so approximate slew -> approximate reverse:

  - slew EXACTLY constant      -> bit-exact reverse.
  - slew APPROXIMATELY held     -> approximately-exact reverse; pattern error scales continuously
                                   with slew drift (no cliff, no discontinuity).
  - slew freely modulated       -> not reversible, by design (live axis).

The middle tier is what matters in practice (slew is rarely held to the bit when playing) and it's
what makes the feature feel trustworthy rather than brittle: nudge slew a hair while scrubbing back
and you land NEAR where you were, not somewhere random.

WHY the continuity holds (not automatic -- earned by the design):
- geometric weights w_j=(1-slew)^j are a continuous function of slew;
- the pattern is a LINEAR blend of FIXED raw draws (raw draws at each position don't move with slew;
  only their weights shift). So slew -> pattern is smooth.
Contrast the avoided failure modes: a discrete slew MODE switch, or the recursive/chained walk
(naive B), would flip the pattern discontinuously on small slew changes -- "approximately held"
would then give nothing like the original. The truncated-FIR's linearity is what buys graceful
degradation. Another vindication of B2 over naive-B / mode-based slew.

## RESOLVED (Rodney, Rack-confirmed): K=6 final, slew stays LIVE (no history ring)
- K=6 confirmed sufficient in Rack once slew was actually connected. No bump to 12. (If ever wanted,
  the idle-recompute guard already makes the bump nearly free.)
- Slew-history ring buffer (to make reverse bit-exact through slew changes): NOT worth the
  complexity. Slew stays a pure LIVE control. Rationale: the graceful-degradation property
  (approximately-held slew -> approximately-exact reverse, continuous) already covers practical
  reverse; the ring would add per-position storage + replay-vs-last-touch semantics + undo
  interaction to buy bit-exactness the live-control framing doesn't need. Keep the model clean.
  Reverse remains: exact at constant slew, approximate as slew drifts, by design.
