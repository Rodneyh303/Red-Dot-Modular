#!/usr/bin/env python3
"""Straits Subgate Base panel — the subgate consumer-group GLOBAL tier.

2 knob rows (RATCHET / GHOST) x 4 decision columns (REST / LEG / ADV / ACC),
one mod jack under each knob (the Causeway mod path — the knob and its jack are
born together). Seamless abutment with the StraitsR family: identical H, top
rule, bg, wave footer and edge rails (straits_art.py).

Text is NOT in the SVG (nanosvg drops <text>) — the widget draws the title, the
row labels and the column captions (the Cantonment lettering idiom). The SVG
carries shapes + invisible anchors only:
  param_sbg_{l}_{d}  — knob anchor (l = level row 0/1, d = decision col 0..3)
  mod_sbg_{l}_{d}    — mod jack anchor (same grid, lower rows)
  light_connect      — the connect-mark anchor

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 16
W  = HP * 5.08

MARGIN   = 5.0
MARGIN_L = 17.0              # left gutter — the widget draws the RATCHET/GHOST row labels here
BANK_W   = W - MARGIN_L - MARGIN
COLS     = 4
COL_PITCH = BANK_W / COLS
KNOB_Y   = [30.0, 48.0]        # RATCHET row, GHOST row
JACK_Y   = [63.0, 77.0]        # matching mod rows
KNOB_R   = 4.5
JACK_R   = 4.0
WAVE_Y   = 118.0
WAVE_H   = 5.5

# Row tints — the RATCHET/GHOST identity (widget lettering reuses these).
TINTS = {
    "ratchet": "#c05850",
    "ghost":   "#5878b8",
}

def gen(dark=True):
    theme = THEMES["dark" if dark else "light"]
    bg = theme["bg"]
    jackface = "#0c0e11" if dark else "#e2ddd2"
    jackring = "#4a4a4a" if dark else "#b0a898"
    knobface = "#2a2e33" if dark else "#e8e2d6"
    px = make_px()
    o = []; A = o.append
    anchors = []   # the kit anchor layer (the anchor audit's read_anchors requires it)
    K = anchors.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    top_rule(A, px, W)

    # Wave-field behind the control area (family texture)
    wave_field(A, px, MARGIN_L, 16.0, BANK_W, 88.0, "#3a4048", theme["wave_op"], n=14)

    for l in range(2):
        tint = TINTS["ratchet" if l == 0 else "ghost"]
        # Row band + identifying dot (the lane idiom)
        A(f'<rect x="{px(MARGIN_L-1)}" y="{px(KNOB_Y[l]-7)}" width="{px(BANK_W+2)}" '
          f'height="{px(26)}" rx="{px(1.5)}" fill="{tint}" fill-opacity="0.08" '
          f'stroke="{tint}" stroke-width="0.3" stroke-opacity="0.45"/>')
        A(f'<circle cx="{px(MARGIN_L-3.0)}" cy="{px(KNOB_Y[l])}" r="{px(1.3)}" fill="{tint}"/>')

        for d in range(COLS):
            cx = MARGIN_L + COL_PITCH*(d+0.5)
            # knob (seat ring; the widget draws the StoreKnob face over it)
            A(f'<circle cx="{px(cx)}" cy="{px(KNOB_Y[l])}" r="{px(KNOB_R)}" fill="{knobface}" '
              f'stroke="{tint}" stroke-width="{px(0.5)}"/>')
            A(f'<line x1="{px(cx)}" y1="{px(KNOB_Y[l])}" x2="{px(cx)}" y2="{px(KNOB_Y[l]-KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
            K(f'<circle id="param_sbg_{l}_{d}" cx="{px(cx)}" cy="{px(KNOB_Y[l])}" r="0.5" fill="none" stroke="none"/>')
            # mod jack (the knob's Causeway path — born together)
            A(f'<circle cx="{px(cx)}" cy="{px(JACK_Y[l])}" r="{px(JACK_R)}" fill="{jackface}" '
              f'stroke="{tint}" stroke-width="{px(0.6)}" stroke-opacity="0.8"/>')
            K(f'<circle id="mod_sbg_{l}_{d}" cx="{px(cx)}" cy="{px(JACK_Y[l])}" r="0.5" fill="none" stroke="none"/>')

    # ── Bottom wave footer (edge-tiling) + rails ──
    bottom_wave_tiling(A, px, W, WAVE_Y, WAVE_H, theme["spine"], theme["wave_op"], n=7)
    panel_rails(A, px, W, 2.0, WAVE_Y + WAVE_H, theme)

    # ── SvgPanelKit component (anchor) layer — THE SINGLE GEOMETRY SOURCE ──
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    K(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    for a in anchors:
        A(a)
    A('</g>')
    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"StraitsSubgateBase_panel_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
