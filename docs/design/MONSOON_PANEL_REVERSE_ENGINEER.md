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

## Steps
1. ✅ Map (this doc).
2. Render-diff harness: cairosvg render live vs generated, pixel diff → "accurate" = measurable.
3. Generator: consolidate the 5 scripts + extract hand base art into parametric code; one mm unit;
   iterate to near-zero diff against live. (Container-friendly: SVG layer only.)
4. [CC] Switch MonsoonWidget fully onto the kit: replace the 26 mm2px placements with findNamed binds;
   move draw() framing/labels to read anchors. Needs Rack build.
5. [CC] Retire hand file + delete dead SVGs (`_GENERATED`, `_40HP`, `_peranakan`, plain 172.72 ones)
   and dead generators (`gen_monsoon.py`, `gen_monsoon_clean.py`, `embed_monsoon.py` once absorbed).
