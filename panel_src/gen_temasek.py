#!/usr/bin/env python3
"""Temasek — the transform companion expander for Change Alley.
~40HP panel. Intra (within-block) verbs LEFT of centre, inter (across-block) RIGHT,
mirrored with jacks to the outside. Top-to-bottom: Collapse, Rotate, Reflect, Scatter.

Per verb × side × pin type (R/M):
  outside: domain jack + domain button
  inside:  codomain button + codomain jack
  centre:  grain knob  (+leader for Collapse, +step for Rotate)
  Scatter row additionally: fwd/back jacks at the outer edge

Panel art (nanosvg-safe): pre-colonial cartographic register — island silhouette,
rhumb lines, hachuring, soundings. All geometric; no text/gradient/filter/mask.
Widget draws all labels (nanosvg drops <text>).
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, svg_open, logo_embed, jack, trim, kit_shape

HP     = 40
PW_MM  = HP * 5.08    # 203.2
PH_MM  = 128.5
PW, PH = px(PW_MM), px(PH_MM)

# ── palette ──────────────────────────────────────────────────────────────────
def pal(dark):
    if dark:
        return dict(body="#18181a", ink="#e8e2d0", dim="#8a8578",
                    frame="#2e2e33", jackwell="#0a0b0c", jackring="#46464c",
                    well="#0f1012", wellring="#3a3a40", edrecess="#101113",
                    edborder="#2e2e33", tabband="#181820",
                    red="#d4001a", gold="#c8960c",
                    land="#1e1a14", sea="#0d1219", rhumb="#2a3040",
                    hatch="#1a1710", sound="#1c2030")
    return  dict(body="#e8e8ea", ink="#2a2a2e", dim="#888d96",
                 frame="#a8aeb6", jackwell="#dadce0", jackring="#9298a0",
                 well="#dcdee2", wellring="#a8aeb6", edrecess="#d8dade",
                 edborder="#c0c4ca", tabband="#cdd4dc",
                 red="#d4001a", gold="#a07808",
                 land="#d4cdb8", sea="#c0cad6", rhumb="#8898b0",
                 hatch="#b8b0a0", sound="#a0afc0")

# ── geometry ─────────────────────────────────────────────────────────────────
MARGIN   = 5.0      # mm from panel edge to outermost jack centre
CX       = PW_MM / 2
N_VERBS  = 4
N_ROWS   = N_VERBS * 2  # per side (R+M per verb)
ROW_H    = (PH_MM - 16.0) / N_ROWS   # ~14mm per row
ROW_TOP  = 8.0      # mm from top to first row centre

# Column layout per side (LEFT side; RIGHT side mirrors)
# From outer edge inward: jack_D  btn_D  [knob(s)]  btn_C  jack_C
J_OUTER   = MARGIN           # domain trigger jack (outermost)
BTN_D     = MARGIN + 8.0     # domain button
KNOB1     = MARGIN + 17.0    # first knob (grain)
KNOB2     = MARGIN + 26.0    # second knob (leader / step)
BTN_C     = MARGIN + 35.0    # codomain button
J_INNER   = MARGIN + 43.0    # codomain trigger jack (innermost)

# Scatter fwd/back: extra jacks at the very outer edge, below the scatter row pair
J_SCATTER_FWD  = MARGIN
J_SCATTER_BACK = MARGIN + 8.5

def rowY(verb, sub):
    """sub 0 = Rhythm, sub 1 = Melody"""
    return ROW_TOP + (verb * 2 + sub + 0.5) * ROW_H

def mirror(x_mm):
    """Mirror x around the panel centre for the right (inter) side."""
    return PW_MM - x_mm

# ── cartographic art helpers ──────────────────────────────────────────────────
def rhumb_lines(t, cx, cy, r, n=12, primary=4):
    """Radiating navigation lines from a compass point, nanosvg-safe (pure lines)."""
    els = []
    for i in range(n):
        angle = math.pi * 2 * i / n
        thick = 0.6 if (i % (n // primary) == 0) else 0.25
        ex = cx + math.cos(angle) * r
        ey = cy + math.sin(angle) * r
        stroke = t["rhumb"]
        els.append(f'<line x1="{cx:.1f}" y1="{cy:.1f}" x2="{px(ex):.1f}" y2="{px(ey):.1f}"'
                   f' stroke="{stroke}" stroke-width="{px(thick):.2f}" opacity="0.7"/>')
    return "\n".join(els)

def hachures(t, x0, y0, x1, y1, spacing=3.5, length=2.5, angle_deg=30):
    """Short parallel strokes suggesting terrain elevation (pre-modern map convention)."""
    els = []
    angle = math.radians(angle_deg)
    dx, dy = math.cos(angle) * px(length/2), math.sin(angle) * px(length/2)
    # fill a band with hachures
    cols = int((x1 - x0) / px(spacing)) + 1
    rows = int((y1 - y0) / px(spacing)) + 1
    for r in range(rows):
        for c in range(cols):
            cx2 = x0 + c * px(spacing) + (r % 2) * px(spacing / 2)
            cy2 = y0 + r * px(spacing)
            els.append(f'<line x1="{cx2-dx:.1f}" y1="{cy2-dy:.1f}" x2="{cx2+dx:.1f}" y2="{cy2+dy:.1f}"'
                       f' stroke="{t["hatch"]}" stroke-width="{px(0.3):.2f}" opacity="0.5"/>')
    return "\n".join(els)

import random
def soundings(t, x0, y0, x1, y1, n=40):
    """Scatter of small dots suggesting sea-depth markings."""
    rng = random.Random(0x7EA5EE4)
    els = []
    for _ in range(n):
        sx = rng.uniform(x0, x1)
        sy = rng.uniform(y0, y1)
        r2 = px(0.5)
        els.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="{r2:.2f}"'
                   f' fill="{t["sound"]}" opacity="0.6"/>')
    return "\n".join(els)

def island_silhouette(t):
    """Rough island outline in pre-survey style (simplified polygon)."""
    # Stylised island shape — not geographically accurate, evocative of old charts.
    pts = [
        (0.22, 0.55), (0.28, 0.48), (0.35, 0.44), (0.42, 0.42),
        (0.50, 0.40), (0.60, 0.41), (0.68, 0.44), (0.74, 0.48),
        (0.78, 0.54), (0.76, 0.60), (0.70, 0.64), (0.60, 0.66),
        (0.50, 0.67), (0.40, 0.66), (0.30, 0.63), (0.24, 0.59),
    ]
    coords = " ".join(f"{px(x * PW_MM):.1f},{px(y * PH_MM):.1f}" for x, y in pts)
    return (f'<polygon points="{coords}" fill="{t["land"]}" stroke="{t["rhumb"]}"'
            f' stroke-width="{px(0.4):.2f}" opacity="0.35"/>')

# ── panel generator ───────────────────────────────────────────────────────────
def gen(dark):
    t   = pal(dark)
    els = []
    E = els.append

    # Background
    E(f'<rect width="{PW:.1f}" height="{PH:.1f}" fill="{t["body"]}"/>')

    # ── cartographic art (behind controls) ──────────────────────────────────
    # Island silhouette
    E(island_silhouette(t))

    # Rhumb lines from compass point (lower-left quadrant, portolan convention)
    comp_x, comp_y = 0.15 * PW_MM, 0.80 * PH_MM
    E(rhumb_lines(t, comp_x, comp_y, 55.0))

    # Hachuring on the land area (centre band)
    lx0, ly0 = px(0.30 * PW_MM), px(0.42 * PH_MM)
    lx1, ly1 = px(0.70 * PW_MM), px(0.60 * PH_MM)
    E(hachures(t, lx0, ly0, lx1, ly1))

    # Soundings in the sea areas (left and right of island)
    E(soundings(t, px(0.05*PW_MM), px(0.45*PH_MM), px(0.25*PW_MM), px(0.70*PH_MM), n=20))
    E(soundings(t, px(0.75*PW_MM), px(0.45*PH_MM), px(0.95*PW_MM), px(0.70*PH_MM), n=20))

    # ── control columns ──────────────────────────────────────────────────────
    verb_names = ["COLLAPSE", "ROTATE", "REFLECT", "SCATTER"]
    for verb in range(N_VERBS):
        for sub in range(2):   # 0=Rhythm 1=Melody
            ry = rowY(verb, sub)
            pin_col = "#f0f0ee" if sub == 0 else t["red"]

            for side in range(2):   # 0=left/intra  1=right/inter
                flip = (side == 1)  # mirror for inter side

                def lx(x_mm):
                    return mirror(x_mm) if flip else x_mm

                # domain jack (outermost)
                E(jack(lx(J_OUTER), ry, t))
                # codomain jack
                E(jack(lx(J_INNER), ry, t))

                # Kit shape markers for buttons (widget draws actual buttons)
                for bx in [BTN_D, BTN_C]:
                    E(f'<circle cx="{px(lx(bx)):.1f}" cy="{px(ry):.1f}" r="{px(3.0):.1f}"'
                      f' fill="{t["frame"]}" stroke="{pin_col}" stroke-width="{px(0.6):.2f}"/>')

                # Grain knob shape
                E(trim(lx(KNOB1), ry, t, t["gold"]))

                # Second knob (Collapse=leader, Rotate=step) — omitted for Reflect/Scatter
                if verb in (0, 1):
                    E(trim(lx(KNOB2), ry, t, t["gold"]))

                # Scatter fwd/back jacks (extra, outside the domain jack)
                if verb == 3 and sub == 0:   # one fwd/back pair per side per type
                    fwd_y = rowY(verb, 0) + ROW_H * 0.25
                    bk_y  = rowY(verb, 0) + ROW_H * 0.75
                    E(jack(lx(J_SCATTER_FWD),  fwd_y, t))
                    E(jack(lx(J_SCATTER_BACK), bk_y,  t))

    # ── centre divider (thin line between intra and inter) ────────────────────
    E(f'<line x1="{px(CX):.1f}" y1="{px(4.0):.1f}" x2="{px(CX):.1f}" y2="{px(PH_MM-4.0):.1f}"'
      f' stroke="{t["dim"]}" stroke-width="{px(0.3):.2f}" opacity="0.4"/>')

    # ── logo ─────────────────────────────────────────────────────────────────
    E(logo_embed(dark, CX - 17.0, PH_MM - 11.0, 34.0))

    # ── title ─────────────────────────────────────────────────────────────────
    # Widget draws "TEMASEK" label (nanosvg drops <text>)

    out_dir = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    os.makedirs(out_dir, exist_ok=True)
    theme  = "dark" if dark else "light"
    fname  = os.path.join(out_dir, f"Temasek_panel_{theme}.svg")
    body   = "\n".join(els)
    with open(fname, "w") as f2:
        f2.write(svg_open(PW_MM, PH_MM) + "\n" + body + "\n</svg>\n")
    print(f"Temasek {theme}: {fname}  ({HP}HP {PW_MM:.1f}x{PH_MM:.1f}px)")

if __name__ == "__main__":
    gen(dark=True)
    gen(dark=False)
