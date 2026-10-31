#!/usr/bin/env python3
"""SandsMacroSendSubpanel panel — the Macro send-knob subpanel.

Spec: docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md §"MACRO PER-VOICE
SENDS → their own subpanel" + §"The MACRO SEND-KNOB subpanel ENCAPSULATES ALL
Macro→East sends". 26HP. Layout:

  7 lane rows x 12 knob columns in three groups — SEND (LEN/OFF/ROT/SPR, the
  existing per-voice mix-in sends moved off the Macro visual's right strip),
  TAP (LOR/SPR, the PRE/POST taps moved with them), DIST SEND (P1/P2/TILT/
  LEVEL/MASS/WARP, the new distribution-param send amounts — the per-voice
  scalers of the Macro-sent global deltas at the fold). Lane names are drawn by
  the widget in the left gutter.

Text is NOT in the SVG (nanosvg drops <text>) — the widget draws the title, the
group labels, the column captions, and the lane names (the family's lettering
idiom). The SVG carries shapes + invisible anchors only, inside a
<g inkscape:label="components"> layer so test/audit_anchor_bind.py can gate the
binds (the module opts in).

Straits seam art (top rule / edge rails / bottom wave footer) via straits_art.py
so the abutment reads as the family's deliberate seam (the StraitsSeamFiller
redraws the live seam colours; the panel art carries the static rails).

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from straits_art import *

HP = 26
W  = HP * 5.08          # 132.08 mm

# ── grid geometry (mm) — MUST match the widget's column centres ───────────────
LANES       = 7
ROW_TOP     = 27.0       # centre y of lane row 0
ROW_PITCH   = 12.6
SEND_X0     = 20.0       # the mix-in send columns (LEN/OFF/ROT/SPR)
SEND_DX     = 9.0
N_SEND      = 4
TAP_X0      = 58.0       # the PRE/POST tap columns (LOR/SPR)
TAP_DX      = 9.0
N_TAP       = 2
DIST_X0     = 78.0       # the distribution-param send columns (P1..WARP)
DIST_DX     = 9.0
N_DIST      = 6
KNOB_R      = 4.2
TAP_R       = 3.2        # the taps read as trimpots (the Macro strip's rhythm)
GUTTER_R    = 14.0       # the lane-name gutter's right edge (widget draws the names)
FOOTER_Y    = 113.0
BAND_TINT_D = "#2a5a56"  # the family's teal band (dark); light theme lightens below
BAND_TINT_L = "#9fc4c0"

def col_x(x0, dx, n):
    return [x0 + i * dx for i in range(n)]

def gen(dark=True):
    theme = THEMES["dark" if dark else "light"]
    # ── the SEAM DESIGN is the HOSTS', not the Straits lanes' ────────────────
    # These subpanels seat ONLY against the Macro visual (the spec's attachment
    # sides), whose edge design is dotmod_design's: bg #16181c dark / #e8e8ea
    # light and NO red top rule. The straits_art THEMES bg (#14171b) + red rule
    # matched StraitsBase+StraitsLane — at a flush host junction they read as a
    # colour step plus a rule that starts/stops dead at the seam: the exact
    # "not snapped together like the straits example" report.
    bg    = "#16181c" if dark else "#e8e8ea"   # dotmod_design theme().bg — the host's
    spine = theme["spine"]
    band  = BAND_TINT_D if dark else BAND_TINT_L
    face  = "#1a2226" if dark else "#e8e8e8"   # knob face (theme-conditional)
    px = make_px()
    o = []; A = o.append

    svg_open(A, px, W)
    bg_fill(A, px, W, bg)
    # (top_rule DROPPED — the host panel carries no red top rule; a rule that
    # exists only right of the seam is a visible "not joined" tell. The
    # StraitsSeamFiller wiring on the widget draws bg-only for the same reason.)

    send_x = col_x(SEND_X0, SEND_DX, N_SEND)
    tap_x  = col_x(TAP_X0,  TAP_DX,  N_TAP)
    dist_x = col_x(DIST_X0, DIST_DX, N_DIST)

    # ── lane rows: tint band per row-group + group separators ─────────────────
    grid_h = LANES * ROW_PITCH
    A(f'<rect x="{px(GUTTER_R - 4.0)}" y="{px(ROW_TOP - ROW_PITCH/2 + 1.2)}" '
      f'width="{px(dist_x[-1] + KNOB_R + 3.0 - (GUTTER_R - 4.0))}" '
      f'height="{px(grid_h + 1.2)}" rx="{px(1.5)}" fill="{band}" fill-opacity="0.08" '
      f'stroke="{band}" stroke-width="0.3" stroke-opacity="0.45"/>')
    # faint vertical separators between the three groups
    for gx in ((send_x[-1] + tap_x[0]) * 0.5, (tap_x[-1] + dist_x[0]) * 0.5):
        A(f'<line x1="{px(gx)}" y1="{px(ROW_TOP - ROW_PITCH/2 + 2.0)}" '
          f'x2="{px(gx)}" y2="{px(ROW_TOP + grid_h - ROW_PITCH/2)}" '
          f'stroke="{spine}" stroke-width="{px(0.3)}" stroke-opacity="0.35"/>')

    # knob faces per row/col (the anchors ride the same centres)
    for l in range(LANES):
        cy = ROW_TOP + l * ROW_PITCH
        for cx in send_x + dist_x:
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{face}" '
              f'stroke="{spine}" stroke-width="{px(0.5)}" stroke-opacity="0.7"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy - KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')
        for cx in tap_x:
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(TAP_R)}" fill="{face}" '
              f'stroke="{spine}" stroke-width="{px(0.45)}" stroke-opacity="0.65"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy - TAP_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.45)}" stroke-opacity="0.75"/>')

    # ── bottom wave footer (edge-tiling, family seam) ─────────────────────────
    bottom_wave_tiling(A, px, W, FOOTER_Y, H - FOOTER_Y - 6.0, spine, theme["wave_op"], n=7)

    # ── L+R edge rails (double-rail seam design, Straits family) ──────────────
    panel_rails(A, px, W, 2.0, FOOTER_Y, theme)

    # ── components layer: the anchors the widget binds (audit-gated) ──────────
    A(f'<g inkscape:label="components">')
    for l in range(LANES):
        cy = ROW_TOP + l * ROW_PITCH
        for i, cx in enumerate(send_x):
            A(f'<circle id="param_msend_{l}_{i}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        for k, cx in enumerate(tap_x):
            A(f'<circle id="param_mtap_{l}_{k}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        for p, cx in enumerate(dist_x):
            A(f'<circle id="param_mdist_{l}_{p}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
    A(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')

    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"SandsMacroSend_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
