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
import math
HP = 22
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
TOP      = 20.0
N_ROWS   = 8
ROW_H    = 10.2
KNOB_R   = 3.0
GRID_TOP = TOP + 2.0
JACK_Y   = TOP + N_ROWS*ROW_H + 8.0

def wave_field(A, t, x0, y0, w, h, colour, n=9):
    """Flowing contour lines (the straits' water) across (x0,y0,w,h)."""
    for i in range(n):
        yy = y0 + h*i/(n-1)
        # a gentle sine contour
        pts = []
        seg = 16
        amp = 1.4 + (i % 3)*0.5
        for k in range(seg+1):
            xx = x0 + w*k/seg
            wy = yy + amp*math.sin(k*0.9 + i*0.7)
            pts.append(f"{px(xx)},{px(wy)}")
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}" '
          f'stroke-width="0.45" stroke-opacity="{t["wave_op"]}"/>')

def knob(A, t, cx, cy, r, face, ring, mono=False):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{face}" '
      f'stroke="{ring}" stroke-width="{px(0.8 if mono else 0.5)}"/>')
    # pointer tick
    A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r*0.8)}" '
      f'stroke="{t["knobtick"]}" stroke-width="{px(0.5)}"/>')
    if mono:  # voice-1 mono ring accent
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+1.0)}" fill="none" '
          f'stroke="{t["red"]}" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')

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
      f'stroke="{t["spine"]}" stroke-width="{px(0.6)}"/>')

    # ── knob grid: 2 columns per side x 8 rows = 16 per bank ──
    # voice index mapping: row r (0..7), col c (0..1) → v = r*2 + c  (v0 = mono at top-left)
    def bank(kind, x_base, col_face, col_ring):
        cw = SIDE_W/2
        for r in range(N_ROWS):
            for c in range(2):
                v = r*2 + c
                cx = x_base + cw*(c+0.5)
                cy = GRID_TOP + ROW_H*(r+0.5)
                mono = (v == 0)
                knob(A, t, cx, cy, KNOB_R, col_face, col_ring, mono)
                A(f'<circle id="param_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
                # voice number dot on the spine side
                sx = SPINE_CX + (-1 if kind=="rest" else 1)*(SPINE_W/2 - 1.2)
                sy = cy
                A(f'<circle cx="{px(sx)}" cy="{px(sy)}" r="{px(0.7)}" '
                  f'fill="{t["spinedot"] if mono else t["spinehi"]}" fill-opacity="{1.0 if mono else 0.5}"/>')
    bank("rest",   MARGIN,                t["restknob"], t["rest"])
    bank("accent", SPINE_CX+SPINE_W/2,    t["accknob"],  t["acc"])

    # ── three poly-cable output jacks along the bottom ──
    labels = [("output_polygate", W*0.30), ("output_polycv", W*0.5), ("output_polyaccent", W*0.70)]
    for jid, jx in labels:
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(3.6)}" fill="{t["jackwell"]}" '
          f'stroke="{t["jackring"]}" stroke-width="0.6"/>')
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(1.6)}" fill="none" stroke="{t["gold"]}" stroke-width="0.4"/>')
        A(f'<circle id="{jid}" cx="{px(jx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
    # a wave sweeping under the jacks (the straits continuing)
    wave_field(A, t, MARGIN, JACK_Y+6, W-2*MARGIN, 8, t["spine"], n=4)

    lcx = W - MARGIN - 3
    A(f'<circle cx="{px(lcx)}" cy="{px(JACK_Y)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
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
