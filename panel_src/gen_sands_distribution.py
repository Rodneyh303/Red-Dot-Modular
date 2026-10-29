#!/usr/bin/env python3
"""SandsDistributionExpander panel — the distribution sub-expander (dual role).

Spec: docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md.
52HP (the Macro visual's width — the family's subpanel gauge). Layout:

  7 lane rows x 6 distribution params (p1, p2, tilt, level, mass, warp) as knob
  columns + one MOD jack column per param (42 jacks — the spec's "each with its
  own MOD JACK": the dynamic-MO routes need patchable modulation), a lane focus
  button per row (which lane the permanent editor shows), and the permanent
  DistributionCurveWidget region on the right (the enlarged popup editor given a
  permanent home; the widget draws its own bezel/gutters, the SVG reserves the
  region + the faint inset).

Text is NOT in the SVG (nanosvg drops <text>) — the widget draws the title, the
column captions, and the lane names (the Cantonment lettering idiom). The SVG
carries shapes + invisible anchors only, inside a <g inkscape:label="components">
layer so test/audit_anchor_bind.py can gate the binds (the module opts in).

Straits seam art (top rule / edge rails / bottom wave footer) via straits_art.py
so the abutment reads as the family's deliberate seam (the StraitsSeamFiller
redraws the live seam colours; the panel art carries the static rails).

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 52
W  = HP * 5.08          # 264.16 mm

# ── grid geometry (mm) ────────────────────────────────────────────────────────
LANES       = 7
N_PARAMS    = 6          # p1, p2, tilt, level(ℓ), mass(m), warp(w)
ROW_TOP     = 27.0       # centre y of lane row 0
ROW_PITCH   = 12.6
KNOB_X0     = 20.0       # knob column cx = KNOB_X0 + p * KNOB_DX
KNOB_DX     = 10.5
KNOB_R      = 4.2
JACK_X0     = 86.0       # mod-jack column cx = JACK_X0 + p * JACK_DX
JACK_DX     = 9.0
JACK_R      = 3.2
FOCUS_X     = 12.5       # lane focus button cx
FOCUS_R     = 2.4
EDITOR_X0   = 141.0      # permanent curve-editor region (the widget hosts itself)
EDITOR_Y0   = 20.0
EDITOR_X1   = W - 4.0
EDITOR_Y1   = 110.0
FOOTER_Y    = 113.0
BAND_TINT_D = "#2a5a56"  # the family's teal band (dark); light theme lightens below
BAND_TINT_L = "#9fc4c0"

def gen(dark=True):
    theme = THEMES["dark" if dark else "light"]
    bg    = theme["bg"]
    spine = theme["spine"]
    band  = BAND_TINT_D if dark else BAND_TINT_L
    face  = "#1a2226" if dark else "#e8e8e8"   # knob face (theme-conditional)
    px = make_px()
    o = []; A = o.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    top_rule(A, px, W)

    # ── lane rows: tint band per row + knob/jack/focus anchors ────────────────
    grid_h = LANES * ROW_PITCH
    A(f'<rect x="{px(2.0)}" y="{px(ROW_TOP - ROW_PITCH/2 + 1.2)}" width="{px(EDITOR_X0 - 4.0)}" '
      f'height="{px(grid_h + 1.2)}" rx="{px(1.5)}" fill="{band}" fill-opacity="0.08" '
      f'stroke="{band}" stroke-width="0.3" stroke-opacity="0.45"/>')

    # knob + jack faces per row/col (the anchors ride the same centres)
    for l in range(LANES):
        cy = ROW_TOP + l * ROW_PITCH
        # knob faces
        for p in range(N_PARAMS):
            cx = KNOB_X0 + p * KNOB_DX
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{face}" '
              f'stroke="{spine}" stroke-width="{px(0.5)}" stroke-opacity="0.7"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy - KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')
        # mod jack sockets
        for p in range(N_PARAMS):
            cx = JACK_X0 + p * JACK_DX
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(JACK_R + 0.9)}" fill="#0c0f12" '
              f'stroke="{spine}" stroke-width="{px(0.5)}" stroke-opacity="0.55"/>')
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(JACK_R)}" fill="#14181d" '
              f'stroke="{spine}" stroke-width="{px(0.45)}" stroke-opacity="0.6"/>')
        # lane focus button seat
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
    for l in range(LANES):
        cy = ROW_TOP + l * ROW_PITCH
        for p in range(N_PARAMS):
            cx = KNOB_X0 + p * KNOB_DX
            A(f'<circle id="param_dexp_{l}_{p}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        for p in range(N_PARAMS):
            cx = JACK_X0 + p * JACK_DX
            A(f'<circle id="input_dexpmod_{l}_{p}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="param_dfocus_{l}" cx="{px(FOCUS_X)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
    A(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')

    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"SandsDistribution_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
