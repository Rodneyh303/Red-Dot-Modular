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

## Scripts
- gen_monsoon_clean.py  : full Monsoon panel generator (native 75 DPI)
- embed_components.py    : core — inject hidden components layer (S = 75/25.4)
- embed_monsoon.py       : Monsoon component list -> embed (75 DPI)
- embed_sands.py         : East/Macro/Mono component lists -> embed
Regenerate: `python panel_src/embed_monsoon.py` etc.
Verify in Rack: `python helper.py createmodule <Module> res/panels/<panel>.svg`

## TODO (deferred)
- Spread output is provably in [0,1) given RNG draws in [0,1) AND all interp/
  spread amounts clamped to [0,1] (convex combination). If any interp param
  range is ever widened beyond [0,1] (e.g. an "over-spread" effect), add a
  defensive clamp(result, 0.f, 1.f) on the spread OUTPUTS in Monsoon.cpp
  (East/West rest/mel/oct) and MonsoonSandsManager.cpp (mono) — currently
  omitted as unnecessary.
