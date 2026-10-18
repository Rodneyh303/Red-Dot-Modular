# CC PROMPT P1 — palette/editor fixes: Kumaraswamy reparam + PDF x-axis autoscale

Standalone, parallel to the main warp work. Two independent sub-tasks; do them as two commits
on one branch. NEITHER depends on the warp Phase 3 prompts. Reversed-power (TODO #2) is NOT
here — it waits for the distmath:: refactor (CC_PROMPT_02 step 0), which doesn't exist yet.

## READ FIRST
1. `CLAUDE.md` (root) — non-negotiables (verify against tests; red→green; branch discipline).
2. `docs/design/CHAT_HANDOFF.md` — primer + working model.
3. `docs/design/TODO_distribution_palette_and_editor.md` — items #1 and #3 (the what-and-why).
4. `src/dsp/Distribution.hpp` (KumaraswamyDistribution ~the class) and
   `src/ui/DistributionCurveWidget.hpp` (the editor; PDF autoscale lives ~L141-191).

Branch: off `lane-expander-refactor`, `git checkout -b fix/palette-editor`. PR into it.
You CANNOT test Rack rendering — for the editor work, say "needs Rack feel-test" and lean on
the math tests for the parts that ARE testable.

────────────────────────────────────────────────────────────────────────────────
## SUB-TASK A — Kumaraswamy (mode, concentration) reparam: mapping on the class + tests
(UI wiring is NOT here — that pairs with the warp UI prompt when param knobs get built. This
task adds the TESTED MAPPING so it's ready and correct when the UI needs it.)

WHY (TODO #1): native (a,b) interact badly as controls (both pull mode AND spread).
(mode, concentration) orthogonalises them. Internal evaluation STAYS (a,b) — this adds a
conversion, it does not change cdf/pdf/invCDF or the .dmcurve schema.

ADD to Distribution.hpp, as static helpers on KumaraswamyDistribution (or free fns beside it):
    static void   abFromModeConc(double mode, double conc, double& a, double& b);
    static void   modeConcFromAb(double a, double b, double& mode, double& conc);
Kumaraswamy mode (for a>1, b>1) = ((a-1)/(a*b-1))^(1/a). Choose a concentration definition
that is monotone and invertible with mode held — a practical choice: conc ties to b (the
peakedness) with mode fixed, solving for a. Document the exact definition you pick in a comment;
what matters is that the two coords are (near-)orthogonal and the round-trip is exact.

EDGE REGIMES: a<=1 or b<=1 have no interior mode (monotone / J / U shapes). Handle explicitly:
define the mapping's behaviour there (e.g. clamp mode to the boundary, let concentration cover
the monotone family) rather than producing NaN. State the choice in the comment.

HARD CONSTRAINT: the (mode,conc)->(a,b) image MUST stay in the valid, STRICTLY-INCREASING
region (a,b>0; and the mapped curve must satisfy the editor's eps-monotonicity floor — a
concentration extreme that drives b->0 FLATTENS the curve = the atom case the editor forbids,
rank-destroying). Clamp in (mode,conc) space to the safe region. Do NOT emit (a,b) that the
editor would reject.

TEST `test/test_kumaraswamy_reparam.cpp` (header-only, model on test_distribution.cpp):
  - ROUND-TRIP: for a grid of valid (mode,conc): abFromModeConc then modeConcFromAb returns the
    input to tolerance. And (a,b)->(mode,conc)->(a,b) for a grid of valid (a,b).
  - MODE CORRECT: for mapped (a,b) with a,b>1, the analytic mode ((a-1)/(ab-1))^(1/a) equals the
    requested mode.
  - VALIDITY: every mapped (a,b) is strictly-increasing per the editor constraint (invCDF strictly
    increasing over a dense u grid; no flat segment within eps).
  - EDGE: a<=1/b<=1 inputs map to the documented boundary behaviour, no NaN/inf.
Register `"test_kumaraswamy_reparam|"` in test/run_all.sh TESTS[]. GREEN.

────────────────────────────────────────────────────────────────────────────────
## SUB-TASK B — PDF x-axis autoscale (2nd x-axis) for open-support types

WHY (TODO #3): all types output [0,1], but LOGIT-NORMAL and LOGIT-SIGMOID have OPEN SUPPORT —
density approaches the 0/1 edges asymptotically, so tight params cram nearly all visible density
into a central sliver (e.g. [0.45,0.55]). On the fixed 0->1 x-axis that's a thin useless spike.
The y-axis already autoscales (DistributionCurveWidget.hpp ~L141-191: sample pdf at N+1 pts,
maxD over the OPEN interval excluding endpoints, normalize fill to maxD). Do the SAME for x.

THE FIX — a second x-range for the PDF, auto-ranged to where the visible density lives:
- After sampling pdfs[] (the existing loop ~L145-149), compute an x-WINDOW [xLo,xHi] capturing
  the visible mass: e.g. the smallest central span where pdf >= some fraction (say 1%) of maxD,
  or the ~[0.5%, 99.5%] CDF quantile span (dist->invCDF is available — cheaper + robust). Pick
  one; document it.
- The PDF overlay + its RHS teal axis then plot against [xLo,xHi] instead of [0,1]: i.e. the PDF
  fill's x maps through [xLo,xHi]->pixels, and a SECOND x-axis (ticked/labelled, teal, matched to
  the PDF) shows that zoomed range. The CDF keeps its TRUE 0->1 x-axis (the main/bottom axis,
  gold) — CDF value semantics are unchanged.
- Only engage the zoom when it helps: if [xLo,xHi] is ~[0,1] already (bounded types: power,
  mobius, triangular, ordinary kumaraswamy), DON'T draw a second x-axis — leave the single 0->1.
  The zoom appears only when the density is actually concentrated (open-support tight params).
- Follow the existing autoscale discipline (the comment at DistributionCurveWidget.hpp:29:
  "smooth-on-release rescale, and a spike display-clamp") — the x-range rescales smoothly on
  release, not jittering live per drag frame.

DISPLAY-ONLY: stored curve, params, math, .dmcurve schema ALL unchanged. CDF x stays 0->1.
No Distribution.hpp change, no engine change — only DistributionCurveWidget.hpp + its axis draw.

TESTABLE PART: factor the x-window computation into a pure free fn
(e.g. `pdfVisibleXRange(const Distribution&, double fracOrQuantile, double& xLo, double& xHi)`)
and test it header-only: logit-normal(mu=0.5,sigma=0.1) returns a narrow central [xLo,xHi];
a bounded type (triangular, power ordinary param) returns ~[0,1]; monotone/degenerate returns
sane bounds, no NaN. Register the test. The RENDERING (second axis drawn correctly) you CANNOT
verify — say "needs Rack feel-test" in the PR.

────────────────────────────────────────────────────────────────────────────────
## DONE =
- A: abFromModeConc/modeConcFromAb on KumaraswamyDistribution; internal (a,b) + schema
  unchanged; test_kumaraswamy_reparam green (round-trip, mode-correct, validity, edge).
- B: pdfVisibleXRange pure fn + its test green; the widget draws a 2nd PDF x-axis when density
  is concentrated, single 0->1 otherwise; CDF x untouched; smooth-on-release.
- `bash test/run_all.sh` ALL GREEN. No warp/engine/schema changes. Two clean commits.
- PR body: paste both tests' output; for B, state clearly that the rendering needs Rack feel-test.

## NOT THIS PROMPT
- Reversed-power distribution (TODO #2) — waits for the distmath:: refactor (CC_PROMPT_02 step 0).
- The Kumaraswamy UI knobs/labels — pair with the warp UI prompt (CC_PROMPT_04).
- Any warp / mirror / w / modulation code.
