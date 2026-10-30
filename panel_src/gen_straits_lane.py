#!/usr/bin/env python3
"""Straits Lane Expander panel — ONE bank of 16 per-voice knobs.
Parameterised by lane tint (rest/accent/qmix/variation/legato). Seamless abutment:
same GRID_TOP, ROW_H, and knob grid as the base + other lane expanders.

Uses shared art helpers from straits_art.py for:
  - Identical top-rule/bg/S/H (seam alignment with base + other lane panels)
  - Wave-field background behind the knob grid (flowing contour "water" lines)
  - Edge-tiling bottom wave footer (continuous across base + any lane combination)
  - Left-edge vertical separator (base→lane or lane→lane boundary)

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 11
W  = HP * 5.08

TINTS = {
    "rest":      dict(tint="#3f7d78", wave="#2a5a56", knob="#1a2e2c", tinttext="#3f7d78"),
    "accent":    dict(tint="#e08a1a", wave="#8a5410", knob="#3a2a10", tinttext="#e08a1a"),
    "qmix":      dict(tint="#8060c0", wave="#4e3a78", knob="#241a3a", tinttext="#8060c0"),
    "variation": dict(tint="#7a9a3a", wave="#5a7a2a", knob="#2a3a14", tinttext="#7a9a3a"),
    "legato":    dict(tint="#3a6a9a", wave="#2a4a7a", knob="#14223a", tinttext="#3a6a9a"),
    # CC_PROMPT_FIX_SUBGATE_PANELS: the SUBGATE per-voice panels reuse THIS generator
    # via tint keys (the settled 6/6/4 165-wide layout — never a new generator or new
    # dimensions). The tints are the subgate identity colours (gen_straits_subgate_base.py
    # TINTS — the base's row tints): ratchet = the red family, ghost = the blue family.
    "ratchet":   dict(tint="#c05850", wave="#7a3630", knob="#3a1a16", tinttext="#c05850"),
    "ghost":     dict(tint="#5878b8", wave="#36486f", knob="#16203a", tinttext="#5878b8"),
}

MARGIN   = 5.0
BANK_W   = W - 2*MARGIN
TOP      = 16.0
N_ROWS   = 6
COLS     = [6, 6, 4]
ROW_H    = 14.77
KNOB_R   = 4.5
GRID_TOP = TOP + 2.0
JACK_Y   = 111.1
WAVE_Y   = JACK_Y + 5.5
WAVE_H   = 5.5

def gen(tintkey, dark=True, subgate=False):
    # subgate=True (CC_PROMPT_FIX_SUBGATE_PANELS): the knob grid, dimensions, 6/6/4
    # layout, wave field and rails are UNCHANGED; the only differences are (1) the
    # ONE 16-channel mod jack in the already-reserved JACK_Y row (the Causeway law —
    # a Straits knob ships with its mod path; the lane panels leave that row to the
    # wave), and (2) the TINT-NEUTRAL anchor prefix param_sgv_ (one widget class
    # binds all eight subgate panels, so the anchors cannot carry the tint key).
    t = TINTS[tintkey]
    theme = THEMES["dark" if dark else "light"]
    bg = theme["bg"]
    spine = theme["spine"]
    jackface = "#0c0e11" if dark else "#e2ddd2"
    jackring = "#4a4a4a" if dark else "#b0a898"
    prefix = "param_sgv_" if subgate else f"param_{tintkey}_"
    px = make_px()
    o = []; A = o.append
    anchors = []   # the kit anchor layer (the audit's read_anchors requires it)
    K = anchors.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    top_rule(A, px, W)

    # ── Wave-field background behind the knob grid (ported from gen_straits.py) ──
    wave_field(A, px, MARGIN, TOP, BANK_W, N_ROWS*ROW_H, t["wave"], theme["wave_op"], n=18)

    # Tint band
    A(f'<rect x="{px(MARGIN-1)}" y="{px(TOP-4)}" width="{px(BANK_W+2)}" height="{px(N_ROWS*ROW_H+6)}" '
      f'rx="{px(1.5)}" fill="{t["tint"]}" fill-opacity="0.08" stroke="{t["tint"]}" '
      f'stroke-width="0.3" stroke-opacity="0.45"/>')
    # Coloured identifying dot above the band. Lane panels: centred (the lane
    # idiom). Subgate panels: the LEFT gutter — the centre carries the LEVEL +
    # DECISION captions there (the widget's lettering), so a centred dot would
    # sit right on the decision text.
    _dot_cx = (MARGIN - 2.5) if subgate else (W / 2)
    A(f'<circle cx="{px(_dot_cx)}" cy="{px(TOP-6)}" r="{px(1.4)}" fill="{t["tint"]}"/>')

    # Knob grid: 3 cols, 6/6/4, col-major (v0 = mono top-left)
    cw = BANK_W / 3
    v = 0
    for c, nrows in enumerate(COLS):
        roff = (N_ROWS - nrows) / 2.0
        for r in range(nrows):
            cx = MARGIN + cw*(c+0.5)
            cy = GRID_TOP + ROW_H*(r+roff+0.5)
            mono = (v == 0)
            face = t["knob"]
            ring = t["tint"]
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{face}" '
              f'stroke="{ring}" stroke-width="{px(0.8 if mono else 0.5)}"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
            if mono:
                A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R+1.0)}" fill="none" '
                  f'stroke="#d4001a" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')
            # Anchor marker (invisible — SvgPanelKit finds by id; emitted in the
            # kit components layer below, where the anchor audit reads them)
            K(f'<circle id="{prefix}{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
            v += 1

    # The subgate panel's ONE 16-channel mod jack (the reserved JACK_Y row — the
    # Causeway mod-coverage law). Anchor input_sgmod (REAL coordinates — the kit
    # places the bound jack by them).
    if subgate:
        A(f'<circle cx="{px(W/2)}" cy="{px(JACK_Y)}" r="{px(4.0)}" fill="{jackface}" '
          f'stroke="{t["tint"]}" stroke-width="{px(0.6)}" stroke-opacity="0.8"/>')
        K(f'<circle id="input_sgmod" cx="{px(W/2)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── Bottom wave footer (edge-tiling: 1 cycle per panel width) ──
    bottom_wave_tiling(A, px, W, WAVE_Y, WAVE_H, spine, theme["wave_op"], n=7)
    # ── L+R edge rails (double-rail seam design) ──
    # Each edge is an inner seam (base↔lane or lane↔lane). At the abutment:
    # panel-A-right-rail + panel-B-left-rail = intentional DOUBLE-RAIL.
    panel_rails(A, px, W, 2.0, WAVE_Y + WAVE_H, theme)

    # Connect mark (the visible dot stays out of the anchor layer; only the anchor is in it)
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
    # The subgate per-voice panels (CC_PROMPT_FIX_SUBGATE_PANELS, Rodney DECIDED option
    # (a)): ONE 6/6/4 165-wide panel per (subgate level x decision) — {RATCHET, GHOST} x
    # {REST, LEGATO, ACCENT, ADVANCE} — eight panels, each 16 voices x ONE decision, the
    # lane layout EXACTLY (col-major v0=mono top-left, the wave field, the seam rails)
    # plus the one 16-ch mod jack. NO crammed multi-decision panel, NO new layout.
    SUBGATE_PANELS = [
        ("ratchet", "rest"), ("ratchet", "legato"), ("ratchet", "accent"), ("ratchet", "advance"),
        ("ghost",   "rest"), ("ghost",   "legato"), ("ghost",   "accent"), ("ghost",   "advance"),
    ]
    for lv, dec in SUBGATE_PANELS:
        for dark in [True, False]:
            suffix = "dark" if dark else "light"
            path = os.path.join(outdir, f"StraitsSubgatePanel_{lv}_{dec}_{suffix}.svg")
            with open(path, 'w') as f:
                f.write(gen(lv, dark, subgate=True))
            print(f"Generated {path}")
    # The lane panels (the original behaviour, unchanged)
    tintkey = sys.argv[1] if len(sys.argv) > 1 else "qmix"
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"StraitsLane_{tintkey}_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(tintkey, dark))
        print(f"Generated {path}")
