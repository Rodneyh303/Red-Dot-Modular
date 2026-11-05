# PHASE DISTRIBUTION SPEC — Kumaraswamy and Dual Kumaraswamy with tilt, gap and atom (Rodney-decided)

Status: spec for review. Stage semantics, formulas and tests. No code yet.

## 1. Scope
Two distributions on [0,1], shown as two menu entries:
- **Kumaraswamy (K)**, member K(α, β).
- **Dual Kumaraswamy (DK)**, the same object read with CDF and quantile swapped.

One implementation: a single class with a reading parameter fixed by the menu entry. The stage chain is shared,
so the two entries cannot drift apart.

## 2. Base functions (closed form)
- F(x) = 1 − (1 − x^α)^β
- F⁻¹(v) = (1 − (1 − v)^(1/β))^(1/α)

## 3. Chain (stage order, fixed)
member → tilt → gap → atom

- **Tilt** (existing, WarpStages): R_p(t) = (1 − p)t / (1 − p·t), inverse R_p⁻¹(y) = y / (1 − p + p·y).
  Tilted CDF F_t(x) = F(R_p⁻¹(x)); tilted quantile Q_t(v) = R_p(F⁻¹(v)).
- **Gap** (new): zero-density interval (a, b) in tilted output coordinates, 0 ≤ a ≤ b ≤ 1. Defined below.
- **Atom** (existing, WarpStages, member argument changed): mass m at level ℓ. Applied to the gapped member.

Stage order is fixed: the atom is applied after the gap, so an atom inside a gap survives and is not swept to an edge.

## 4. Gap (new stage)
Let F_a = F_t(a), F_b = F_t(b), p = F_b − F_a (the gap's probability mass).

**Convention: uniform rescale by 1/(1 − p).** Mass is scaled uniformly, so neither side is favoured, and the
normalised density outside the gap is unchanged. The density is multiplied by 1/(1 − p) everywhere outside the gap.

CDF G:
- G(x) = F_t(x) / (1 − p)              for x < a
- G(x) = F_a / (1 − p)                  for a ≤ x < b (flat: zero density)
- G(x) = (F_t(x) − p) / (1 − p)         for x ≥ b

Checks: G is continuous at a and at b, G(1) = 1, and F_a/(1 − p) ≤ 1 because F_b ≤ 1.

Quantile Q_G:
- Q_G(v) = Q_t((1 − p)·v)                 for v ≤ F_a / (1 − p)
- Q_G(v) = Q_t((1 − p)·v + p)             for v > F_a / (1 − p)

Q_G jumps from a to b at v = F_a / (1 − p). That jump is the gap.

**Clamp:** p ≤ 1 − ε, with ε a spec parameter (start at 1e-6, set by test). p = 1 removes all mass and is invalid.
The clamp is on the mass p, not on the output width b − a. The b = 1 case is therefore valid whenever F_a > 0.

**Width zero is neutral:** a = b gives p = 0, G = F_t, Q_G = Q_t.

## 5. Atom (existing, applied to the gapped member)
Let G(ℓ) be the gapped CDF at the atom level:
- G(ℓ) = F_t(ℓ) / (1 − p)              for ℓ < a
- G(ℓ) = F_a / (1 − p)                  for a ≤ ℓ < b
- G(ℓ) = (F_t(ℓ) − p) / (1 − p)         for ℓ ≥ b

Placement quantile q0 = G(ℓ)·(1 − m). Composed quantile:
- Q(u) = Q_G(u / (1 − m))               for u < q0
- Q(u) = ℓ                              for q0 ≤ u ≤ q0 + m
- Q(u) = Q_G((u − m) / (1 − m))         for u > q0 + m

Atom inside a gap is allowed; the spike sits at ℓ in the zero-density interval.

**Overlap rule (accepted):** if the atom plateau covers the gap's jump point, the plateau wins and the jump lands at
the plateau edge. This is accepted as the consequence of the user using both features.

## 6. Readings
- **K reading:** CDF = G (gapped, tilted member); quantile = Q(u) as above.
- **DK reading:** the same chain with CDF and quantile exchanged. Its CDF is the composed quantile and its quantile is
  the composed CDF. An atom on K appears as a gap on DK, and a gap on K as an atom on DK.

The DK reading uses the same stage formulas with the roles swapped, so it is not a separate implementation.

## 7. Cost
Per draw: one F or F⁻¹ evaluation, plus one R_p or R_p⁻¹ evaluation. Per parameter change: F_t(a), F_t(b), p, and G(ℓ).
All closed form.

## 8. Tests (must pass before the implementation is accepted)
1. G is continuous, non-decreasing and G(1) = 1 across a grid of (α, β, p, a, b).
2. Width zero reproduces the untouched distribution exactly (bit-identical).
3. a = 0 and b = 1 give valid quantiles, with the clamp engaging only when p ≈ 1.
4. Round trip: Q(G(x)) = x for x outside the gap, to float tolerance.
5. The jump in Q_G is at v = F_a/(1 − p), landing at a then b.
6. Normalised density outside the gap is unchanged by the gap (the rescale is uniform).
7. An atom inside a gap sits at ℓ; an atom whose plateau covers the jump point wins.
8. DK reading: an atom on K becomes a gap on DK, verified numerically at several parameter sets.

## 9. Decisions and remaining items
Decided (Rodney):
- **Flat direction: no direction memory.** A flat carries no direction. Direction is the sign of the change that
  moves the playhead. Direction only matters when motion resumes, so nothing needs storing during a stillness.
  With PPQN quantisation, "flat" means the quantised position has not changed, so sub-position jitter is absorbed
  and cannot flip direction. No dead-band is needed beyond the quantisation.
- **Panel controls (decided):** per lane on the distribution module, TWO knobs (gap location, gap size) next to
  the atom knobs, and TWO modulation jacks next to the atom jacks. The gap uses the SAME Macro/East voice
  delegation and opt-out as every other distribution control  no new routing. The atom block grows by two
  knobs and two jacks per lane; the gap sits immediately beside the atom so the two read as a pair. The gap
  jacks go on the base panel next to the atom jacks (not a subpanel); if the all-lane modulation subpanel is
  adopted later, the gap jacks move with the atom jacks, since they are treated identically.
- **ε = 1e-6** for the p clamp.
- **Menu label: "Dual Kumaraswamy".**

Remaining:
- The modulation-subpanel decision (all-lane, parked) would move the gap jacks with the atom jacks if adopted; it does not block the gap stage.
- Test runs for tests 1–8 once the implementation exists.


## 10. Distribution plot  the EFFECTIVE warped curve (display-only, Rodney)
The DISTRIBUTION plot must show the EFFECTIVE warped distribution at the current SIGNED warp depth, NOT the
bare target. Today it draws the selected member (or its dual past zero), so the drawn curve jumps at the
zero crossing while the engine output barely moves. The plot must track the knob continuously through zero.

### The transform being plotted
Warped quantile, k = |w|, T = Q for w >= 0 (member composed quantile) or G for w < 0 (the dual, composed CDF):
    W(u) = (1-k)*u + k*T(u)

### PDF fill  CLOSED FORM (sweep u over [0,1], no inversion; one sweep also gives the CDF trace)
W'(u) = (1-k) + k*T'(u), and for a quantile T, T'(u) = 1 / f(T(u)) where f is the target density. So the
density of the warped variable at the output point x = W(u) is:
    f_W(x) = 1 / [ (1-k) + k / f(T(u)) ]     at the point x = W(u)
Draw the fill by sweeping u across [0,1]: at each u, plot (W(u), f_W(u)) directly. Closed form given the
target's f and T.

### CDF trace  CLOSED FORM, PARAMETRIC (no bisection, no integration)
For W monotone with U uniform, P(W(U) <= W(u)) = P(U <= u) = u. So the warped CDF at output x = W(u) is exactly u.
The CDF trace is the parametric curve (W(u), u): plot W(u) on the output axis and u on the probability axis.
Consistency with the PDF: f_W(W(u)) * W'(u) = 1, so integrating f_W from 0 to W(u) returns u. Integrating
numerically would only add error. Sweep u across [0,1] once; the PDF fill and the CDF trace come from the same points.
(Supersedes an earlier note that proposed bisection. Bisection solves W(u) = y, which is the inverse problem
that the parametric sweep avoids.)

### Limits and the atom/gap asymmetry (state in the UI reasoning, not a bug)
- k = 0: f_W = 1 (uniform, the flat midpoint). The curve is the diagonal; no jump at zero.
- k = 1: f_W = f(T(u)) (the target's own density).
- The blend is RECIPROCAL, not linear:
  - ATOM (target density -> infinity): f_W -> 1/(1-k). The spike stays finite and grows only as k -> 1, so a
    small warp barely raises it and it shoots up near full depth  it FADES IN gradually.
  - GAP (target density -> 0): f_W -> 0 for ANY k > 0  the gap OPENS ABRUPTLY as soon as warp leaves zero.
- So atoms fade in with depth and gaps open at once. This asymmetry is real (not a plot artefact); the two
  sides of the warp knob feel different, and the plot shows it honestly.

### Scope
Display-only. The engine transform is unchanged; this is purely what the DISTRIBUTION plot draws. The plot
redraws on warp-depth or parameter change (not per sample).

════════════════════════════════════════════════════════════════════════════════
## 11. CC CATCH-UP  status and remaining work (for CC; build on feat/phase-duals-gaps)

### Status (verified against the branch)
| Item | State | Evidence |
|---|---|---|
| Gap stage, 1/(1-p) uniform rescale | LANDED | WarpStages.hpp: gapQuantile, kGapEps = 1e-6 (the clamp), branch at v* = F_a/(1-p) |
| Atom after gap (composed) | LANDED | WarpStages.hpp: Q(u) = Q_G(u/(1-m)) / ell / Q_G((u-m)/(1-m)) |
| Dual reading by SIGN of warp depth | LANDED | warp w<0 blends uniform with the composed CDF (the dual) |
| Warp mix in quantile space | LANDED | WarpF.hpp: warped(u) = (1-w)u + w F.invCDF(u) |
| Acceptance test file | PRESENT, CI green | test/test_phase_distribution.cpp (ok() harness); the 8-to-check mapping is still to do |
| Flat-direction, no memory | DECIDED | sign of the moving change; flat carries no direction |
| Gap panel: 2 knobs + 2 jacks per lane, next to atom, same delegation | LANDED | user-confirmed (controls added, CI green); knobs = cell slots gapLoc (6) and gapSize (7) in SandsModifierResolver, written by SandsDistributionExpander |
| Distribution plot = EFFECTIVE warped curve (section 10) | NOT FOUND | no W(u) sweep in the plot code on the branch  REMAINING |

### CC work, in order
1. **Verify the 8 acceptance tests (section 8) against test_phase_distribution.cpp.** Map each test to an existing
   check; add the missing ones. Do not change the landed convention or the clamp. Gate: all 8 pass.
2. **Effective-curve plot (section 10).** Sweep u over [0,1] once; per point plot (W(u), f_W(u)) for the PDF fill
   and (W(u), u) for the CDF trace (PARAMETRIC, no bisection, no integration). W = (1-k)u + k*T(u), k = |w|,
   T = Q for w >= 0, G for w < 0. Display-only; redraw on warp-depth or parameter change, not per sample.
   Gate: the plotted curve is continuous through w = 0; at k = 0 the trace is the diagonal and f_W = 1.
3. **Gap panel controls: LANDED** (user-confirmed, CI green). Nothing further, except a Rack check that the two jacks sit beside the atom jacks.
4. **Confirm the flat-direction rule** in the phase input path: a flat carries no direction and direction is the
   sign of the change that moves the playhead. Gate: a held input followed by a move takes the sign of the move.

### Still open (do not build until decided)
- **Morph shape:** the warp currently morphs member -> uniform -> dual (uniform-centred). A direct member-to-dual
  crossfade is a different formula and would need a monotonicity check. Decide before any change.
- **Modulation-subpanel (all-lane):** parked; does not block items 1-4.

### Gates (all items)
- run_all green on both SDK legs.
- No change to landed gap convention, clamp, or dual sign rule.
- ≤16 bit-identical behaviour where the feature is inactive (warp at 0, no gap set).
