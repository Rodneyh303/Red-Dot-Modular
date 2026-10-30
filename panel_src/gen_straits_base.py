#!/usr/bin/env python3
"""Straits Base panel — the ONE base surface (CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE):
the existing frame + IO (5 poly-cable outs + quantiser CV in) + voice count, PLUS the
subgate GLOBAL grid (2 knob rows x 4 decision columns + their mod jacks — the Causeway
mod path, born together). Lane expanders dock right; the subgate per-voice panels dock
after the lane run.

The subgate grid section is gen_straits_subgate_base.py's artwork VERBATIM (same 16HP
width, so MARGIN_L/COL_PITCH/rows transfer with identical numbers) — that generator is
RETIRED by the merge (the subgate base module is deleted; its panel joins this one).

Text is NOT in the SVG (nanosvg drops <text>) — the widget draws the title, the row
labels, the column captions, the MOD + VOICES labels (the Cantonment lettering idiom).
The SVG carries shapes + the kit components anchor layer:
  param_voicecount    — the voice-count knob (the freed band below the grid)
  param_sbg_{l}_{d}   — the 8 global subgate knobs (l = level row 0/1, d = dec 0..3)
  mod_sbg_{l}_{d}     — the 8 global mod jacks (same grid, lower rows)
  output_* / input_quantcv — the 6 IO jacks
  light_connect       — the connect-mark anchor

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 16
W  = HP * 5.08

MARGIN   = 5.0
MARGIN_L = 17.0              # left gutter — the widget draws the RATCHET/GHOST row labels here
JACK_Y   = 111.1             # the IO row (5 poly-cable outs + the quantiser CV in)
JACK_R   = 4.0
KNOB_R   = 4.5
WAVE_Y   = JACK_Y + 5.5      # bottom wave footer Y
WAVE_H   = 5.5

# ── the subgate global grid (gen_straits_subgate_base.py's geometry VERBATIM) ──
SG_BANK_W    = W - MARGIN_L - MARGIN
SG_COLS      = 4
SG_COL_PITCH = SG_BANK_W / SG_COLS
SG_KNOB_Y    = [30.0, 48.0]   # RATCHET row, GHOST row
SG_MOD_Y     = [63.0, 77.0]   # the matching mod rows
VOICE_Y      = 91.0           # the voice-count knob — the freed band below the grid

# Row tints — the RATCHET/GHOST identity (widget lettering reuses these).
TINTS = {
    "ratchet": "#c05850",
    "ghost":   "#5878b8",
}

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
    anchors = []   # the kit anchor layer (the anchor audit's read_anchors requires it)
    K = anchors.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    top_rule(A, px, W)

    # ── The subgate global grid (the retired gen_straits_subgate_base.py, verbatim):
    # wave field, 2 row bands + gutter dots, 4 knobs + 4 mod jacks per row ──
    wave_field(A, px, MARGIN_L, 16.0, SG_BANK_W, 88.0, "#3a4048", theme["wave_op"], n=14)

    for l in range(2):
        tint = TINTS["ratchet" if l == 0 else "ghost"]
        # Row band + identifying dot (the lane idiom)
        A(f'<rect x="{px(MARGIN_L-1)}" y="{px(SG_KNOB_Y[l]-7)}" width="{px(SG_BANK_W+2)}" '
          f'height="{px(26)}" rx="{px(1.5)}" fill="{tint}" fill-opacity="0.08" '
          f'stroke="{tint}" stroke-width="0.3" stroke-opacity="0.45"/>')
        A(f'<circle cx="{px(MARGIN_L-3.0)}" cy="{px(SG_KNOB_Y[l])}" r="{px(1.3)}" fill="{tint}"/>')

        for d in range(SG_COLS):
            cx = MARGIN_L + SG_COL_PITCH*(d+0.5)
            # knob (seat ring; the widget draws the StoreKnob face over it)
            A(f'<circle cx="{px(cx)}" cy="{px(SG_KNOB_Y[l])}" r="{px(KNOB_R)}" fill="{knobface}" '
              f'stroke="{tint}" stroke-width="{px(0.5)}"/>')
            A(f'<line x1="{px(cx)}" y1="{px(SG_KNOB_Y[l])}" x2="{px(cx)}" y2="{px(SG_KNOB_Y[l]-KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
            K(f'<circle id="param_sbg_{l}_{d}" cx="{px(cx)}" cy="{px(SG_KNOB_Y[l])}" r="0.5" fill="none" stroke="none"/>')
            # mod jack (the knob's Causeway path — born together)
            A(f'<circle cx="{px(cx)}" cy="{px(SG_MOD_Y[l])}" r="{px(JACK_R)}" fill="{jackface}" '
              f'stroke="{tint}" stroke-width="{px(0.6)}" stroke-opacity="0.8"/>')
            K(f'<circle id="mod_sbg_{l}_{d}" cx="{px(cx)}" cy="{px(SG_MOD_Y[l])}" r="0.5" fill="none" stroke="none"/>')

    # ── Voice count knob (centred in the freed band below the grid) ──
    vcx, vcy = W/2, VOICE_Y
    A(f'<circle cx="{px(vcx)}" cy="{px(vcy)}" r="{px(KNOB_R)}" fill="{knobface}" '
      f'stroke="{knobring}" stroke-width="{px(0.5)}"/>')
    A(f'<line x1="{px(vcx)}" y1="{px(vcy)}" x2="{px(vcx)}" y2="{px(vcy-KNOB_R*0.8)}" '
      f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
    K(f'<circle id="param_voicecount" cx="{px(vcx)}" cy="{px(vcy)}" r="0.5" fill="none" stroke="none"/>')

    # ── The IO row: 5 poly-cable outputs + the quantiser CV in ──
    jack_ids = ["output_polygate", "output_polystepgate", "output_polyslegato",
                "output_polycv", "output_polyaccent", "input_quantcv"]
    n = len(jack_ids)
    spacing = (W - 2*MARGIN) / n
    for i, jid in enumerate(jack_ids):
        jx = MARGIN + spacing*(i+0.5)
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(JACK_R)}" fill="{jackface}" '
          f'stroke="{jackring}" stroke-width="{px(0.5)}"/>')
        K(f'<circle id="{jid}" cx="{px(jx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── Bottom wave footer (edge-tiling) + rails ──
    bottom_wave_tiling(A, px, W, WAVE_Y, WAVE_H, spine, theme["wave_op"], n=7)
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
        path = os.path.join(outdir, f"StraitsBase_panel_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
