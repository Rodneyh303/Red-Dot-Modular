# CC PROMPT  Phase distribution: effective-curve plot + acceptance-test mapping

Authority: docs/design/PHASE_DISTRIBUTION_SPEC.md (sections 8, 10, 11). Build on feat/phase-duals-gaps.
Already LANDED and must NOT change: gap stage (1/(1-p) rescale, kGapEps = 1e-6), atom-after-gap composition,
dual reading by warp sign, warp mix in quantile space, gap panel controls (slots 6/7). Do not re-derive these.

## TASK 1  Map the 8 acceptance tests (spec section 8)
Read test/test_phase_distribution.cpp. For each of the 8 spec tests, record: covered by which check (file:line),
or MISSING. Add only the MISSING ones. Do not alter existing checks or the landed convention.
The 8 tests (section 8):
1. G continuous, non-decreasing, G(1)=1 over a grid of (alpha, beta, p, a, b).
2. Width zero reproduces the untouched distribution bit-identically.
3. a=0 and b=1 give valid quantiles; clamp engages only when p ~ 1.
4. Round trip Q(G(x)) = x outside the gap, to float tolerance.
5. Jump in Q_G is at v = F_a/(1-p), landing a then b.
6. Normalised density outside the gap unchanged (uniform rescale).
7. Atom inside a gap sits at ell; atom plateau covering the jump point wins.
8. Dual reading: an atom on the original becomes a gap on the dual (numerical check, several parameter sets).
Output: a table, test -> check -> status. Gate: all 8 covered and passing.

## TASK 2  Effective-curve plot (spec section 10), display only
Replace the distribution plot's bare-target drawing with the EFFECTIVE warped curve at the current signed depth.
Transform (k = |w|, T = Q for w >= 0, T = G for w < 0):
    W(u) = (1-k)*u + k*T(u)
Sweep u over [0,1] ONCE, N points (a few hundred). Per point:
- PDF fill:  point (W(u), f_W(u)) with f_W(u) = 1 / [ (1-k) + k / f(T(u)) ]   (closed form)
- CDF trace: point (W(u), u)                                              (PARAMETRIC: no bisection, no integration)
Limits to check: k = 0 gives the diagonal CDF and f_W = 1. k = 1 gives f_W = f(T(u)).
Redraw only on warp-depth or parameter change, never per sample. Display-only: engine untouched.
Do NOT use bisection or numerical integration for the CDF. The parametric point is exact.
Edge: where f(T(u)) -> 0 (gap), guard the division: f_W -> 0 for any k > 0 (gap opens at once). Where f -> infinity
(atom), f_W -> 1/(1-k) (spike fades in). Clamp the division, do not let it produce inf/NaN in the draw.

## GATES
- Task 1: all 8 spec tests covered and passing.
- Task 2: curve continuous through w = 0 (no jump in the drawn target at the zero crossing); k = 0 is the diagonal;
  W(u) monotone; no inf/NaN for any (alpha, beta, k, feature) in the test grid.
- run_all green on both SDK legs.
- Behaviour unchanged when warp = 0 and no gap set (bit-identical engine output).

## DO NOT
- Change the gap convention, the clamp, the atom composition, the dual sign rule, or the warp mix.
- Add a morph-shape change (uniform-centred vs direct crossfade is still undecided; leave as is).
- Build the all-lane modulation subpanel (parked).
