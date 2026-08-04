#!/usr/bin/env python3
"""Changi T3 -- Intertropical arranged-output breakout (28HP), airport diagram.

T3 breaks out Intertropical's ARRANGED 8-channel output. Same airport-apron idiom as T1/T2:
central runway spine (vertical, splitting 4+4 channels), curved taxiway spurs, apron-stand
shapes around jack clusters. Organised BY CHANNEL (all 5 signals per channel in one column)
because arranged channels go to one synth voice.

nanosvg-safe (solid fills/strokes, elliptical-arc paths OK; no gradient/mask/text/url).
Kit id markers: output_ch<0..7>_<gate|cv|accent|step|stepleg> + light_connect
"""
import math
HP  = 28
W   = HP * 5.08
H   = 128.5
S   = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#0e120e", red="#d4001a", ink="#f0ead8",
                  gold="#c8960c", goldhi="#e8c050", golddim="#6a5214", goldfaint="#3a2e0c",
                  tarmac="#161a15", tarmachi="#20261e", apron="#1c221a",
                  jackwell="#0a0c08", jackring="#5a4a1c",
                  gatecol="#6c8cd4", cvcol="#26a69a", acccol="#e0a020",
                  stepcol="#7cc48c", slegcol="#c48cd4",
                  paint="#c8b048", paintdim="#8a7628"),
    # cooled bg (#e8e8e4 vs old #e8e4d4) removes the yellowish tinge
    "light": dict(bg="#e8e8e4", red="#d4001a", ink="#2a2418",
                  gold="#8a6a10", goldhi="#a88420", golddim="#b0a070", goldfaint="#ccc8b0",
                  tarmac="#d4d2cc", tarmachi="#c8c6be", apron="#cccac0",
                  jackwell="#d6d4cc", jackring="#a09060",
                  gatecol="#4c6ab0", cvcol="#1c7a70", acccol="#a87400",
                  stepcol="#2c9a4c", slegcol="#9a4cb0",
                  paint="#9a8020", paintdim="#b0a868"),
}

MARGIN   = 5.0
TITLE_H  = 15.0
FOOTER_Y = H - 8.0
RW_W     = 8.0        # runway width (mm)
N_CH     = 8          # 8 output channels total (4 left of runway, 4 right)
N_SIG    = 5          # signals per channel

# 5 signal rows -- each has a colour key
SIGS = [("GATE", "gate",    "gatecol"),
        ("CV",   "cv",      "cvcol"),
        ("ACC",  "accent",  "acccol"),
        ("STEP", "step",    "stepcol"),
        ("SLEG", "stepleg", "slegcol")]

def taxiway(A, t, x0, y0, x1, y1, curve=0.0, w=1.4):
    mx, my = (x0+x1)/2 + curve, (y0+y1)/2
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px(my)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["goldfaint"]}" stroke-width="{px(w)}" stroke-opacity="0.9"/>')
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px(my)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["gold"]}" stroke-width="0.3"/>')

def jack(A, t, cx, cy, r, col, kid):
    A(f'<rect x="{px(cx-r-1.4)}" y="{px(cy-r-1.4)}" width="{px(2*(r+1.4))}" height="{px(2*(r+1.4))}" '
      f'fill="none" stroke="{t["golddim"]}" stroke-width="0.25" stroke-opacity="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r+0.7)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.6"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r*0.5)}" fill="none" stroke="{col}" stroke-width="0.5"/>')
    A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

def plane(A, t, cx, cy, s, col, ang=0.0):
    ca, sa = math.cos(ang), math.sin(ang)
    def rot(dx, dy): return (cx+dx*ca-dy*sa, cy+dx*sa+dy*ca)
    nose = rot(0,-s); tl = rot(s*0.28,s*0.5); tail = rot(0,s*0.28); tr = rot(-s*0.28,s*0.5)
    A(f'<polygon points="{px(nose[0])},{px(nose[1])} {px(tl[0])},{px(tl[1])} {px(tail[0])},{px(tail[1])} {px(tr[0])},{px(tr[1])}" fill="{col}"/>')
    wl, wr = rot(-s*0.9,0), rot(s*0.9,0)
    A(f'<line x1="{px(wl[0])}" y1="{px(wl[1])}" x2="{px(wr[0])}" y2="{px(wr[1])}" stroke="{col}" stroke-width="{px(0.5)}"/>')

def control_tower(A, t, cx, cy):
    A(f'<line x1="{px(cx)}" y1="{px(cy+6)}" x2="{px(cx)}" y2="{px(cy-3)}" stroke="{t["gold"]}" stroke-width="{px(0.9)}"/>')
    A(f'<line x1="{px(cx-1.4)}" y1="{px(cy+6)}" x2="{px(cx-1.4)}" y2="{px(cy-2)}" stroke="{t["golddim"]}" stroke-width="0.4"/>')
    A(f'<line x1="{px(cx+1.4)}" y1="{px(cy+6)}" x2="{px(cx+1.4)}" y2="{px(cy-2)}" stroke="{t["golddim"]}" stroke-width="0.4"/>')
    A(f'<polygon points="{px(cx-2.6)},{px(cy-3)} {px(cx+2.6)},{px(cy-3)} {px(cx+1.6)},{px(cy-6)} {px(cx-1.6)},{px(cy-6)}" '
      f'fill="{t["tarmachi"]}" stroke="{t["goldhi"]}" stroke-width="0.5"/>')
    for wx in [-1.0, -0.33, 0.33, 1.0]:
        A(f'<line x1="{px(cx+wx)}" y1="{px(cy-3.4)}" x2="{px(cx+wx)}" y2="{px(cy-5.6)}" stroke="{t["goldhi"]}" stroke-width="0.3"/>')
    A(f'<line x1="{px(cx)}" y1="{px(cy-6)}" x2="{px(cx)}" y2="{px(cy-8.5)}" stroke="{t["gold"]}" stroke-width="0.4"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy-8.8)}" r="{px(0.7)}" fill="{t["red"]}"/>')
    # threshold bars top + bottom of runway
    for base_y, dirn in [(TITLE_H+1, 1), (FOOTER_Y-3, -1)]:
        for k in range(5):
            bx = W/2 - RW_W/2 + 0.8 + k*(RW_W-1.6)/5
            A(f'<rect x="{px(bx)}" y="{px(base_y)}" width="{px((RW_W-1.6)/5*0.55)}" height="{px(2.0)}" fill="{t["paint"]}" fill-opacity="0.7"/>')

def apron_curve(A, t, x0, y0, x1, y1, side):
    """Curved apron outline wrapping a group of jacks (the sweeping T1 shapes)."""
    sgn = -1 if side == 0 else 1
    mx = (x0+x1)/2 + sgn*8
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px((y0+y1)/2)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["gold"]}" stroke-width="{px(0.9)}" stroke-opacity="0.7"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o=[]; A=o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # --- RUNWAY (central vertical spine, splits 4+4 channels) ---
    rw_x = W/2 - RW_W/2
    A(f'<rect x="{px(rw_x)}" y="{px(TITLE_H)}" width="{px(RW_W)}" height="{px(FOOTER_Y-TITLE_H)}" '
      f'fill="{t["tarmac"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
    # centreline dashes
    ndash = 22
    for d in range(ndash):
        dy = TITLE_H + (FOOTER_Y-TITLE_H)*d/ndash + 1
        A(f'<rect x="{px(W/2-0.35)}" y="{px(dy)}" width="{px(0.7)}" height="{px(2.4)}" fill="{t["paint"]}" fill-opacity="0.8"/>')

    # --- CONTROL TOWER + planes in title band ---
    control_tower(A, t, W*0.5, 8.0)
    for pxp in [0.12, 0.26, 0.74, 0.88]:
        plane(A, t, W*pxp, 8.5, 1.5, t["gold"], ang=(0.3 if pxp<0.5 else -0.3))

    # --- TAXIWAY NETWORK (spurs from runway to each side's channel cluster) ---
    # The 8 channels sit in two groups of 4 (left=ch0-3, right=ch4-7)
    # We draw sweeping spurs at 3 vertical positions (top/mid/bot of each side)
    side_w = W/2 - RW_W/2 - MARGIN
    cluster_ys = [TITLE_H + (FOOTER_Y-TITLE_H)*f for f in [0.2, 0.5, 0.8]]
    for cy_clust in cluster_ys:
        for side, sgn in ((0,-1),(1,1)):
            x_run  = W/2 + sgn*RW_W/2
            x_out  = MARGIN + 4 if side==0 else W-MARGIN-4
            taxiway(A, t, x_run, cy_clust-5, x_out, cy_clust-3, curve=sgn*8)
            taxiway(A, t, x_run, cy_clust+5, x_out, cy_clust+3, curve=sgn*8)
            taxiway(A, t, x_out, cy_clust-3, x_out, cy_clust+3, curve=sgn*2, w=1.0)

    # perimeter loop
    A(f'<rect x="{px(MARGIN-1)}" y="{px(TITLE_H+2)}" width="{px(W-2*MARGIN+2)}" height="{px(FOOTER_Y-TITLE_H-2)}" '
      f'fill="none" stroke="{t["goldfaint"]}" stroke-width="1.0" stroke-opacity="0.5"/>')

    # --- JACK GRID (8 channels x 5 signals, organised BY CHANNEL) ---
    JR = 3.1
    # each side has 4 channels spread across the half-width
    GRID_TOP = TITLE_H + 4.0
    GRID_BOT = FOOTER_Y - 2.0
    row_h = (GRID_BOT - GRID_TOP) / N_SIG     # vertical: 5 signal rows
    left_w  = W/2 - RW_W/2 - MARGIN           # usable width per side
    col_w   = left_w / 4                       # 4 channels per side

    for side in (0, 1):
        sgn   = -1 if side==0 else 1
        x_base = MARGIN if side==0 else W/2 + RW_W/2

        # apron curves per signal row (like T1's per-band curves) -- wraps pairs of rows
        # outer-edge curve at the panel margin side, inner-edge near the runway
        x_outer = x_base + (0.5 if side==0 else -0.5)
        x_inner = x_base + (left_w - 0.5 if side==0 else 0.5 + 0)
        x_outer = MARGIN if side==0 else W-MARGIN
        x_inner = W/2 - RW_W/2 - 0.5 if side==0 else W/2 + RW_W/2 + 0.5
        # draw a sweeping curve wrapping each signal row group
        for ri in range(N_SIG):
            y0 = GRID_TOP + row_h*ri + 1.0
            y1 = GRID_TOP + row_h*(ri+1) - 1.0
            mx = x_outer + (6 if side==0 else -6)
            my = (y0+y1)/2
            A(f'<path d="M {px(x_outer)} {px(y0)} Q {px(mx)} {px(my)} {px(x_outer)} {px(y1)}" '
              f'fill="none" stroke="{t["gold"]}" stroke-width="{px(0.7)}" stroke-opacity="0.6"/>')

        for ch_side in range(4):
            ch_global = ch_side if side==0 else ch_side+4
            cx = x_base + col_w*(ch_side+0.5)

            # channel header chip
            A(f'<rect x="{px(x_base+col_w*ch_side+0.6)}" y="{px(GRID_TOP-2.5)}" '
              f'width="{px(col_w-1.2)}" height="{px(2.2)}" '
              f'fill="{t["apron"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
            # small plane on alternating channels
            if ch_side % 2 == 0:
                plane(A, t, cx, GRID_TOP-1.4, 0.9, t["goldhi"], ang=(0.8*sgn))

            # holding-point dashes under header
            A(f'<line x1="{px(x_base+col_w*ch_side+1)}" y1="{px(GRID_TOP+0.2)}" '
              f'x2="{px(x_base+col_w*(ch_side+1)-1)}" y2="{px(GRID_TOP+0.2)}" '
              f'stroke="{t["paintdim"]}" stroke-width="0.4" stroke-dasharray="{px(1)} {px(1)}"/>')

            # 5 signal jacks stacked vertically
            for ri, (lab, kind, colkey) in enumerate(SIGS):
                cy = GRID_TOP + row_h*(ri+0.5)
                jack(A, t, cx, cy, JR, t[colkey], f"output_ch{ch_global}_{kind}")

    # signal row guides (faint horizontal lines across the full width)
    for ri in range(N_SIG):
        cy = GRID_TOP + row_h*(ri+0.5)
        A(f'<line x1="{px(MARGIN)}" y1="{px(cy)}" x2="{px(W/2-RW_W/2)}" y2="{px(cy)}" '
          f'stroke="{t["goldfaint"]}" stroke-width="0.3" stroke-opacity="0.4"/>')
        A(f'<line x1="{px(W/2+RW_W/2)}" y1="{px(cy)}" x2="{px(W-MARGIN)}" y2="{px(cy)}" '
          f'stroke="{t["goldfaint"]}" stroke-width="0.3" stroke-opacity="0.4"/>')

    # connect light
    lcx = W - MARGIN - 4
    A(f'<circle cx="{px(lcx)}" cy="{px(FOOTER_Y+3)}" r="{px(1.6)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(FOOTER_Y+3)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "ChangiT3_panel_dark.svg"), (False, "ChangiT3_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Changi T3 {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP)")

if __name__ == "__main__":
    main()
