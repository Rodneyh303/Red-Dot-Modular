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

════════════════════════════════════════════════════════════════════════════════
## THE DEEPER LOGIC ERROR — crammed all decisions-per-level onto one panel (Rodney)

VERIFIED structure on the branch: CC made per-LEVEL panels (modelStraitsSubgatePanelRatchet,
modelStraitsSubgatePanelGhost) — but each one crams 16 voices × MULTIPLE decision columns (rest + legato +
accent + advance) onto a SINGLE panel = the 180-wide / 154-circle monster. That is the cram.

### The established pattern organizes by DECISION, not by level
A StraitsLane panel = 16 voices × ONE decision (one lane = one bank of 16, 6/6/4, 165-wide). CC organized by
SUBGATE LEVEL (one panel per ratchet/ghost, ALL decisions on it). WRONG axis. The subgate per-voice panels
must follow the lane pattern: ONE panel = 16 voices × ONE decision, 6/6/4, 165-wide.

### The real tension CC was (silently) avoiding — ASK RODNEY, don't cram
Organizing by decision means up to 2 levels × 4 decisions (rest/legato/accent/advance) = up to EIGHT
per-voice subgate panels. CC crammed to avoid 8 panels. That panel-count tradeoff is a DESIGN DECISION for
Rodney, NOT something to silently resolve by cramming. Options to put to Rodney:
  (a) One panel per (level × decision): 6/6/4 165-wide each, matches lanes exactly, but up to 8 panels.
  (b) One panel per decision spanning BOTH levels (ratchet+ghost for REST on one panel): fewer panels,
      but each would need a layout beyond plain 6/6/4 (two level-banks) — a NEW layout, so needs sign-off.
  (c) A reduced decision set per level (maybe not all of rest/legato/accent/advance need per-voice on both
      levels) — Rodney to say which decisions actually warrant per-voice subgate panels.
DEFAULT if unsure: (a) — one 6/6/4 165-wide panel per (level, decision), reusing gen_straits_lane.py via a
tint key. Do NOT ship the crammed 180-wide multi-decision panel.

### So the FIX logic (not just the SVG)
1. The MODULE structure: each subgate per-voice panel module = ONE decision's 16 voices (like a lane
   expander is one lane's 16 voices), NOT one level's all-decisions. Restructure
   StraitsSubgatePanel{Ratchet,Ghost} accordingly (or per Rodney's chosen option a/b/c).
2. The PANEL SVG: 6/6/4, 165-wide, gen_straits_lane.py + tint key, per the fix above.
3. RAISE the panel-count tradeoff (a/b/c) with Rodney BEFORE building — the cram was an attempt to dodge
   it; surface it instead.

════════════════════════════════════════════════════════════════════════════════
## DECIDED (Rodney): OPTION (a) — one panel per (level × decision). Do NOT ask, BUILD IT.
- One 6/6/4, 165-wide panel per (subgate level × decision): {RATCHET, GHOST} × {REST, LEGATO, ACCENT,
  ADVANCE} = the full set, each its OWN per-voice panel (16 voices, 6/6/4, col-major v0=mono), generated
  from gen_straits_lane.py with a tint key. Same as a lane expander, one decision each.
- Each is its own attachable module (like each StraitsLane* is its own module), snapping per the fixed
  Straits snap invariants.
- NO crammed multi-decision panel. NO new layout. NO panel-count question — (a) is chosen; build it.
