# Step 5 (scrub knob) -- status / open tuning

## DONE + working
- MIX knobs repurposed as the dice SCRUB position; ScrubKnobT widget (src/ui/Controls.hpp),
  Scrub_Small_Cog alias, bound to RHYTHM/MELODY_MIX in MonsoonWidget. Param range unchanged
  (0..1 scaled *6 internally) -- CV/persistence untouched.
- ScrubKnobT derives from rack::app::ParamWidget (NOT Knob/SvgKnob) and FULLY OWNS drag + SVG render.
  This was the key fix: on SvgKnob the base drag machinery drove the value and washed out the warp
  (felt linear no matter the strength). On ParamWidget our onDragMove is the only thing touching the
  param. Renders/turns correctly (Rodney confirmed).
- DEFAULT feel = magnetism: smootherstep-applied-twice warp toward the 7 draw detents (33x
  compression at a detent, ~46% "stuck zone" per cell). Continuous -- value never quantised, CV
  morphs, park anywhere. Confirmed felt in Rack.
- CTRL held = click-through: discrete stepping, one draw per STEP_PIXELS.

## OPEN (deferred by Rodney -- revisit after living with it)
- The DETENT FEEL is not dialed in. At MAGNET=0.9 + smootherstep^2 it feels "steppier" than Rodney
  wants; unsure what's right for this control. Tunables (all in ScrubKnobT):
    MAGNET      = 0.9   detent strength 0 (pure smooth) .. 1 (soft-snap). Lower = subtler.
    DRAG_PIXELS = 500   full-range drag travel (higher = slower/finer overall).
    the WARP SHAPE itself (smootherstep^2) -- could go back to single smoothstep (gentler) or a
    custom curve with a narrower stuck-zone if "steppy" is the complaint.
  Likely direction if "too steppy": lower MAGNET (~0.5-0.7) and/or single smoothstep, so the pull is
  a gentle settle rather than a near-freeze. Decide by feel in a real patch.
- Position INDICATORS at the 7 draws (tick marks / draw-offset readout) -- NOT built yet. Pairs with
  panel relayout. Helps land on draws in both smooth + click-through.
- Hard-snap on Shift -- still deferred (overlaps dice/last-dice gates).
