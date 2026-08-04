#!/usr/bin/env python3
"""Changi T3 — Intertropical arranged-output breakout (28HP), airport diagram.

The third Changi terminal. T1/T2 break out Monsoon's RAW 16-voice engine; T3 breaks out
Intertropical's ARRANGED 8-channel output — the Lantern parallel (Intertropical arranges, Lantern
visualises, T3 jacks it out). 8 channels × 5 signals (GATE/CV/ACCENT/STEP/STEP-LEG) = 40 jacks,
organized BY CHANNEL (each channel's five signals in one column) because an arranged channel goes to
one synth voice, so its signals want to be together.

Same airport-apron idiom as T1/T2 (gold taxiways, control tower, aircraft glyphs) so the whole Changi
family reads as a set. Layout: 8 CHANNEL COLUMNS across, each column a vertical stack of 5 signal jacks.

nanosvg-safe (solid fills/strokes, elliptical-arc paths OK; no gradient/mask/text/url).

Kit id markers: output_ch<0..7>_<gate|cv|accent|step|stepleg> + light_connect
"""
import math
HP = 28
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#0e120e", red="#d4001a", ink="#f0ead8",
                  gold="#c8960c", goldhi="#e8c050", golddim="#6a5214", goldfaint="#3a2e0c",
                  tarmac="#161a15", tarmachi="#20261e", apron="#1c221a",
                  jackwell="#0a0c08", jackring="#5a4a1c",
                  gatecol="#6c8cd4", cvcol="#26a69a", acccol="#e0a020", stepcol="#7cc48c", slegcol="#c48cd4",
                  paint="#c8b048", paintdim="#8a7628"),
    "light": dict(bg="#e8e4d4", red="#d4001a", ink="#2a2418",
                  gold="#8a6a10", goldhi="#a88420", golddim="#c0b088", goldfaint="#d4caa0",
                  tarmac="#d0ccbc", tarmachi="#c4c0b0", apron="#ccc8b6",
                  jackwell="#d8d0b8", jackring="#a89860",
                  gatecol="#4c6ab0", cvcol="#1c7a70", acccol="#a87400", stepcol="#2c9a4c", slegcol="#9a4cb0",
                  paint="#9a8020", paintdim="#b0a068"),
}

MARGIN = 6.0
TITLE_H = 15.0
# 5 signal rows (stacked per channel column). Colours per row.
SIGS = [("GATE", "gate", "gatecol"),
        ("CV",   "cv",   "cvcol"),
        ("ACC",  "accent","acccol"),
        ("STEP", "step", "stepcol"),
        ("SLEG", "stepleg","slegcol")]
N_CH = 8
GRID_TOP = 24.0
GRID_BOT = 112.0
LABEL_W  = 8.0    # left gutter for row (signal) labels

def taxiway(A, t, x0, y0, x1, y1, curve=0.0, w=1.4):
    mx, my = (x0+x1)/2 + curve, (y0+y1)/2
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px(my)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["goldfaint"]}" stroke-width="{px(w)}" stroke-opacity="0.9"/>')
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px(my)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["gold"]}" stroke-width="0.3"/>')

def jack(A, t, cx, cy, r, col):
    A(f'<rect x="{px(cx-r-1.4)}" y="{px(cy-r-1.4)}" width="{px(2*(r+1.4))}" height="{px(2*(r+1.4))}" '
      f'fill="none" stroke="{t["golddim"]}" stroke-width="0.25" stroke-opacity="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+0.7)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r*0.5)}" fill="none" stroke="{col}" stroke-width="0.5"/>')

def plane(A, t, cx, cy, s, col, ang=0.0):
    ca, sa = math.cos(ang), math.sin(ang)
    def rot(dx, dy): return (cx+dx*ca-dy*sa, cy+dx*sa+dy*ca)
    nose = rot(0, -s); tl = rot(s*0.28, s*0.5); tail = rot(0, s*0.28); tr = rot(-s*0.28, s*0.5)
    A(f'<polygon points="{px(nose[0])},{px(nose[1])} {px(tl[0])},{px(tl[1])} {px(tail[0])},{px(tail[1])} {px(tr[0])},{px(tr[1])}" fill="{col}"/>')
    wl, wr = rot(-s*0.9, 0), rot(s*0.9, 0)
    A(f'<line x1="{px(wl[0])}" y1="{px(wl[1])}" x2="{px(wr[0])}" y2="{px(wr[1])}" stroke="{col}" stroke-width="{px(0.5)}"/>')

def control_tower(A, t, cx, cy):
    A(f'<line x1="{px(cx)}" y1="{px(cy+6)}" x2="{px(cx)}" y2="{px(cy-3)}" stroke="{t["gold"]}" stroke-width="{px(0.9)}"/>')
    A(f'<polygon points="{px(cx-2.6)},{px(cy-3)} {px(cx+2.6)},{px(cy-3)} {px(cx+1.6)},{px(cy-6)} {px(cx-1.6)},{px(cy-6)}" '
      f'fill="{t["tarmachi"]}" stroke="{t["goldhi"]}" stroke-width="0.5"/>')
    A(f'<line x1="{px(cx)}" y1="{px(cy-6)}" x2="{px(cx)}" y2="{px(cy-8.5)}" stroke="{t["gold"]}" stroke-width="0.4"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy-8.8)}" r="{px(0.7)}" fill="{t["red"]}"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o=[]; A=o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # control tower + planes in the title band
    control_tower(A, t, W*0.5, 8.0)
    for pxp in [0.12, 0.26, 0.74, 0.88]:
        plane(A, t, W*pxp, 8.5, 1.5, t["gold"], ang=(0.3 if pxp<0.5 else -0.3))

    # apron field
    field_x = MARGIN + LABEL_W
    field_w = W - MARGIN - field_x
    A(f'<rect x="{px(field_x-1)}" y="{px(GRID_TOP-3)}" width="{px(field_w+2)}" height="{px(GRID_BOT-GRID_TOP+6)}" '
      f'fill="none" stroke="{t["goldfaint"]}" stroke-width="1.0" stroke-opacity="0.5"/>')

    # channel columns: 8 apron stands across; each column a vertical stack of 5 signal jacks
    col_w = field_w / N_CH
    rows  = len(SIGS)
    row_h = (GRID_BOT - GRID_TOP) / rows
    JR = 3.1

    # signal row labels (left gutter) + faint row guide taxiways
    for ri, (lab, kind, colkey) in enumerate(SIGS):
        cy = GRID_TOP + row_h*(ri+0.5)
        A(f'<rect x="{px(MARGIN)}" y="{px(cy-2.2)}" width="{px(LABEL_W-1)}" height="{px(4.4)}" '
          f'fill="{t["apron"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
        taxiway(A, t, field_x, cy, W-MARGIN, cy, curve=0.0, w=0.8)

    # channel header strip + per-channel column of jacks
    for ch in range(N_CH):
        x0 = field_x + col_w*ch
        cx = x0 + col_w*0.5
        # channel header apron chip
        A(f'<rect x="{px(x0+0.6)}" y="{px(GRID_TOP-3.0)}" width="{px(col_w-1.2)}" height="{px(2.4)}" '
          f'fill="{t["apron"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
        # a taxiing plane atop alternate columns for texture
        plane(A, t, cx, GRID_TOP-1.8, 1.0, t["goldhi"], ang=(1.2 if ch % 2 == 0 else -1.2))
        for ri, (lab, kind, colkey) in enumerate(SIGS):
            cy = GRID_TOP + row_h*(ri+0.5)
            jack(A, t, cx, cy, JR, t[colkey])
            A(f'<circle id="output_ch{ch}_{kind}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    # footer + connect light
    A(f'<line x1="{px(MARGIN)}" y1="{px(GRID_BOT+4)}" x2="{px(W-MARGIN)}" y2="{px(GRID_BOT+4)}" stroke="{t["golddim"]}" stroke-width="0.4"/>')
    lcx = W - MARGIN - 4
    A(f'<circle cx="{px(lcx)}" cy="{px(GRID_BOT+8)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(GRID_BOT+8)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "ChangiT3_panel_dark.svg"), (False, "ChangiT3_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Changi T3 {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
