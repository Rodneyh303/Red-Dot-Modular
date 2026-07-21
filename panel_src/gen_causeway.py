#!/usr/bin/env python3
"""Causeway — poly modulation interface for Straits (22HP), as a dense trussed BRIDGE.

The whole panel is the Johor-Singapore Causeway rendered as an engineering drawing: twin truss
carriageways running the full height, each attenuator row a bridge BAY sitting INSIDE the girder
framework (knobs intersecting the beams, like Interchange's controls sit in its hex lattice), a
perspective bridge span with towers + piers across the top, water beneath, gold linework throughout.

Controls (voice 0 = mono/ch0, 1..15 poly):
  16 REST attenuverters (left carriageway) + 16 ACCENT attenuverters (right carriageway)
  + global REST + global ACCENT attenuators + 2 CV inputs (FROM MONSOON / TO STRAITS).

nanosvg-safe (solid fills/strokes, elliptical arcs ok; no gradient/mask/text/url).

Kit id markers (widget binds):
  param_restatt_<0..15> / param_accatt_<0..15> / param_restatt_global / param_accatt_global
  input_restcv / input_accentcv / light_connect
"""
import math, os, re
HP = 22
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#0c0d0a", red="#d4001a", ink="#f0ead8",
                  gold="#b8901c", goldhi="#e8c050", golddim="#5a4818", goldfaint="#3a3014",
                  rest="#4c8c86", restknob="#16211f", acc="#e0951e", accknob="#2e2210",
                  knobface="#191b17", knobring="#4a4428", knobtick="#d8c890",
                  jackwell="#0a0b08", jackring="#5a4a1c",
                  water="#1e4658", waterhi="#3a7088", node="#c8a83c", deck="#1a2018",
                  # ── STRUCTURE palette (decoupled from controls): the truss is weathered
                  #    STEEL, not amber, so it recedes behind the amber/teal knob rings and
                  #    reads as scaffold rather than fusing into one gold mesh. Cool blue-grey
                  #    at low value; the amber family above stays reserved for CONTROLS. ──
                  steel="#3c4650", steeldim="#2a333c", steelfaint="#1c242c",
                  steelhi="#586470", steelnode="#4a5560",
                  wave_op=0.5),
    "light": dict(bg="#e6e2d4", red="#d4001a", ink="#2a2418",
                  gold="#8a6a10", goldhi="#a88420", golddim="#c0b088", goldfaint="#cabf98",
                  rest="#3a7a74", restknob="#c4dad6", acc="#b0740e", accknob="#e0d0b0",
                  knobface="#ece6d8", knobring="#a89860", knobtick="#5a4a20",
                  jackwell="#d8d0b8", jackring="#a89860",
                  water="#6a9aae", waterhi="#4a7a90", node="#8a6a10", deck="#cdc9b8",
                  # STRUCTURE palette — light theme gets its OWN value scheme (not an
                  # inversion): a cool slate truss DARKER than the cream ground so it reads
                  # as drawn lines, with amber/teal reserved for controls as in dark.
                  steel="#8792a0", steeldim="#a7b0bc", steelfaint="#c2c9d2",
                  steelhi="#6a7686", steelnode="#7a8593",
                  wave_op=0.72),
}

MARGIN   = 4.5
CARR_W   = 46.0                       # each carriageway (truss + knobs) width
GAP      = W - 2*MARGIN - 2*CARR_W    # central gap (spine truss)
CARR_X   = [MARGIN, W - MARGIN - CARR_W]
SPINE_X0 = MARGIN + CARR_W
SPAN_TOP = 14.0
SPAN_H   = 14.0
BRIDGE_TOP = SPAN_TOP + SPAN_H + 4.0
N_ROWS   = 8
ROW_H    = 9.6
BAYS_H   = N_ROWS * ROW_H
KNOB_R   = 2.7
GLOBAL_Y = BRIDGE_TOP - 3.0
WATER_Y  = BRIDGE_TOP + BAYS_H + 3.0

def truss_frame(A, t, x0, w, y0, h, rows):
    """Continuous truss girder over (x0,y0,w,h) with `rows` bays. Dense, layered like Interchange:
    deck plating, double chords, X-bracing + sub-diagonals, gusset plates, rivet nodes."""
    x1 = x0 + w
    bay = h / rows
    # ── deck plating panels (faint alternating fill for depth) ──
    for r in range(rows):
        yy = y0 + bay*r
        fillc = t["deck"] if r % 2 == 0 else t["bg"]
        A(f'<rect x="{px(x0+2.2)}" y="{px(yy)}" width="{px(w-4.4)}" height="{px(bay)}" '
          f'fill="{fillc}" fill-opacity="0.35"/>')
    # ── outer + inner top/bottom chords (double lines = girder depth) ──
    for xx in (x0, x1, x0+2.2, x1-2.2):
        A(f'<line x1="{px(xx)}" y1="{px(y0)}" x2="{px(xx)}" y2="{px(y0+h)}" stroke="{t["steel"]}" stroke-width="0.5"/>')
    # ── horizontal cross-beams (double) at every bay line ──
    for r in range(rows+1):
        yy = y0 + bay*r
        A(f'<line x1="{px(x0)}" y1="{px(yy)}" x2="{px(x1)}" y2="{px(yy)}" stroke="{t["steel"]}" stroke-width="0.5"/>')
        A(f'<line x1="{px(x0+2.2)}" y1="{px(yy+0.5)}" x2="{px(x1-2.2)}" y2="{px(yy+0.5)}" stroke="{t["steeldim"]}" stroke-width="0.3"/>')
    # ── X-bracing + sub-diagonals in each bay (denser web) ──
    for r in range(rows):
        ya, yb = y0+bay*r, y0+bay*(r+1); ym=(ya+yb)/2; xm=(x0+x1)/2
        A(f'<line x1="{px(x0+2.2)}" y1="{px(ya)}" x2="{px(x1-2.2)}" y2="{px(yb)}" stroke="{t["steeldim"]}" stroke-width="0.35" stroke-opacity="0.8"/>')
        A(f'<line x1="{px(x1-2.2)}" y1="{px(ya)}" x2="{px(x0+2.2)}" y2="{px(yb)}" stroke="{t["steeldim"]}" stroke-width="0.35" stroke-opacity="0.8"/>')
        # sub-diagonals from mid-span to corners (finer web)
        A(f'<line x1="{px(xm)}" y1="{px(ya)}" x2="{px(x0+2.2)}" y2="{px(ym)}" stroke="{t["steelfaint"]}" stroke-width="0.25"/>')
        A(f'<line x1="{px(xm)}" y1="{px(ya)}" x2="{px(x1-2.2)}" y2="{px(ym)}" stroke="{t["steelfaint"]}" stroke-width="0.25"/>')
    # ── gusset plates (small diamonds) at bay intersections + rivet nodes ──
    for r in range(rows+1):
        yy = y0 + bay*r
        for xx in (x0, x1, x0+2.2, x1-2.2):
            A(f'<polygon points="{px(xx)},{px(yy-0.9)} {px(xx+0.9)},{px(yy)} {px(xx)},{px(yy+0.9)} {px(xx-0.9)},{px(yy)}" '
              f'fill="{t["steeldim"]}" stroke="{t["steel"]}" stroke-width="0.2"/>')
            A(f'<circle cx="{px(xx)}" cy="{px(yy)}" r="{px(0.4)}" fill="{t["node"]}"/>')

def spine_truss(A, t, x0, w, y0, h, rows):
    """Central parallel-truss spine between the carriageways (the causeway median)."""
    x1 = x0 + w
    bay = h / rows
    cx = (x0+x1)/2
    for xx in (x0+1, x1-1, cx):
        A(f'<line x1="{px(xx)}" y1="{px(y0)}" x2="{px(xx)}" y2="{px(y0+h)}" '
          f'stroke="{t["steel"]}" stroke-width="0.45"/>')
    for r in range(rows):
        ya, yb = y0+bay*r, y0+bay*(r+1)
        A(f'<line x1="{px(x0+1)}" y1="{px(ya)}" x2="{px(cx)}" y2="{px(yb)}" stroke="{t["steeldim"]}" stroke-width="0.3"/>')
        A(f'<line x1="{px(x1-1)}" y1="{px(ya)}" x2="{px(cx)}" y2="{px(yb)}" stroke="{t["steeldim"]}" stroke-width="0.3"/>')
    for r in range(rows+1):
        yy = y0+bay*r
        A(f'<rect x="{px(x0+1)}" y="{px(yy-0.6)}" width="{px(w-2)}" height="{px(1.2)}" fill="{t["steeldim"]}" fill-opacity="0.5"/>')

def bridge_span(A, t):
    """Perspective bridge span across the top: deck, twin towers, cable sweeps, piers."""
    y = SPAN_TOP + SPAN_H*0.5
    # deck line receding (two parallel = perspective)
    A(f'<line x1="{px(MARGIN)}" y1="{px(y)}" x2="{px(W-MARGIN)}" y2="{px(y)}" stroke="{t["steel"]}" stroke-width="0.7"/>')
    A(f'<line x1="{px(MARGIN+6)}" y1="{px(y-3)}" x2="{px(W-MARGIN-6)}" y2="{px(y-3)}" stroke="{t["steeldim"]}" stroke-width="0.4"/>')
    # towers
    for tx in (W*0.30, W*0.70):
        A(f'<line x1="{px(tx)}" y1="{px(y+3)}" x2="{px(tx)}" y2="{px(SPAN_TOP-2)}" stroke="{t["steelhi"]}" stroke-width="0.7"/>')
        A(f'<line x1="{px(tx-2)}" y1="{px(y+3)}" x2="{px(tx-2)}" y2="{px(SPAN_TOP)}" stroke="{t["steel"]}" stroke-width="0.4"/>')
        A(f'<line x1="{px(tx+2)}" y1="{px(y+3)}" x2="{px(tx+2)}" y2="{px(SPAN_TOP)}" stroke="{t["steel"]}" stroke-width="0.4"/>')
    # cable catenaries between towers and to the ends
    pts = [(MARGIN, y), (W*0.30, SPAN_TOP-2), (W*0.70, SPAN_TOP-2), (W-MARGIN, y)]
    for (x1,y1),(x2,y2) in zip(pts, pts[1:]):
        midx=(x1+x2)/2; midy=min(y1,y2)-2
        A(f'<path d="M {px(x1)} {px(y1)} Q {px(midx)} {px(midy)} {px(x2)} {px(y2)}" '
          f'fill="none" stroke="{t["steel"]}" stroke-width="0.4" stroke-opacity="0.8"/>')
    # vertical hangers from cable to deck
    for k in range(1, 20):
        hx = MARGIN + (W-2*MARGIN)*k/20
        A(f'<line x1="{px(hx)}" y1="{px(y-6)}" x2="{px(hx)}" y2="{px(y)}" stroke="{t["steelfaint"]}" stroke-width="0.25"/>')
    # piers descending into water below the deck
    for px_ in (W*0.14, W*0.46, W*0.54, W*0.86):
        A(f'<line x1="{px(px_)}" y1="{px(y)}" x2="{px(px_)}" y2="{px(y+4)}" stroke="{t["steeldim"]}" stroke-width="0.4"/>')

def knob(A, t, cx, cy, r, ring, mono=False):
    # knob well recess so the beams read as passing BEHIND the knob
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+1.0)}" fill="{t["bg"]}"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{t["knobface"]}" '
      f'stroke="{ring}" stroke-width="{px(0.9 if mono else 0.6)}"/>')
    A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r*0.8)}" '
      f'stroke="{t["knobtick"]}" stroke-width="{px(0.45)}"/>')
    if mono:
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+1.6)}" fill="none" '
          f'stroke="{t["red"]}" stroke-width="{px(0.5)}" stroke-opacity="0.85"/>')

def plusminus(A, t, cx, cy, r):
    # +/- attenuverter markings flanking a knob
    A(f'<line x1="{px(cx-r-2.2)}" y1="{px(cy)}" x2="{px(cx-r-1.0)}" y2="{px(cy)}" stroke="{t["steeldim"]}" stroke-width="0.4"/>')
    A(f'<line x1="{px(cx+r+1.0)}" y1="{px(cy)}" x2="{px(cx+r+2.2)}" y2="{px(cy)}" stroke="{t["steeldim"]}" stroke-width="0.4"/>')
    A(f'<line x1="{px(cx+r+1.6)}" y1="{px(cy-0.6)}" x2="{px(cx+r+1.6)}" y2="{px(cy+0.6)}" stroke="{t["steeldim"]}" stroke-width="0.4"/>')

def water(A, t, x0, y0, w, h, n=5):
    for i in range(n):
        yy = y0 + h*i/(n-1)
        pts=[]; seg=20; amp=1.0+(i%2)*0.6
        for k in range(seg+1):
            xx=x0+w*k/seg; wy=yy+amp*math.sin(k*0.8+i*0.9)
            pts.append(f"{px(xx)},{px(wy)}")
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{t["water"]}" '
          f'stroke-width="0.4" stroke-opacity="{t["wave_op"]}"/>')

def cvjack(A, t, cx, cy, col):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(3.4)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(1.5)}" fill="none" stroke="{col}" stroke-width="0.5"/>')

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
    o=[]; A=o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # ── bridge span across the top ──
    bridge_span(A, t)
    # ── CV inputs at the ends (FROM MONSOON / TO STRAITS) ──
    cvjack(A, t, MARGIN+4, SPAN_TOP+SPAN_H*0.5, t["rest"])
    A(f'<circle id="input_restcv" cx="{px(MARGIN+4)}" cy="{px(SPAN_TOP+SPAN_H*0.5)}" r="0.5" fill="none" stroke="none"/>')
    cvjack(A, t, W-MARGIN-4, SPAN_TOP+SPAN_H*0.5, t["acc"])
    A(f'<circle id="input_accentcv" cx="{px(W-MARGIN-4)}" cy="{px(SPAN_TOP+SPAN_H*0.5)}" r="0.5" fill="none" stroke="none"/>')

    # ── truss frameworks FIRST (so knobs sit inside them) ──
    truss_frame(A, t, CARR_X[0], CARR_W, BRIDGE_TOP, BAYS_H, N_ROWS)
    truss_frame(A, t, CARR_X[1], CARR_W, BRIDGE_TOP, BAYS_H, N_ROWS)
    spine_truss(A, t, SPINE_X0, GAP, BRIDGE_TOP, BAYS_H, N_ROWS)

    # ── global attenuators above each carriageway ──
    knob(A, t, CARR_X[0]+CARR_W*0.5, GLOBAL_Y, KNOB_R+0.5, t["rest"])
    A(f'<circle id="param_restatt_global" cx="{px(CARR_X[0]+CARR_W*0.5)}" cy="{px(GLOBAL_Y)}" r="0.5" fill="none" stroke="none"/>')
    knob(A, t, CARR_X[1]+CARR_W*0.5, GLOBAL_Y, KNOB_R+0.5, t["acc"])
    A(f'<circle id="param_accatt_global" cx="{px(CARR_X[1]+CARR_W*0.5)}" cy="{px(GLOBAL_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── attenuverter knobs INSIDE the truss bays: 2 cols x 8 rows per carriageway = 16 ──
    def bank(kind, cxbase, ring):
        cw = CARR_W/2
        for r in range(N_ROWS):
            for c in range(2):
                v = r*2 + c
                cx = cxbase + cw*(c+0.5)
                cy = BRIDGE_TOP + ROW_H*(r+0.5)
                mono = (v==0)
                knob(A, t, cx, cy, KNOB_R, ring, mono)
                plusminus(A, t, cx, cy, KNOB_R)
                A(f'<circle id="param_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
    bank("restatt", CARR_X[0], t["rest"])
    bank("accatt",  CARR_X[1], t["acc"])

    # ── voice numbers down the spine (nodes) ──
    bay = BAYS_H/N_ROWS
    for r in range(N_ROWS):
        for c in range(2):
            v=r*2+c
            sy = BRIDGE_TOP + ROW_H*(r+0.5)
        # a node per row on the spine centre
        A(f'<circle cx="{px(W/2)}" cy="{px(BRIDGE_TOP+bay*(r+0.5))}" r="{px(0.9)}" fill="{t["node"]}"/>')

    # ── water beneath ──
    water(A, t, MARGIN, WATER_Y, W-2*MARGIN, 11)
    lcx = W - MARGIN - 3
    A(f'<circle cx="{px(lcx)}" cy="{px(WATER_Y+12)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(WATER_Y+12)}" r="0.5" fill="none" stroke="none"/>')
    # dot.modular wordmark — centred horizontally, lower band (a bit below the Sands panels' y≈113)
    A(logo_embed(dark, (W - 36.0) / 2.0, 116.0, 36.0))
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Causeway_panel_dark.svg"), (False, "Causeway_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Causeway {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
