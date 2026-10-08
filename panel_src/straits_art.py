#!/usr/bin/env python3
"""StraitsR shared panel-art helpers — wave fields, edge-tiling bottom wave,
vertical separators, and shared constants for seamless base+lane abutment.

Imported by gen_straits_base.py and gen_straits_lane.py (and later Causeway's
lane generators). All functions take `A` (an append callable) and `px` (the
mm→px scale function) so they work with any panel generator's coordinate system.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import math

# ── Shared constants — MUST be identical across base + lane for seamless seam ──
H       = 128.5            # panel height (mm) — identical across all StraitsR panels
S       = 75 / 25.4       # mm → px scale (75 DPI)
TOP_RULE_H = 1.2           # red top-rule height (mm)
TOP_RULE_COLOR = "#d4001a"  # red top-rule colour

def make_px():
    """Return the px() function (mm → rounded px). Shared so base+lane are bit-aligned."""
    def px(v):
        return round(v * S, 2)
    return px

# ── Theme colours (dark/light) — shared bg + spine ──
THEMES = {
    "dark":  dict(bg="#14171b", spine="#5a6470", spinehi="#8a94a0", spinedot="#4c7ac0",
                  wave_op=0.5),
    "light": dict(bg="#dcdcdc", spine="#b0b8c0", spinehi="#8a94a0", spinedot="#4c6ab0",
                  wave_op=0.75),
}

# ── 1. Wave-field background (ported from gen_straits.py:77) ──────────────────
def wave_field(A, px, x0, y0, w, h, colour, wave_op=0.5, n=22):
    """Flowing contour 'water' lines across (x0,y0,w,h). Dense field — many
    fine contours with varied amplitude/phase so it reads as moving water."""
    seg = 40
    for i in range(n):
        yy = y0 + h * i / (n - 1)
        pts = []
        amp = 0.8 + (i % 4) * 0.55
        phase = i * 0.55
        freq = 0.55 + (i % 3) * 0.15
        for k in range(seg + 1):
            xx = x0 + w * k / seg
            wy = yy + amp * math.sin(k * freq + phase) + 0.35 * math.sin(k * 1.7 + phase * 1.3)
            pts.append(f"{px(xx)},{px(wy)}")
        op = wave_op * (0.55 + 0.45 * (i % 2))
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}" '
          f'stroke-width="0.4" stroke-opacity="{op:.2f}"/>')


# ── 2. Edge-tiling bottom wave footer ──────────────────────────────────────────
def bottom_wave_tiling(A, px, W, y, h, colour, wave_op=0.5, n=7):
    """Bottom wave footer with EDGE-TILING: periodic with period = panel width W,
    phase-anchored so f(0) = f(W) = 0. Every panel's left and right edges sit at
    the same wave phase/Y → seamless across ANY base+lane combination.

    Uses sin(2π * x / W) so the wave completes exactly 1 full cycle per panel.
    Different panel widths (base 12HP, lane 11HP) give slightly different
    wavelengths, but the EDGES match — reads as a continuous wave that gently
    changes wavelength at boundaries (like a natural water surface).
    """
    seg = 60
    for i in range(n):
        yy = y + h * i / (n - 1)
        pts = []
        amp = 0.6 + (i % 3) * 0.4
        phase_off = i * 0.4
        for k in range(seg + 1):
            xx = W * k / seg
            # Period = W: sin(2π * x / W) — guarantees f(0) = f(W) = 0
            wy = yy + amp * math.sin(2 * math.pi * k / seg + phase_off)
            pts.append(f"{px(xx)},{px(wy)}")
        op = wave_op * (0.6 + 0.4 * (i % 2))
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}" '
          f'stroke-width="0.4" stroke-opacity="{op:.2f}"/>')


# ── 3. Inter-panel vertical separator line ─────────────────────────────────────
def separator_line(A, px, x, y_top, y_bot, colour, sw=0.6):
    """Vertical separator spine at a panel boundary. Drawn at the LEFT edge of
    each lane panel (and the RIGHT edge of the base) so abutted sub-panels show
    the separator as the original Straits did between banks."""
    A(f'<line x1="{px(x)}" y1="{px(y_top)}" x2="{px(x)}" y2="{px(y_bot)}" '
      f'stroke="{colour}" stroke-width="{px(sw)}"/>')


# ── 4. Top-rule (red bar at the top) — shared for seam alignment ───────────────
def top_rule(A, px, W):
    """Red top-rule. MUST be identical (Y, height, colour) across base + lane
    for a seamless seam."""
    A(f'<rect x="0" y="0" width="{px(W)}" height="{px(TOP_RULE_H)}" fill="{TOP_RULE_COLOR}"/>')


# ── 5. Background fill — shared for seam alignment ─────────────────────────────
def bg_fill(A, px, W, bg_colour):
    """Background fill. MUST be identical colour across base + lane."""
    A(f'<rect width="{px(W)}" height="{px(H * S)}" fill="{bg_colour}"/>')


# ── 6. SVG header — shared opener with identical viewBox height ────────────────
def svg_open(A, px, W):
    """SVG opening tag with shared H/S so all panels have identical pixel height."""
    PW = px(W)
    PH = px(H)
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
