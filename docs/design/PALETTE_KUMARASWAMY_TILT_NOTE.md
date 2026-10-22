# PALETTE NOTE — "Kumaraswamy-tilt" (KwMO-Uniform) member proposal

STATUS: PROPOSAL — Rodney's call pending (filed 2026-10-22). Not scheduled; does not block warp
Phase 3. SOURCE: Rodney uploaded the KwMO paper alongside "Kumaraswamy is one of our output
distributions and we plan to introduce clamp on distributions which generates Marshall-Olkin
copula". This note is the PALETTE half of that thread. The CLAMP half (singular branch, rail
atoms, lambda>0) lives in OUTPUT_LAYER_DISTRIBUTION_MAPPING.md (DMO/common-shock) and is
explicitly OUT OF SCOPE here — see the scope fence below.

────────────────────────────────────────────────────────────────────────────────
## The math (one line)

KwMO family with UNIFORM baseline = the shipped Kumaraswamy quantile composed with a
one-parameter rational MO-tilt map:

    Q(u) = R_p( H(u) )
    H(u) = (1-(1-u)^(1/b))^(1/a)      ← existing KumaraswamyWarp quantile
    R_p(t) = (1-p)·t / (1 - p·t)      ← MO tilt: closed form, monotone (0,1)->(0,1) for all p<1

Paper: Alizadeh, Tahir, Cordeiro, Mansoor, Zubair, Hamedani, "The Kumaraswamy Marshal-Olkin
Family of Distributions", J. Egyptian Math. Soc. 23 (2015) 546-557, doi:10.1016/j.joems.2014.12.002.
Eq (14) is their closed-form quantile (their p; p = 1-p_bar in the survival form
F_MO = F/(p_bar+(1-p_bar)F)). Table 1: p=0 reduces to plain Kw-G (our member); a=b=1 reduces to
plain MO; rows 4-5 are the PH/RH models. Citation also filed in
OUTPUT_LAYER_DISTRIBUTION_MAPPING.md next to the "lambda=0 CORNER" statement.

## Musical meaning — the hazard-tilt knob

The MO tilt's defining property: the hazard ratio h_KwMO/h_G is MONOTONE on the support —
increasing in x for p<0, decreasing for p ∈ (0,1). One knob, predictable direction: it bends the
event density toward the bottom rail (p>0) or the top rail (p<0) while the existing
(mode, concentration) knobs keep their meaning. Their Fig 2 shows the span of hazard shapes the
family covers (constant / increasing / decreasing / bathtub / upside-down bathtub / S), i.e. the
preset/UI space this one extra knob unlocks on top of Kumaraswamy.

## Engineering fit

- NEUTRAL PARAMETER: p=0 → R_p = identity → BIT-IDENTICAL to the shipped KumaraswamyWarp.
  House neutral-parameter rule (same contract as MovingAverageCopula r==0 short-circuit).
- COST: one multiply + one divide per draw, double precision, stateless, pure function of
  position — the distmath free-fn / value-variant concrete-type pattern (CC_PROMPT_02 precedent,
  item #2 reversed-power's "no distmath gating" ruling applies).
- PERSISTENCE: NEW kind tag via the existing strcmp pattern in WarpF.hpp (suggested
  "kumaraswamy-tilt"; exact string follows the typeName convention). Purely additive — existing
  "kumaraswamy" patches load as the untouched p=0-shaped member; no compat shims.
- AUTHORING: reuses the ALREADY-LANDED KumaraswamyDistribution::modeConcFromAb /
  abFromModeConc helpers (WarpCurvePopup uses them today). The tilt composes AFTER the
  (mode,conc)->(a,b) map, so the three knobs stay orthogonal by construction. TODO item #1's
  later unification onto the Distribution class absorbs this member for free.
- WARP SLOTS: new variant member KumaraswamyTiltWarp{a,b,p} beside KumaraswamyWarp in the
  WarpF.hpp variant + fromDistribution/toDistribution mapping + a third params row ("tilt") in
  the WarpCurvePopup param block.

## Constraints

- VALID RANGE: p < 1 with an AUTHORING CLAMP (house convention: R_MAX=0.97 / CONC_MAX=5 style).
  p→1⁻ collapses R_p → 0 for every u<1: the atom-at-zero, rank-destroying — the editor's
  forbidden case. Suggested P_MAX = 0.95, authored as a signed tilt in (−P_MAX, +P_MAX).
- Negative p is mathematically valid (upward tilt) and R_p stays strictly increasing for every
  p<1 — no extra floor beyond the symmetric authoring clamp.
- Respect the editor's eps-monotonicity floor (same constraint item #1 puts on the (mode,conc)
  inverse map); the tilt must not be allowed to push the effective curve flat.

## NOT the clamp/atom feature (scope fence)

This member is ABSOLUTELY CONTINUOUS (paper eq (5) is a pure density; no atoms anywhere) — the
lambda=0 side of the MO family tree, the "lambda=0 CORNER" of OUTPUT_LAYER_DISTRIBUTION_MAPPING.md.
The clamp feature's target is the SINGULAR branch — rail atoms, lambda>0, the DMO/common-shock
construction (Albanese-Crepey-Hoskinson-Saadeddine, QF 21(1) 2021; Marshall-Olkin 1997 for the
singular diagonal). Different object, different references, different feature. Do not conflate
the two when reading "Marshall-Olkin" in the docs.

## Tests (mirror the reversed-power battery)

1. p=0 BIT-IDENTITY vs existing KumaraswamyWarp across the (mode,conc) grid.
2. Authoring round-trip: (mode,conc,tilt) → evaluate → inverse map → same params (within eps).
3. eps-monotonicity floor: effective (a,b) stays strictly increasing for all clamped tilts.
4. Uniform-marginal preservation through the lane-quantile feed (distribution battery contract).
5. Tilt-direction sanity: median of Q shifts toward 0 for p>0, toward 1 for p<0; |shift|
   monotone in |p|.
6. JSON round-trip with the new kind tag; pre-existing "kumaraswamy" patches unaffected.

## Sequencing

- Any time — independent of warp Phase 3 prompts; reuses landed helpers, so item #1 is NOT a
  blocker (it only unifies where the canonical map lives).
- Pointer filed in TODO_distribution_palette_and_editor.md → Sequencing.
