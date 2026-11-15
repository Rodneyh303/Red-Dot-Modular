# CC FIX  distribution plot ignores NEGATIVE warp depth (dual side draws the bare target)

Build on feat/phase-duals-gaps. Display-only. Engine is NOT changed.

## SYMPTOM (Rack, confirmed on screenshots)
Warp at 0 = diagonal. +0.2 = blended curve (correct). -0.01 = the FULL dual S-curve already. -1.0 = same full dual.
The plot never blends on the negative side, so it looks like a switch.

## ROOT CAUSE (confirmed in code)
DistributionCurveWidget.hpp:196:
    const bool effective = (warpDepth >= 0.f) && bool(warpT);
A negative depth fails the gate and falls through to the bare-target branch (dist->pdf). The widget therefore
ignores depth for the whole negative side.

## ROOT CAUSE 2 (spec gap, will bite the fix)
Spec section 10 gives f_W = 1/[(1-k) + k/f(T(u))]. That assumes T is a QUANTILE, so T'(u) = 1/f(T(u)).
On the DUAL reading T is a CDF (the composed member CDF g), so T'(u) = g'(u), the composed member density at u.
Correct dual formula:
    f_W = 1 / [ (1-k) + k * g'(u) ]
An atom makes g' a delta and a gap makes it zero at that point. If the host passes the dual's quantile density
into the quantile formula, the dual fill is wrong even after the gate is fixed.

## FIX
1. GATE. Remove the sign from the gate. effective = bool(warpT). The widget draws the effective curve whenever a
   target is supplied, for both signs. warpDepth keeps its signed value; the widget uses k = |w| via
   warpStages::warpEffectiveK (already sign-agnostic, WarpStages.hpp:286). Keep the bare-target branch only for
   "no target supplied" (the legacy path).
2. READING-AWARE TARGET. The host (SandsDistributionExpander.cpp, the warpT/warpTpdf setup near lines 411-415 and
   the mix note near 485) must supply, by sign:
   - w >= 0 (quantile reading): warpT(u) = member composed quantile Q(u);  warpTpdf = f(Q(u)) (the member density).
   - w <  0 (dual reading):     warpT(u) = member composed CDF g(u);       warpTpdf = g'(u) (the composed member density).
   The widget must not inspect the sign. It receives the two callables and the signed depth.
3. DUAL PDF FORMULA. Add a second display helper (or a flag on the existing one) for the dual reading:
       f_W = 1 / [ (1-k) + k * g'(u) ]
   with the same floor/guard as warpEffectivePdf so a gap (g' = 0) and an atom (g' -> inf) stay finite.
   Limits: k = 0 gives f_W = 1 (uniform). For the dual, g' = 0 over a gap, so f_W -> 1/(1-k) there, and an atom
   gives a delta; render the atom as the existing marker, not a spike that overflows the axis.
4. CDF TRACE. Unchanged in form: point (W(u), u) where W(u) = (1-k)u + k*T(u). For the dual, T(u) = g(u), the
   trace is parametric in u as before. No bisection, no integration.
5. Keep the gap/atom MARKERS on the dual exchanged, as the existing dualMarkers helper already does.

## GATES
- Sign sweep: for w in {-1.0, -0.5, -0.2, -0.05, -0.01, 0, +0.01, +0.05, +0.2, +0.5, +1.0}, the plotted curve
  changes continuously with w. At -0.01 the curve is near the diagonal, NOT the full dual.
- The effective CDF trace at each sampled w equals the engine blend (1-k)u + k*T(u) at the same u (test the
  widget's xs against warpedValue's dual branch, same k, same u). Positive and negative sides both pass.
- The effective PDF fill integrates to ~1 (trapezoid over the trace) for the test loadouts, both signs.
- k = 0 is the diagonal and f_W = 1 for both readings.
- No inf/NaN in the fill for any (member, tilt, gap, atom, w) in the test grid. Atom and gap render as markers.
- No change to engine output. Existing DistributionCurveWidget tests stay green.
- run_all green on both SDK legs.

## DO NOT
- Change the engine blend, the gap convention, the clamp, the atom composition, or the dual sign rule.
- Change the morph shape (uniform-centred is decided).
- Compute the CDF by bisection or integration. The parametric trace is exact.
