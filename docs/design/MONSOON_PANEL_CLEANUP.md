# Monsoon panel — clutter diagnosis + cleanup plan

Panel in use: `res/panels/Monsoon_panel_{dark,light}_monsoon.svg` (588 elements). This is the ONE
hand-authored rich panel; every other module has a from-scratch parametric generator. Its control
framing (wells, recesses, labels) is drawn at RUNTIME in `MonsoonWidget::draw()`, not baked into the
SVG — that split is the root of several problems below.

---

## A. Findings (measured, not guessed)

### A1. The SVG uses features nanosvg CANNOT render
Rack renders panel SVGs with nanosvg. The panel contains:

| Feature | Count | What Rack actually shows |
|---|---|---|
| `<pattern id="grain">` | 1 | not rendered → flat/black fill |
| `<linearGradient id="skyGrad">` | 1 | not rendered |
| `<linearGradient id="treeGrad">` | 1 | **the Supertrees** — not rendered |
| `<radialGradient id="ringGlow">` | 1 | not rendered |
| `fill="url(#...)"` refs | 4 | each = a silently broken fill |

**This is very likely the "shading around the knobs that just looks bad"** (`ringGlow`) and part of
why the artwork reads as muddy. Every `url(#…)` fill is a fill nanosvg drops or renders black. The
panel was authored as if for a browser, not for Rack.

**Fix:** replace each gradient with a stack of solid-fill shapes (the technique already used by
`gen_interchange.py`, which stands in flat gold/teal tints for the master's radial glows). No gradient,
no `url()`, no `<pattern>`, no `<text>`, no `<mask>` — the panel-generator house rule.

### A2. Overlapping sub-panels obscure the Supertrees
The bottom three rows draw multiple sub-panel/recess rectangles that overlap each other AND sit on top
of the tree artwork. Because the control framing is drawn at runtime *over* the SVG, the SVG's careful
composition is covered by opaque runtime rects.

**Fix options (pick one, don't mix):**
1. **Move framing into the SVG** (preferred, matches every other module): the generator emits wells /
   recesses / group boxes as part of the artwork, so overlap is resolved once at authoring time and the
   trees can be composed *around* the controls. Runtime `draw()` then draws nothing but live values.
2. Keep runtime framing, but make it non-opaque (stroke-only outlines) and shrink each group box to the
   true control bounds so trees show between them.

### A3. Note-fader guide lines don't align
There are parallel guide lines meant to sit between the 12 note faders. They're authored as absolute
coordinates in the SVG while the faders are positioned from a different source (runtime / kit markers),
so the two drift. Same class as the Shophouse shutter bug: **geometry defined twice.**

**Fix:** single-source it. Emit the guide lines from the same generator loop that emits the fader
markers, so a line is always drawn at `(fader[i].x + fader[i+1].x)/2`. Then they cannot drift.

### A4. Mode E button missing
`param_MODE_PARAM` exists, but the panel's mode row is missing the **E** position. Engine has
`executeModeE()` (see `MonsoonModeController.cpp`). Panel needs the extra button/segment + label, and
the mode switch's range must cover it.

**Check when fixing:** the mode param's `configParam` max value, the widget's button count, and the
panel's drawn segments must all agree — three places, so verify all three (the enumerate-all-gates rule).

### A5. Pattern length / offset / BPM knob shading
These use `ringGlow` (a radialGradient). nanosvg can't draw it → the ugly artefact. Replace with either
nothing, or a solid-fill ring (2–3 concentric flat circles) as Interchange does.

---

## B. Clutter reduction (design)

The bottom three rows are dense because every control gets its own framed box. Suggested hierarchy:

1. **One recess per functional GROUP**, not per control (dice/slew/mix as a single well, transport as
   another). Fewer, larger boxes read calmer and leave sky/tree visible between them.
2. **Stroke-only group outlines** (0.3–0.4px, `plastersh`-equivalent) instead of filled rects — the
   artwork shows through, which is the point of having artwork.
3. **Let the trees breathe:** keep the upper ~40% of the panel free of framing entirely; put the
   trees + rain there, and let only thin rain strokes cross into the control zone.
4. **Align to a grid.** Define `COL_X[]` / `ROW_Y[]` once in the generator; every control, label, well
   and guide line reads from it. This kills alignment drift permanently.

---

## C. Reverse-engineering the Supertrees / clouds / rain — confidence assessment

**Honest answer: high confidence (~85%) for a faithful from-scratch generator, because the artwork is
already parametric in structure.** What's actually there:

- **Supertrees** = `<polygon>` trunk (4-point taper: two base x's, two top x's) + `<ellipse>` canopy
  (cx, cy, rx, ry). Sampled: trunk `48.98,485.67 87.08,485.67 72.79,387.40 63.27,387.40` with canopy
  `cx=68.03 cy=387.40 rx=27.51 ry=7.70`. That is a **trivially parameterisable** shape:
  `tree(x, height, trunk_w_base, trunk_w_top, canopy_rx, canopy_ry)`. There are ~12 polygons + 23
  ellipses → roughly a dozen trees at varying heights. Easy.
- **Rain** = 325 `<line>` elements with `stroke-dasharray="3.5,3.6"`, all sharing a steep slope
  (sampled: dx≈55.75, dy≈396.6 → ~82° from horizontal). So: one angle, one dash pattern, jittered x
  offsets across the panel. **A 6-line loop reproduces this.**
- **Clouds / sky** = `skyGrad` (a vertical gradient) — the ONE genuinely lossy part, since nanosvg
  can't do gradients. Reproduce as 6–10 stacked solid bands stepping the colour (the standard flat
  substitute), or as a field of overlapping low-opacity ellipses for a softer cloud mass.
- **`ringGlow` / `grain`** = drop entirely; substitute flat rings / a couple of faint scanlines.

**Where the 15% risk sits:** the gradients are the only thing that can't be reproduced exactly, so a
generated panel will look *different* — flatter — in the sky and glow. But it will look *better in Rack*
than the current file, because Rack isn't rendering those gradients anyway. In other words: the parts I
can't reproduce are the parts Rack already fails to draw.

**Precedent:** `gen_interchange.py` is exactly this job done well (rich lattice, controls embedded,
flat-fill stand-ins for the master's radial glows), and Rodney rated it a success. The Supertrees are
*structurally simpler* than the Peranakan hex lattice.

**Recommended approach:** `panel_src/gen_monsoon.py`, built in layers, rendering after each:
sky bands → rain → trees → grid + group recesses (stroke-only) → control markers → guide lines from the
same loop as the fader markers. Emit `param_*` / `input_*` / `output_*` id markers so the widget binds by
name (as every other module does), and delete the runtime framing from `MonsoonWidget::draw()` as those
move into the SVG.

**Note:** `gen_monsoon_clean.py` exists but writes `*_GENERATED.svg` and is NOT the source of the active
panel — deliberately, so it can never clobber the hand-made file. A real `gen_monsoon.py` should follow the
same safety (write to a new name first, compare renders, then switch the widget over).

---

## D. Suggested order of work
1. Kill the four `url(#…)` fills (immediate visible win: knob shading artefact, tree fill, sky).
2. Mode E button (functional gap, not cosmetic).
3. Single-source the fader guide lines.
4. Group-recess consolidation + stroke-only outlines (the real declutter).
5. Only then: full `gen_monsoon.py` from-scratch generator.

Steps 1–4 are edits to the existing SVG + widget; step 5 is the rebuild. Doing 1–4 first means step 5
starts from a panel whose *intent* is already clear.

---

## E. Lantern light theme (open)

`Lantern_panel_light.svg` exists and ships, but `Lantern.cpp:848` hard-loads
`Lantern_panel_dark.svg` unconditionally — so the light panel has never been used. The
Lantern has no theme handling of any kind; its only `findMonsoonEitherSide` calls read
engine state.

**DONE** (`7ae7768`): swap copied from `StraitsEastSandsVisual.cpp:932-937` — both SVGs
cached, `step()` follows `monsoon->lightTheme`, `setBackground` on change; holds the last
theme with no host attached.

Trivially safe because the panel carries **no text**: both SVGs are 19 lines / 8 rects /
6 circles and nothing else, and every `nvgText` in `Lantern.cpp` is inside `LanternDisplay`
— on the LCD. Nothing can be left in the wrong colour.

**Found while doing it:** the Lantern panel has **no silkscreen at all**. VIEW / ZOOM /
FOLLOW / DISPLAY / ROLL are named via `configSwitch` (so they tooltip) but nothing is
printed beside them. Separate gap; worth a pass before release.

**Do NOT theme the LCD.** The note-grid display stays dark on both themes, same rule as
the Sands lane wells (see `SandsVisualEditorV4::setTheme`): its cell colours are
high-luminance and tuned against a dark ground, and — the real point — the grid encodes
state in colour and alpha, so lightening the ground makes low-alpha cells read as *absent*
rather than *quiet*. Theme the panel around it; leave the screen alone.

Note for the record: the Sands commit originally cited "the Lantern has no light theme"
as evidence that displays shouldn't theme. That was wrong — it has no light theme because
nobody wrote one. **Absence is not a decision.** The rule rests on the contrast maths and
the alpha semantics, not on precedent.
