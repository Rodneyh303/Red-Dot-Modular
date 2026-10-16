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
- vector<Vec2> points, each (x,y) in [0,1], sorted by x, MONOTONE non-decreasing in y; endpoints pinned
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
