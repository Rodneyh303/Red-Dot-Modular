#!/usr/bin/env python3
"""PhaseDistortion panel — the STANDALONE phase-distortion module (the §13
comparison build).

Spec: PHASE_DISTRIBUTION_SPEC §13 + docs/design/CC_PROMPT_PHASE_DISTORTION_MODULE.md.
34HP. Built ALONGSIDE the SandsDistributionExpander (A) — do not modify A. Layout:

  8 channel rows x 8 distribution params (p1, p2, tilt, level(ℓ), mass(m),
  warp(w), gaploc(a), gapsize(b−a)) as knob columns in the expander's VISUAL
  order (P1 P2 TILT LEVEL MASS | GAPLOC GAPSIZE | WARP — the gap pair
  immediately beside the atom pair, WARP trailing), one PHASE IN jack and one
  PHASE OUT jack per channel (the channel IS the jack — a Monsoon patched to
  output k uses parameter set k), a channel-focus button per row (which set the
  permanent DistributionCurveWidget editor shows), and the permanent
  DistributionCurveWidget region on the right (the §10 effective plot; the
  widget draws its own bezel/gutters, the SVG reserves the region + the faint
  inset).

Text is NOT in the SVG (nanosvg drops <text>) — the widget draws the title, the
column captions, the channel numerals, and the one-sample-hop note (the
Cantonment lettering idiom). The SVG carries shapes + invisible anchors only,
inside a <g inkscape:label="components"> layer so test/audit_anchor_bind.py can
gate the binds (the module opts in).

Panel design follows the SandsDistributionExpander's (the distribution-module
conventions): the hosts' dotmod_design background (#16181c dark / #e8e8ea
light — no red top rule), the family's teal row band, L+R edge rails, the
bottom wave footer, via straits_art.py.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 34
W  = HP * 5.08          # 172.72 mm

# ── grid geometry (mm) — MUST match PhaseDistortionWidget's constants ─────────
CHANNELS    = 8
N_PARAMS    = 8          # p1, p2, tilt, level, mass, warp, gaploc, gapsize
VISUAL_ORDER = [0, 1, 2, 3, 4, 6, 7, 5]
ROW_TOP     = 24.0       # centre y of channel row 0
ROW_PITCH   = 12.0
KNOB_X0     = 24.0       # knob column cx = KNOB_X0 + col * KNOB_DX (col = the VISUAL column)
KNOB_DX     = 10.0
KNOB_R      = 4.2
IN_X        = 14.5       # phase-in jack cx per row
OUT_X       = 103.5      # phase-out jack cx per row
JACK_R      = 3.2
FOCUS_X     = 8.0        # channel-focus button cx
FOCUS_R     = 2.4
EDITOR_X0   = 112.0      # permanent curve-editor region (the widget hosts itself)
EDITOR_Y0   = 20.0
EDITOR_X1   = W - 4.0
EDITOR_Y1   = 110.0
FOOTER_Y    = 113.0
BAND_TINT_D = "#2a5a56"  # the family's teal band (dark); light theme lightens below
BAND_TINT_L = "#9fc4c0"

def gen(dark=True):
    theme = THEMES["dark" if dark else "light"]
    # the hosts' dotmod_design background (the distribution expander's seam
    # decision, carried: this family's panels carry no red top rule)
    bg    = "#16181c" if dark else "#e8e8ea"
    spine = theme["spine"]
    band  = BAND_TINT_D if dark else BAND_TINT_L
    face  = "#1a2226" if dark else "#e8e8e8"   # knob face (theme-conditional)
    px = make_px()
    o = []; A = o.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)

    # ── channel rows: tint band per row block + knob/jack/focus anchors ───────
    grid_h = CHANNELS * ROW_PITCH
    A(f'<rect x="{px(2.0)}" y="{px(ROW_TOP - ROW_PITCH/2 + 1.2)}" width="{px(EDITOR_X0 - 4.0)}" '
      f'height="{px(grid_h + 1.2)}" rx="{px(1.5)}" fill="{band}" fill-opacity="0.08" '
      f'stroke="{band}" stroke-width="0.3" stroke-opacity="0.45"/>')

    # knob + jack faces per row/col (the anchors ride the same centres; the ANCHOR
    # id carries the PARAM index, the POSITION carries the visual column — the
    # expander's discipline)
    for ch in range(CHANNELS):
        cy = ROW_TOP + ch * ROW_PITCH
        # knob faces
        for col in range(N_PARAMS):
            cx = KNOB_X0 + col * KNOB_DX
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{face}" '
              f'stroke="{spine}" stroke-width="{px(0.5)}" stroke-opacity="0.7"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy - KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')
        # phase in/out jack sockets (in = dark well with a lighter collar; out =
        # the same socket with the family's accent ring so the direction reads)
        accent = "#c8a24a" if not dark else "#c8a24a"
        for cx, ring in ((IN_X, spine), (OUT_X, accent)):
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(JACK_R + 0.9)}" fill="#0c0f12" '
              f'stroke="{ring}" stroke-width="{px(0.5)}" stroke-opacity="0.75"/>')
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(JACK_R)}" fill="#14181d" '
              f'stroke="{spine}" stroke-width="{px(0.45)}" stroke-opacity="0.6"/>')
        # channel-focus button seat
        A(f'<circle cx="{px(FOCUS_X)}" cy="{px(cy)}" r="{px(FOCUS_R + 0.8)}" fill="none" '
          f'stroke="{spine}" stroke-width="{px(0.4)}" stroke-opacity="0.5"/>')

    # ── permanent editor region: faint inset (the widget draws bezel/gutters) ─
    A(f'<rect x="{px(EDITOR_X0)}" y="{px(EDITOR_Y0)}" width="{px(EDITOR_X1 - EDITOR_X0)}" '
      f'height="{px(EDITOR_Y1 - EDITOR_Y0)}" rx="{px(1.5)}" fill="{band}" fill-opacity="0.05" '
      f'stroke="{spine}" stroke-width="0.3" stroke-opacity="0.35"/>')

    # ── bottom wave footer (edge-tiling, family seam) ─────────────────────────
    bottom_wave_tiling(A, px, W, FOOTER_Y, H - FOOTER_Y - 6.0, spine, theme["wave_op"], n=7)

    # ── L+R edge rails (double-rail seam design, Straits family) ──────────────
    panel_rails(A, px, W, 2.0, FOOTER_Y, theme)

    # ── components layer: the anchors the widget binds (audit-gated) ──────────
    A(f'<g inkscape:label="components">')
    for ch in range(CHANNELS):
        cy = ROW_TOP + ch * ROW_PITCH
        for col in range(N_PARAMS):
            cx = KNOB_X0 + col * KNOB_DX
            A(f'<circle id="param_pd_{ch}_{VISUAL_ORDER[col]}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="input_pdin_{ch}" cx="{px(IN_X)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="output_pdout_{ch}" cx="{px(OUT_X)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="param_pdfocus_{ch}" cx="{px(FOCUS_X)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')

    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"PhaseDistortion_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
