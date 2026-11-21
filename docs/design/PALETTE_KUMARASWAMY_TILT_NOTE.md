# PALETTE NOTE — "Kumaraswamy-tilt" (KwMO-Uniform) member proposal

STATUS: SEQUENCED BY RODNEY 2026-10-22 — TRY THE TILT WITH THE CLAMP FEATURE, then decide if
it is a KEEPER to roll out broadly. Filed 2026-10-22; not scheduled ahead of the clamp; does
not block warp Phase 3. 2026-10-22 Q&A EXTENSION: the MO tilt applies to ALL palette members (R_p is EXACTLY
the shipped Mobius member's invCDF at a=1-p — see PALETTE-WIDE APPLICABILITY); the tilt brings
no rails of its own but composes with the planned clamps as their mass feeder (RAILS section);
p=1 ruled out in favor of the clamp; P_MAX=0.95 marked PROVISIONAL pending a feel test.
Decision widened to: member-as-filed vs SHARED TILT STAGE.
CLAMP DIRECTION (Rodney 2026-10-22): clamp-by-target-mass with BOTH rails is "a better design
for clamp" — two-rail model + lambda semantics filed in the RAILS section.
SOURCE: Rodney uploaded the KwMO paper alongside "Kumaraswamy is one of our output
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
  forbidden case. P_MAX = 0.95 PROVISIONAL, authored as a signed tilt in (−P_MAX, +P_MAX);
  provenance, feel-test derivation recipe and the p=1 ruling: P_MAX PROVENANCE section below.
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

────────────────────────────────────────────────────────────────────────────────
## PALETTE-WIDE APPLICABILITY (Rodney Q&A, filed 2026-10-22)

Q: can the MO variation apply to the other palette members? A: YES — all of them, one shared
mechanism, and the tilt curve is ALREADY IN THE PALETTE:

- IDENTITY: R_p(t) = (1-p)t/(1-pt) is EXACTLY MobiusDistribution's invCDF with a = 1-p
  (a·u/(1-(1-a)·u)). The shipped "mobius" member IS the Marshall-Olkin extended uniform —
  the paper's construction viewed from Monsoon is "compose the Mobius warp after another
  member". Same validity domain (p<1 <=> a>0), same rail directions (p>0 / a<1 toward 0).
- CLOSURE: m_a ∘ m_b = m_{ab} (verified by hand: ab·t/(1-(1-ab)t)) — on the Mobius member
  itself the tilt is ABSORBED by its parameter (a <- a·(1-p)); nothing new needed there.
- UNIVERSALITY: R_p is a monotone self-map of [0,1] (fixes 0 and 1, strictly increasing for
  p<1, slope in [(1-p), 1/(1-p)]). Monotone ∘ monotone = valid quantile for EVERY member —
  no per-member derivation. p=0 bit-identity holds palette-wide in one stroke. Cost identical
  everywhere: one multiply + one divide per draw.
- PER-MEMBER VERDICTS: uniform -> tilt alone IS Mobius (shipped; the reference curve/test);
  kumaraswamy -> this proposal (the paper's named instance); power / reversed-power -> compose
  cleanly, two rail-bends in series, both mirror corners covered; triangular -> composes,
  stays in [0,1]; logit-sigmoid / logit-normal -> open-support character preserved (R_p maps
  (0,1) onto (0,1)); truncated-normal -> edges stay edges, edge density re-weighted but
  finite; pchip (drawn) -> monotone ∘ monotone, stored curve untouched — "draw, then tilt";
  mobius -> absorbed (a <- a·(1-p)).
- CONVENTION (pinned): SAMPLE-SIDE tilt — out = R_p(member.invCDF(u)) — matches the filed
  eq-(14) form and the output-layer transfer-curve framing; the knob's meaning is
  member-independent ("bend toward bottom/top rail"). The alternative u-side tilt
  (member.invCDF(R_p(u)) — the classical MO-extended-X, G(x)=F(x)/(1-p+pF(x)), MO 1997) is
  also universal and citable, but the resulting shape depends on the member, so the knob feel
  changes per member. Do not ship both meanings on one knob.
- HONEST CAVEAT: the hazard-ratio-monotone property is the paper's result for the Kw case;
  for other members the qualitative "one knob, monotone rail-bend" survives, but the Fig-2
  taxonomy (bathtub/UBT/S) is not guaranteed verbatim per member. Label the knob "rail tilt",
  not a per-member hazard promise.
- IMPLEMENTATION SHAPE: ONE shared stage, not nine subclasses — either a tilted(base,p)
  wrapper kind tag or an output-layer tilt knob beside the palette. The Kw case is the paper's
  named instance; uniform is the reference; Mobius is absorbed.

## RAILS — composition with the planned clamps (Rodney Q&A, filed 2026-10-22)

Q: does the tilt directly bring in rails, or does it compose with the planned top/bottom clamp
rails? A: NO DIRECT RAILS; YES IT COMPOSES, as the clamp's mass feeder.

- TILT ALONE: absolutely continuous for EVERY p<1 — pure density re-weighting, no atom
  anywhere. Near the approached rail the density multiplier is 1/(1-p) (bottom for p>0): at
  P_MAX=0.95 a 20x CEILING — bounded, no divergence created even on members whose density
  already runs hot at an edge (Power k<1, Kw a<1 — the DIST_EDGE_EPS proxies just get
  re-weighted). The atom exists only in the p->1 limit — the case P_MAX forbids. The tilt is
  the APPROACH to the rail; it never arrives.
- WITH THE CLAMP (house order warp -> clamp): the clamp sees the TILTED distribution and
  captures what the tilt left under its threshold. Bottom rail, threshold eps: untilted atom
  mass ~ F_base(eps); tilted lambda_eff = F_base(R_p^-1(eps)) = F_base(eps/(1-p+p·eps)), and
  R_p^-1(eps) > eps for p>0 — downward tilt ENLARGES the bottom atom, shrinks the top one,
  monotone in p. Near the rail the amplifier is ~1/(1-p) again — the same constant as the
  density ceiling. Top rail: exact mirror for p<0.
  Musical reading: TILT = how much mass gathers NEAR the rail; CLAMP = how much SNAPS ONTO it.
  Orthogonal stages that cooperate.
- CALIBRATION DECISION (Rodney's call): house order makes the clamp's captured mass
  tilt-dependent (clamp-then-tilt would preserve it — R_p(0)=0 pins the rail atom — but that
  is not the pipeline order). Either clamp by TARGET ATOM MASS (reads the post-warp
  distribution, auto-compensates; lambda stays calibrated under any tilt) or by THRESHOLD
  (lambda_eff moves with the tilt knob — one knob continuously pre-shapes what the clamp
  catches). Nothing structural breaks either way: both stages stateless + monotone; the tilt
  is STRICTLY monotone so it adds no flat segments; the clamp's flats remain the only atoms.
  DECIDED (Rodney 2026-10-22): TARGET-ATOM-MASS — the two-rail model below.
- CLAMP-BY-TARGET-MASS — TWO-RAIL MODEL (Rodney: "a better design for clamp", 2026-10-22):
  BOTH rails on ONE member, each with its own target mass, each ONE closed-form solve on the
  same tilted CDF:
      bottom: eps_B = R_p(Q_base(lambda_B))     (capture x <= eps_B -> bottom rail)
      top:    eps_T = R_p(Q_base(1 - lambda_T)) (capture x >= eps_T -> top rail)
  Disjoint value regions -> the clamps COMMUTE, never fight; post-clamp mass =
  lambda_B + middle + lambda_T = 1 exactly. SOLE CONSTRAINT: lambda_B + lambda_T < 1 (joint
  authoring clamp, e.g. <= 0.9) — the leftover 1 - lambda_B - lambda_T IS the non-rail
  material. The lambda_B + lambda_T -> 1 limit is the two-point Bernoulli (coin flip between
  rails — hard hocket between two fixed values) — rank-destroying; stay short of it.
  WHAT lambda DIRECTLY INDICATES: the per-draw MARGINAL rail-hit probability = the expected
  FRACTION OF STEPS that output exactly the rail value (lambda_B = 0.1 -> one step in ten
  snaps to the bottom rail). Three-knob decomposition of the heard texture:
      lambda_B / lambda_T = DENSITY of rail hits (how often each rail fires);
      r (copula)          = CLUSTERING of those hits (MA autocorrelation -> runs of rail hits,
                            not coin-flip scatter);
      p (tilt)            = the material BETWEEN hits + where the capture nets sit (masses
                            stay exact; the approached net narrows, the far net widens).
  WHAT lambda IS NOT: (1) not a clock rate — the rhythm of rail hits is fixed by the RNG
  stream (deterministic, scrub/reverse-safe); lambda sets average density only. (2) not the
  DMO lambda — these are per-lane marginal atom masses (per-draw Bernoulli probabilities),
  NOT the copula tail-dependence coefficient; the joint co-hit rate stays emergent through
  C(eps_B, eps_T); calibrated joint lambda remains DMO's deferred territory. (3) not a level
  control — WHAT the rail value is, is separate; lambda only says HOW OFTEN.
  MASS vs THRESHOLD contrast (both closed-form, both stateless/reversible): mass mode =
  lambda exact, capture position drifts with tilt (the rail-event RATE is the musical
  object); threshold mode = position exact, lambda drifts with tilt (the rail POSITION is
  the musical object). MASS MODE IS THE LANDED DESIGN.
- CITATION FENCE STAYS PER-BRANCH: the composed object (KwMO warp + clamp atom) is NOT
  "KwMO" — the paper governs only the continuous piece; the atom piece stands on MO 1997 /
  DMO as filed.

## P_MAX PROVENANCE + THE p=1 RULING (Rodney Q&A, filed 2026-10-22)

WHERE 0.95 CAME FROM: a PROVISIONAL SUGGESTION from this note's first filing — chosen by
ANALOGY to the house clamp pattern, NOT derived. Honest comparison with the two precedents it
leans on:
- R_MAX = 0.97 (MovingAverageCopula) IS derived: geometric weights degenerate as r -> 1; 0.97
  keeps the sum well-conditioned.
- CONC_MAX = 5 (Kumaraswamy) IS derived: numerical cliff mapped (conc=8 -> invCDF flat/ties,
  the numerical atom) and peak height ~3.7 at conc=5 judged musically ample.
- P_MAX = 0.95 has NO equivalent derivation yet. The math is valid for ALL p<1 — no numerical
  cliff analogous to conc=8 (the only hard failure is exactly p=1, ruled out below). So the
  number must come from a FEEL TEST.
DERIVATION RECIPE (when we do it): uniform member, tilt at 0.90 / 0.95 / 0.98 / 0.99 through
slew + quantizer on a real patch; find where rail-HUG becomes rail-PINNED by ear; set the top
stop there and round down to a clean constant. Audibility taper for orientation — median of
the tilted uniform R_p(0.5): 4.8% of range at p=0.95, 1% at 0.99, 0.1% at 0.999 — differences
above ~0.95-0.99 get progressively harder to hear post-slew/quantizer. Also check the editor
display at those settings (the edge spike stays finite, but usable?). Knob-real-estate rule:
the top stop belongs where further travel stops being audible (the CONC_MAX logic), not where
the math ends.

COULD WE ALLOW p=1 INSTEAD OF THE CLAMP? Ruled NO for now — p=1 is the mass-1 atom at the
bottom rail (R_1(t) = 0 for all t<1; every draw returns exactly the rail):
- PROS: family closure (the endpoint is genuinely MO 1997's singular branch); one-knob
  uniform->pinned gesture; no magic constant.
- CONS: (1) NaN at exactly 1.0 — R_1(1) = 0/0, and members DO saturate Q(u)=1.0 in double
  precision at high concentration (the conc=8 finding); (2) p>1 is catastrophic — the
  denominator's pole t=1/p lands INSIDE (0,1), the quantile folds back negative/inf; the
  clamp margin is what keeps modulation overshoot / automation ramps / hand-edited JSON away
  from it (the float-equality boundary problem: 1.0000001 in a patch -> reject or clamp?);
  (3) rank destruction legal — constant output is the editor's forbidden case, exceptions
  carved into copula identification / the eps-monotonicity gate / PDF display; (4) behavior
  cliff under modulation through 1.0 — the lane snaps dense->dead and back mid-flight;
  (5) dead knob travel — above ~0.95-0.99 nothing audibly changes while the math degrades;
  (6) it is a DEGENERATE SHADOW of the clamp feature: bottom-rail-only, mass-100%, no
  calibration — pin-to-rail done properly is the clamp (calibratable lambda, BOTH rails,
  designed pipeline position, the co-clipping story).
- IF EVER WANTED: ship as an explicit mode — runtime guard p >= 1 -> rail constant (NaN-safe,
  also covers p>1), editor rail-flag instead of the PDF, modulation hard-stop, dedicated test
  (constant output, no NaN, JSON round-trip). Four special cases for one point the clamp
  supersedes.
- ENFORCEMENT NOTE: the clamp is EVAL-SIDE (clamp p into [-P_MAX, +P_MAX] before computing
  R_p — the clampR precedent) — modulation overshoot is caught at the evaluation boundary, so
  neither the knob nor a modulator can ever present the hot path with |p| >= 1.

## Tests (mirror the reversed-power battery)

1. p=0 BIT-IDENTITY vs existing KumaraswamyWarp across the (mode,conc) grid.
2. Authoring round-trip: (mode,conc,tilt) → evaluate → inverse map → same params (within eps).
3. eps-monotonicity floor: effective (a,b) stays strictly increasing for all clamped tilts.
4. Uniform-marginal preservation through the lane-quantile feed (distribution battery contract).
5. Tilt-direction sanity: median of Q shifts toward 0 for p>0, toward 1 for p<0; |shift|
   monotone in |p|.
6. JSON round-trip with the new kind tag; pre-existing "kumaraswamy" patches unaffected.
7. PALETTE-WIDE (if the shared-stage path is chosen): for EVERY member, the composite
   invCDF u -> R_p(member.invCDF(u)) is strictly increasing across the editor grid for all
   |p| <= P_MAX; Mobius absorption identity tilt(Mobius(a), p) == Mobius(a·(1-p)) to fp
   tolerance; uniform member + tilt == pure Mobius reference curve.

## Sequencing

- RODNEY'S CALL (2026-10-22): try the tilt WHEN WE DO THE CLAMP — evaluate the pair together
  (the mass-feeder composition IS the interesting behavior; a standalone tilt evaluation
  would only show the continuous rail-hug and miss the composed behavior), then decide if the
  tilt is a KEEPER worth rolling out broadly (the shared tilt stage). Until then this note
  stays a filed proposal; nothing schedules it ahead of the clamp.
- When that work starts: the P_MAX feel test (derivation recipe above) and the clamp
  calibration decision (mass vs threshold) are ONE listening session — the two knobs'
  calibrations interact by design (lambda_eff = F_base(R_p^-1(eps))).
- Reuses landed helpers (modeConcFromAb/abFromModeConc), so item #1 is NOT a blocker (it only
  unifies where the canonical map lives). Tests 1-7 stand as written when implementation day
  comes.
- Pointer filed in TODO_distribution_palette_and_editor.md → Sequencing.
