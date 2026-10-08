#!/usr/bin/env python3
"""Straits Base panel — frame + IO (5 poly-cable outs + quantiser CV in) + voice count.
No lane knobs. Lane expanders dock right.

Uses shared art helpers from straits_art.py for:
  - Identical top-rule/bg/S/H (seam alignment with lane panels)
  - Edge-tiling bottom wave footer (continuous across base + any lane combination)
  - Right-edge vertical separator (base→lane boundary)

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 12
W  = HP * 5.08

MARGIN = 5.0
JACK_Y = 111.1
JACK_R = 4.0
KNOB_R = 4.5
WAVE_Y = JACK_Y + 5.5     # bottom wave footer Y
WAVE_H = 5.5              # bottom wave footer height

def gen(dark=True):
    theme = THEMES["dark" if dark else "light"]
    bg = theme["bg"]
    spine = theme["spine"]
    jackface = "#0c0e11" if dark else "#e2ddd2"
    jackring = "#4a4a4a" if dark else "#b0a898"
    knobface = "#2a2e33" if dark else "#e8e2d6"
    knobring = "#4a5058" if dark else "#b0a898"
    px = make_px()
    o = []; A = o.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    top_rule(A, px, W)

    # Voice count knob (centred, upper area)
    vcx, vcy = W/2, 30.0
    A(f'<circle cx="{px(vcx)}" cy="{px(vcy)}" r="{px(KNOB_R)}" fill="{knobface}" '
      f'stroke="{knobring}" stroke-width="{px(0.5)}"/>')
    A(f'<line x1="{px(vcx)}" y1="{px(vcy)}" x2="{px(vcx)}" y2="{px(vcy-KNOB_R*0.8)}" '
      f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
    A(f'<circle id="param_voicecount" cx="{px(vcx)}" cy="{px(vcy)}" r="0.5" fill="none" stroke="none"/>')

    # 5 output jacks + 1 input jack, evenly spaced at JACK_Y
    jack_ids = ["output_polygate", "output_polystepgate", "output_polyslegato",
                "output_polycv", "output_polyaccent", "input_quantcv"]
    n = len(jack_ids)
    spacing = (W - 2*MARGIN) / n
    for i, jid in enumerate(jack_ids):
        jx = MARGIN + spacing*(i+0.5)
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(JACK_R)}" fill="{jackface}" '
          f'stroke="{jackring}" stroke-width="{px(0.5)}"/>')
        A(f'<circle id="{jid}" cx="{px(jx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── Bottom wave footer (edge-tiling: 1 cycle per panel width) ──
    bottom_wave_tiling(A, px, W, WAVE_Y, WAVE_H, spine, theme["wave_op"], n=7)

    # ── L+R edge rails (double-rail seam design) ──
    # Left = outer frame (assembly far-left); Right = inner seam (base→lane).
    # At the abutment: base-right-rail + lane-left-rail = intentional DOUBLE-RAIL.
    panel_rails(A, px, W, 2.0, WAVE_Y + WAVE_H, theme)

    # Connect mark anchor
    A(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"StraitsBase_panel_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
