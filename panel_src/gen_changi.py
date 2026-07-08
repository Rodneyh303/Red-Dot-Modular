#!/usr/bin/env python3
"""Changi — per-voice gate/CV/accent output expander (24HP), airport-themed with a Jewel
Rain Vortex spine.

Airport identity (primary): gold taxiway/apron linework, a control tower in the title, aircraft
glyphs by the gate group, runway centre-line markings. Jewel touch (secondary, restrained): a slim
side-on Rain Vortex — gold toroidal oculus with a water column falling to a pool — running the
central gap between the left and right jack columns, so the panel is unmistakably *Changi* and not
just any airport.

16 voices (voice 1 = mono/ch0, voices 2..16 = poly), 3 jacks each:
  GATE band (top), ACCENT band (middle), CV band (bottom) — 8 jacks left | spine | 8 jacks right.

nanosvg-safe (solid fills/strokes, elliptical-arc paths OK; no gradient/mask/text/url).

Kit id markers (widget binds; voice v 0..15, v0 = mono):
  output_gate_<0..15> / output_accent_<0..15> / output_cv_<0..15>
  light_connect
"""
import math
HP = 24
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#12140f", red="#d4001a", ink="#f0ead8",
                  gold="#c8960c", goldhi="#e8c050", golddim="#7a5c10",
                  jackwell="#0a0b08", jackring="#5a4a1c",
                  gatecol="#6c8cd4", acccol="#e0a020", cvcol="#26a69a",
                  terrace="#2e3e30", terrsh="#1e2a20", leaf="#4a7a50",
                  ring="#c8960c", ringhi="#f0d080", water="#9fdcea", waterhi="#e8f8fc",
                  pool="#2a7a86", poolhi="#8fe8dc", mist="#b8d4dc",
                  panelline="#3a3218", tarmac="#1a1c15"),
    "light": dict(bg="#e8e4d4", red="#d4001a", ink="#2a2418",
                  gold="#a07a00", goldhi="#c8960c", golddim="#c8bc90",
                  jackwell="#d8d0b8", jackring="#a89860",
                  gatecol="#4c6ab0", acccol="#a87400", cvcol="#1c7a70",
                  terrace="#9cb89c", terrsh="#7a9878", leaf="#5a8a60",
                  ring="#a07a00", ringhi="#c8960c", water="#7fb8cc", waterhi="#c8e8f0",
                  pool="#5a9aa6", poolhi="#3a7a86", mist="#b8d4dc",
                  panelline="#c8bc90", tarmac="#d0ccb8"),
}

MARGIN   = 7.0
SPINE_W  = 15.0
SIDE_W   = (W - 2*MARGIN - SPINE_W) / 2
SPINE_X0 = MARGIN + SIDE_W
SPINE_CX = W/2

TITLE_H = 15.0
BANDS   = [("GATES (GATE OUT)", "gate", 0),
           ("ACCENT GATE OUT",  "accent", 1),
           ("CV OUT",           "cv", 2)]
BAND_TOP = 20.0
BAND_H   = 30.0
BAND_GAP = 2.0
FOOTER_Y = BAND_TOP + 3*(BAND_H+BAND_GAP)

def jack(A, t, cx, cy, r, col):
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+0.7)}" fill="{t["jackwell"]}" '
      f'stroke="{t["jackring"]}" stroke-width="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r*0.5)}" fill="none" '
      f'stroke="{col}" stroke-width="0.5"/>')

def plane(A, t, cx, cy, s, col):
    # tiny top-down aircraft glyph
    A(f'<polygon points="{px(cx)},{px(cy-s)} {px(cx+s*0.28)},{px(cy+s*0.5)} {px(cx)},{px(cy+s*0.28)} '
      f'{px(cx-s*0.28)},{px(cy+s*0.5)}" fill="{col}"/>')
    A(f'<line x1="{px(cx-s*0.9)}" y1="{px(cy)}" x2="{px(cx+s*0.9)}" y2="{px(cy)}" '
      f'stroke="{col}" stroke-width="{px(0.5)}"/>')
    A(f'<line x1="{px(cx-s*0.3)}" y1="{px(cy+s*0.55)}" x2="{px(cx+s*0.3)}" y2="{px(cy+s*0.55)}" '
      f'stroke="{col}" stroke-width="{px(0.4)}"/>')

def control_tower(A, t, cx, cy):
    # slim control tower silhouette for the title area
    A(f'<line x1="{px(cx)}" y1="{px(cy+5)}" x2="{px(cx)}" y2="{px(cy-3)}" '
      f'stroke="{t["gold"]}" stroke-width="{px(0.8)}"/>')
    A(f'<polygon points="{px(cx-2.2)},{px(cy-3)} {px(cx+2.2)},{px(cy-3)} {px(cx+1.4)},{px(cy-5.5)} '
      f'{px(cx-1.4)},{px(cy-5.5)}" fill="{t["gold"]}"/>')
    A(f'<line x1="{px(cx)}" y1="{px(cy-5.5)}" x2="{px(cx)}" y2="{px(cy-7.5)}" '
      f'stroke="{t["goldhi"]}" stroke-width="{px(0.5)}"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy-7.8)}" r="{px(0.6)}" fill="{t["red"]}"/>')

def rain_vortex(A, t, cx, top_y, w, h):
    oculus_ry = w*0.30
    oculus_rx = w*0.48
    ocy = top_y + oculus_ry + 1
    pool_y = top_y + h
    col_top_w = w*0.30
    col_bot_w = w*0.60
    # oculus back rim
    A(f'<ellipse cx="{px(cx)}" cy="{px(ocy)}" rx="{px(oculus_rx)}" ry="{px(oculus_ry)}" '
      f'fill="none" stroke="{t["ring"]}" stroke-width="{px(1.6)}"/>')
    # water column strands
    n = 26
    for k in range(n):
        f = k/(n-1)
        xt = cx + (f-0.5)*col_top_w
        xb = cx + (f-0.5)*col_bot_w
        j = math.sin(k*2.3)*0.3
        op = 0.30 + 0.45*(1-abs(f-0.5)*2)
        A(f'<line x1="{px(xt+j)}" y1="{px(ocy+oculus_ry*0.3)}" x2="{px(xb+j)}" y2="{px(pool_y-2)}" '
          f'stroke="{t["water"]}" stroke-width="{px(0.5)}" stroke-opacity="{op:.2f}"/>')
    for k in range(8):
        f = 0.5 + (k-4)/20
        xt = cx + (f-0.5)*col_top_w*0.6
        xb = cx + (f-0.5)*col_bot_w*0.6
        A(f'<line x1="{px(xt)}" y1="{px(ocy)}" x2="{px(xb)}" y2="{px(pool_y-2)}" '
          f'stroke="{t["waterhi"]}" stroke-width="{px(0.45)}" stroke-opacity="0.45"/>')
    # oculus front rim (over the water)
    A(f'<path d="M {px(cx-oculus_rx)} {px(ocy)} '
      f'A {px(oculus_rx)} {px(oculus_ry)} 0 0 0 {px(cx+oculus_rx)} {px(ocy)}" '
      f'fill="none" stroke="{t["ringhi"]}" stroke-width="{px(1.6)}"/>')
    # pool
    A(f'<ellipse cx="{px(cx)}" cy="{px(pool_y)}" rx="{px(col_bot_w*0.7)}" ry="{px(w*0.10)}" '
      f'fill="{t["pool"]}"/>')
    A(f'<ellipse cx="{px(cx)}" cy="{px(pool_y)}" rx="{px(col_bot_w*0.7)}" ry="{px(w*0.10)}" '
      f'fill="none" stroke="{t["poolhi"]}" stroke-width="0.5"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # ── faint apron/taxiway linework across the whole panel (gold, low opacity) ──
    for gx in range(1, 6):
        xx = W*gx/6
        A(f'<line x1="{px(xx)}" y1="{px(TITLE_H)}" x2="{px(xx)}" y2="{px(FOOTER_Y)}" '
          f'stroke="{t["panelline"]}" stroke-width="0.3" stroke-opacity="0.5"/>')

    # ── central Rain Vortex spine (slim, runs the band region) ──
    # rain_vortex(A, t, SPINE_CX, BAND_TOP-1, SPINE_W, 3*(BAND_H+BAND_GAP)-4)
    # runway centre-line dashes down the spine (over the vortex base)
    for d in range(6):
        dy = FOOTER_Y - 4 - d*4
        A(f'<rect x="{px(SPINE_CX-0.5)}" y="{px(dy)}" width="{px(1.0)}" height="{px(2.2)}" '
          f'fill="{t["goldhi"]}" fill-opacity="0.5"/>')

    # ── title: control tower + planes ──
    control_tower(A, t, W*0.5, 10.5)

    # ── three jack bands: 8 left | spine | 8 right ──
    JR = 3.2
    for label, kind, bi in BANDS:
        by = BAND_TOP + bi*(BAND_H+BAND_GAP)
        col = t["gatecol"] if kind=="gate" else (t["acccol"] if kind=="accent" else t["cvcol"])
        # band header strip
        A(f'<rect x="{px(MARGIN)}" y="{px(by)}" width="{px(W-2*MARGIN)}" height="{px(3.4)}" '
          f'fill="{t["tarmac"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
        # 2 rows x 4 cols per side = 8 per side = 16
        rows, cps = 2, 4
        jy0 = by + 7.0
        row_h = (BAND_H-7.0)/rows
        for side in (0, 1):
            x0 = MARGIN + 3.0 if side==0 else SPINE_X0 + SPINE_W + 1.5
            cw = (SIDE_W-4.5)/cps
            for r in range(rows):
                for c in range(cps):
                    v = (0 if side==0 else 8) + r*cps + c   # voice index 0..15
                    cx = x0 + cw*(c+0.5)
                    cy = jy0 + row_h*(r+0.5)
                    jack(A, t, cx, cy, JR, col)
                    A(f'<circle id="output_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        # a plane glyph or two near the gate band header
        if kind=="gate":
            plane(A, t, MARGIN+SIDE_W*0.5, by+1.7, 1.4, t["gold"])
            plane(A, t, W-MARGIN-SIDE_W*0.5, by+1.7, 1.4, t["gold"])

    # ── footer: connect light + baseline apron ──
    A(f'<line x1="{px(MARGIN)}" y1="{px(FOOTER_Y+1)}" x2="{px(W-MARGIN)}" y2="{px(FOOTER_Y+1)}" '
      f'stroke="{t["golddim"]}" stroke-width="0.4"/>')
    lcx = W - MARGIN - 4
    A(f'<circle cx="{px(lcx)}" cy="{px(FOOTER_Y+5)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(FOOTER_Y+5)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Changi_panel_dark.svg"), (False, "Changi_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Changi {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
