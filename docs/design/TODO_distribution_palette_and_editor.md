# TODO — distribution palette + editor refinements (from 2026-10-18b design session)

Three items. None blocks warp Phase 3 (prompts 01-04); all improve the distribution layer +
editor. Detail/reasoning: SESSION_LOG_2026-10-18b.md (copula sections) + this note.

────────────────────────────────────────────────────────────────────────────────
## 1. Kumaraswamy reparameterisation — UI PRESENTATION ONLY (internal stays (a,b))

WHY: Kumaraswamy F(x)=1-(1-x^a)^b. The native (a,b) are a POOR control basis — both knobs
pull mode AND spread, so they interact badly (turning a to move the peak also moves the
width). Reparameterise the CONTROL to orthogonalised coords: (mode, concentration) — one knob
moves location, the other moves shape, independently. Better feel.

BONUS (the reason it's more than cosmetics): this basis also makes the two params MEAN the two
common-factor modulation types from the copula work — shared-MODE modulation = location
co-movement factor; shared-CONCENTRATION modulation = dispersion co-movement factor. So the two
param-mod slots become legible as the two factor kinds. (Also: b is the FRAILTY param —
multiplicative in the hazard of 1-x^a, so shared-b modulation at r=0 gives a closed-form
Archimedean generator; a is not. The (mode,concentration) basis sits cleanly over this.)

SCOPE:
- UI/authoring ONLY. Internal evaluation stays native (a,b) — the distmath:: free fns and the
  .dmcurve `params{}` keep (a,b). Author in (mode,concentration), evaluate in (a,b).
- ADD THE MAPPING to the Distribution class (Rodney): a (mode,concentration)->(a,b) converter
  (and its inverse) as a static helper on KumaraswamyDistribution (or a free fn beside it), so
  the editor/UI calls one canonical map rather than reinventing it. Mode of Kumaraswamy =
  ((a-1)/(ab-1))^(1/a) for a,b>1; handle the a<=1 / b<=1 edge regimes explicitly.
- CONSTRAINT: the inverse map MUST land in the valid+strict-increase region (a,b>0, and respect
  the editor's eps-monotonicity floor). A concentration extreme can push b->0 and FLATTEN the
  curve = the atom case (rank-destroying) the editor forbids. So clamp in the new coords to the
  same safe region the raw params would. Add a param-validity test alongside the distribution
  battery (round-trip (mode,conc)->(a,b)->(mode,conc), and assert the mapped (a,b) stays
  strictly-increasing per the editor constraint).

────────────────────────────────────────────────────────────────────────────────
## 2. ADD "reversed-power" distribution to the palette

WHY: F(x)=1-(1-x)^theta is the MIRROR of Power (x^theta). It's frailty in -log(1-x), and it's
the ONLY marginal form in the palette that, at r=0 with shared-theta modulation, yields the
genuine JOE copula (and the BB families) — the mirror corner to Power's Clayton. Cheap inverse
(F^-1(u) = 1-(1-u)^(1/theta)), bounded-density away from the edge, one param. Fills the
upper-tail-frailty corner Power (lower-tail) leaves empty.

SCOPE:
- New concrete type in Distribution.hpp: ReversedPowerDistribution(theta), typeName
  "reversed-power", with cdf/pdf/invCDF as distmath:: free fns (shared-math pattern from
  CC_PROMPT_02). Edge-density floor like Power (DIST_EDGE_EPS) for theta<1.
- Add to the palette list comment, the type-tag list, the devhost, the test_distribution
  battery (contract + json round-trip), and MonotoneCurve::fromDistribution coverage.
- Straightforward palette addition — NOW IN CC_PROMPT_P1 sub-task C (no distmath gating; there
  is no distmath layer — CC_PROMPT_02 uses a value-variant with single-impl concrete types).

────────────────────────────────────────────────────────────────────────────────
## 3. PDF display scaling — editor bug  [CORRECTED 2026-10-18b: it's a Y-MAX fix, not X-axis]

CORRECTION: the original '2nd x-axis' framing was WRONG — the PDF never leaves [0,1] on x.
The observed problem (trunc-normal mu+0.48 sigma0.50: flat-topped clipped teal dome) is the
PDF Y-MAX excluding real FINITE edge-maxima. maxD is taken over the OPEN interval (correct for
DIVERGENT edges: arcsine/power k<1), but trunc-normal has a real finite edge max that then
clips flat. FIX: include finite edge-maxima in maxD, still exclude divergent ones. Display-only,
single 0->1 x-axis kept. Full spec: CC_PROMPT_P1_palette_editor_START.md sub-task B.

[original x-axis text below, superseded — kept for context]
## 3-OLD. PDF X-AXIS autoscale (2nd x-axis) — editor display bug

WHAT WE HAVE: dual Y-axis is done (DMCURVE_EDITOR §2) — LHS CDF 0->1, RHS PDF 0->AUTO-MAX
(the y-autoscale we added). X axis is fixed 0->1.

THE GAP: some distributions have OPEN SUPPORT — density approaches the 0/1 edges
asymptotically, so for tight params nearly ALL the visible density is crammed into a narrow
central sliver. On a fixed 0->1 x-axis that renders as a thin useless spike. Affected types:
LOGIT-NORMAL (mu,sigma) and LOGIT-SIGMOID (a,b) — "open support, zero edges" in the palette
notes; tight sigma / heavy params concentrate mass in e.g. [0.45,0.55]. (TruncatedNormal is
bounded-bell with non-zero edges — less acute but can also benefit. Power/Mobius/Triangular/
Kumaraswamy with ordinary params are fine on 0->1.)

THE FIX: a SECOND X-AXIS for the PDF, auto-ranged to where the visible density actually lives —
the PDF x-domain autoscales to the central mass (e.g. the [q_lo, q_hi] span capturing ~99% of
density, or where pdf > some fraction of pdf-max), ticked/labelled, mirroring how the RHS PDF
y-axis already autoscales. The CDF keeps its true 0->1 x-axis (LHS); the PDF reads against the
zoomed x-range (RHS/2nd x-axis). Same two-scales-one-plot pattern as the dual Y-axis, applied
to X.
- Follow the existing y-autoscale discipline (DMCURVE_EDITOR §: smooth-on-release rescale + a
  spike display-clamp) so the x-rescale doesn't jitter live while dragging.
- Keep it DISPLAY-only: the stored curve + the math are unchanged; this is purely how the PDF
  is framed for the eye. The CDF/value semantics (0->1) are untouched.
- Belongs in the editor widget (src/ui/DistributionCurveWidget.hpp) + its axis-draw; no engine
  or Distribution.hpp change.

────────────────────────────────────────────────────────────────────────────────
## Sequencing
- #2 (reversed-power) — DONE as CC_PROMPT_P1 sub-task C (a plain Distribution.hpp class).
- #1 (Kumaraswamy reparam) any time; the mapping on the Distribution class is small + testable;
  the UI wiring pairs naturally with the warp UI work (prompt 04) since that's when the param
  knobs/labels get built.
- #3 (x-axis autoscale) any time — self-contained editor-widget fix, independent of warp.
