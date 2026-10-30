# CC PROMPT — FIX subgate panels + module structure (DECIDED — build, do not ask)

Two errors to fix together. DECISION IS MADE (option a below) — there is NO question to raise; BUILD IT.

## THE TWO ERRORS
1. PANEL REINVENTED: the subgate panels were made as a NEW, DIFFERENT panel (180.0 wide, 154 circles, new
   gen_straits_subgate_panel.py) instead of the SETTLED 6/6/4, 165.0-wide, col-major, v0=mono lane layout.
2. WRONG AXIS (the cram): CC made per-LEVEL panels (Ratchet, Ghost), each cramming 16 voices × MULTIPLE
   decisions (rest+legato+accent+advance) onto ONE panel. The established pattern is 16 voices × ONE
   decision per panel (one StraitsLane = one lane = one bank of 16). Organize by DECISION, not by level.

## THE DECISION (option a — final)
One 6/6/4, 165.0-wide per-voice panel for EACH (subgate level × decision):
  {RATCHET, GHOST} × {REST, LEGATO, ACCENT, ADVANCE}  → the full set, each its OWN panel + OWN module.
Each panel = 16 voices (col-major, v0=mono top-left), ONE decision, generated from gen_straits_lane.py via
a tint key — identical layout to a StraitsLane expander. Each is its own attachable module (like each
StraitsLane* is), snapping per the fixed Straits snap invariants (stable owned-ID list + unconditional
reposition). NO crammed multi-decision panel, NO new layout, NO 180-wide anything.

## THE ESTABLISHED GENERATOR (reuse, do not reinvent)
panel_src/gen_straits_lane.py: COLS=[6,6,4] (line 33), col-major v0=mono (line 63), width 165.0, TINTS dict
(line 23) parameterises tint/wave/knob/tinttext, def gen(tintkey, dark=True). Every Straits lane panel is
made this way. Add subgate tint keys and call gen() — the grid, dims, wave field, seam fillers all come
UNCHANGED.

## THE FIX STEPS
1. DELETE: panel_src/gen_straits_subgate_panel.py; the 180-wide res/panels/StraitsSubgatePanel_*.svg;
   gen_straits_subgate_base.py IF it diverges from gen_straits_base.py (the subgate base must reuse
   gen_straits_base.py's pattern, same as the lane base).
2. MODULE STRUCTURE: restructure so each subgate per-voice panel module = ONE (level, decision)'s 16
   voices — 8 modules total ({RATCHET,GHOST}×{REST,LEGATO,ACCENT,ADVANCE}), each shaped like a StraitsLane
   expander module. (Replaces the two cram modules modelStraitsSubgatePanel{Ratchet,Ghost}.)
3. PANELS: add 8 tint keys to gen_straits_lane.py's TINTS (one per level×decision, colours consistent with
   the palette / level+decision identity) and generate each from gen() UNCHANGED — 165.0 wide, 6/6/4,
   col-major v0=mono, wave field, seam fillers.
4. SNAP: each module attaches/snaps via the fixed Straits snap invariants; seam-abuts the subgate base.

## DONE =
- gen_straits_subgate_panel.py + 180-wide SVGs DELETED; divergent subgate base reconciled to
  gen_straits_base.py.
- 8 subgate per-voice modules ({RATCHET,GHOST}×{REST,LEGATO,ACCENT,ADVANCE}), each 16 voices × ONE
  decision, panel from gen_straits_lane.py + tint key — 165.0 wide, 6/6/4, col-major v0=mono, visually
  identical to the lane panels, seam-abutting the subgate base, snapping per the fixed invariants.
- No crammed panel, no new layout, no 180-wide anything. run_all + anchor audit green.

## WHY (so it doesn't recur)
The 6/6/4 165-wide col-major form is the SETTLED Straits per-voice panel. New per-voice panels REUSE
gen_straits_lane.py via a tint key — never a new generator, new dimensions, or a different organizing axis.
One lane/decision = one panel = one bank of 16. One generator, parameterised by tint — the "one mechanism,
not twins / reuse don't reinvent" discipline that applies everywhere in this codebase.
