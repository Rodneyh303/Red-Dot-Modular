#!/usr/bin/env python3
"""Intertropical scene-sequencer panel generator (SCAFFOLD).

Produces res/panels/Intertropical_panel_{dark,light}.svg.

Layout (top -> bottom):
  brand strip | repeat-select bars (8) | 8x16 voice-membership grid | poly outputs

Design rules (dotmod_design.py / nanosvg): no masks/gradients/filters; every shape
carries its own paint; NO <text> for control labels (the widget draws those). Panel art
is STATIC geometry only -- all live state (membership fill, active scene, repeat progress,
playhead) is widget-drawn OVER this panel. Cells are store-backed toggle widgets, not
params (Intertropical is de-parammed from the start).

STATUS: scaffold. Geometry + wells + brand + output row are laid out and parametric;
final art polish (motif, exact spacing, light theme tuning) is TODO. Marker ids follow the
"param_<role>_<col>_<row>" / "output_<name>" convention so the widget can bind by name.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dotmod_design as D
from dotmod_design import px, theme, svg_open, logo_embed, jack

#  Panel geometry (mm) 
HP        = 22                      # TODO confirm: grid + outputs want width; 22HP ~= 111.8mm
PW_MM     = HP * 5.08
PH_MM     = 128.5                   # Eurorack 3U
MARGIN    = 6.0

N_SCENES  = 8                       # grid columns
N_VOICES  = 16                      # grid rows (voice membership)
N_REPEATS = 8                       # repeat-select segments per scene

#  Vertical bands 
BRAND_Y   = 6.0                     # wordmark baseline area
REPEAT_TOP= 16.0                    # top of the repeat-select bars
REPEAT_H  = 12.0                    # height of the 8-segment repeat bar
GRID_TOP  = REPEAT_TOP + REPEAT_H + 4.0
OUT_Y     = PH_MM - 12.0            # poly output jack row

#  Horizontal: 8 scene columns across the usable width 
GRID_L    = MARGIN + 8.0            # leave a left gutter for future row labels/voice swatches
GRID_R    = PW_MM - MARGIN
COL_W     = (GRID_R - GRID_L) / N_SCENES
GRID_BOT  = OUT_Y - 8.0
ROW_H     = (GRID_BOT - GRID_TOP) / N_VOICES

def col_x(c):  return GRID_L + c * COL_W          # left edge of scene column c
def col_cx(c): return col_x(c) + COL_W / 2.0
def row_y(r):  return GRID_TOP + r * ROW_H        # top edge of voice row r
def row_cy(r): return row_y(r) + ROW_H / 2.0

CELL_INSET = 0.9   # gap between cell and column edge (mm)


def cell_well(cx_mm, cy_mm, w_mm, h_mm, t):
    """A static membership-cell well. Widget fills it (voiceColour) when the voice is IN."""
    x = px(cx_mm - w_mm/2); y = px(cy_mm - h_mm/2)
    return (f'<rect x="{x:.1f}" y="{y:.1f}" width="{px(w_mm):.1f}" height="{px(h_mm):.1f}" '
            f'rx="{px(1.0):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="1"/>')


def repeat_bar(c, t):
    """8-segment vertical LED bar above scene column c. Static wells; widget lights N=count
    and marks the current repeat. Segment 0 (bottom) = 1 repeat ... segment 7 (top) = 8."""
    out = []
    seg_h = REPEAT_H / N_REPEATS
    w = COL_W - 2*CELL_INSET
    for s in range(N_REPEATS):
        cy = REPEAT_TOP + REPEAT_H - (s + 0.5) * seg_h   # bottom-up
        out.append(cell_well(col_cx(c), cy, w*0.7, seg_h*0.72, t))
    return "".join(out)


def build(dark):
    t = theme(dark)
    PW, PH = px(PW_MM), px(PH_MM)
    s = [svg_open(PW, PH)]
    # panel background
    s.append(f'<rect x="0" y="0" width="{PW}" height="{PH}" fill="{t["bg"]}"/>')

    # brand strip (real wordmark)
    s.append(logo_embed(dark, MARGIN, BRAND_Y, 34.0))
    # (module name "Intertropical" -> widget-drawn label, NOT panel text)

    # repeat-select bars (row 0 role), one per scene
    for c in range(N_SCENES):
        s.append(repeat_bar(c, t))

    # voice-membership grid: 8 scene columns x 16 voice rows of cell wells
    w = COL_W - 2*CELL_INSET
    h = ROW_H - 2*CELL_INSET
    for c in range(N_SCENES):
        for r in range(N_VOICES):
            s.append(cell_well(col_cx(c), row_cy(r), w, h, t))
    # left gutter: 16 voice swatches (widget draws voiceColour; panel leaves the wells)
    for r in range(N_VOICES):
        s.append(cell_well(MARGIN + 4.0, row_cy(r), 4.0, h*0.7, t))

    # poly outputs: GATE, CV, ACCENT, LEGATO, SLEG (labels widget-drawn)
    names = ["gate", "cv", "accent", "legato", "sleg"]
    n = len(names)
    span = GRID_R - GRID_L
    for i, nm in enumerate(names):
        jx = GRID_L + (i + 0.5) * (span / n)
        s.append(jack(jx, OUT_Y, t))

    s.append('</svg>')
    return "".join(s)


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    outdir = os.path.join(root, "res", "panels")
    os.makedirs(outdir, exist_ok=True)
    for dark, suffix in [(True, "dark"), (False, "light")]:
        svg = build(dark)
        p = os.path.join(outdir, f"Intertropical_panel_{suffix}.svg")
        open(p, "w").write(svg)
        print(f"wrote {p}  ({len(svg)} bytes)")


if __name__ == "__main__":
    main()
