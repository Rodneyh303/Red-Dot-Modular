# Panel SVG conventions for VCV Rack (dot.modular)

## CRITICAL: scale is 75 DPI, not 96
VCV Rack defines 1 HP = 15px = 5.08mm, i.e. **75 DPI = 75/25.4 = 2.9528 px/mm**.
Rack's `mm2px(mm)` uses this, and it sizes a panel from the SVG's
`width`/`height` attributes (read as raw px). Inkscape/the SVG spec default
to **96 DPI (3.7795 px/mm)** — authoring a panel there makes it 1.28x too
large, so the artwork drifts away from the `mm2px`-placed widgets.

## Rule: viewBox == width == height, all in 75 DPI px
A correct panel header for an N-HP, 128.5mm-tall module:
    width  = N * 5.08 * 75/25.4  (= N * 15)  px
    height = 128.5 * 75/25.4      (= 379.43)  px
    viewBox = "0 0 <width> <height>"
A control at (x_mm, y_mm) is drawn at cx = x_mm * 75/25.4, cy = y_mm * 75/25.4
— identical to what mm2px gives the widget, so panel art and widgets align
with zero scaling tricks.

Monsoon (40HP): 600 x 379.43 px.

## If artwork was authored in Inkscape (96 DPI)
Rebake it into 75 DPI by wrapping all visible content (NOT <defs>) in
`<g transform="scale(0.78125)">` (0.78125 = 75/96) and setting the root
viewBox/width/height to the 75 DPI values. Result: original art preserved
exactly, coordinates now consistent.

## Component layer (helper.py)
Each panel carries a hidden `<g inkscape:label="components" style="display:none">`
of circles at the 75 DPI positions of every param/input/output/light:
  params #ff0000, inputs #00ff00, outputs #0000ff, lights #00ffff
nanosvg respects display:none so they never render in Rack; helper.py reads
them to (re)generate matching C++ addParam/addInput placement.

## Panel regeneration — authoritative map (run from the Red-Dot-Modular/ dir)

This is the single source of truth for WHICH script regenerates WHICH active
panel. Do not infer from filenames — several look plausible but are stale or
write to non-active outputs (see "Do NOT run" below).

| Panel (module)                 | Active res/panels file(s)                                              | Regenerate command                     | Kind      |
|--------------------------------|------------------------------------------------------------------------|----------------------------------------|-----------|
| Sands **East**                 | `StraitsEastSandsVisual_48HP.svg` (+ `_light`)                          | `python panel_src/gen_east_clean.py`   | generator |
| Sands **Macro** + **Mono**     | `StraitsSandsMacroVisual_48HP.svg`, `SandsMonoVisual_48HP.svg` (+`_light`) | `python panel_src/gen_macro_mono.py`   | generator |
| **Monsoon** (main)             | `Monsoon_panel_dark_monsoon.svg`, `Monsoon_panel_light_monsoon.svg`     | `python panel_src/monsoon_art.py`      | generator |

Order does not matter between panels; each command is independent and fully
overwrites its own output(s). Run the two Sands generators after any change to
`src/ui/SandsGrid.hpp` geometry (LANE_H / lane counts) or to the C++ bind ids,
since the generators mirror those constants.

### Sands generators (`gen_east_clean.py`, `gen_macro_mono.py`)
Native 75-DPI, self-contained, idempotent — safe to re-run any time. They emit
BOTH the visible artwork AND the `components` layer (SvgPanelKit `param_*` /
`input_*` / `output_*` markers), editor-lane indexed to match the C++ binds
exactly (`StraitsEastSandsVisual.cpp`, `StraitsSandsMacroVisual.cpp`; the Mono
widget places controls positionally, so its markers are advisory). They both
`import dotmod_design as D` (shared helpers).

### Monsoon generator (`monsoon_art.py`) — the panel is GENERATED now; do not hand-edit
Reverse-engineered from the former hand-maintained panels and verified against them under nanosvg
(see docs/design/MONSOON_PANEL_REVERSE_ENGINEER.md). mm-native, native 75 DPI, no scale wrapper,
every element carries its own paint/opacity (nanosvg does NOT compound opacity through `<g>`).
ONE layout table in the file drives BOTH the visible control wells and the `components` layer
(SvgPanelKit `param_*`/`input_*`/`output_*`/`light_*`, `r="3"`, visible — never `display:none`).
To add/move a Monsoon control: edit the layout table, re-run, keep the C++ bind ids in step.
Verify changes with `python panel_src/panel_diff.py OLD.svg NEW.svg` (renders with nanosvg — build
it once with `panel_src/tools/build_nsvgrender.sh`).

**Superseded by monsoon_art.py — do NOT run on the generated panels** (they rewrite layers in place
and would clobber the generated ones): `embed_cluster_art.py`, `fader_level_markers.py`,
`mode_column.py`, `degradient_monsoon.py`; and `supertree.py` is an old approximation (10 fronds,
fake gradient) — the exact rules live in monsoon_art.py.

### Do NOT run (stale / wrong-output — these caused a "no controls render" regression)
- `embed_monsoon.py` + `embed_components.py` — write UNprefixed ids (e.g.
  `NOTE_VALUE_PARAM`, not `param_NOTE_VALUE_PARAM`) AND `style="display:none"`
  (nanosvg drops hidden shapes), and their coordinate list has drifted from the
  active panel. Running either blanks EVERY Monsoon control (SvgPanelKit logs
  "param/input/output shape not found" for all). Left in the tree only as
  historical reference; treat as dead.
- `gen_monsoon.py` — writes `Monsoon_panel_*_GENERATED.svg`, an ALTERNATIVE base
  that is NOT the active panel. Harmless but does not update what Rack loads.
- `gen_monsoon_clean.py` — writes `*_GENERATED.svg` too (see its own header
  WARNING); not the active source.

### Shared modules (imported, never run directly)
- `dotmod_design.py`  : palette / logo / MBS+waves motif / recess helpers (`px`, `theme`)
- `embed_components.py`: legacy hidden-layer injector (see "Do NOT run")
- `embed_cluster_art.py`: legacy Monsoon cluster-art injector (see "Do NOT run")

### Render-verify after regenerating
There is no cairosvg step wired in this repo; verification is done by BUILDING
and loading in Rack:
1. `make` (MSYS2 mingw64 toolchain).
2. Open Rack, add the module, and check the log for
   `[SvgKit] … shape not found` — there must be NONE.
3. Eyeball the panel: every jack/knob/button renders and aligns with its label.
(Optional external check if cairosvg is installed:
`python -c "import cairosvg; cairosvg.svg2png(url='res/panels/<panel>.svg', write_to='/tmp/x.png')"`
to confirm the SVG parses.)

## TODO (deferred)
- Spread output is provably in [0,1) given RNG draws in [0,1) AND all interp/
  spread amounts clamped to [0,1] (convex combination). If any interp param
  range is ever widened beyond [0,1] (e.g. an "over-spread" effect), add a
  defensive clamp(result, 0.f, 1.f) on the spread OUTPUTS in Monsoon.cpp
  (East/West rest/mel/oct) and MonsoonSandsManager.cpp (mono) — currently
  omitted as unnecessary.
