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
        tick="#888888", dot="#dc2626", rail_dot="#dc2626",
        # opacities that differ between themes (measured)
        cloud_op="0.42", tree_line_op="0.40", rail_op="0.50", divider_op="0.40",
        big5_op="0.65", phase_op="0.50", flyer_outer_op="0.40",
    ),
    "light": dict(                       # measured from Monsoon_panel_light_monsoon.svg
        bg="#e6e8ec", sky="#c8d4e0", cloud="#c8d0d8",
        tree_fill="#bdc8d2", tree_line="#c0202a",
        rail="#c0202a", divider="#c0c8d0",
        big5_ring="#9a6800", phase_ring="#9a9a9a",
        flyer_glow="#26a69a", flyer="#9a6800",
        tick="#999999", dot="#dc2626", rail_dot="#dc2626",
        cloud_op="0.32", tree_line_op="0.30", rail_op="0.40", divider_op="0.30",
        big5_op="0.72", phase_op="0.40", flyer_outer_op="0.30",
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
        e.ellipse(cx, cy, rx, ry, fill=t["cloud"], op=t["cloud_op"])

    geos = [supertree_geometry(*tr) for tr in TREES]
    for g in geos:                                   # tree fill layer
        e.polygon(g["trunk"], fill=t["tree_fill"], op="0.78")
        e.ellipse(*g["canopy"], fill=t["tree_fill"], op="0.78")
    for g in geos:                                   # tree line layer
        e.polygon(g["trunk"], stroke=t["tree_line"], sw=_w(0.45), op=t["tree_line_op"])
        e.ellipse(*g["canopy"], stroke=t["tree_line"], sw=_w(0.45), op=t["tree_line_op"])
        for fr in g["fronds"]:
            e.line(*fr, t["tree_line"], _w(0.45), t["tree_line_op"])
        for r in g["rungs"]:
            e.line(*r, t["tree_line"], _w(0.60), t["tree_line_op"])

    big5 = [BIG5_X0 + i * BIG5_PITCH for i in range(BIG5_N)]
    lower = [LOWER_DROP_X0 + i * LOWER_DROP_PITCH for i in range(LOWER_DROP_N)]
    for y in RAIL_YS:                                # skyway rails
        e.line(RAIL_X0, y, RAIL_X1, y, t["rail"], _w(0.6), t["rail_op"])
    for x in big5:
        e.line(x, RAIL_YS[0], x, BIG5_DROP_TOP, t["rail"], _w(0.6), t["rail_op"])
    for x in lower:
        e.line(x, RAIL_YS[2], x, LOWER_DROP_BOT, t["rail"], _w(0.6), t["rail_op"])
    for x in big5:                                   # rail dots inherit the rail stroke in the original
        e.circle(x, RAIL_YS[0], _w(1.8), fill=t["rail_dot"], stroke=t["rail"], sw=_w(0.6), op=t["rail_op"])
    for x in lower:
        e.circle(x, RAIL_YS[2], _w(1.5), fill=t["rail_dot"], stroke=t["rail"], sw=_w(0.6), op=t["rail_op"])

    for y in DIVIDER_YS:
        e.line(0, y, W_MM, y, t["divider"], _w(0.5), t["divider_op"])
    d = _f(_w(2.0) * S)
    e.line(DIVIDER_VX, RAIL_YS[2], DIVIDER_VX, H_MM, t["divider"], _w(0.5), "0.30",
           f' stroke-dasharray="{d},{d}"')

    for x in big5:
        e.circle(x, BIG5_Y, BIG5_R_OUT, stroke=t["big5_ring"], sw=_w(0.9), op=t["big5_op"])
        e.circle(x, BIG5_Y, BIG5_R_IN, stroke=t["big5_ring"], sw=_w(0.5), op="0.55")
    for i in range(PHASE_N):
        x = PHASE_X0 + i * PHASE_PITCH
        e.circle(x, PHASE_Y, PHASE_R_OUT, stroke=t["phase_ring"], sw=_w(0.6), op=t["phase_op"])
        e.circle(x, PHASE_Y, PHASE_R_IN, stroke=t["phase_ring"], sw=_w(0.4), op="0.55")

    fx, fy = FLYER_C
    for r, op in FLYER_GLOW:
        e.circle(fx, fy, r, fill=t["flyer_glow"], op=op)
    e.circle(fx, fy, FLYER_R, stroke=t["flyer"], sw=_w(0.9), op="0.70")
    e.circle(fx, fy, FLYER_R_OUTER, stroke=t["flyer"], sw=_w(0.4), op=t["flyer_outer_op"])
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


# ══ LAYOUT TABLE — every control, light and jack. THE single source for positions. ════════════════
# Emitted as the SVG `components` layer (kit anchors: MonsoonWidget binds by id via SvgPanelKit), and
# the visible control wells in `cluster-art` are derived from the SAME entries — they cannot drift.
# Reproduces the CURRENT panel exactly (incl. the Sept-19 q-mix hand edits). Known oddities kept
# faithfully, to be revisited with the dice/phase rework:
#   * BPM/LEN/OFFSET knobs sit at y=60 but their rings (PHASE_Y) are centred at y=58.
#   * QMIX_DICE_LIGHT is centred ON its button (87); its twins RHYTHM/MELODY_DICE_LIGHT sit on the
#     light row (93) below theirs.
#   * QMIX_LEVEL knob occupies an output-jack slot (182,120) and has no well.
#   * PHASE_PARAM (Mode E phase knob) is a TEMPORARY placement at (178,72).
#   * ROW2 (q-mix sub-row, hand-placed Sept 19 at exactly 280px = 94.83mm) COLLIDES with the light
#     row: RHYTHM/MELODY_DICE_LIGHT (y=93) sit 1.8mm above LAST_DICE_R/M (y=94.83). No wells.
MODE_PARAM_XY = (194.0, 60.0)
MODE_LIGHT_X, MODE_LIGHT_Y0, MODE_LIGHT_PITCH = 197.5, 13.0, 9.0

BIG5_IDS = ["NOTE_VALUE_PARAM", "VARIATION_PARAM", "LEGATO_PARAM", "REST_PARAM", "ACCENT_KNOB"]
PHASE_KNOB_IDS = ["BPM_PARAM", "PATTERN_LENGTH_PARAM", "PATTERN_OFFSET_PARAM"]
PHASE_KNOB_Y = 60.0

ROW_X0, ROW_PITCH, ROW_Y, ROW_LIGHT_Y = 12.0, 16.7, 87.0, 93.0     # 12-slot control row
# (param id, well kind, light id or None, light on button?)
ROW = [
    ("DICE_SLEW_R_PARAM",  "trim",  None, False),
    ("DICE_SLEW_M_PARAM",  "trim",  None, False),
    ("DICE_R_PARAM",       "seat_red",  "RHYTHM_DICE_LIGHT", False),
    ("DICE_M_PARAM",       "seat_red",  "MELODY_DICE_LIGHT", False),
    ("DICE_Q_PARAM",       "seat_gold", "QMIX_DICE_LIGHT",   True),
    ("LAST_DICE_Q_PARAM",  "seat_gold", None, False),
    ("RHYTHM_MIX_PARAM",   "mix",   None, False),
    ("MELODY_MIX_PARAM",   "mix",   None, False),
    ("LOCK_PARAM",         "util",  "LOCK_LIGHT",     False),
    ("MUTE_PARAM",         "util",  "MUTE_LIGHT",     False),
    ("RESET_BUTTON_PARAM", "util",  "RESET_LIGHT",    False),
    ("RUN_GATE_PARAM",     "util",  "RUN_GATE_LIGHT", False),
]

# q-mix sub-row under slots 2-5 (same column grid): (slot, param id)
ROW2_Y = 280.0 / S75
ROW2 = [(2, "LAST_DICE_R_PARAM"), (3, "LAST_DICE_M_PARAM"), (4, "QMIX_MIX_PARAM"), (5, "DICE_SLEW_Q_PARAM")]

JACK_PITCH, JACK_Y = 17.0, (105.0, 120.0)
IN_X0, OUT_X0 = 15.0, 114.0
INPUTS = [["RUN_GATE_INPUT", "RESET_TRIGGER_INPUT", "SEED_INPUT", "GATE1_INPUT", "GATE2_INPUT", "GATE3_MOD_INPUT"],
          ["CLK_INPUT", "LENGTH_INPUT", "OFFSET_INPUT", "CV1_INPUT", "CV2_INPUT", "CV3_MOD_INPUT"]]
OUTPUTS = [["GATE_OUTPUT", "TIE_OUTPUT", "LEGATO_OUTPUT", "TIE_OR_LEGATO_OUTPUT", "ACCENT_OUTPUT"],
           ["CV_OUTPUT", "SEED_OUTPUT", "RUN_GATE_OUTPUT", "RESET_TRIGGER_OUTPUT"]]
QMIX_LEVEL_XY = (OUT_X0 + 4 * JACK_PITCH, JACK_Y[1])     # knob in the 5th bottom output slot

STEP_LEDS, STEP_LED_R = 16, 14.0                          # on the Flyer, centred FLYER_C
PHASE_PARAM_XY = (178.0, 72.0)                            # TEMPORARY (see note above)

ANCHOR_R_PX = 3.0                                         # anchors are invisible; radius is nominal


def layout():
    """Ordered list of (kit_id, x_mm, y_mm). Existing ids match the live panel; entries marked NEW
    are controls the widget currently places itself with hardcoded mm2px (see reverse-engineer doc)."""
    A = [("param_MODE_PARAM", *MODE_PARAM_XY)]
    for k in range(6):
        A.append((f"light_MODE_{'ABCDEF'[k]}_LIGHT", MODE_LIGHT_X, MODE_LIGHT_Y0 + k * MODE_LIGHT_PITCH))
    names = [f"SEMI{i}_PARAM" for i in range(12)] + ["OCT_LO_PARAM", "OCT_HI_PARAM"]
    A += [(f"param_{n}", x, FADER_Y_MM) for n, x in zip(names, FADERS_MM)]
    A += [(f"param_{n}", BIG5_X0 + i * BIG5_PITCH, BIG5_Y) for i, n in enumerate(BIG5_IDS)]
    A += [(f"param_{n}", PHASE_X0 + i * PHASE_PITCH, PHASE_KNOB_Y) for i, n in enumerate(PHASE_KNOB_IDS)]
    A += [(f"param_{p}", ROW_X0 + i * ROW_PITCH, ROW_Y) for i, (p, _, _, _) in enumerate(ROW)]
    A += [(f"param_{p}", ROW_X0 + slot * ROW_PITCH, ROW2_Y) for slot, p in ROW2]
    for r, row in enumerate(INPUTS):
        A += [(f"input_{n}", IN_X0 + i * JACK_PITCH, JACK_Y[r]) for i, n in enumerate(row)]
    for r, row in enumerate(OUTPUTS):
        A += [(f"output_{n}", OUT_X0 + i * JACK_PITCH, JACK_Y[r]) for i, n in enumerate(row)]
    A.append(("param_QMIX_LEVEL_PARAM", *QMIX_LEVEL_XY))
    # NEW anchors — replace the widget's remaining hardcoded placements
    for i, (p, _, light, on_btn) in enumerate(ROW):
        if light:
            A.append((f"light_{light}", ROW_X0 + i * ROW_PITCH, ROW_Y if on_btn else ROW_LIGHT_Y))
    fx, fy = FLYER_C
    for i in range(STEP_LEDS):
        a = i / STEP_LEDS * 2 * math.pi - math.pi / 2
        A.append((f"light_STEP{i}_LIGHT", fx + STEP_LED_R * math.cos(a), fy + STEP_LED_R * math.sin(a)))
    A.append(("param_PHASE_PARAM", *PHASE_PARAM_XY))
    return A


NEW_ANCHOR_PREFIXES = ("light_RHYTHM_DICE", "light_MELODY_DICE", "light_QMIX_DICE", "light_LOCK",
                       "light_MUTE", "light_RESET", "light_RUN_GATE", "light_STEP", "param_PHASE_PARAM")


def components(S=S75):
    o = ['<g inkscape:label="components" inkscape:groupmode="layer" id="components">']
    for kid, x, y in layout():
        o.append(f'<circle id="{kid}" cx="{_f(x*S)}" cy="{_f(y*S)}" r="{ANCHOR_R_PX*S/S75:g}" '
                 f'fill="none" stroke="none"/>')
    o.append("</g>")
    return "\n".join(o)


# ── Control wells (visible `cluster-art`), derived from the layout table ─────────────────────────
THEMES["dark"].update(well="#0f1114", well_line="#2a2f37", seat_red="#dc2626",
                      seat_gold="#c8960c", mix_teal="#26a69a")
THEMES["light"].update(well="#d4d6d9", well_line="#c0c4ca", seat_red="#c0202a",
                       seat_gold="#a07808", mix_teal="#1a8276")
WELL = dict(trim=3.4, mix=3.6, util=2.6, jack=4.4)   # radii mm; seats are 6mm squares, corner 1.2
SEAT, SEAT_RX = 6.0, 1.2


def cluster_art(S=S75, theme="dark"):
    t = THEMES[theme]
    px1, px09 = 1.0 / S75, 0.9 / S75          # authored as 1px / 0.9px at 75dpi
    e = _E(S)
    for i, (_, kind, _, _) in enumerate(ROW):
        x = ROW_X0 + i * ROW_PITCH
        if kind.startswith("seat"):
            col = t["seat_red"] if kind == "seat_red" else t["seat_gold"]
            e.o.append(f'<rect x="{e.P(x-SEAT/2)}" y="{e.P(ROW_Y-SEAT/2)}" width="{e.P(SEAT)}" '
                       f'height="{e.P(SEAT)}" rx="{e.P(SEAT_RX)}" fill="{t["well"]}" stroke="{col}" '
                       f'stroke-width="{e.P(px09)}"/>')
        else:
            col = t["mix_teal"] if kind == "mix" else t["well_line"]
            e.circle(x, ROW_Y, WELL[kind], fill=t["well"], stroke=col, sw=px09 if kind == "util" else px1)
    for rows, x0 in ((INPUTS, IN_X0), (OUTPUTS, OUT_X0)):
        for r, row in enumerate(rows):
            for i in range(len(row)):
                e.circle(x0 + i * JACK_PITCH, JACK_Y[r], WELL["jack"], fill=t["well"],
                         stroke=t["well_line"], sw=px1)
    return ('<g inkscape:label="cluster-art" inkscape:groupmode="layer" id="cluster-art">\n'
            + "\n".join(e.o) + "\n</g>")


def panel(theme="dark"):
    """The complete Monsoon panel SVG (75dpi, 40HP)."""
    S = S75
    return ('<svg xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
            f'xmlns="http://www.w3.org/2000/svg" width="{_f(W_MM*S)}" height="{H_MM*S:.4f}" '
            f'viewBox="0 0 {_f(W_MM*S)} {H_MM*S:.4f}">\n'
            f'<!-- GENERATED by panel_src/monsoon_art.py — do not hand-edit. Edit the layout table. -->\n'
            + base_art(S, theme) + "\n" + logo(S, theme) + "\n" + cluster_art(S, theme) + "\n"
            + components(S) + "\n</svg>\n")


if __name__ == "__main__":
    # usage: python panel_src/monsoon_art.py [OUT_DIR]
    #   default OUT_DIR = res/panels  -> writes the ACTIVE panels MonsoonWidget loads:
    #   Monsoon_panel_{dark,light}_monsoon.svg
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "res", "panels")
    for th in ("dark", "light"):
        path = os.path.join(out_dir, f"Monsoon_panel_{th}_monsoon.svg")
        open(path, "w").write(panel(th))
        print("wrote", os.path.normpath(path))
