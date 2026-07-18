#!/usr/bin/env python3
"""Straits — poly expander panel (22HP), styled as the flowing straits between two shores.

The refactored single Straits carries 16 REST + 16 ACCENT probability knobs (voice 1 = mono/ch0,
voices 2..16 = poly) plus three 16ch poly-cable outs (gate/CV/accent). The old East/West split is
gone — instead the two knob banks are REST (left, muted) and ACCENT (right, vibrant), the vibrancy
itself the literal rest-vs-accent distinction. A field of flowing contour "wave" lines runs behind
each bank (the straits' water), tinted to each side. A central voice spine (1..16) organises rows;
voice 1 (mono) is marked distinctly.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).

Kit id markers (widget binds; voice v 0..15, v0 = mono/voice 1):
  param_rest_<0..15>     REST probability knob   (v0 → mono REST_PARAM, v1..15 → POLY_REST_PARAM_*)
  param_accent_<0..15>   ACCENT probability knob (v0 → mono ACCENT_PARAM, v1..15 → POLY_ACCENT_PARAM_*)
  output_polygate / output_polycv / output_polyaccent   16ch cable outs
  light_connect
"""
import math, os, re
HP = 26
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#14171b", red="#d4001a", ink="#f0f0f0",
                  # REST = muted, cool; ACCENT = vibrant, warm
                  rest="#3f7d78", restwave="#2a5a56", restknob="#1a2e2c",
                  acc="#e08a1a", accwave="#8a5410", accknob="#3a2a10",
                  spine="#5a6470", spinehi="#8a94a0", spinedot="#4c7ac0",
                  knobface="#2a2e33", knobring="#4a5058", knobtick="#c0c8d0",
                  jackwell="#0c0e11", jackring="#4a4a4a", gold="#c8960c",
                  panelmid="#1a1d21", wave_op=0.5),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a",
                  rest="#5a9a94", restwave="#6fa8a2", restknob="#c8ddd9",
                  acc="#c88018", accwave="#d09a48", accknob="#e4d4b8",
                  spine="#b0b8c0", spinehi="#8a94a0", spinedot="#4c6ab0",
                  knobface="#e8e2d6", knobring="#b0a898", knobtick="#5a5040",
                  jackwell="#e2ddd2", jackring="#b0a898", gold="#b07d00",
                  panelmid="#d0d0d0", wave_op=0.75),
}

MARGIN   = 5.0
SPINE_W  = 10.0
SPINE_CX = W/2
SIDE_W   = (W - 2*MARGIN - SPINE_W) / 2
TOP      = 16.0
N_ROWS   = 6                    # 6 rows max per column
# MIRRORED banks so each bank's short column sits on the panel's OUTER edge, freeing its
# bottom two cells for that bank's I/O jacks -- cables then exit AWAY from the knob field
# instead of crossing it. REST short col first (left/outer), ACCENT short col last
# (right/outer). Column-major within each bank.
COLS_REST = [4, 6, 6]           # col0 (outer-left): 4 knobs -> 2 free cells for CV + GATE
COLS_ACC  = [6, 6, 4]           # col2 (outer-right): 4 knobs -> 1 free cell (ACCENT out) + 1 blank
ROW_H    = 14.77                # 43.6px -- Compact(tight) arc dia 43.4px kisses, never crosses
KNOB_R   = 4.5                  # painted preview under the Compact body (5.0mm)
GRID_TOP = TOP + 2.0
# jacks live INSIDE the grid (rows 5-6 of each short column), not in a bottom strip.
JACK_R5  = GRID_TOP + ROW_H*(4 + 0.5)   # row-5 cell centre
JACK_R6  = GRID_TOP + ROW_H*(5 + 0.5)   # row-6 cell centre
JACK_Y   = TOP + N_ROWS*ROW_H + 6.5     # logo band reference (kept for wordmark)

def wave_field(A, t, x0, y0, w, h, colour, n=22):
    """Flowing contour lines (the straits' water) across (x0,y0,w,h). Dense field — many
    fine contours with varied amplitude/phase so it reads as moving water, not a few lines."""
    seg = 40
    for i in range(n):
        yy = y0 + h*i/(n-1)
        pts = []
        amp = 0.8 + (i % 4)*0.55
        phase = i*0.55
        freq = 0.55 + (i % 3)*0.15
        for k in range(seg+1):
            xx = x0 + w*k/seg
            wy = yy + amp*math.sin(k*freq + phase) + 0.35*math.sin(k*1.7 + phase*1.3)
            pts.append(f"{px(xx)},{px(wy)}")
        op = t["wave_op"] * (0.55 + 0.45*(i % 2))   # alternate darker/lighter for depth
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}" '
          f'stroke-width="0.4" stroke-opacity="{op:.2f}"/>')

def knob(A, t, cx, cy, r, face, ring, mono=False):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{face}" '
      f'stroke="{ring}" stroke-width="{px(0.8 if mono else 0.5)}"/>')
    # pointer tick
    A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r*0.8)}" '
      f'stroke="{t["knobtick"]}" stroke-width="{px(0.5)}"/>')
    if mono:  # voice-1 mono ring accent
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+1.0)}" fill="none" '
          f'stroke="{t["red"]}" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')

# ── dot.modular wordmark embed (nondestructive wordmark crop — strips positioning
#    brackets + blank margins via a translate/scale transform; source logo untouched). ──
_LOGO_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "logo")
def logo_embed(dark, x_mm, y_mm, target_w_mm):
    path = os.path.join(_LOGO_DIR, "dot-modular-logo-dark.svg" if dark else "dot-modular-logo-light.svg")
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]
    body = re.sub(r'<polyline\b[^>]*/>', '', body)   # strip the 2 positioning brackets
    CX, CY, CW = 26.0, 65.0, 657.0                   # wordmark crop (matches *-tight.svg)
    sc = px(target_w_mm) / CW
    tx, ty = px(x_mm), px(y_mm)
    return f'<g transform="translate({tx:.2f},{ty:.2f}) scale({sc:.5f}) translate({-CX:.2f},{-CY:.2f})">{body}</g>'

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # ── wave fields behind each bank ──
    wave_field(A, t, MARGIN, TOP, SIDE_W, N_ROWS*ROW_H, t["restwave"])
    wave_field(A, t, SPINE_CX+SPINE_W/2, TOP, SIDE_W, N_ROWS*ROW_H, t["accwave"])

    # ── side tint bands (subtle) ──
    A(f'<rect x="{px(MARGIN-1)}" y="{px(TOP-4)}" width="{px(SIDE_W+2)}" height="{px(N_ROWS*ROW_H+6)}" '
      f'rx="{px(1.5)}" fill="{t["rest"]}" fill-opacity="0.06" stroke="{t["rest"]}" stroke-width="0.3" stroke-opacity="0.4"/>')
    A(f'<rect x="{px(SPINE_CX+SPINE_W/2-1)}" y="{px(TOP-4)}" width="{px(SIDE_W+2)}" height="{px(N_ROWS*ROW_H+6)}" '
      f'rx="{px(1.5)}" fill="{t["acc"]}" fill-opacity="0.08" stroke="{t["acc"]}" stroke-width="0.3" stroke-opacity="0.5"/>')

    # ── bank labels ──
    def lbl_dot(cx, cy, col):  # small colour marker (label text drawn at runtime / left implicit)
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(1.4)}" fill="{col}"/>')
    lbl_dot(MARGIN+SIDE_W*0.5, TOP-6, t["rest"])
    lbl_dot(SPINE_CX+SPINE_W/2+SIDE_W*0.5, TOP-6, t["acc"])

    # ── central voice spine 1..16 ──
    A(f'<line x1="{px(SPINE_CX)}" y1="{px(TOP)}" x2="{px(SPINE_CX)}" y2="{px(TOP+N_ROWS*ROW_H)}" '
      f'stroke="{t["spine"]}" stroke-width="{px(0.4)}" stroke-opacity="0.5"/>')

    # ── knob grid: 3 columns per bank, COLUMN-major, mirrored short columns ──
    # REST 4/6/6: col0 (outer-left) = voices 0-3, col1 = 4-9, col2 = 10-15.
    # ACCENT 6/6/4: col0 = voices 0-5, col1 = 6-11, col2 (outer-right) = 12-15.
    # v0 = mono, red-ringed, top of the bank's OUTER column so it neighbours that
    # bank's jacks (the "special" column: inherited-mono + I/O together).
    # The knob-count columns are top-aligned (no vertical centring) so the freed cells
    # are always the BOTTOM of the short column -- that is where the jacks go.
    def bank(kind, x_base, cols, col_face, col_ring):
        cw = SIDE_W/3
        v = 0
        # column render order is left-to-right, but the SHORT column must be on the
        # panel's outer edge: for rest that is col index 0 (left), for accent col index 2
        # (right). cols is already authored that way (4,6,6 vs 6,6,4).
        for c, nrows in enumerate(cols):
            for r in range(nrows):
                cx = x_base + cw*(c+0.5)
                cy = GRID_TOP + ROW_H*(r+0.5)
                mono = (v == 0)
                knob(A, t, cx, cy, KNOB_R, col_face, col_ring, mono)
                A(f'<circle id="param_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
                v += 1
    bank("rest",   MARGIN,             COLS_REST, t["restknob"], t["rest"])
    bank("accent", SPINE_CX+SPINE_W/2, COLS_ACC,  t["accknob"],  t["acc"])

    # ── I/O jacks INSIDE the grid, in the freed bottom cells of each short column ──
    cw = SIDE_W/3
    rest_col0_cx = MARGIN + cw*0.5                       # outer-left column
    acc_col2_cx  = (SPINE_CX+SPINE_W/2) + cw*2.5         # outer-right column
    def outjack(jid, cx, cy, label_gold=True):
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(4.4)}" fill="{t["jackwell"]}" '
          f'stroke="{t["jackring"]}" stroke-width="0.8"/>')
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(2.0)}" fill="none" stroke="{t["gold"]}" stroke-width="0.5"/>')
        A(f'<circle id="{jid}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
    # rest short column (col0), rows 5 & 6: POLY CV then POLY GATE
    outjack("output_polycv",   rest_col0_cx, JACK_R5)
    outjack("output_polygate", rest_col0_cx, JACK_R6)
    # accent short column (col2), row 5: POLY ACCENT; row 6 left BLANK (balances rest's 2nd jack)
    outjack("output_polyaccent", acc_col2_cx, JACK_R5)

    lcx = W - MARGIN - 3
    A(f'<circle cx="{px(lcx)}" cy="{px(JACK_Y)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
    # dot.modular wordmark — centred horizontally, lower band (a bit below the Sands panels' y≈113)
    A(logo_embed(dark, (W - 34.0) / 2.0, 120.5, 34.0))
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Straits_panel_dark.svg"), (False, "Straits_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Straits {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
