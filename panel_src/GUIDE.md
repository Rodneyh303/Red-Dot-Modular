# dot.modular Panel Generation Guide

How to (re)generate every panel in `panel_src/`, and how the output connects to
the C++ via **SvgPanelKit**. Written against the scripts as they actually are —
where a script is a stale or dead end, it says so.

---

## 0. The two golden rules

**Run every script from the repo root**, never from inside `panel_src/`:

```bash
cd /path/to/Red-Dot-Modular
python3 panel_src/gen_<thing>.py
```

The scripts write to `res/panels/...` and read `res/logo/...` using paths
relative to the working directory. Running from `panel_src/` gives
`FileNotFoundError: res/panels/...`.

**nanosvg-safe is non-negotiable.** VCV renders panels with nanosvg, which does
NOT support: `<mask>`, gradients, filters, `url(#…)`, `fill-rule:evenodd`, or
paint inherited from a parent `<g>`. Every shape must carry its *own*
fill/stroke/opacity. Control *labels* are NOT `<text>` in the SVG — the widget
draws them at runtime with `nvgText`. All generators already obey this; if you
hand-edit an SVG, keep to it or controls/art will silently vanish in Rack.

The scale constant everywhere is `S = 75.0/25.4` (≈2.9528 px/mm). This is **75
DPI, not 96** — a past 96-DPI bug caused a 1.28× panel-size drift. If you see
`3.7795` in an old script, that's the bug; the correct value is `75/25.4`.

---

## 1. Two panel architectures (this is the key distinction)

There are **two different ways** a panel connects controls to the widget, and
which one a module uses determines which generator you run and what the SVG must
contain.

### A) Data-driven / kit-bound (`gen_layout.py` + SvgPanelKit)

The modern path. A JSON layout is the single source of truth; the generator
emits BOTH the SVG (with invisible `id="input_…"`/`param_…`/`output_…` marker
shapes) AND a `src/gen/<Struct>.gen.hpp` of `constexpr` mm coordinates. The
widget either binds to the SVG markers via SvgPanelKit, or reads the gen.hpp
coords. **Currently only Raffles (formerly Causeway) uses the full JSON pipeline.**

### B) Bespoke art generators (one script per panel)

Most panels predate the JSON pipeline. Each has a hand-written generator that
bakes the art + (for kit-bound modules) the marker layer directly. These do
**not** read a JSON; the geometry lives in constants at the top of the script,
which **must be kept in sync by hand** with the matching `*.hpp` widget
constants (the scripts say which constants, e.g. "Must match
StraitsSandsMacroVisual.hpp: COL_J1=8 …").

Within (B), a module is *either*:
- **kit-bound** — widget calls `bindInput("input_…")` etc., so the SVG must
  carry a `#components` marker layer; or
- **coordinate-placed** — widget calls `createInputCentered(mm2px(Vec(x,y)), …)`
  with its own constants, so the SVG markers are irrelevant (the SVG is pure
  art + wells).

You don't usually need to care which, *except*: if you add a control to a
coordinate-placed module you edit the **widget**; if you add one to a kit-bound
module you must add a **marker** to the SVG (regenerate) AND a `bind…` call.

Quick check which a module is:
```bash
grep -c 'bindInput\|bindParam\|loadPanel' src/<Module>.cpp   # >0 => kit-bound
grep -c 'createInputCentered\|mm2px(Vec'   src/<Module>.cpp   # >0 => coordinate-placed
```
<<<<<<< HEAD
Observed today: Raffles, Surge, StraitsEast = kit-bound; Interchange,
=======
Observed today: Causeway, Junction, StraitsEast = kit-bound; Interchange,
>>>>>>> origin/refactor/surge-to-junction
MonsoonSandsVisual = coordinate-placed.

---

## 2. Per-panel cheat-sheet

Run all from repo root. "Outputs" are under `res/panels/` unless noted.

### Raffles (formerly Causeway) — the ONLY JSON-pipeline panel
```bash
python3 panel_src/gen_layout.py panel_src/layouts/raffles.json
#   or regenerate every JSON panel:
python3 panel_src/gen_layout.py --all
```
- Source of truth: `panel_src/layouts/raffles.json`.
- Emits: `Raffles_panel_{dark,light}.svg` + `src/gen/RafflesLayout.gen.hpp`.
- Validates all control ids against the enum and prints `[ok] all N control ids
  present`. (gen_causeway.py has been deleted.)
- **`gen_causeway.py` has been DELETED** (was the older SVG-only generator). It
  emits art but no gen.hpp and predates the JSON. Kept only for reference.
- To add a control: edit the JSON (`controls` array, `id`/`kind`/`x_mm`/`y_mm`),
  rerun, then add the `bind…` call in `MonsoonRafflesExpander.cpp`.

### Sands visual panels — Mono, Macro, East, West
Shared design language lives in `dotmod_design.py` (palette, logo, MBS+waves
motif, recesses). The generators import it.
```bash
python3 panel_src/gen_macro_mono.py     # emits BOTH Macro and Mono Sands panels
python3 panel_src/gen_east_clean.py      # Straits East (40HP)
python3 panel_src/gen_west.py            # Straits West (36HP, mirrors East)
```
- `gen_macro_mono.py` is one script, two panels: `gen_macro(dark, W_MM=203.2)`
  and `gen_mono(dark)`. (The docstring title says "Macro (26HP)" but the
  `gen_macro` default is `W_MM=203.2` = 40HP — a doc/code mismatch in the script
  itself. Trust the code; pass an explicit `W_MM` for a different width.)
- Geometry constants at the top of each script **must match** the widget hpp
  (`COL_J1=8, J2=18, A1=30, A2=39, SPREAD_X=49, ED_X=58`, `TAB_TOP_OFFSET_MM=5`,
  etc.). Each script names the hpp it must mirror.
- These are **coordinate-placed** for the prob-out jacks we added (the widget
  uses `createOutputCentered(mm2px(Vec(PROB_OUT_X, …)))`), but otherwise carry
  the art + wells. East/West also carry a marker layer for kit binds.
- ⚠️ **Known staleness:** `embed_sands.py` references `SandsMonoVisual_34HP.svg`
  and the active panels are now 40HP (and 42HP after the prob-out widen). The
  +2HP widens for the probability outs were done by **surgical SVG canvas edits**
  (600→629.95 px on the `width`/`viewBox`/bg-rect), NOT by regeneration, because
  the generators still emit the old width. If you regenerate a Sands panel you
  will get the **pre-prob-out width back** and lose the right-hand jack strip —
  re-apply the widen, or update `W_MM` in the generator first. (Proper fix:
  bump `W_MM` in `gen_macro_mono.py`/`gen_east_clean.py` to 213.36 and
  reposition, then regenerate. Deferred.)

### Monsoon (the main module) — DON'T regenerate the art
The active Monsoon panels are the rich **568-element hand-tuned** artwork:
`res/panels/Monsoon_panel_{dark,light}_monsoon.svg`. They are NOT generated.
What the scripts do:
```bash
python3 panel_src/embed_monsoon.py        # swaps the hidden #components marker layer
python3 panel_src/embed_cluster_art.py    # injects dice/slew/mix + output recess furniture
```
- `embed_monsoon.py` reads the rich panel and replaces ONLY the hidden
  components layer (the bind markers); art untouched. Idempotent.
- `embed_cluster_art.py` adds the static control-cluster furniture as its own
  managed layer; idempotent (strips its prior layer first). Run after
  `embed_monsoon.py` by convention.
- `gen_monsoon_clean.py` writes to `*_GENERATED.svg` (a SIMPLER alternative
  base) so it can NEVER clobber the active rich panel. Most of Monsoon's control
  framing is drawn at runtime in `MonsoonWidget::draw()`, not baked in the SVG.
- `gen_monsoon_components.py` / `embed_components.py`: the helper.py-compatible
  marker convention (params `#ff0000`, inputs `#00ff00`, outputs `#0000ff`,
  lights `#00ffff`, custom `#ffff00`); used to author the components layer.
- The Monsoon LastDice/LastTrial test-button markers we added were hand-inserted
  into the rich SVGs (cy=280 row), not regenerated — same surgical approach as
  the Sands widen.

<<<<<<< HEAD
### CV / modulation expanders — Surge, Raffles-art, Interchange, Straits CV
=======
### CV / modulation expanders — Junction, Causeway-art, Interchange, Straits CV
>>>>>>> origin/refactor/surge-to-junction
```bash
python3 panel_src/gen_junction.py (gen_surge.py deleted)            # Junction 8HP (big-5 CV) dark+light
python3 panel_src/gen_straits_cv.py       # Straits East+West poly CV (390x380 px)
python3 panel_src/gen_interchange.py      # Interchange tonal-CV (270x380 px)
python3 panel_src/embed_interchange_markers.py  # OR: keep hand art, inject markers only
```
<<<<<<< HEAD
- **Surge** and **Raffles** also have *renamed* art-variant generators with
=======
- **Junction** and **Causeway** also have *renamed* art-variant generators with
>>>>>>> origin/refactor/surge-to-junction
  Singapore-themed hero graphics:
  - `gen_junction.py` — Junction (was Junction): Bugis pinisi schooner, 120×380 px.
  - `gen_raffles.py` — Raffles (was Causeway): raffle-ticket fan, 180×380 px.
  These are alternative art treatments matching the same control geometry.
- **Interchange** is px-native (270×380) to match hard-coded widget coords, and
  is **coordinate-placed**. Two options: regenerate with `gen_interchange.py`
  (changes the look), or `embed_interchange_markers.py` (keeps the hand art,
  only injects the SvgPanelKit `#components` markers — preferred if you like the
  current look).
- **Straits CV** (`gen_straits_cv.py`) is the single source for the East/West
  poly CV expanders (390×380 px), peranakan art + kit marker layer.

### Recolour passes (palette only, geometry untouched)
```bash
python3 panel_src/recolour_family_peranakan.py        # whole family → Peranakan palette
python3 panel_src/recolour_interchange_peranakan.py   # just Interchange
```
- These remap **colours only** on panels already in use; they leave geometry and
  any `#components` marker shapes alone. Safe. Monsoon gets a conservative map
  that does NOT touch its named `#ff0000/#00ff00/#00ffff` component anchors.

---

## 3. SvgPanelKit — how the SVG binds to the widget

`src/ui/SvgPanelKit.hpp`. Mixin-style helpers a `ModuleWidget` uses to place
controls from named SVG shapes instead of literal coordinates.

### The marker contract
A bind looks up an SVG shape **by its `id`** and places the control at that
shape's centre. The id convention:

| Control | id prefix | bind call |
|---|---|---|
| param | `param_` | `bindParam<Trimpot>("param_FOO_PARAM", FOO_PARAM)` |
| input | `input_` | `bindInput<PJ301MPort>("input_FOO_INPUT", FOO_INPUT)` |
| output | `output_` | `bindOutput<PJ301MPort>("output_FOO_OUTPUT", FOO_OUTPUT)` |
| light | `light_` | `bindLight<...>("light_FOO", FOO_LIGHT)` |

The marker shape itself is an invisible circle (`fill:none; stroke:none`, small
r) inside a `<g id="components">`. **Avoid `display:none`** on the layer —
nanosvg may drop display:none shapes from the parsed list, and then `findNamed`
can't see them (this is why `embed_interchange_markers.py` uses a *visible*
none-painted layer, not the older `display:none` style).

### How placement works (units!)
On `loadPanel(file)`, the kit caches every shape with a non-empty `id`. A bind
calls `findNamed(id)`, takes `centerOf(shape)` = midpoint of the shape's
**bounds in the SVG's own coordinate space**, and puts the widget there.

So **marker coordinates are in the panel's native units** — which for these
panels is **px** (the SVG is authored at `px = mm*S`). You do NOT wrap kit-bound
marker coords in `mm2px`; the SVG already baked the scale. (Contrast:
coordinate-placed widgets call `createInputCentered(mm2px(Vec(x_mm,…)))` — *they*
convert, because they're using raw mm constants, not SVG markers.)

### Missing markers are non-fatal
Every `bind…` is warn-and-skip: if the id isn't in the SVG it logs
`[SvgKit] input shape not found: <id>` and places nothing — no crash. This is
why you can add a `bind…` call BEFORE drawing the marker (the control just won't
appear until the SVG has the marker). We used this to stage the Monsoon
LastDice buttons.

### Variadic / prefix binds
```cpp
bindParams<Trimpot>("atten_", A0,A1,A2,A3,A4,…);  // atten_0, atten_1, … in order
findPrefixed("input_")                             // all shapes with that id prefix
forEachMatched("voice_(\\d+)_lane_(\\d+)", cb)     // regex with capture groups
```

---

## 4. Common workflows

**Add a jack to Raffles (JSON module):**
1. Add a `controls` entry to `panel_src/layouts/raffles.json`
   (`{"id":"RAFFLES_…","kind":"input","x_mm":…,"y_mm":…}`).
2. `python3 panel_src/gen_layout.py panel_src/layouts/raffles.json`
3. Add the `bindInput("input_RAFFLES_…", …)` call in the widget.
4. Confirm the validator prints `[ok] all N control ids present`.

**Add a jack to a Sands panel (coordinate-placed, e.g. prob-outs):**
1. Add the output id + `config(…, NUM_OUTPUTS, …)` in the hpp.
2. `createOutputCentered<PJ301MPort>(mm2px(Vec(X, rowY(l))), module, ID)` in the
   widget constructor.
3. Widen the panel if needed: either bump `W_MM` in the generator and
   regenerate, or surgically widen the SVG canvas (`width`, `viewBox`, bg-rect).
   Mind the stale-width caveat in §2.

**Change only colours:** use a `recolour_*` script; never hand-edit hex across
dozens of shapes.

**Verify an SVG is well-formed after any edit:**
```bash
python3 -c "import xml.dom.minidom as m; m.parse('res/panels/<file>.svg'); print('OK')"
```

---

## 5. Gotchas, collected

- Run from repo root (§0).
- 75 DPI (`75/25.4`), never 96 / `3.7795`.
- `gen_layout.py` is canonical for JSON modules; `gen_causeway.py` was deleted.
- Regenerating a **Sands** panel reverts the prob-out width widen — see §2.
- Don't regenerate **Monsoon** art; use the `embed_*` scripts (the active panel
  is hand-authored).
- nanosvg ignores `<text>` and `display:none`-dropped shapes, masks, gradients,
  `url(#…)`, evenodd — keep markers visible-but-unpainted and labels in the
  widget.
- Kit marker coords are **px** (no `mm2px`); coordinate-placed widget constants
  are **mm** (wrapped in `mm2px`). Don't mix them.
- Bespoke generators hold geometry constants that must match the widget hpp by
  hand — the scripts name which constants.
