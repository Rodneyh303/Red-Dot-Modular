#!/usr/bin/env python3
"""Causeway — poly modulation interface for Straits (22HP), styled as the Johor–Singapore Causeway.

Carries per-voice CV attenuators that modulate Straits' REST + ACCENT probability levels:
  16 REST attenuators (left) + 16 ACCENT attenuators (right)  (voice 0 = mono/ch0, 1..15 poly)
  + global REST + global ACCENT attenuators
  + 2 CV inputs (poly rest mod, poly accent mod) — 16ch, ch0 = mono.

Motif: the Causeway bridge — a trussed roadway crossing water. The two attenuator banks are the
two carriageways; truss girders run between the numbered voice channels; the CV inputs enter at the
ends as "FROM MONSOON" (left) and "TO STRAITS" (right). Water ripples beneath. REST muted / ACCENT
vibrant, matching the Straits rest-vs-accent colour language.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).

Kit id markers (widget binds; voice v 0..15, v0 = mono):
  param_restatt_<0..15> / param_accatt_<0..15>   per-voice attenuators
  param_restatt_global / param_accatt_global      global attenuators
  input_restcv / input_accentcv                   16ch CV in
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
    "dark":  dict(bg="#101319", red="#d4001a", ink="#f0f0f0",
                  rest="#3f7d78", restknob="#1a2e2c",
                  acc="#e08a1a", accknob="#3a2a10",
                  truss="#7a6a3a", trusshi="#b09850", trussdim="#4a4020",
                  deck="#2a2e35", deckline="#3a4048", water="#2a5a72", waterhi="#4a86a8",
                  knobface="#2a2e33", knobring="#4a5058", knobtick="#c0c8d0",
                  jackwell="#0c0e11", jackring="#4a4a4a", gold="#c8960c",
                  wave_op=0.5),
    "light": dict(bg="#e2e0d6", red="#d4001a", ink="#1a1a1a",
                  rest="#5a9a94", restknob="#c8ddd9",
                  acc="#c88018", accknob="#e4d4b8",
                  truss="#a89050", trusshi="#8a7238", trussdim="#c4b488",
                  deck="#c4c8cc", deckline="#a8b0b8", water="#7ba8c0", waterhi="#5a8aa4",
                  knobface="#e8e2d6", knobring="#b0a898", knobtick="#5a5040",
                  jackwell="#e2ddd2", jackring="#b0a898", gold="#b07d00",
                  wave_op=0.72),
}

MARGIN   = 5.0
SPINE_W  = 12.0
SPINE_CX = W/2
SIDE_W   = (W - 2*MARGIN - SPINE_W) / 2
BRIDGE_TOP = 26.0
N_ROWS   = 8
ROW_H    = 9.6
KNOB_R   = 2.7
GLOBAL_Y = BRIDGE_TOP - 6.0
CV_Y     = 16.0
WATER_Y  = BRIDGE_TOP + N_ROWS*ROW_H + 4.0

def truss_span(A, t, x0, y0, w, h):
    """A truss girder panel (the causeway structure) across (x0,y0,w,h) — zigzag web + chords."""
    A(f'<rect x="{px(x0)}" y="{px(y0)}" width="{px(w)}" height="{px(h)}" '
      f'fill="{t["deck"]}" fill-opacity="0.4" stroke="{t["trussdim"]}" stroke-width="0.3"/>')
    # top & bottom chords
    for yy in (y0, y0+h):
        A(f'<line x1="{px(x0)}" y1="{px(yy)}" x2="{px(x0+w)}" y2="{px(yy)}" '
          f'stroke="{t["truss"]}" stroke-width="0.4"/>')
    # zigzag web
    seg = 8
    up = True
    pts = []
    for k in range(seg+1):
        xx = x0 + w*k/seg
        yy = y0 if up else y0+h
        pts.append(f"{px(xx)},{px(yy)}")
        up = not up
    A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{t["truss"]}" '
      f'stroke-width="0.35" stroke-opacity="0.7"/>')

def water(A, t, x0, y0, w, h, n=5):
    for i in range(n):
        yy = y0 + h*i/(n-1)
        pts = []
        seg = 18
        amp = 1.0 + (i % 2)*0.6
        for k in range(seg+1):
            xx = x0 + w*k/seg
            wy = yy + amp*math.sin(k*0.8 + i*0.9)
            pts.append(f"{px(xx)},{px(wy)}")
        A(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{t["water"]}" '
          f'stroke-width="0.4" stroke-opacity="{t["wave_op"]}"/>')

def knob(A, t, cx, cy, r, ring, mono=False):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{t["knobface"]}" '
      f'stroke="{ring}" stroke-width="{px(0.8 if mono else 0.5)}"/>')
    A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r*0.8)}" '
      f'stroke="{t["knobtick"]}" stroke-width="{px(0.45)}"/>')
    if mono:
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+1.0)}" fill="none" '
          f'stroke="{t["red"]}" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')

def cvjack(A, t, cx, cy, col):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(3.4)}" fill="{t["jackwell"]}" '
      f'stroke="{t["jackring"]}" stroke-width="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(1.5)}" fill="none" stroke="{col}" stroke-width="0.5"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # ── the bridge span header: a shallow suspension/truss silhouette across the top ──
    span_y = CV_Y - 3
    A(f'<line x1="{px(MARGIN)}" y1="{px(span_y)}" x2="{px(W-MARGIN)}" y2="{px(span_y)}" '
      f'stroke="{t["truss"]}" stroke-width="0.6"/>')
    # bridge towers + cables
    for tx in (W*0.28, W*0.72):
        A(f'<line x1="{px(tx)}" y1="{px(span_y)}" x2="{px(tx)}" y2="{px(span_y-7)}" '
          f'stroke="{t["trusshi"]}" stroke-width="0.6"/>')
        # cable sweeps
        A(f'<path d="M {px(tx-W*0.22)} {px(span_y)} Q {px(tx)} {px(span_y-9)} {px(tx+W*0.22)} {px(span_y)}" '
          f'fill="none" stroke="{t["truss"]}" stroke-width="0.35" stroke-opacity="0.7"/>')

    # ── CV inputs at the ends: FROM MONSOON (left), TO STRAITS (right) ──
    cvjack(A, t, MARGIN+5, CV_Y, t["rest"])
    A(f'<circle id="input_restcv" cx="{px(MARGIN+5)}" cy="{px(CV_Y)}" r="0.5" fill="none" stroke="none"/>')
    cvjack(A, t, W-MARGIN-5, CV_Y, t["acc"])
    A(f'<circle id="input_accentcv" cx="{px(W-MARGIN-5)}" cy="{px(CV_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── global attenuators (one per side, above the banks) ──
    knob(A, t, MARGIN+SIDE_W*0.5, GLOBAL_Y, KNOB_R+0.4, t["rest"])
    A(f'<circle id="param_restatt_global" cx="{px(MARGIN+SIDE_W*0.5)}" cy="{px(GLOBAL_Y)}" r="0.5" fill="none" stroke="none"/>')
    knob(A, t, SPINE_CX+SPINE_W/2+SIDE_W*0.5, GLOBAL_Y, KNOB_R+0.4, t["acc"])
    A(f'<circle id="param_accatt_global" cx="{px(SPINE_CX+SPINE_W/2+SIDE_W*0.5)}" cy="{px(GLOBAL_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── central truss spine (the causeway structure between the two carriageways) ──
    truss_span(A, t, SPINE_CX-SPINE_W/2, BRIDGE_TOP, SPINE_W, N_ROWS*ROW_H)

    # ── the two carriageways: truss deck behind each bank of attenuators ──
    truss_span(A, t, MARGIN, BRIDGE_TOP, SIDE_W, N_ROWS*ROW_H)
    truss_span(A, t, SPINE_CX+SPINE_W/2, BRIDGE_TOP, SIDE_W, N_ROWS*ROW_H)

    # ── attenuator grids: 2 cols x 8 rows = 16 per side ──
    def bank(kind, x_base, ring):
        cw = SIDE_W/2
        for r in range(N_ROWS):
            for c in range(2):
                v = r*2 + c
                cx = x_base + cw*(c+0.5)
                cy = BRIDGE_TOP + ROW_H*(r+0.5)
                mono = (v == 0)
                knob(A, t, cx, cy, KNOB_R, ring, mono)
                A(f'<circle id="param_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
                # voice number tick on the spine
                sx = SPINE_CX + (-1 if kind=="restatt" else 1)*(SPINE_W/2 - 1.0)
                A(f'<circle cx="{px(sx)}" cy="{px(cy)}" r="{px(0.7)}" '
                  f'fill="{t["trusshi"] if mono else t["truss"]}" fill-opacity="{1.0 if mono else 0.6}"/>')
    bank("restatt", MARGIN,               t["rest"])
    bank("accatt",  SPINE_CX+SPINE_W/2,   t["acc"])

    # ── water beneath the causeway ──
    water(A, t, MARGIN, WATER_Y, W-2*MARGIN, 12)

    lcx = W - MARGIN - 3
    A(f'<circle cx="{px(lcx)}" cy="{px(WATER_Y+13)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(WATER_Y+13)}" r="0.5" fill="none" stroke="none"/>')
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
