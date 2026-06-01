# Panel component-layer workflow

Each production panel in `res/panels/` now contains a hidden Inkscape layer
`components` (`style="display:none"`) holding circles at the exact mm/px
positions of every param, input, output, and light. nanosvg (VCV's renderer)
respects `display:none`, so these never render in Rack — but Rack's
`helper.py` can read them to (re)generate matching C++ placement code.

## Colour convention (helper.py)
- params  : `#ff0000` (red)
- inputs  : `#00ff00` (green)
- outputs : `#0000ff` (blue)
- lights  : `#00ffff` (cyan)

Circle `cx,cy` = component centre. Element `id` = component name.

## Verify with helper.py
```
cd <Rack SDK>
python helper.py createmodule <Module> res/panels/<panel>.svg
# or to just diff positions, inspect the generated addParam/addInput lines
```

## Regenerate the component layers
```
python panel_src/embed_monsoon.py   # Monsoon dark+light
python panel_src/embed_sands.py     # East, Macro, Mono dark+light
```
Positions in those scripts are the single source of truth and match the
hand-written coordinates in the widget .cpp files.

## Artwork notes
- Monsoon rain group opacity reduced 0.55 → 0.15, clouds 0.42 → 0.20
  so controls remain legible.
