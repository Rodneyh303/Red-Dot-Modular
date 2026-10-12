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
