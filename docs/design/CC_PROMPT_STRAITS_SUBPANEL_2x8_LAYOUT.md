# CC PROMPT — try Straits lane-expander 2×8 layout with smaller knobs (branch experiment)

CONTEXT: current Straits lane expanders use Themed_Compact_Cog_Dim knobs in a 3-column layout (6+6+4 = 16
voices). We want to try a NARROWER layout: 2 columns of 8 voices using the NEXT SIZE DOWN knobs.

## THE CHANGE
In StraitsLaneExpander.cpp/.hpp, swap the knob type from:
    Themed_Compact_Cog_Dim
to:
    Themed_Small_Cog_Dim

And reflow the grid to 2 COLUMNS × 8 ROWS (voice 0=mono mirror at top, voices 1-15 poly, filling
column-major or row-major — whichever reads best with the voice numbering).

## PANEL ARTWORK
The panel waves/background SVG will need adjustment for the narrower width. Regenerate from
gen_cantonment.py-style generator (or hand-edit the SVG) to fit the 2×8 grid in a tighter panel. The
panel should still carry the lane's colour accent and the StraitsSeamFiller edge matching.

## DO THIS ON A BRANCH
Branch off lane-expander-refactor (e.g. feat/straits-2x8-layout). This is an EXPERIMENT — we want to see
if the smaller knobs are usable and the narrower panel works visually before committing. Keep the current
3-col layout on trunk untouched.

## DONE =
- Lane expanders use Themed_Small_Cog_Dim in a 2×8 grid.
- Panel artwork adjusted for the narrower width (waves, seam fillers, colour accent).
- Resnap hardening still works (the resnap is position-based, not size-dependent).
- Screenshot of the result for Rodney to evaluate.
- On a branch, not trunk.

## NOT THIS PROMPT
- New Straits knobs (subgate thresholds, atom params). New lanes. This is purely the LAYOUT experiment.
