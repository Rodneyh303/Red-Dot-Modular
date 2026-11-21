# TODO — distribution + editor refinements (not blocking Phase 3; do when convenient)

Added 2026-10-18b. Three independent items on the distribution palette / .dmcurve editor.
None blocks the warp build (prompts 01-04); all improve feel/correctness of the palette.

────────────────────────────────────────────────────────────────────────────────
## 1. Kumaraswamy: reparameterise (mode, concentration) — UI PRESENTATION ONLY

PROBLEM: the native (a,b) params interact strongly — both pull the mode AND both pull the
tails — so a user turning a to move the peak also moves the spread. Bad control feel. And the
two params are NOT interchangeable as MODULATION targets (see the copula analysis,
SESSION_LOG_2026-10-18b.md §5 + the r=0 frailty thread): b is a FRAILTY param
(1-(1-x^a)^b is multiplicative in the hazard of (1-x^a) → shared-b modulation at r=0 gives a
closed-form Archimedean generator); a is NOT (it sits inside the inner power). So the two
"param slots" mean qualitatively different things.

FIX: present the UI in orthogonalised coordinates — (mode, concentration) (or mode, tail-weight).
One knob moves location, the other moves shape, independently. This also makes the two
modulation slots MEAN the two factor types: shared-mode modulation = location co-movement
factor; shared-concentration = dispersion co-movement factor (the two common-factor types from
the session). Legible.

SCOPE (Rodney): UI PRESENTATION ONLY. Internal evaluation stays (a,b) — the distmath:: free fn
and the Kumaraswamy invCDF/cdf/pdf are unchanged. Add the (mode,concentration)→(a,b) MAPPING on
the Distribution class (a static/member helper on KumaraswamyDistribution, or a small free fn
beside it), used at the AUTHORING boundary (editor knobs / lower()), NOT in the hot path. Same
pattern as lower(): author in the nice basis, evaluate in the native one.
  - Kumaraswamy mode = ((a-1)/(a·b-1))^(1/a) for a>1, b>1; invert numerically or with a decent
    approximation. Document the valid region.
CAUTION: the inversion must stay inside valid params (a,b>0) AND the editor's strict-increase
ε-floor — a concentration knob to the extreme can push b→0 and flatten the curve (= the atom
case). Clamp in the new coordinates. Add a param-validity test alongside test_distribution.

────────────────────────────────────────────────────────────────────────────────
## 2. Add "reverse power": F(x) = 1 - (1-x)^θ

The mirror of PowerDistribution (F(x)=x^θ). WHY it's worth having, beyond symmetry: at r=0 with
shared-θ modulation it is a FRAILTY form on -log(1-x) → Archimedean, and specifically it is the
route to the genuine JOE copula (1-(1-x)^θ is the Joe marginal form). PowerDistribution piles
mass toward 1; reverse-power piles toward 0 — the missing half of the palette's asymmetry, and
the only place the actual Joe copula is reachable in the whole construction (r=0 + this marginal
+ the right mixing law). Cheap: one more concrete Distribution subclass + its distmath:: free
fns (invCDF/cdf/pdf), one DistKind enum entry, lower() case, one test battery entry. Mirror of
the existing power class throughout.
(λ caveat unchanged: Archimedean SHAPE, but λ=0 under a bounded knob; genuine Joe tail
dependence needs unbounded mixing, not a knob. The generator is real; the tail is off.)

────────────────────────────────────────────────────────────────────────────────
## 3. Editor: x-axis autoscale (a 2nd x-axis) for sub-range distributions

CURRENT STATE: DistributionCurveWidget.hpp samples the PDF on a FIXED x-domain [0,1]
(pdf((double)i/N), ~L144-150) and auto-scales only the Y (density) axis (maxD over the open
interval, RHS scale ~L263).

PROBLEM: some distributions concentrate their mass in a SUB-INTERVAL of [0,1] — truncated-normal
with tight σ, Möbius/logit-sigmoid piling mass near one edge. On the fixed [0,1] x-domain the
interesting structure is crushed into a sliver and most of the plot width is empty.

FIX: add x-axis autoscaling as a SECOND x-axis (beneath the fixed [0,1] primary) that zooms the
horizontal to the EFFECTIVE SUPPORT — the sub-range where the density is non-negligible (e.g.
the [q_lo, q_hi] quantile band, or where pdf > ε·maxD). The primary [0,1] axis stays (it's the
true domain / the uniform reference); the secondary is the "zoomed to where the mass is" view.
Mirror the Y-axis auto-max discipline already there: smooth-on-release rescale, open-interval
sampling (don't let a divergent edge define the scale), a floor to avoid 0/0.
  - Decide: is the 2nd x-axis always on, or only when effective support < some fraction of [0,1]?
    (Probably: show it only when the distribution's mass occupies notably less than the full
    range, so the common case stays a clean single [0,1] axis.)
  - Which distributions trigger it: trunc-normal (tight σ), and any kind whose effective support
    is a proper sub-interval. Uniform/triangular/power across the full range → primary only.

────────────────────────────────────────────────────────────────────────────────
All three are palette/editor polish. Priority order is probably 2 (cheap, adds a real copula
corner) → 3 (editor legibility) → 1 (nice control feel, most work). None gates warp.
