# CC FIX  DistributionCurveWidget rebuilds 200 points every frame (CPU)

Build on feat/phase-duals-gaps. Display-only fix; no engine or audio change.

## SYMPTOM
The standalone PhaseDistortion module and any panel hosting the curve plot sit at ~5-6% CPU even when the module is
DISCONNECTED and all parameters are at identity. CPU does not fall on disconnect.

## ROOT CAUSE (confirmed)
DistributionCurveWidget::draw() (src/ui/DistributionCurveWidget.hpp ~line 197-214) samples the EFFECTIVE warp curve at
kPlotN = 200 points on EVERY frame. Each point evaluates the warp mix, invCDF and density (warpEffectivePdf /
warpEffectivePdfDual). There is NO change-gate: a static curve costs the same as a moving one, and disconnection is
irrelevant because the plot is UI, not signal. The widget's own comment says the rebuild should happen "never per
sample ... on the hosts' refresh detectors", but the sampling is inside draw().

## FIX  cache the sampled curve; rebuild only on change
1. Move the 200-point sample loop OUT of draw() into a rebuildCurve() that fills cached arrays xs_[], cdf_[], pdfs_[].
2. draw() only STROKES the cached arrays (nvgMoveTo/LineTo over xs_/cdf_/pdfs_). No evaluation in draw().
3. Rebuild only when an input to the curve changed. Compute a cheap fingerprint each frame from the inputs the sample
   loop reads and rebuild when it differs:
   - the distribution identity / kind,
   - its parameters (the P1..WARP / gap / atom values the curve binds),
   - warpDepth (signed) and the reading (warpTDual),
   - the selected channel (if the plot follows a selection),
   - box size (geometry change).
   A fingerprint of those (e.g. a small FNV of the bound floats + ints) compared to the last rebuild's fingerprint is
   enough. This is the SweepSig pattern already used elsewhere  reuse that approach, do not invent a new one.
4. First draw (no cache yet) rebuilds once.

## APPLIES TO BOTH
The same widget backs the standalone PhaseDistortion plot, the SandsPhaseDistSubpanel plot, and the Sands Distribution
expander plot. Fixing DistributionCurveWidget fixes all of them. Confirm the Sands distribution plot CPU also drops
(it showed ~1.7% from the same cause).

## GATES
- Visual output IDENTICAL to before: the drawn curve is pixel-identical for any static state (compare a screenshot
  before/after at several warp depths and params).
- CPU: with the module disconnected and parameters static, the per-frame cost drops to the stroke-only path (expect the
  5-6% to fall to a fraction). Moving a parameter rebuilds once and settles.
- A parameter change, a warpDepth change, a channel-selection change, and a resize each trigger exactly one rebuild.
- No rebuild on frames where nothing changed (assert via a rebuild counter in a debug/test build).
- No engine or audio change; run_all green on both SDK legs.

## DO NOT
- Change the curve maths (warpEffectivePdf / dual, the sample formula). Only WHEN it is evaluated.
- Change the engine, the transform primitive, or the store.
- Add a timer-based throttle instead of a change-gate (a throttle still rebuilds a static curve; the fingerprint does not).
