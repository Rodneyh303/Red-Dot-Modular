# .dmcurve editor — build plan (the fun one)
A monotone CDF editor (edit the CDF directly; F^-1 derived) for the output-distribution mapping. See
docs/design/OUTPUT_LAYER_DISTRIBUTION_MAPPING.md for the feature it serves.

## v1 decisions (LOCKED, Rodney)
1. **Popup** (not own-module) — driven by LIVE PREVIEW co-visibility: the editor must be visible AT THE SAME
   TIME as the module's bars/visualiser so "drag curve -> bars redistribute" works. Lean NON-MODAL floating
   (patch keeps running -> see+hear preview live while dragging); MODAL is the simpler fallback if non-modal
   focus handling in VCV is painful. [sub-decision: non-modal vs modal — decide at build]
2. **Small number of points** to start; CLICK to add, DOUBLE-CLICK to remove. (No guided-insertion feature —
   "suggested actions" just meant these interaction verbs.)
3. **Pinned endpoints** (0,0)-(1,1) — true CDF, always valid. (Free endpoints = later option.)
4. **Live preview** — drag the curve -> the Sands bars / visualiser redistribute live (the uniform<->
   transformed KEYSTONE), ideally while the patch runs. This feature is WHY popup (co-visibility).
5. **Saves with the PATCH by default** (patch state). Shipped DEFAULTS + user saves + shared curves are ALL
   .dmcurve files (ONE format, one load path, open/studyable — users copy a shipped .dmcurve, modify, save).
   .dmcurve library export/import optional.

## FORMAT — JSON (consistent with .dmtune; Rodney)
.dmcurve is JSON. Rationale:
- Consistency with .dmtune (one house style; reuse the jansson plumbing already used for tunings).
- UNIFIES patch-state and file: curves save with the patch anyway (VCV module state = JSON via dataToJson),
  so .dmcurve = the SAME JSON object written to a file. ONE schema, ONE serializer, no translation between
  "in patch" and "in file". A custom text format would need two serializers for the same data.
- Open-ecosystem benefit fully preserved: JSON is open, documented, human-readable, trivially third-party-
  generatable (the differentiator vs closed BWCURVE was "open+documented", which JSON delivers).
- Native to VCV (patches are JSON) + robust (solved parsing, jansson already a dependency).

### v1 schema
```json
{
  "format": "dmcurve",
  "version": 1,
  "type": "drawn",            // drawn | triangular | power | logit-normal | kumaraswamy
  "interp": "pchip",
  "endpoints": "pinned",      // pinned | free
  "points": [[0,0],[0.3,0.1],[0.7,0.6],[1,1]],
  "params": {},               // parametric types: {a,b}|{mu,sigma}|{k}|{mode}; empty for drawn
  "name": "", "author": "", "description": ""   // optional library metadata
}
```
MonotoneCurve serialize/deserialize produce/consume this as a json_t* (jansson, like .dmtune); the module's
dataToJson/dataFromJson embed the SAME object for patch state. One schema, both destinations.

## PHASE 1 (do FIRST — pure, testable, de-risks everything): `MonotoneCurve` math module
No VCV, no nanovg — just MonotoneCurve.hpp/.cpp + test_monotone_curve.cpp.
Data:
- vector<Vec2> points, each (x,y) in [0,1], sorted by x, STRICTLY INCREASING in y with minimum slope
  epsilon ~1e-3 (y_{k+1} - y_k >= epsilon*(x_{k+1} - x_k)); endpoints pinned
  *** 2026-10-18: TIGHTENED from "monotone non-decreasing" — a flat segment is an ATOM, which breaks Sklar
  uniqueness, destroys rank info and gives stuck unisons. Drag handler CLAMPS against the epsilon floor
  (resistance at near-flat, not an error). See OUTPUT_LAYER_DISTRIBUTION_MAPPING.md EDIT section and
  SESSION_LOG_2026-10-18.md section 8. ***
  (0,0),(1,1); size ~2..32.
Core math:
- float evalCDF(float x)      — PCHIP (Fritsch-Carlson monotone cubic): segment-search (x sorted) + cubic.
  The edited object (the CDF).
- float evalInverse(float u)  — F^-1 (runtime transform) = reflect control points across diagonal (swap
  x<->y — still monotone/valid) + PCHIP the reflected set. Cache reflected curve; rebuild on edit.
- float pdf(float x)          — analytic SLOPE of the CDF (derivative of the PCHIP segment) — live density
  display.
Editing:
- setPoint(i,p)  — move, CLAMPED monotone (y in [left.y,right.y], x in [left.x,right.x]; endpoints' x pinned)
- addPoint(x)    — insert on the curve (y=evalCDF(x) so it lands ON the curve, no jump)
- removePoint(i) — not endpoints
Serialization:
- json_t* toJson() / bool fromJson(json_t*)  — the v1 schema above; VALIDATE monotonicity on load
  (reject/clamp non-monotone).
Unit tests (the safety that just proved its worth this session):
- monotone preserved under all edits; evalCDF(0)==0, evalCDF(1)==1; evalInverse(evalCDF(x))~=x (round-trip);
  pdf ~= numerical d/dx evalCDF; toJson->fromJson identical; non-monotone .dmcurve -> rejected/clamped.

## PHASE 2 — the widget (nanovg curve editor)
Draw CDF + draggable monotone points (small set to start) + live PDF overlay (slope = density) + optional
ghosted F^-1; hit-test/drag with monotone clamp; click-add / double-click-remove; pinned endpoints; light/
dark. Standalone-renderable for iteration.

## PHASE 3 — integration
Wire to lane distribution state (Macro-global / Sands-unlock); .dmcurve load/save + shipped default .dmcurve
files + library; LIVE PREVIEW (drag -> bars redistribute). Curve in patch = the JSON object (dataToJson);
.dmcurve file = same object.

## Build order: Phase 1 (math, pure, tested) -> Phase 2 (widget) -> Phase 3 (integration + live preview).

---

## Widget display refinements (Phase 2 iteration — Rodney, from first render)
First render is strong (CDF yellow + PDF teal fill + CDF/PDF labels, dark theme). Refinements:

### 1. SQUARE the diagram area
The curve is a [0,1]->[0,1] map — SAME unit both axes — so square is the CORRECT aspect, not just tidy:
- slope reads TRUE (PDF = slope of CDF; "steep = dense" only reads honestly at 1:1 aspect; a non-square box
  distorts it — current tall box exaggerates the PDF spike vertically).
- the DIAGONAL (0,0)->(1,1) = uniform; in a square it's a true 45deg line the eye reads instantly as
  "uniform baseline" (ties to the uniform<->transformed keystone).
- F^-1 (if ever shown) = reflection across the diagonal — clean mirror only in a square.
Freed panel height (square canvas in a tall panel) = space for controls/selector/toggle.
Add a faint ghosted DIAGONAL as the uniform reference line.

### 2. DUAL Y-AXIS — CDF and PDF are different scales (fixes the current PDF CLIPPING)
CDF in [0,1] (bounded); PDF in [0, inf) (density, unbounded). Sharing one y-axis clips the PDF (current bug)
or squashes the CDF. So:
- LHS y-axis = CDF, 0->1, ticked/labeled (yellow — colour-matched to the CDF curve).
- RHS y-axis = PDF density, 0 -> AUTO-MAX, ticked/labeled (teal — colour-matched to the PDF fill).
- X axis = 0->1 (the value/input), ticked.
The RHS scale is CHEAP: the fill is already auto-normalized to fit the box, so the max-density value is
ALREADY computed — the RHS axis just LABELS that auto-max (top = maxDensity, linear ticks to 0). "PDF is
hard" was really "FIXED/absolute PDF scaling is hard"; AUTO-NORMALIZED PDF + labeled RHS axis is easy and
fixes the clipping.
- Colour-match axis labels to curves so the eye knows which curve reads which scale (yellow LHS, teal RHS).
- PDF rescales LIVE as the curve is edited (auto-max follows the curve — honest; the density really grew).

### Later-if-needed (do NOT pre-build)
- Smooth-on-release rescale (fix RHS-label jumpiness during drag) — only if the live rescale feels jumpy.
- Spike display-CLAMP (near-vertical CDF -> density->huge -> auto-max dominates -> rest of PDF flattens
  invisibly): cap the spike at ~95% box height + mark clipped — ONLY if spikes prove to crush the rest in
  practice. Start with plain auto-normalize.

---

## Phase 2.5 additions (Rodney — after first full-size render; drag feel confirmed good)
Display + dual-axis + square all confirmed working (parametric Kumaraswamy AND drawn PCHIP both render —
the draw-against-Distribution design validated). Three additions, all devhost-isolated, all build on what
exists:

### 1. All distributions in the devhost selector (trivial)
Cycle the selector through ALL built-ins: triangular, power, kumaraswamy, logit-normal, arcsine,
truncated-normal + drawn PCHIP. One-line list addition. Lets Rodney eyeball all 6 parametric shapes render
(a visual cross-check on the math tests).

### 2. Parametric param editing — ~2 generic param knobs, relabeled per distribution, LIVE update
Parametric dists are edited by their PARAMS (knobs), not by dragging. Param counts: kumaraswamy(a,b),
logit-normal(mu,sigma), truncated-normal(mu,sigma), triangular(mode), power(k), arcsine(none). Max = 2.
- Two GENERIC param knobs ("Param 1 / Param 2") that REMAP + RELABEL per selected distribution (knob1=a/mu/
  mode/k; knob2=b/sigma/unused). Label shows the live meaning+value ("a: 2.0"). Arcsine -> both inert/hidden.
- LIVE: turning a knob reshapes the curve in real time (same live-preview magic as dragging, for parametric)
  -- turn "a", watch the Kumaraswamy morph.
- Simplest UI (2 knobs always, relabeled) vs context-sensitive appearing knobs (more machinery). Go simple.

### 3. "Convert to editable" — parametric -> NEARBY PCHIP (cheap, monotone-safe, high-value)
The start-from-a-shape-then-tweak workflow. NOT too much — it's ~10 lines + a button:
- MonotoneCurve::fromDistribution(const Distribution& d, int n=~9): sample d.cdf(x) at n x-values -> use as
  control points -> PCHIP interpolates. Monotone-safe for FREE (a valid CDF is monotone -> monotone points
  -> valid PCHIP). You already have addPoint-lands-on-curve; this is the batch version.
- "NEARBY", not exact: PCHIP through ~9 points ~= the parametric, not = it. That's the POINT — an editable
  starting shape close to the parametric, then drag to customize. ("I want something LIKE a Kumaraswamy but
  with a bump here" -> pick it -> Convert -> drag.)
- N: start even-spaced ~9; could go adaptive later (more points where CDF curves hard). Refine if the
  approximation loses shape.
- UNIFIES parametric + drawn: parametrics are the PRESETS; convert-to-PCHIP makes any preset a tweakable
  drawn curve. Also: shipped default .dmcurve files can BE converted-parametrics (editable starting points).
- UI: a "Convert to editable" button (devhost; Phase 3 = the real panel). On convert, the widget switches to
  the drawn/editable mode with the sampled curve.

### Small display nitpicks (optional, not urgent) from the full-size render
- Top-right "view" label collides with the RHS axis labels — nudge spacing.
- "view (edit off)" sidebar text slightly redundant with the "view" overlay — tidy.
- Gridlines faint against near-black — maybe nudge contrast. Taste.
