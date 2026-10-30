# CC PROMPT — FIX subgate panels: use the EXISTING 6/6/4 lane generator, do NOT reinvent

PROBLEM (Rodney): the subgate per-voice panels (StraitsSubgatePanel_*.svg) were REINVENTED as a NEW,
DIFFERENT panel (180.0 wide, 154 circles, a new gen_straits_subgate_panel.py / gen_straits_subgate_base.py)
instead of looking like ALL the existing per-voice lane expanders — the SETTLED 6/6/4 layout, 165.0 wide,
col-major, v0=mono top-left. This was gone over and agreed. STOP reinventing — REUSE the established
generator.

## THE ESTABLISHED PATTERN (already exists — use it)
panel_src/gen_straits_lane.py makes the per-voice lane panels:
- COLS = [6, 6, 4] (line 33) — 3 columns, 6/6/4, col-major, v0=mono top-left (line 63). THE layout.
- Width 165.0, same GRID_TOP / ROW_H / knob grid as the base + other lane expanders. Seamless abutment.
- Parameterised by a TINT key (line 23 TINTS dict: rest/accent/qmix/variation/legato → tint/wave/knob/
  tinttext colours). def gen(tintkey, dark=True).
Every existing Straits lane expander panel is generated this way. The subgate panels MUST be the SAME.

## THE FIX
1. DELETE the reinvented generators + their output: panel_src/gen_straits_subgate_panel.py,
   gen_straits_subgate_base.py (if it diverges from gen_straits_base.py — check; the subgate BASE should
   likewise reuse gen_straits_base.py's pattern), and the wrong SVGs res/panels/StraitsSubgatePanel_*.svg
   (180-wide/154-circle ones).
2. For each subgate per-voice panel type (one per subgate: GHOST, RATCHET — whatever the set is), generate
   it from gen_straits_lane.py by ADDING its tint entry to the TINTS dict and calling gen(tintkey):
   - Add subgate tint keys (e.g. "ghost", "ratchet") to the TINTS dict with appropriate tint/wave/knob/
     tinttext colours (pick colours consistent with the existing palette / the subgate identity).
   - The knob grid, dimensions (165.0), 6/6/4 layout, col-major v0=mono, wave field, seam fillers — ALL
     come from gen_straits_lane.py UNCHANGED. Do NOT change the layout.
3. The subgate panels each carry ONE bank of 16 per-voice knobs for that subgate's decision set — same as a
   lane panel carries 16 per-voice knobs. If a subgate panel needs MULTIPLE decisions per voice (rest +
   legato + accent + advance per voice), that is a LAYOUT question to raise with Rodney BEFORE building —
   do NOT silently invent a wider/denser panel. The DEFAULT expectation is: it looks like the existing
   lane panels (6/6/4, 165-wide). If more knobs per voice are genuinely needed, ASK how to lay them out
   (likely: one panel per decision, same as one lane panel per lane), do not reinvent.
4. Regenerate all subgate SVGs from the corrected generator; confirm 165.0 width, 6/6/4, matching the
   existing lane panels visually (seamless abutment with the StraitsSubgateBase).

## WHY (so it doesn't recur)
The 6/6/4 165-wide col-major layout is the SETTLED Straits per-voice panel form (gen_straits_lane.py). New
per-voice panels REUSE it via a tint key — they never get a new generator or new dimensions. Reinventing a
panel means it won't abut, won't match, and throws away the proven layout. One generator, parameterised by
tint — the same "one mechanism, not twins" discipline as everywhere else.

## DONE =
- gen_straits_subgate_panel.py / divergent subgate-base generator DELETED; 180-wide SVGs DELETED.
- Subgate per-voice panels generated from gen_straits_lane.py (tint keys added), 165.0 wide, 6/6/4,
  col-major v0=mono, seam-abutting the subgate base — visually identical in layout to the existing lane
  panels.
- If multiple decisions per voice were the reason for the reinvention: STOPPED and asked Rodney for the
  layout (one-panel-per-decision default), did not invent.
