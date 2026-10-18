# CC PROMPT P1 — palette/editor fixes: Kumaraswamy reparam + PDF y-max edge-clip fix

Standalone, parallel to the main warp work. Two independent sub-tasks; do them as two commits
on one branch. NEITHER depends on the warp Phase 3 prompts. Reversed-power (TODO #2) is NOT
here — it waits for the distmath:: refactor (CC_PROMPT_02 step 0), which doesn't exist yet.

## READ FIRST
1. `CLAUDE.md` (root) — non-negotiables (verify against tests; red→green; branch discipline).
2. `docs/design/CHAT_HANDOFF.md` — primer + working model.
3. `docs/design/TODO_distribution_palette_and_editor.md` — items #1 and #3 (the what-and-why).
4. `src/dsp/Distribution.hpp` (KumaraswamyDistribution + TruncatedNormalDistribution classes)
   and `src/ui/DistributionCurveWidget.hpp` (the editor; PDF scaling ~L141-191).

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
## SUB-TASK B — fix the PDF y-max so edge-heavy densities don't clip

(This is a Y-AXIS fix. The PDF never leaves [0,1] on X — there is NO x-axis change. An earlier
"2nd x-axis" idea was a misdiagnosis and is dropped.)

THE SYMPTOM (observed in Rack): truncated-normal with a wide sigma (e.g. mu +0.48, sigma 0.50
or 0.67) draws a FLAT-TOPPED teal PDF dome — clipped against the ceiling instead of reaching a
rounded peak. (The CDF in the same view looks like it has near-vertical "atom" jumps at the
corners — it does NOT; see the note at the end of this sub-task.)

THE BUG (DistributionCurveWidget.hpp ~L145-191): maxD is taken over the OPEN interval —
`if (i > 0 && i < N && pdfs[i] > maxD)` (L149) excludes the endpoints — then the fill clamps
`if (d > 1.0) d = 1.0` (L184). Excluding endpoints is CORRECT for DIVERGENT densities (arcsine,
power k<1, Kumaraswamy a<1/b<1: edge→∞, a finite floored proxy must NOT define the scale). It is
WRONG for BOUNDED densities whose true max is AT or NEAR an endpoint — TRUNCATED-NORMAL
especially ("bounded bell, NON-ZERO edge density"): a wide sigma puts the real peak near/outside
[0,1], so the genuine maximum is at the edge. maxD excludes it → the fill normalises to a
too-small interior max → the edge regions clip flat. That flat top is the bug.

THE FIX — distinguish "divergent edge" from "high FINITE edge":
- Divergent edge → keep excluding it (the proxy is not a real max).
- Finite edge that is a real maximum → INCLUDE it in maxD so the fill doesn't clip.
Robust discrimination: for each endpoint, compare pdf(edge) to pdf(just inside, e.g. 1/(2N)).
If the endpoint is finite and comparable to its neighbour (not exploding — not >> neighbour and
below a divergence threshold), it's a REAL max → count it. If it's blowing up → divergent →
exclude as now. (Alternative, simpler: cap every sample at a sane multiple — say 8–10× — of the
INTERIOR max before taking maxD; a divergent edge is capped to "tall but finite" and counts
partially, a finite edge counts fully. Pick whichever is cleaner; document it.)
Net: trunc-normal's edge-max defines the scale → the dome reaches its true peak, nothing clips;
power(k<1) still scales to its interior so the divergent spike doesn't flatten everything else.

DISPLAY-ONLY: stored curve, params, math, schema ALL unchanged. X axis stays a single true
0->1 (CDF gold, bottom). Only the PDF Y normalisation changes. DistributionCurveWidget.hpp only;
no Distribution.hpp / engine change.

TESTABLE: factor the max-finding into a pure free fn
`double pdfVisibleYMax(const Distribution&, int N=200)` and test it header-only:
  - trunc-normal(mu=0.48, sigma=0.50): the returned max INCLUDES the edge density (assert it
    equals the actual max over the CLOSED [0,1] sampling — the edge is not excluded).
  - power(k=0.5): the returned max is the INTERIOR-bounded one (the divergent 0-edge does NOT
    blow the scale — assert finite and ~ the interior, not the floored edge proxy).
  - triangular / ordinary kumaraswamy: unchanged, interior peak defines it.
Register `"test_pdf_ymax|"` in run_all.sh. GREEN. The RENDERING (dome no longer clipped) you
CANNOT verify — say "needs Rack feel-test" in the PR; the pure-fn test proves the scale logic.

NOT A BUG — do NOT "fix" the CDF: with the same edge-heavy params the CDF looks like it has
ATOMS (near-vertical segments at x≈0 and x≈1). It does NOT. CDF slope = density; high edge
density → steep (near-vertical) CDF corners that READ as jumps but are continuous. Verified:
TruncatedNormalDistribution::cdf is correctly renormalised ((Phi((x-mu)/sigma)-Pa)/Z) and pinned
to exactly 0 at x<=0 and 1 at x>=1 — the curve DOES span 0->1; the 0->~0.2 climb just happens in
a sub-pixel x-sliver so only the vertical shows. SAME ROOT as the dome clip. Once the y-max fix
lands, the tall-PDF-at-edges will visually EXPLAIN the steep CDF corners. The editor's strict-
increase constraint guarantees no TRUE atoms exist, so a near-vertical CDF segment is always high
density, never a jump. (Optional: a one-line widget comment to that effect. Not required.)

────────────────────────────────────────────────────────────────────────────────
## DONE =
- A: abFromModeConc/modeConcFromAb on KumaraswamyDistribution; internal (a,b) + schema
  unchanged; test_kumaraswamy_reparam green (round-trip, mode-correct, validity, edge).
- B: pdfVisibleYMax pure fn + its test green; finite edge-maxima now define the PDF scale (no
  flat-top clip on trunc-normal) while divergent edges stay excluded; X axis stays single 0->1.
- `bash test/run_all.sh` ALL GREEN. No warp/engine/schema changes. Two clean commits.
- PR body: paste both tests' output; for B, state clearly that the rendering needs Rack feel-test.

## NOT THIS PROMPT
- Reversed-power distribution (TODO #2) — waits for the distmath:: refactor (CC_PROMPT_02 step 0).
- The Kumaraswamy UI knobs/labels — pair with the warp UI prompt (CC_PROMPT_04).
- Any warp / mirror / w / modulation code.
- The CDF rendering (it is correct — see the "NOT A BUG" note in sub-task B).
