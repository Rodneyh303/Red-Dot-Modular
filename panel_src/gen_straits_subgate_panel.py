#!/usr/bin/env python3
"""Straits Subgate Panel — the subgate consumer-group PER-VOICE tier (one level).

16 voice rows (V1 mono at top, voices 2..16 below) x 4 decision columns
(REST / LEG / ADV / ACC), plus one 16-channel mod jack per column (ch v =
voice v — the Causeway per-voice path). Parameterised by level tint
(ratchet / ghost); the two panels abut the subgate base seamlessly (identical
H, top rule, bg, wave footer, edge rails).

Text is NOT in the SVG (nanosvg drops <text>) — the widget draws the title and
column captions (the Cantonment lettering idiom). Invisible anchors:
  param_sbp_{d}_{v}  — knob anchor (d = decision col, v = voice row)
  mod_sbp_{d}        — the column's mod jack anchor
  light_connect      — the connect-mark anchor

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 12
W  = HP * 5.08

MARGIN   = 4.0
BANK_W   = W - 2*MARGIN
COLS     = 4
COL_PITCH = BANK_W / COLS
GRID_TOP = 17.0
N_ROWS   = 16
ROW_H    = 5.625            # 16 rows in 90mm
KNOB_R   = 2.6              # seat ring — the widget's Small Cog face sits over it
JACK_Y   = 112.0
JACK_R   = 4.0
WAVE_Y   = 118.0
WAVE_H   = 5.5

TINTS = {
    "ratchet": dict(tint="#c05850", wave="#8a3c36", knob="#3a2624", text="#c05850"),
    "ghost":   dict(tint="#5878b8", wave="#3c528a", knob="#1e2a44", text="#5878b8"),
}

def gen(tintkey, dark=True):
    t = TINTS[tintkey]
    theme = THEMES["dark" if dark else "light"]
    bg = theme["bg"]
    jackface = "#0c0e11" if dark else "#e2ddd2"
    jackring = "#4a4a4a" if dark else "#b0a898"
    px = make_px()
    o = []; A = o.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    top_rule(A, px, W)

    # Wave-field behind the grid (family texture)
    wave_field(A, px, MARGIN, GRID_TOP-2, BANK_W, N_ROWS*ROW_H+4, t["wave"], theme["wave_op"], n=16)

    # Tint band + identifying dot (the lane idiom)
    A(f'<rect x="{px(MARGIN-1)}" y="{px(GRID_TOP-3)}" width="{px(BANK_W+2)}" '
      f'height="{px(N_ROWS*ROW_H+6)}" rx="{px(1.5)}" fill="{t["tint"]}" fill-opacity="0.08" '
      f'stroke="{t["tint"]}" stroke-width="0.3" stroke-opacity="0.45"/>')
    A(f'<circle cx="{px(W/2)}" cy="{px(GRID_TOP-5)}" r="{px(1.4)}" fill="{t["tint"]}"/>')

    # Knob grid: 4 cols x 16 rows, col-major within each column the rows are voices.
    # Voice 0 (V1 mono) gets the red ring (the lane's mono idiom).
    for d in range(COLS):
        cx = MARGIN + COL_PITCH*(d+0.5)
        for v in range(N_ROWS):
            cy = GRID_TOP + ROW_H*(v+0.5)
            mono = (v == 0)
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["knob"]}" '
              f'stroke="{t["tint"]}" stroke-width="{px(0.7 if mono else 0.4)}"/>')
            if mono:
                A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R+0.9)}" fill="none" '
                  f'stroke="#d4001a" stroke-width="{px(0.4)}" stroke-opacity="0.85"/>')
            A(f'<circle id="param_sbp_{d}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        # Column's mod jack (16ch, ch v = voice v)
        A(f'<circle cx="{px(cx)}" cy="{px(JACK_Y)}" r="{px(JACK_R)}" fill="{jackface}" '
          f'stroke="{t["tint"]}" stroke-width="{px(0.6)}" stroke-opacity="0.8"/>')
        A(f'<circle id="mod_sbp_{d}" cx="{px(cx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── Bottom wave footer (edge-tiling) + rails ──
    bottom_wave_tiling(A, px, W, WAVE_Y, WAVE_H, theme["spine"], theme["wave_op"], n=7)
    panel_rails(A, px, W, 2.0, WAVE_Y + WAVE_H, theme)

    # Connect mark anchor
    A(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    tintkey = sys.argv[1] if len(sys.argv) > 1 else "ratchet"
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"StraitsSubgatePanel_{tintkey}_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(tintkey, dark))
        print(f"Generated {path}")
