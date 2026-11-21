# AB mix (scrub blend) — variance-preserving copula fix + the rho formula

## What A and B are (checked, not assumed)
The scrub blend (rhythmMix/melodyMix/qmixMix, "A<->B morph") interpolates between TWO ADJACENT SLEW
WINDOWS: z_A = sumZ(window@N-f, r), z_B = sumZ(window@N-f-1, r). So A and B are NOT independent raw draws
— they're overlapping K-tap slew-windowed values, BOTH at the current slew r (A and B ARE connected to
slew). Their windows overlap K-1 of K taps -> A and B are CORRELATED.

## The rho formula — already in the code, and rho = r
The correlation between the two adjacent windows is the lag-1 output autocorrelation:
  rho = MovingAverageCopula::lagCorr(r, 1) = Sum_j w_j * w_{j+1}   (already implemented + tested,
  MovingAverageCopula.hpp:84-93).
The slew weights are GEOMETRIC + L2-normalised (w_j proportional to r^j, Sum w_j^2 = 1,
MovingAverageCopula.hpp:38-45). For geometric L2-normalised weights this telescopes:
  Sum w_j w_{j+1} = r * Sum w_j^2 = r.
So **rho = r** (exact in the K->inf limit; ~exact for finite K since r^K is tiny at R_MAX=0.97).
=> The slew parameter IS the lag-1 temporal correlation (confirms Rodney's memory: r=0 no temporal
dependence, r->1 near-perfect positive, r<0 negative / alternating-sign weights = hocket). The geometric
design was CHOSEN so r is literally the correlation.

## The fix — variance-preserving blend ("AB mix as a copula")
The current blend is un-normalised: (1-frac)*z_A + frac*z_B. A convex combo of two N(0,1) with
correlation rho has variance (1-frac)^2 + frac^2 + 2*frac*(1-frac)*rho, which is < 1 for frac in (0,1)
when rho < 1 -> marginal pulled to centre -> NON-uniform. Fix: normalise to unit variance:

  z = [ (1-frac)*z_A + frac*z_B ] / sqrt( (1-frac)^2 + frac^2 + 2*frac*(1-frac)*rho ),   rho = r

This preserves unit variance -> N(0,1) -> UNIFORM after PhiFast at EVERY frac. Behaviour:
- r == 0 (rho=0): denom = sqrt((1-frac)^2 + frac^2) -> the SLERP-equivalent normalisation -> FIXES THE
  r==0 BUG (currently the raw un-normalised linear blend of two independent draws -> max distortion).
- high r (rho->1): denom -> sqrt((1-frac+frac)^2) = 1 -> no correction (z_A ~= z_B already unit-variance).
  Matches "high slew -> minimal distortion".
Cheap: rho = r (or one lagCorr call per r), plus one sqrt + divide per blend. Deterministic/reversible
(pure fn of z_A, z_B, frac, r).

## Why this is "AB mix as a copula" + per-step consequence
This IS the copula-correct combine: it couples A and B while PRESERVING their marginals (Sklar), vs the
broken convex average that reshaped the marginal. Unifies with the principle: the copula is the engine's
one COMBINING primitive (cross-voice correlation AND A<->B morph) — marginal-preserving, per-step-safe.
The MA (slew r, depth K) is the RESHAPING operation (reshapes the marginal by averaging) -> phrase-locked.
Taxonomy: couple/select (copula correlation, discrete position select, variance-preserved AB morph) =
marginal-preserving = per-step-OK; average (slew, K, un-normalised blend) = marginal-reshaping =
phrase-locked / or must be variance-corrected.

---

## RESOLVED continuous scrub — OU/AR(1) BRIDGE with Philox noise (Rodney; research-backed)
Deep-dived (extended research). The ONLY construction meeting ALL five requirements (exact endpoints +
uniform marginal at EVERY frac + model-consistent r^frac dependence + deterministic/reversible + cheap)
is the OU/AR(1) bridge with SEEDED noise:

  X_f = a(r,f)·z_A + b(r,f)·z_B + sqrt(v(r,f))·xi,   then PhiFast(X_f) -> uniform

with closed forms (theta = -ln r):
  a(r,f) = (r^f - r^(2-f)) / (1 - r^2)          = sinh(theta(1-f))/sinh(theta)
  b(r,f) = (r^(1-f) - r^(1+f)) / (1 - r^2)       = sinh(theta·f)/sinh(theta)
  v(r,f) = 1 - (r^(2f) + r^(2-2f) - 2r^2)/(1-r^2) = 2·sinh(theta·f)·sinh(theta(1-f))/sinh(theta)
Endpoints: v(0)=v(1)=0 -> X=z_A / z_B exactly. Unit variance at every f -> uniform after Phi. Exact r^f /
r^(1-f) dependence on the endpoints -> model-consistent.

### Why this fits us natively (and is EASY — Rodney)
- **xi = ONE random draw from ANOTHER Philox** (separate key domain from the base draws). That's it — we
  already have Philox; the "noise" is just another position-keyed draw. So REVERSIBILITY holds OVER THE
  INTERPOLATION: scrub back to the same (position, f) -> same xi (Philox is a pure fn of its counter) ->
  same interpolated value. The interpolation inherits the engine's addressability/reversibility for free.
- It's a FORMULA, not a model — far less work than a "full OU copula model". Closed-form a/b/v (a few
  sinh, or a small LUT in quantised f) + one Philox draw + PhiFast. Reuses sumZ for z_A, z_B.

### Details to get right
- **SEPARATE Philox key domain for xi** vs the base z draws — xi MUST be independent of (z_A, z_B) or the
  marginal/endpoint-correlation breaks. One-line discipline (distinct key/counter range).
- **Noise structure for a SWEEP (we scrub continuously):** use ONE xi per INTERVAL (rank-one bridge,
  position-keyed) carried across the frac sweep -> CONTINUOUS in f (smooth scrub), exact pointwise,
  cheapest (one normal per interval). (Per-(n,f) independent keys give white jitter across f -> wrong for
  sweeping. K-term sine series is the higher-fidelity option if the rank-one path-correlation isn't
  smooth enough.)
- **Quantise f deterministically** (e.g. 32-bit) before keying xi, so the same frac always reproduces.

### MA vs AR note
Our weights are a FINITE GEOMETRIC MA (w_j ~ r^j), which is statistically AR(1)/OU (geometric MA == AR(1);
lag-m corr = r^m). So the OU bridge is the right MODEL for our correlation (rho=r), even though we
implement as MA. CAVEAT: z_A, z_B are two OVERLAPPING finite-MA windows (share K-1 draws), so they have
extra shared-draw structure beyond "corr=r"; the OU bridge is EXACT in correlation but APPROXIMATES the
finite-overlapping-MA joint law. Fine for scrub (want smooth/uniform/plausible morph, not exact finite-MA
joint).

### Prior art / confidence
Variance-normalised blend (the earlier fix) == Fink/Holters/Zolzer DAFx-16 "power-complementary" crossfade;
it and the rescaled OU mean m/sqrt(1-v) are the best DETERMINISTIC (no-noise) options but OVER-correlate
with endpoints (not model-consistent) — only the seeded-noise bridge is fully model-consistent. Seeded
Brownian-bridge sampling is established (Virtual Brownian Tree, Brownian Interval); the OU time-change
wrapper is a derivation (verify by Monte Carlo: variance=1 and endpoint corrs r^f/r^(1-f) at several r,f).
Formulas need 0 < r < 1; r<0 (anti-correlation/hocket) needs separate handling (no OU embedding) — TBD.

### Noise magnitude — range of sqrt(v) across the interval
sqrt(v) = 0 at BOTH endpoints (f=0,1 -> pinned exactly to z_A/z_B), rising SYMMETRICALLY to a MAX at the
midpoint f=1/2:
  sqrt(v(1/2)) = sqrt((1 - r)/(1 + r))
So sqrt(v) ranges [0 .. sqrt((1-r)/(1+r))]. Since rho = r = the slew value:

  slew r | max sqrt(v) at midpoint | meaning
  0.0    | 1.00   | full unit noise — endpoints independent, mid-interval is a FRESH N(0,1)
  0.5    | 0.577  | substantial injection
  0.9    | 0.229  | small — points nearly identical
  0.97   | 0.123  | very small (R_MAX)
  ->1.0  | ->0    | none — endpoints perfectly correlated, middle deterministic

NOISE AMPLITUDE TRACKS (1-r): lots of fresh noise at LOW slew (independent endpoints -> the scrub reveals
NEW material between positions), vanishing at HIGH slew (near-identical endpoints -> smooth morph, almost
pure blend). Profile vs f is a flat-topped bump (0 at ends, ~flat across the middle): e.g. r=0.5 gives
sqrt(v)~0.5 at f=0.25/0.75, 0.577 at f=0.5.
Implication: noise is BOUNDED (<=1, at r=0) and cheap (one sqrt of the closed form, or a 2D LUT in
quantised (r,f)). Confirms the bridge handles r==0 correctly: mid-interval = independent N(0,1) (the RIGHT
value, vs the old linear blend's variance-reduced wrong one). The bridge injects EXACTLY the variance the
conditional mean is missing, scaled by endpoint independence.

---

## OU-bridge noise xi — counter/key design (reversible by construction) (Rodney)
### One xi per consecutive A/B PAIR (= one per main-draw-counter change)
xi is NOT keyed by frac. One xi per adjacent-position PAIR (N-f, N-f-1), HELD CONSTANT across the frac
sweep within that pair. Only a(r,f), b(r,f), sqrt(v(r,f)) vary with frac; xi is fixed. This is the
"rank-one bridge" — the right noise structure for a SWEEP: smooth in f (only a/b/v move), exact pointwise.
(The within-interval over-correlation caveat is IRRELEVANT for scrub: we read ONE point at a time, never a
dense sub-path, so the joint law of multiple mid-interval points doesn't matter.)

### Keying — separate stream key + the pair's position index
- **Separate KEY per stream** (independence): rhythm/melody/qmix bridge-xi get their own keys, distinct
  from the base-draw keys (S/S+1/S+2) AND from each other — extend the existing cross-stream decorrelation
  (e.g. S+4/S+5/S+6). xi MUST be independent of the base draws (z_A,z_B) and across streams, or the
  marginal / endpoint-correlation / stream-independence breaks.
- **COUNTER = the pair's (lower) position index** — a consistent per-pair identifier. NO frac in the key
  (so no frac-quantisation needed). Each distinct pair -> distinct index -> distinct xi.

### Reversible BY CONSTRUCTION — decrement the counter, get the same xi (Rodney)
xi is a PURE FUNCTION of its counter (Philox), and the counter is the pair's position index. So a BACKWARD
dice roll / REVERSE scrub = DECREMENT the counter -> regenerates the IDENTICAL xi (Philox is a deterministic
bijection on the counter, signed-space). No stored state, no bookkeeping: forward to index k and backward
to index k both give xi_k = Philox(key, k).
So the WHOLE interpolation is bidirectionally reversible: BOTH ingredients are counter-addressed + bijective
— the base draws (z_A,z_B) by their position counters, and the bridge xi by the pair's position counter.
Backward = decrement the counters = exact regeneration of everything. This is the SAME signed-bijection
property the bidirectional buffer uses for the main draws; xi inherits it for FREE by using the same
position-counter cadence (one xi per draw-counter change). (If xi were keyed by a float frac, or drawn from
running state, backward would NOT reproduce it — keying by the position counter is what makes it reversible.)

### Continuity across pair boundaries (despite distinct per-pair xi)
At frac=1 of pair (N-f, N-f-1) the value = z_{N-f-1} exactly (sqrt(v)=0 -> xi term vanishes); at frac=0 of
the next pair (N-f-1, N-f-2) it = z_{N-f-1} exactly too. SAME value -> CONTINUOUS across the boundary, even
though the two pairs have DIFFERENT xi — because the noise is zero at the integer endpoints where pairs
meet. So per-pair xi gives a continuous scrub.
