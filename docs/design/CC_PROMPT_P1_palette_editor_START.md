# CC PROMPT P1 — palette/editor: reversed-power + Kumaraswamy reparam + PDF y-max edge-clip fix

Standalone, parallel to the main warp work. THREE independent sub-tasks; do them as three
commits on one branch. NONE depends on the warp Phase 3 prompts. (The old "distmath:: refactor"
gating on reversed-power is GONE — there is no distmath layer; see CC_PROMPT_02, which uses a
value-variant with single-implementation concrete types. Reversed-power is just a new
Distribution.hpp class, added here like any other palette type.)

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
    static void abFromModeConc(double mode, double conc, double& a, double& b);
    static void modeConcFromAb(double a, double b, double& mode, double& conc);

### USE THIS EXACT DEFINITION (decided — do NOT re-derive or search for a "better" one):
**conc := a** (the left shape exponent), and b is SOLVED CLOSED-FORM to hold the mode. This has
an exact inverse BOTH ways (no root-finding), which conc:=b does NOT (it's transcendental in a).
Forward (abFromModeConc), for mode m ∈ (0,1), conc c ≥ 1:
    a = c
    if (c == 1) { a = 1; b = 1; return; }          // uniform; mode irrelevant (documented)
    b = (a - 1 + m^a) / (a * m^a)                   // holds mode exactly; derivation below
Inverse (modeConcFromAb), for a,b (interior-mode case a>1,b>1):
    conc = a
    mode = ((a - 1) / (a*b - 1))^(1/a)
Derivation (put in the comment): mode^a = (a-1)/(ab-1)  ⟹  b = (a-1+m^a)/(a·m^a). And one can
show b-1 = (a-1)(1-m^a)/(a·m^a) > 0 for a>1, m<1, so this always lands in the a>1,b>1 unimodal
region. Orthogonality is EXACT in the sense that matters for a UI: the conc knob never moves the
mode (b is solved to hold it), and the mode knob never moves the mode (trivially). That is all
"(near-)orthogonal" needs to mean here.

### RANGES + CLAMPS (fixed numbers — stop here, do not optimise):
    conc ∈ [1, 100]   (CONC_MAX = 100; 1 = uniform, up = more peaked; clamp inputs to this)
    mode ∈ [0.02, 0.98]  (MODE_EPS = 0.02; clamp the mode INPUT to this before computing)
The mode clamp at 0.02 (NOT 1e-3) is deliberate and is the WHOLE answer to the float-flatness
worry below — a mode of 0.02 with conc 100 gives a large-but-finite b whose CDF is still
strictly increasing across a 1000-pt grid in double. Wider isn't needed; tighter re-opens the
problem. Just use 0.02.

### THE VALIDITY REQUIREMENT IS LOOSER THAN THE OLD PROMPT IMPLIED — read this:
The PARAMETRIC distribution is NOT the editor's edited object and does NOT face the editor's
MIN_SPACING / eps-monotonicity gate. (Verified: MonotoneCurve.hpp:82-92 — a parametric only
meets that gate IF the user hits "convert to editable", which SAMPLES the CDF at 9 points; until
then it's evaluated by its exact analytic invCDF, which is strictly increasing for ALL a,b>0.)
So you do NOT need to prove a safe region or guard against the editor rejecting the curve. The
only real requirement is: **the mapping returns finite a,b>0 and never NaN/inf over the clamped
input box.** The clamps above guarantee that. Do NOT derive transcendental safe-region bounds —
that rabbit hole is why this section was rewritten.

### EDGE / DEGENERATE (just handle these three, no analysis):
  - conc == 1 (any mode)  → (a,b) = (1,1), uniform. modeConcFromAb(1,1): the mode formula is
    0/0 → return mode = 0.5, conc = 1 (documented sentinel).
  - modeConcFromAb with a<=1 OR b<=1 (no interior mode — a J/U/monotone shape that did NOT come
    from this forward map): return conc = clamp(a,1,100), mode = (density decreasing ? 0.0 : 1.0)
    i.e. clamp mode to whichever boundary the mass sits against. This path only matters for
    curves authored directly in (a,b); the forward map never produces it.
  - mode input outside [0.02,0.98] or conc outside [1,100] → clamp to the box, then proceed.

TEST `test/test_kumaraswamy_reparam.cpp` (header-only, model on test_distribution.cpp):
  - ROUND-TRIP: grid of (mode ∈ [0.05,0.95] step 0.05) × (conc ∈ [1.5,100] a few values):
    abFromModeConc then modeConcFromAb returns the input to ~1e-9. (Stay off the 0.02/0.98/1.0
    boundaries for the exact round-trip; those are the clamped/sentinel cases, tested separately.)
  - MODE CORRECT: for the mapped (a,b), the analytic mode ((a-1)/(ab-1))^(1/a) == the requested
    mode to ~1e-9. (This is the orthogonality guarantee.)
  - FINITE + VALID: every mapped (a,b) over the full clamped box is finite, a>0, b>0, and the
    Kumaraswamy invCDF is strictly increasing over a 1000-pt u grid in double (it will be — this
    asserts the clamps are sufficient, NOT a tight bound).
  - EDGE: conc=1 → (1,1); modeConcFromAb(1,1) → (mode 0.5, conc 1); an a<=1 / b<=1 input →
    documented boundary, no NaN/inf.
Register `"test_kumaraswamy_reparam|"` in test/run_all.sh TESTS[]. GREEN.

TIME-BOX: this is ~40 lines of code + a test. If you find yourself deriving float-saturation
thresholds or transcendental region boundaries, STOP — the clamps above already handle it; that
analysis is not wanted and is why this was rewritten.

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
## SUB-TASK C — add ReversedPowerDistribution to the palette

WHY (TODO #2): F(x)=1-(1-x)^theta is the MIRROR of Power (x^theta) — frailty in -log(1-x), the
upper-tail counterpart to Power's lower-tail. Fills the one empty palette corner; it's the only
marginal form that, at r=0 with shared-theta modulation, yields the genuine Joe copula (vs
Power→Clayton). Cheap closed-form inverse, one param, bounded density away from the edge.

ADD to src/dsp/Distribution.hpp — a new concrete class, same shape as PowerDistribution
(verified formulas, mirror of Power's x<->1-x):
    class ReversedPowerDistribution : public Distribution {
    public:
        double theta;   // > 0 (validated on load)
        explicit ReversedPowerDistribution(double t) : theta(t) {}
        const char* typeName() const override { return "reversed-power"; }
        double cdf(double x) const override {
            x = distClamp01(x);
            return 1.0 - std::pow(1.0 - x, theta);
        }
        double pdf(double x) const override {
            x = distClamp01(x);
            double tail = 1.0 - x;
            if (tail < DIST_EDGE_EPS) tail = DIST_EDGE_EPS;   // divergent at x=1 for theta<1
            return theta * std::pow(tail, theta - 1.0);
        }
        double invCDF(double u) const override {
            u = distClamp01(u);
            return 1.0 - std::pow(1.0 - u, 1.0 / theta);
        }
        json_t* toJson() const override {
            json_t* p = json_object();
            json_object_set_new(p, "theta", json_real(theta));
            return paramEnvelope(p);
        }
    };

WIRE IT IN (the standard new-type checklist — grep "power" / PowerDistribution to find each site):
  - the type-tag dispatch in fromJson (add "reversed-power" → ReversedPowerDistribution).
  - the palette list comment (WARPS section) + the type-tag list comment.
  - the devhost (so it's selectable/feel-testable).
  - MonotoneCurve::fromDistribution coverage (convert-to-editable).

TEST: add reversed-power to the existing test_distribution.cpp battery — the same contract
checks every type gets (cdf/pdf/invCDF consistency: cdf(invCDF(u))≈u; pdf≈d/dx cdf numerically;
monotone invCDF; edge-floor finite for theta<1) + a JSON round-trip (toJson→fromJson preserves
theta). No new test file needed; it's a palette member like the others. Run_all green.

────────────────────────────────────────────────────────────────────────────────
## DONE =
- A: abFromModeConc/modeConcFromAb on KumaraswamyDistribution; internal (a,b) + schema
  unchanged; test_kumaraswamy_reparam green (round-trip, mode-correct, validity, edge).
- B: pdfVisibleYMax pure fn + its test green; finite edge-maxima now define the PDF scale (no
  flat-top clip on trunc-normal) while divergent edges stay excluded; X axis stays single 0->1.
- C: ReversedPowerDistribution in Distribution.hpp, wired (fromJson tag, palette comments,
  devhost, fromDistribution); test_distribution battery + JSON round-trip cover it, green.
- `bash test/run_all.sh` ALL GREEN. No warp/engine changes (C adds a palette type only; no
  schema break — it's a new optional type tag). Three clean commits.
- PR body: paste tests' output; for B, state clearly that the rendering needs Rack feel-test.

## NOT THIS PROMPT
- The Kumaraswamy UI knobs/labels — pair with the warp UI prompt (CC_PROMPT_04).
- Any warp / mirror / w / modulation engine code.
- The CDF rendering (it is correct — see the "NOT A BUG" note in sub-task B).
