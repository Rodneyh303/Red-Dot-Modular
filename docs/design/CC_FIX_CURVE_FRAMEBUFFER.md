# CC FIX  DistributionCurveWidget re-fills every frame; wrap in a FramebufferWidget

Build on feat/phase-duals-gaps. Display-only; no engine or audio change.

## EVIDENCE
- VTune bottom-up: top hotspot ~7.4s in libRack.dll resolving to nvgFill <- rack::window::svgDraw. The cost is
  drawing (fills), not DSP.
- Deleting the phase distortion expander drops Monsoon CPU from 2.3% to 0.5%. So Monsoon's extra cost is the plot
  going LIVE when the expander resolves, and it is per-frame fill.
- The earlier fix (CC_FIX_CURVE_WIDGET_CPU) cached the 200-point SAMPLE SWEEP but draw() still FILLS every frame:
  DistributionCurveWidget::draw (src/ui/DistributionCurveWidget.hpp:303) does nvgFill for the bezel (320), the well
  (324), and the PDF area (391), plus the grid/CDF strokes, on EVERY frame. Caching the samples did not stop the fill.

## FIX  framebuffer the static plot
Wrap the curve render in a rack::widget::FramebufferWidget so the fills run ONLY when the cached curve changed, not
every frame. StoreBound.hpp already uses FramebufferWidget  copy that pattern.
- Move the bezel/well/grid/CDF/PDF draw into a child widget (the "curve layer") hosted by a FramebufferWidget.
- The framebuffer re-renders only when marked dirty. Mark it dirty from the SAME change-gate the sample cache already
  uses (the CurveSig fingerprint, DistributionCurveWidget.hpp:88): when captureCurveSig differs from the last, call
  fb->setDirty() (and rebuild the sample cache as now). When the fingerprint is unchanged, the framebuffer is reused
  with ZERO nvgFill.
- The LIVE overlays that must track per frame (the input-phase marker, the editCurve drag points, the "view" readout)
  stay OUTSIDE the framebuffer, drawn over it each frame. Only the static curve/fill/grid goes in the framebuffer.
- Box resize marks the framebuffer dirty (geometry change), as the sample cache already treats it.

## APPLIES TO ALL HOSTS
The same widget backs the phase subpanel plot, the standalone PhaseDistortion plot, and the Sands Distribution plot.
One framebuffer fix covers all three. Confirm each host's CPU drops with a static curve.

## GATES
- Visual output pixel-identical to before for any static state (screenshot compare at several warp depths/params).
- Monsoon CPU with a phase expander present and a static curve returns to ~0.5% (the no-expander baseline), i.e. the
  per-frame nvgFill is gone when nothing changed.
- A parameter / warp / channel change re-renders the framebuffer exactly once, then it settles.
- Live overlays (phase marker, drag points) still update every frame (they are outside the framebuffer).
- No engine or audio change; run_all green on both SDK legs.

## DO NOT
- Change the curve maths or the sample cache (keep the CC_FIX_CURVE_WIDGET_CPU fingerprint; this fix ADDS the
  framebuffer on top of it).
- Put the live overlays inside the framebuffer (they would freeze).
- Introduce a timed repaint; the dirty flag is driven by the fingerprint, not a timer.
