# Monsoon panel — reverse-engineering map (step 1) + target design

GOAL (Rodney): a fresh, accurate reverse-engineer of the live Monsoon panel into a parametric generator,
with ALL controls placed via the SvgPanelKit (bind-by-name from SVG anchors) — no hardcoded coordinates
in MonsoonWidget. Supersedes the abandoned `gen_monsoon.py` attempt (its premise — "hand panel unfixable
because nanosvg can't render gradients" — is obsolete: `degradient_monsoon.py` already removed every
gradient/pattern/url(); the live file has 0 url() refs, 0 text).

Live panel: `res/panels/Monsoon_panel_{dark,light}_monsoon.svg`, viewBox 600 x 379.43 (Rack px @75dpi;
600px = 40HP = 203.2mm). Loaded by MonsoonWidget. ~464 elements.

## Coordinate hazard
Art is drawn in a LEGACY 768-wide space wrapped in `<g transform="scale(0.78125)">` (768 x 0.78125 = 600).
The `components` + `cluster-art` layers sit OUTSIDE that group in true 600-space. Two coordinate systems
in one file. The generator must emit ONE unit system (mm, converted once) — do not carry the 0.78125 wrap.

## Element map — who owns what (measured)
| Group | Elements | Content | Origin |
|---|---|---|---|
| base rect | 1 | background #121416 | hand |
| main[0] | 10 rect | sky bands | `degradient_monsoon.py` (was skyGrad) |
| main[1] | 11 ellipse | clouds (#1e2530 @.42) | hand |
| main[2] | 6 polygon + 6 ellipse | Supertree silhouettes (#18202a) | hand (was treeGrad); rules in `supertree.py` |
| main[3] | 108 (96 line) | Supertree lattice (red #dc2626) | hand |
| main[4] | 23 line+circle | red skyway/cable detail | hand |
| main[5] | 6 line | full-height dividers (#222830) | hand |
| main[6] | 10 circle @y83 | Big-Five knob rings (ochre #b87820) | hand |
| main[7] | 6 circle @y219 | BPM/LEN/OFFSET knob rings | hand |
| main[8-10] | 23 | Singapore Flyer: hub glow rings (teal, was ringGlow), ring, 16 spokes | hand + degradient |
| main[11] | 91 line | fader level-marker ticks | `fader_level_markers.py` |
| loose circle | 1 | red dot top-right | hand |
| logo | 31 | dot.modular Barlow logo, translate+scale(.26) | shared logo asset |
| cluster-art | 33 | dice/slew/mix cluster wells/seats | `embed_cluster_art.py` |
| components | 67 | kit anchors: 40 param, 12 input, 9 output, 6 light | `embed_monsoon.py` + `fader_level_markers.py` (SEMI anchors) + `mode_column.py` (MODE + lights) |

Roughly: ~230 hand-drawn (mostly base art: supertrees, clouds, flyer, rings) vs ~200 script-owned.
Also: `recolour_family_peranakan.py` recolours in place (no elements of its own).
Removed-by-comment (now deleted or moved to runtime): rain (54), HP guides, well rings, cluster recesses.

## Third layer NOT in the SVG
Control framing (wells, recesses) and ALL LABELS are drawn at runtime in `MonsoonWidget::draw()` — hence
0 `<text>`. An SVG-only reproduction has no labels.

## Kit adoption today (partial)
MonsoonWidget already uses `dotModular::Compose<…>` SvgPanelKit (incl. dev live-reload `kitStep()`):
discrete controls, SEMI faders, DICE_Q bind by name. But **26 `mm2px(` hardcoded placements remain.**
Peers (Straits, Causeway, Changi, Junction, Raffles, Shophouse, Sikit, MicroTuning, Intertropical,
Keppel) are further along.

## Target design (decided by the kit goal)
**The single layout table lives in the GENERATOR and is emitted as the SVG `components` layer.** C++ reads
positions only via `findNamed()/centerOf()`. So:
1. Generator owns every coordinate → emits art + anchors from the same table (can't drift).
2. MonsoonWidget: zero `mm2px` placement; every control binds by name; runtime framing/labels in `draw()`
   read anchor positions (`centerOf(findNamed(..))`), not their own coordinates.
3. Then the kit's dev live-reload means layout iteration = edit table → regenerate → see it live, no rebuild.
4. Adding the 6th Big-Five (q-mix) knob becomes one table row.

## Findings during step 3 (important)
- **nanosvg opacity rule (measured).** nanosvg — the parser Rack uses — INHERITS fill/stroke from `<g>`
  but does NOT compound opacity: the INNERMOST `opacity` wins (0.55 inside a 0.65 group renders 0.55 in
  Rack; browsers/cairosvg give 0.36). `fill-opacity` does multiply. cairosvg therefore misreports Rack
  for nested-opacity elements. The diff harness now renders with nanosvg by default
  (`panel_src/tools/nsvgrender`). Generator writes each element's EFFECTIVE nanosvg opacity explicitly,
  so what Rack shows is preserved and the file renders the same in any renderer.
  (Corrects `dotmod_design.py`'s note "nanosvg does NOT inherit paint from a parent <g>": paint IS
  inherited; OPACITY is what doesn't compound. The house rule of per-element paint is still right.)
- **All base art is authored in mm** at 96dpi; the 0.78125 wrapper converted to 75dpi. Generator is
  mm-native and emits at 75dpi with NO wrapper — verified lossless.
- **Logo: the family logo has a bug; Monsoon's copy is the fixed one.** Canonical `res/logo` draws the
  circuit trace OVER the letters with a dark halo, which slices the 'l' → reads "moduiar" (visible on
  Change Alley). Monsoon's embedded variant draws the trace BEHIND the letters and reads correctly. Kept
  verbatim as `panel_src/assets/monsoon_logo_dark.svgfrag`. FOLLOW-UP (family-wide, Rodney to decide):
  fix `res/logo` to this layering → fixes every `logo_embed()` panel.
- **Tree 4 bug fixed** (canopy was 4mm off its trunk). The only intentional divergence.

## Status — step 3 COMPLETE: the Monsoon panel is generated
`python panel_src/monsoon_art.py` writes the ACTIVE `res/panels/Monsoon_panel_{dark,light}_monsoon.svg`.
One mm layout table drives base art, control wells (`cluster-art`) and kit anchors (`components`).
Verified vs the former hand panels under nanosvg — dark 0.038%, light 0.085% differing, ALL of it:
  * tree-4 canopy fix (intentional), and
  * 0.1px coordinate rounding in the old embed_cluster_art.py on seat/MIX well edges (generator is exact;
    proven by re-rounding -> 0 diff).
All 67 existing kit anchors reproduced with 0.000px error, so MonsoonWidget binds unchanged.
Light theme measured: same geometry, recoloured, and 7 opacities differ (in THEMES).
Former post-processing scripts are superseded — see panel_src/README.md (do NOT run them).

## 6th Big-Five knob (q-mix) — measured constraint
At the current 26mm pitch the 6th knob sits at x=146 and its ring overlaps the Flyer ring by 17mm.
Six knobs in the existing 16..120 span needs 20.8mm pitch < 22mm ring diameter — also collides.
=> GROW THE PANEL: shift Flyer + phase knobs + mode column right by >= 19mm => +4HP min (44HP),
   +5HP comfortable (45HP). In the generator: W_MM, FLYER_C, PHASE_X0, MODE_* , RAIL/DIVIDER extents,
   BIG5_N=6 + BIG5_IDS += the q-mix param. Everything tied to the table follows.

## CC HANDOFF (step 4) — move MonsoonWidget fully onto the kit
New anchors now in the panel (24), ready to bind:
  * control-row lights: light_RHYTHM_DICE_LIGHT, light_MELODY_DICE_LIGHT, light_QMIX_DICE_LIGHT,
    light_LOCK_LIGHT, light_MUTE_LIGHT, light_RESET_LIGHT, light_RUN_GATE_LIGHT
  * 16 step LEDs: light_STEP0_LIGHT .. light_STEP15_LIGHT (Flyer, r=14)
  * param_PHASE_PARAM (Mode E phase knob — currently a TEMPORARY hardcoded (178,72))
Replace the widget's hardcoded mm with anchor binds / anchor-relative drawing:
  * the 7 row lights (rx(i), ROWYL) and 16 step lights (RCX/RCY/RLED formula) -> bind by id
  * PHASE_PARAM createParamCentered(mm2px(178,72)) -> bindParam("param_PHASE_PARAM")
  * LABELS still in raw mm -> derive from existing anchors (labelAt/centerOf):
      - dial arc labels arcLabel(16/42/68/94/120, 22, ...) -> Big-Five param anchors
      - control-row labels rowLbl(12+i*16.7, 81)          -> cluster param anchors
      - note names + fader numerals (7.5+i*9)             -> param_SEMIn anchors
      - LO / HI (119/128, 43)                             -> param_OCT_LO/HI anchors
      - "MODE" title (193, 6)                             -> relative to light_MODE_A_LIGHT
  * STALE LABELS: slots 4-5 still read "TRIAL R"/"TRIAL M"; since 7b17019 they are DICE_Q /
    LAST_DICE_Q (cause of the "DICE Q" / "TRIAL R" overlap in Rodney's screenshot).
Known layout oddities reproduced faithfully — for the dice/phase rework, NOT to fix silently:
  * BPM/LEN/OFFSET knobs at y=60, their rings centred y=58 (2mm off).
  * QMIX_DICE_LIGHT centred ON its button; its twins R/M dice lights sit 6mm below theirs (y=93).
  * q-mix sub-row (LAST_DICE_R/M, QMIX_MIX, DICE_SLEW_Q at y=94.83) collides with the light row (93).
  * QMIX_LEVEL knob occupies output slot (182,120), no well.

## Steps
1. ✅ Map (this doc).
2. ✅ Render-diff harness (nanosvg default): cairosvg render live vs generated, pixel diff → "accurate" = measurable.
3. ✅ Generator: consolidate the 5 scripts + extract hand base art into parametric code; one mm unit;
   iterate to near-zero diff against live. (Container-friendly: SVG layer only.)
4. [CC] Switch MonsoonWidget fully onto the kit: replace the 26 mm2px placements with findNamed binds;
   move draw() framing/labels to read anchors. Needs Rack build.
5. [CC] Retire hand file + delete dead SVGs (`_GENERATED`, `_40HP`, `_peranakan`, plain 172.72 ones)
   and dead generators (`gen_monsoon.py`, `gen_monsoon_clean.py`, `embed_monsoon.py` once absorbed).
