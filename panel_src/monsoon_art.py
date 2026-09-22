#!/usr/bin/env python3
"""monsoon_art.py — mm-native generators for the Monsoon panel base art.

Reverse-engineered EXACTLY from the live res/panels/Monsoon_panel_*_monsoon.svg
(see docs/design/MONSOON_PANEL_REVERSE_ENGINEER.md). All geometry is authored in millimetres;
`S` (px per mm) converts at emit time:
    S96 = 768/203.2  — the live file's legacy 96-dpi art space (inside scale(0.78125)); use to A/B
    S75 = 600/203.2  — Rack 75-dpi panel space; the target (no scale wrap)

Supertrees — every rule measured across all 6 live trees (fit residual < 0.0003):
  canopy:  rx = 0.28*h        ry = 0.28*rx         (h = trunk height, ground -> canopy centre)
  trunk:   base width per-tree (data), top width = base/4, base sits on the panel ground (128.5mm)
  rungs:   4, at f = 0.24, 0.40, 0.58, 0.78 of h below canopy; centred on TRUNK;
           half-width = bh*(3/8 + 11/24*f)          (bh = trunk base half-width)
  fronds:  12, theta_i = 180 - i*180/11 (i = 0..11, endpoints included), start ON the canopy ellipse,
           end = start + (A*rx*cos, -(B - C*|cos|)*rx),  C = 0.11405, B = 3C, A = 7C/3
Fronds and rungs are centred on the TRUNK. Tree 4's canopy ellipse is drawn +4mm off its trunk in
the original (fronds stay on the trunk, so they float off the canopy edge) — almost certainly a bug in
the original generation. Preserved as data (canopy_cx) for exact reproduction; set canopy_cx = trunk_cx
to fix. Rungs are emitted bottom-up (f descending) to match live element order.
"""
import math

PANEL_H_MM = 128.5
S96 = 768 / 203.2
S75 = 600 / 203.2

# (trunk_cx, canopy_cx, height, base_width)  — all mm, measured from live
TREES = [
    (18.0,  18.0,  26.0, 10.08),
    (44.0,  44.0,  18.0,  7.92),
    (76.0,  76.0,  32.0, 12.60),
    (116.0, 120.0, 22.0,  9.00),
    (152.0, 152.0, 28.0, 10.80),
    (186.0, 186.0, 20.0,  8.64),
]
CANOPY_R   = 0.28
RUNG_F     = (0.24, 0.40, 0.58, 0.78)
RUNG_HW    = (3 / 8, 11 / 24)
FROND_C    = 0.11405
FROND_B    = 3 * FROND_C
FROND_A    = 7 * FROND_C / 3
N_FRONDS   = 12

# stroke widths are authored in the legacy 96-dpi px; keep them in mm so any S is exact
SW_OUTLINE_MM = 0.45 / S96
SW_RUNG_MM    = 0.60 / S96

THEMES = {
    "dark": dict(tree_fill="#18202a", tree_fill_op="0.78", tree_line="#dc2626", tree_line_op="0.40"),
}


def _f(v):
    return f"{v:.2f}"


def supertree_geometry(tcx, ccx, h, bw, ground=PANEL_H_MM):
    """Pure geometry in mm (no SVG). Returns dict of primitives."""
    rx = CANOPY_R * h
    ry = CANOPY_R * rx
    cy = ground - h
    bh, th = bw / 2, bw / 8
    trunk = [(tcx - bh, ground), (tcx + bh, ground), (tcx + th, cy), (tcx - th, cy)]
    rungs = []
    for f in reversed(RUNG_F):
        y = cy + h * f
        hw = bh * (RUNG_HW[0] + RUNG_HW[1] * f)
        rungs.append((tcx - hw, y, tcx + hw, y))
    fronds = []
    for i in range(N_FRONDS):
        t = math.radians(180 - i * 180 / (N_FRONDS - 1))
        c, s = math.cos(t), math.sin(t)
        sx, sy = tcx + rx * c, cy - ry * s
        fronds.append((sx, sy, sx + FROND_A * rx * c, sy - (FROND_B - FROND_C * abs(c)) * rx))
    return dict(trunk=trunk, canopy=(ccx, cy, rx, ry), rungs=rungs, fronds=fronds)


def supertrees(S, theme="dark"):
    """Return (fill_layer, line_layer) SVG strings at px scale S."""
    th = THEMES[theme]
    fill = [f'<g fill="{th["tree_fill"]}" opacity="{th["tree_fill_op"]}" stroke="none">']
    line = [f'<g fill="none" stroke="{th["tree_line"]}" stroke-width="{_f(SW_OUTLINE_MM*S)}" '
            f'opacity="{th["tree_line_op"]}">']
    for tree in TREES:
        g = supertree_geometry(*tree)
        pts = " ".join(f"{_f(x*S)},{_f(y*S)}" for x, y in g["trunk"])
        cx, cy, rx, ry = g["canopy"]
        poly = f'<polygon points="{pts}"/>'
        ell = f'<ellipse cx="{_f(cx*S)}" cy="{_f(cy*S)}" rx="{_f(rx*S)}" ry="{_f(ry*S)}"/>'
        fill += [poly, ell]
        line += [poly, ell]
        for x1, y1, x2, y2 in g["fronds"]:
            line.append(f'<line x1="{_f(x1*S)}" y1="{_f(y1*S)}" x2="{_f(x2*S)}" y2="{_f(y2*S)}"/>')
        for x1, y1, x2, y2 in g["rungs"]:
            line.append(f'<line x1="{_f(x1*S)}" y1="{_f(y1*S)}" x2="{_f(x2*S)}" y2="{_f(y2*S)}" '
                        f'stroke-width="{_f(SW_RUNG_MM*S)}"/>')
    fill.append("</g>")
    line.append("</g>")
    return "\n".join(fill), "\n".join(line)
