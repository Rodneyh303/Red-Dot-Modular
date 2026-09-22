#!/usr/bin/env python3
"""monsoon_art.py — mm-native generator for the Monsoon panel BASE ART.

Reverse-engineered from the live res/panels/Monsoon_panel_*_monsoon.svg
(see docs/design/MONSOON_PANEL_REVERSE_ENGINEER.md). All geometry is authored in millimetres;
`S` (px per mm) converts only at emit time:
    S75 = 600/203.2  — Rack 75-dpi panel space. THE TARGET. No scale wrapper.
    S96 = 768/203.2  — the live file's legacy 96-dpi art space (inside scale(0.78125)); A/B only.

PAINT RULE (measured against nanosvg, the parser Rack uses — see panel_src/tools/):
  nanosvg inherits fill/stroke from <g>, but does NOT compound opacity through groups — the
  INNERMOST opacity wins (a 0.55 ring inside a 0.65 group renders at 0.55 in Rack; a browser would
  give 0.36). So every element here carries its OWN fill, stroke, stroke-width and EFFECTIVE
  opacity. That reproduces what Rack shows today exactly, and makes the file render identically in
  nanosvg, cairosvg and browsers. No gradients, patterns, masks, url() or <text>.

Stroke widths/dash lengths were authored in 96-dpi px; they are stored in mm (/S96) so any S is exact.

INTENTIONAL DIVERGENCES from the live panel (both agreed with Rodney):
  1. Tree 4 canopy moved onto its trunk (116mm). The original drew it at 120mm, 4mm off the trunk,
     so its fronds floated off the canopy edge.
  (Considered and REJECTED: swapping to the shared dotmod_design.logo_embed(). Under nanosvg the
   canonical res/logo artwork draws the circuit trace OVER the letters with a dark halo, which slices
   the 'l' so it reads "moduiar" — the same defect visible on Change Alley. Monsoon's own variant draws
   the trace BEHIND the letters and reads correctly, so it is kept verbatim in panel_src/assets/.
   Follow-up for the family: fix res/logo to match this layering.)
"""
import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

W_MM, H_MM = 203.2, 128.5          # 40HP
S75 = 600 / 203.2
S96 = 768 / 203.2


def _f(v):
    return f"{v:.2f}"


def _w(px96):
    """A stroke width / dash length authored in 96-dpi px, as mm."""
    return px96 / S96


# ── Themes (dark measured from live; light still to be measured) ─────────────────────────────────
THEMES = {
    "dark": dict(
        bg="#121416", sky="#0e1420", cloud="#1e2530",
        tree_fill="#18202a", tree_line="#dc2626",
        rail="#dc2626", divider="#222830",
        big5_ring="#b87820", phase_ring="#6a6a6a",
        flyer_glow="#26a69a", flyer="#c8960c",
        tick="#888888", dot="#dc2626",
    ),
}

# ── Layout table (mm). THE single source for base-art positions. ─────────────────────────────────
SKY_BANDS = 10                      # 4mm bands from the top, opacity 0.95 -> 0.05
SKY_BAND_MM = 4.0

# clouds: (cx, cy, rx, ry) — hand-placed in the original, on a 1mm grid
CLOUDS = [(15, 10, 14, 7), (28, 8, 18, 8), (42, 11, 14, 7), (55, 9, 16, 7), (8, 14, 10, 5),
          (68, 10, 16, 6), (80, 16, 10, 5), (135, 8, 18, 7), (150, 6, 14, 6), (165, 9, 16, 5),
          (178, 7, 10, 5)]

BIG5_X0, BIG5_PITCH, BIG5_N, BIG5_Y = 16.0, 26.0, 5, 22.0   # Big-Five knob row
BIG5_R_OUT, BIG5_R_IN = 11.0, 7.5

PHASE_X0, PHASE_PITCH, PHASE_N, PHASE_Y = 148.0, 15.0, 3, 58.0   # BPM / LEN / OFFSET
PHASE_R_OUT, PHASE_R_IN = 6.0, 4.0

FLYER_C = (162.0, 30.0)             # Singapore Flyer = 16-step LED ring
FLYER_GLOW = [(20, "0.022"), (16, "0.027"), (12, "0.045"), (8, "0.047"), (4, "0.050")]
FLYER_R, FLYER_R_OUTER = 20.0, 24.0
FLYER_SPOKES, FLYER_SPOKE_R0, FLYER_SPOKE_R1 = 16, 18.5, 23.0

RAIL_X0, RAIL_X1 = 8.0, 195.2       # red skyway rails
RAIL_YS = (42.0, 78.0, 98.0)
BIG5_DROP_TOP = 33.0                # knob -> rail drop lines
LOWER_DROP_X0, LOWER_DROP_PITCH, LOWER_DROP_N, LOWER_DROP_BOT = 104.0, 18.0, 5, 100.1

DIVIDER_YS = (14.0, 42.0, 78.0, 98.0, 113.0)
DIVIDER_VX = 93.0                   # dashed vertical, from 98mm to the ground

STATUS_DOT = (199.2, 4.0, _w(4.0))  # red dot, top-right

LOGO_POS_MM = (290.8 / S96, 2.0 / S96)   # origin of the 717x190 logo space (live placement)
LOGO_SCALE_MM = 0.26 / S96               # logo units -> mm

# Supertrees: (trunk_cx, canopy_cx, height, base_width) — see supertree_geometry()
TREES = [
    (18.0,  18.0,  26.0, 10.08),
    (44.0,  44.0,  18.0,  7.92),
    (76.0,  76.0,  32.0, 12.60),
    (116.0, 116.0, 22.0,  9.00),    # canopy was 120 in the original (divergence 1)
    (152.0, 152.0, 28.0, 10.80),
    (186.0, 186.0, 20.0,  8.64),
]

# Fader ticks — absorbed from fader_level_markers.py. FADERS_MM is the ONLY place a fader x exists.
FADERS_MM = [7.5 + i * 9.0 for i in range(12)] + [119.0, 128.0]
FADER_Y_MM = 59.75
FADER_TOP_MM, FADER_BOT_MM = 45.0, 74.5
FADER_LEVELS = 9                    # divisions across the travel; the two end rows are dropped
TICK_LEN_MM = 3.4


# ── Supertree geometry (exact; fit residual < 0.0003 across all 6 live trees) ────────────────────
CANOPY_R = 0.28                     # rx = 0.28*h, ry = 0.28*rx
RUNG_F = (0.24, 0.40, 0.58, 0.78)   # rungs at these fractions of h below the canopy
RUNG_HW = (3 / 8, 11 / 24)          # half-width = bh*(3/8 + 11/24*f)
FROND_C = 0.11405
FROND_B, FROND_A = 3 * FROND_C, 7 * FROND_C / 3
N_FRONDS = 12


def supertree_geometry(tcx, ccx, h, bw, ground=H_MM):
    """Pure mm geometry. Fronds and rungs centre on the TRUNK; the canopy on canopy_cx."""
    rx, cy = CANOPY_R * h, ground - h
    ry = CANOPY_R * rx
    bh, th = bw / 2, bw / 8
    trunk = [(tcx - bh, ground), (tcx + bh, ground), (tcx + th, cy), (tcx - th, cy)]
    rungs = []
    for f in reversed(RUNG_F):      # emitted bottom-up, matching live element order
        hw = bh * (RUNG_HW[0] + RUNG_HW[1] * f)
        rungs.append((tcx - hw, cy + h * f, tcx + hw, cy + h * f))
    fronds = []
    for i in range(N_FRONDS):
        t = math.radians(180 - i * 180 / (N_FRONDS - 1))
        c, s = math.cos(t), math.sin(t)
        sx, sy = tcx + rx * c, cy - ry * s
        fronds.append((sx, sy, sx + FROND_A * rx * c, sy - (FROND_B - FROND_C * abs(c)) * rx))
    return dict(trunk=trunk, canopy=(ccx, cy, rx, ry), rungs=rungs, fronds=fronds)


# ── Emitters: every element carries its own paint ────────────────────────────────────────────────
class _E:
    def __init__(self, S):
        self.S, self.o = S, []

    def P(self, v):
        return _f(v * self.S)

    def line(self, x1, y1, x2, y2, stroke, sw, op, extra=""):
        self.o.append(f'<line x1="{self.P(x1)}" y1="{self.P(y1)}" x2="{self.P(x2)}" y2="{self.P(y2)}" '
                      f'stroke="{stroke}" stroke-width="{self.P(sw)}" opacity="{op}"{extra}/>')

    def circle(self, cx, cy, r, fill="none", stroke=None, sw=None, op=None):
        a = f' stroke="{stroke}" stroke-width="{self.P(sw)}"' if stroke else ""
        o = f' opacity="{op}"' if op is not None else ""
        self.o.append(f'<circle cx="{self.P(cx)}" cy="{self.P(cy)}" r="{self.P(r)}" fill="{fill}"{a}{o}/>')

    def ellipse(self, cx, cy, rx, ry, fill="none", stroke=None, sw=None, op=None):
        a = f' stroke="{stroke}" stroke-width="{self.P(sw)}"' if stroke else ""
        o = f' opacity="{op}"' if op is not None else ""
        self.o.append(f'<ellipse cx="{self.P(cx)}" cy="{self.P(cy)}" rx="{self.P(rx)}" ry="{self.P(ry)}" '
                      f'fill="{fill}"{a}{o}/>')

    def polygon(self, pts, fill="none", stroke=None, sw=None, op=None):
        p = " ".join(f"{self.P(x)},{self.P(y)}" for x, y in pts)
        a = f' stroke="{stroke}" stroke-width="{self.P(sw)}"' if stroke else ""
        o = f' opacity="{op}"' if op is not None else ""
        self.o.append(f'<polygon points="{p}" fill="{fill}"{a}{o}/>')

    def rect(self, x, y, w, h, fill, op=None):
        o = f' opacity="{op}"' if op is not None else ""
        self.o.append(f'<rect x="{self.P(x)}" y="{self.P(y)}" width="{self.P(w)}" height="{self.P(h)}" '
                      f'fill="{fill}"{o}/>')


def base_art(S=S75, theme="dark"):
    """Every base-art element, in live painter order, as SVG markup at scale S."""
    t = THEMES[theme]
    e = _E(S)

    e.rect(0, 0, W_MM, H_MM, t["bg"])
    for k in range(SKY_BANDS):
        e.rect(0, k * SKY_BAND_MM, W_MM, SKY_BAND_MM, t["sky"], f"{0.95 - 0.1 * k:.3f}")
    for cx, cy, rx, ry in CLOUDS:
        e.ellipse(cx, cy, rx, ry, fill=t["cloud"], op="0.42")

    geos = [supertree_geometry(*tr) for tr in TREES]
    for g in geos:                                   # tree fill layer
        e.polygon(g["trunk"], fill=t["tree_fill"], op="0.78")
        e.ellipse(*g["canopy"], fill=t["tree_fill"], op="0.78")
    for g in geos:                                   # tree line layer
        e.polygon(g["trunk"], stroke=t["tree_line"], sw=_w(0.45), op="0.40")
        e.ellipse(*g["canopy"], stroke=t["tree_line"], sw=_w(0.45), op="0.40")
        for fr in g["fronds"]:
            e.line(*fr, t["tree_line"], _w(0.45), "0.40")
        for r in g["rungs"]:
            e.line(*r, t["tree_line"], _w(0.60), "0.40")

    big5 = [BIG5_X0 + i * BIG5_PITCH for i in range(BIG5_N)]
    lower = [LOWER_DROP_X0 + i * LOWER_DROP_PITCH for i in range(LOWER_DROP_N)]
    for y in RAIL_YS:                                # skyway rails
        e.line(RAIL_X0, y, RAIL_X1, y, t["rail"], _w(0.6), "0.50")
    for x in big5:
        e.line(x, RAIL_YS[0], x, BIG5_DROP_TOP, t["rail"], _w(0.6), "0.50")
    for x in lower:
        e.line(x, RAIL_YS[2], x, LOWER_DROP_BOT, t["rail"], _w(0.6), "0.50")
    for x in big5:                                   # rail dots inherit the rail stroke in the original
        e.circle(x, RAIL_YS[0], _w(1.8), fill=t["rail"], stroke=t["rail"], sw=_w(0.6), op="0.50")
    for x in lower:
        e.circle(x, RAIL_YS[2], _w(1.5), fill=t["rail"], stroke=t["rail"], sw=_w(0.6), op="0.50")

    for y in DIVIDER_YS:
        e.line(0, y, W_MM, y, t["divider"], _w(0.5), "0.40")
    d = _f(_w(2.0) * S)
    e.line(DIVIDER_VX, RAIL_YS[2], DIVIDER_VX, H_MM, t["divider"], _w(0.5), "0.30",
           f' stroke-dasharray="{d},{d}"')

    for x in big5:
        e.circle(x, BIG5_Y, BIG5_R_OUT, stroke=t["big5_ring"], sw=_w(0.9), op="0.65")
        e.circle(x, BIG5_Y, BIG5_R_IN, stroke=t["big5_ring"], sw=_w(0.5), op="0.55")
    for i in range(PHASE_N):
        x = PHASE_X0 + i * PHASE_PITCH
        e.circle(x, PHASE_Y, PHASE_R_OUT, stroke=t["phase_ring"], sw=_w(0.6), op="0.50")
        e.circle(x, PHASE_Y, PHASE_R_IN, stroke=t["phase_ring"], sw=_w(0.4), op="0.55")

    fx, fy = FLYER_C
    for r, op in FLYER_GLOW:
        e.circle(fx, fy, r, fill=t["flyer_glow"], op=op)
    e.circle(fx, fy, FLYER_R, stroke=t["flyer"], sw=_w(0.9), op="0.70")
    e.circle(fx, fy, FLYER_R_OUTER, stroke=t["flyer"], sw=_w(0.4), op="0.40")
    for k in range(FLYER_SPOKES):
        a = math.radians(-90 + k * 360 / FLYER_SPOKES)
        c, s = math.cos(a), math.sin(a)
        e.line(fx + FLYER_SPOKE_R0 * c, fy + FLYER_SPOKE_R0 * s, fx + FLYER_SPOKE_R1 * c,
               fy + FLYER_SPOKE_R1 * s, t["flyer"], _w(0.7), "0.68")

    ys = [FADER_TOP_MM + (FADER_BOT_MM - FADER_TOP_MM) * k / (FADER_LEVELS - 1)
          for k in range(FADER_LEVELS)][1:-1]
    for i in range(len(FADERS_MM) - 1):
        cx = (FADERS_MM[i] + FADERS_MM[i + 1]) / 2
        for y in ys:
            e.line(cx - TICK_LEN_MM / 2, y, cx + TICK_LEN_MM / 2, y, t["tick"], _w(1.5), "0.75",
                   ' stroke-linecap="round"')

    e.circle(*STATUS_DOT, fill=t["dot"])
    return "\n".join(e.o)


def logo(S=S75, theme="dark"):
    """Monsoon's own wordmark variant (see note above), placed exactly as in the live panel."""
    here = os.path.dirname(os.path.abspath(__file__))
    frag = open(os.path.join(here, "assets", f"monsoon_logo_{theme}.svgfrag")).read()
    x, y = LOGO_POS_MM
    return (f'<g transform="translate({x*S:.4f},{y*S:.4f}) scale({LOGO_SCALE_MM*S:.6f})">\n'
            f'{frag}</g>')
