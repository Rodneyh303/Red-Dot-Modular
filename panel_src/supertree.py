#!/usr/bin/env python3
"""Parametric Supertree — reverse-engineered from the master Monsoon panel.

Every rule below was measured from `res/panels/Monsoon_panel_dark_monsoon.svg` (all 6 trees),
not invented. A tree is defined by ONE free parameter, `rx` (canopy x-radius), plus its
position (cx, base_y).

  canopy:   ry = rx / 3.5727                     (identical ratio on all 6 master trees)
  trunk:    height    = 3.5722 * rx              (same constant — a happy coincidence)
            base_w    = 1.3850 * rx
            top_w     = 0.3461 * rx
  fronds:   10 branches, θ_i = -180° + 180°*(i+1)/11   (even spacing, 11 divisions)
            start ON the canopy ellipse; length = 0.328 * rx
            direction = ellipse normal with the x-component weighted x1.88
            (fitted: 1.26px RMS against master's 27.51px canopy — <5%)
  rungs:    4, at f = (y-cy)/height = 0.24, 0.40, 0.58, 0.78   (Δf = 0.16 + 0.02k)
            half_width(f) = rx * (0.3359 + 0.317*(f - 0.24))   (linear; perspective ladder)

NANOSVG NOTE: master fills the trunk+canopy with `url(#treeGrad)` — a vertical linearGradient
(#18202a, opacity 0 → 0.70 → 0.90). nanosvg CANNOT render gradients, so in Rack the trees are
currently drawn wrong. Here the fade is reproduced with stacked solid slices of stepped opacity.
"""
import math

RY_RATIO   = 3.5727
H_RATIO    = 3.5722
BASE_W_R   = 1.3850
TOP_W_R    = 0.3461
N_FRONDS   = 10
FROND_LEN  = 0.3280
FROND_XW   = 1.88
RUNG_F     = [0.24, 0.40, 0.58, 0.78]
RUNG_HW0   = 0.3359
RUNG_HW_SL = 0.317

def supertree(cx, base_y, rx, t, slices=14, ochre=True):
    """Emit SVG elements for one supertree. Returns a list of strings (panel px coords)."""
    o = []
    ry     = rx / RY_RATIO
    height = H_RATIO * rx
    cy     = base_y - height
    wb, wt = BASE_W_R * rx, TOP_W_R * rx

    def edge(f):                       # trunk half-width at fraction f down from canopy
        return (wt + (wb - wt) * f) / 2

    # ── trunk: stacked slices, opacity stepping 0 → 0.90 downward (flat stand-in for treeGrad) ──
    for k in range(slices):
        f0, f1 = k / slices, (k + 1) / slices
        y0, y1 = cy + height * f0, cy + height * f1
        op = 0.90 * min(1.0, (f0 / 0.60)) if f0 < 0.60 else 0.90
        if op < 0.02:
            continue
        h0, h1 = edge(f0), edge(f1)
        o.append(f'<polygon points="{cx-h0:.2f},{y0:.2f} {cx+h0:.2f},{y0:.2f} '
                 f'{cx+h1:.2f},{y1:.2f} {cx-h1:.2f},{y1:.2f}" '
                 f'fill="{t["tree"]}" fill-opacity="{op:.2f}" stroke="none"/>')

    # ── rungs (perspective ladder) ──
    for f in RUNG_F:
        y  = cy + height * f
        hw = rx * (RUNG_HW0 + RUNG_HW_SL * (f - 0.24))
        o.append(f'<line x1="{cx-hw:.2f}" y1="{y:.2f}" x2="{cx+hw:.2f}" y2="{y:.2f}" '
                 f'stroke="{t["rung"]}" stroke-width="0.6" stroke-opacity="0.40"/>')

    # ── canopy disc (side-on ellipse) ──
    o.append(f'<ellipse cx="{cx:.2f}" cy="{cy:.2f}" rx="{rx:.2f}" ry="{ry:.2f}" '
             f'fill="{t["tree"]}" fill-opacity="0.90" stroke="none"/>')

    # ── 10 fan fronds, starting on the ellipse, splaying along the weighted normal ──
    for i in range(N_FRONDS):
        th = math.radians(-180.0 + 180.0 * (i + 1) / (N_FRONDS + 1))
        sx, sy = cx + rx * math.cos(th), cy + ry * math.sin(th)
        dx, dy = math.cos(th) / rx * FROND_XW, math.sin(th) / ry
        m = math.hypot(dx, dy); dx, dy = dx / m, dy / m
        L = FROND_LEN * rx
        col = t["ochre"] if (ochre and i in (0, N_FRONDS - 1)) else t["rung"]
        op  = 0.65 if (ochre and i in (0, N_FRONDS - 1)) else 0.40
        o.append(f'<line x1="{sx:.2f}" y1="{sy:.2f}" x2="{sx+dx*L:.2f}" y2="{sy+dy*L:.2f}" '
                 f'stroke="{col}" stroke-width="0.5" stroke-opacity="{op:.2f}"/>')
    return o

# Palettes. Master's own fill (#18202a on #0d1117) is near-invisible — faithfully reproducing it
# reproduced the problem. These lift the tree well clear of the ground while keeping master's OCHRE
# accent (#b87820 family), which is the colour Rodney wants (NOT purple).
THEME_DARK  = dict(tree="#2c3a4a", rung="#5b6f84", ochre="#d99a2b")
THEME_LIGHT = dict(tree="#8794a2", rung="#4b5866", ochre="#a06a12")
# The original, for reference / A-B against master:
THEME_MASTER = dict(tree="#18202a", rung="#222830", ochre="#b87820")
