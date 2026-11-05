# PHASE DISTRIBUTION SPEC — Kumaraswamy and Dual Kumaraswamy with tilt, gap and atom (Rodney-decided)

Status: spec for review, as amended by the bidirectional warp (see §9). Stage semantics,
formulas and tests.

## 1. Scope
The menu choice is the ORIGINAL distribution only (as amended — Rodney: "we go back to
making only the original a menu choice"). The dual is not a menu entry and not a stored
state: it is one of TWO READINGS of the selected curve, selected per cell by the SIGN of
the warp depth:
- **Kumaraswamy (K)** (and every palette member likewise), member K(α, β) — the menu
  entry.
- **Dual Kumaraswamy (DK)**, the same object read with CDF and quantile swapped — the
  warp depth's NEGATIVE side (§6a).

One implementation: a single class; the reading is a property of the sign, not of the
stored value. The stage chain is shared, so the two readings cannot drift apart.

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

## 6a. The bidirectional warp (as amended)
The warp depth w is SIGNED, [−1, +1], 0 = no warp. The mix stays in quantile space with
the uniform as the reference (Rodney: "Blending in quantile space, (1−|w|)·u + |w|·Q(u),
keeps the uniform as the reference and is the one I'd choose same as current"):
- **w ≥ 0:** out = (1−w)·u + w·Q(u) — uniform blended with the selected distribution's
  composed QUANTILE, exactly as the unidirectional warp always did (bit-identical).
- **w < 0:** out = (1−|w|)·u + |w|·G(u) — uniform blended with the composed CDF, the DUAL
  reading (Rodney: "Negative blends uniform and the dual distribution.").

The knob axes are [−1,+1] everywhere the depth is authored (the distribution expander's
WARP column, the East/Macro warp-level knobs); modulation folds through the same signed
clamp, so CV can cross zero freely — the crossing is audible, not a clamp.

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
8. DK reading: an atom on K becomes a gap on DK, verified numerically at several parameter sets
   (reached at the bank level by w = −1 over the bare member, per the amendment).

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
- **Menu label: "Dual Kumaraswamy"** (SUPERSEDED — the dual left the menu; see the
  amendment below).
- **Duals for EVERY distribution** (Rodney: "For each of our distributions on distribution module add the
  dual. Example is for kumaraswamy but we can apply to each. So we double the number of distribution choices.") —
  the duals exist for every member still, but SUPERSEDED IN FORM by the amendment below: they are no longer
  second menu entries. Identity (the uniform) is its own dual (F(x) = x swaps to itself) on either path; the
  drawn (freehand) curve's dual reads the SAME points with CDF and quantile exchanged. One implementation: the
  dual is the reading selected by the warp depth's sign — never a second copy of any formula.
- **THE BIDIRECTIONAL WARP (amendment, Rodney: "I want to change this slightly. Instead of
  having the dual of each distribution separately selectable we go back to making only the
  original a menu choice. But we make the warp knob bidirectional instead of
  unidirectional. Positive blends uniform and selected distribution - as now. Negative
  blends uniform and the dual distribution." and, on the blend space: "Blend space.
  Blending in quantile space, (1−|w|)·u + |w|·Q(u), keeps the uniform as the reference and
  is the one I'd choose same as current."):** the menu returns to the bare palette; the
  warp depth becomes a SIGNED axis [−1,+1]; the SIGN selects the reading (§6a) — positive
  the member's composed quantile as always, negative the composed CDF (the dual). The
  blend space is unchanged. Legacy patches that persisted a dual as a stored state load
  as the member at depth −w — the same sound bit-exactly, so no patch re-authors.
- **Atom↔gap exchange is the dual reading's property for every member (decided, Rodney: "We also add zero density
  gaps with controls and modulation similar to atoms. Atom dual is a gap and vice versa."):** an atom on a
  distribution reads as a gap on its dual and vice versa (§6, test 8) — now a first-class promise for every menu
  member, not just Kumaraswamy.
- **Branch base (decided, Rodney: "You can build off rhe 32 step branch on a new branch. Stage 5 ci green but not
  merged yet."):** the implementation branches off `feat/steps-32-stage1` (the STEPS_32 feature branch, Stage 5
  CI-green, unmerged) on a NEW branch; the phase-distribution work rides above the 32-step work, never mixed
  into it.

Remaining:
- The modulation-subpanel decision (all-lane, parked) would move the gap jacks with the atom jacks if adopted; it does not block the gap stage.
- Test runs for tests 1–8 once the implementation exists.
