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
