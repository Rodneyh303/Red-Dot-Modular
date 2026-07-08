#!/usr/bin/env python3
"""Changi — per-voice gate/CV/accent output expander (24HP), as a dense AIRPORT DIAGRAM.

Airport-first (per Rodney): the panel reads as an aeronautical apron/taxiway chart — dense gold
taxiway routes curving between the jack clusters, runway with centre-line + threshold markings, a
control tower, aircraft glyphs at gates, holding-point bars, apron stand outlines. The jacks are the
aircraft stands; taxiway lines connect them like a real airport ground chart. (The Jewel vortex was
removed — it read badly. A Changi-specific touch can be layered later once the airport reads well.)

16 voices (voice 0 = mono/ch0, voices 2..16 = poly), 3 jacks each: GATE / ACCENT / CV.

nanosvg-safe (solid fills/strokes, elliptical-arc paths OK; no gradient/mask/text/url).

Kit id markers: output_{gate,accent,cv}_<0..15> + light_connect
"""
import math
HP = 24
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#0e120e", red="#d4001a", ink="#f0ead8",
                  gold="#c8960c", goldhi="#e8c050", golddim="#6a5214", goldfaint="#3a2e0c",
                  tarmac="#161a15", tarmachi="#20261e", apron="#1c221a",
                  jackwell="#0a0c08", jackring="#5a4a1c", gatecol="#6c8cd4", acccol="#e0a020", cvcol="#26a69a",
                  paint="#c8b048", paintdim="#8a7628"),
    "light": dict(bg="#e8e4d4", red="#d4001a", ink="#2a2418",
                  gold="#8a6a10", goldhi="#a88420", golddim="#c0b088", goldfaint="#d4caa0",
                  tarmac="#d0ccbc", tarmachi="#c4c0b0", apron="#ccc8b6",
                  jackwell="#d8d0b8", jackring="#a89860", gatecol="#4c6ab0", acccol="#a87400", cvcol="#1c7a70",
                  paint="#9a8020", paintdim="#b0a068"),
}

MARGIN = 6.0
TITLE_H = 15.0
BANDS = [("GATES", "gate", 0, "gatecol"),
         ("ACCENT", "accent", 1, "acccol"),
         ("CV", "cv", 2, "cvcol")]
BAND_TOP = 22.0
BAND_H   = 30.0
BAND_GAP = 2.5
FOOTER_Y = BAND_TOP + 3*(BAND_H+BAND_GAP)

def taxiway(A, t, x0, y0, x1, y1, curve=0.0, w=1.4):
    """A gold taxiway route (double edge lines + faint centreline) from (x0,y0) to (x1,y1),
    optionally bowed by `curve` for the sweeping apron look."""
    mx, my = (x0+x1)/2 + curve, (y0+y1)/2
    # centreline (dashed feel via faint solid)
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px(my)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["goldfaint"]}" stroke-width="{px(w)}" stroke-opacity="0.9"/>')
    A(f'<path d="M {px(x0)} {px(y0)} Q {px(mx)} {px(my)} {px(x1)} {px(y1)}" '
      f'fill="none" stroke="{t["gold"]}" stroke-width="0.3"/>')

def jack(A, t, cx, cy, r, col):
    # apron stand outline (dashed box) behind the jack
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
    A(f'<line x1="{px(cx-1.4)}" y1="{px(cy+6)}" x2="{px(cx-1.4)}" y2="{px(cy-2)}" stroke="{t["golddim"]}" stroke-width="0.4"/>')
    A(f'<line x1="{px(cx+1.4)}" y1="{px(cy+6)}" x2="{px(cx+1.4)}" y2="{px(cy-2)}" stroke="{t["golddim"]}" stroke-width="0.4"/>')
    A(f'<polygon points="{px(cx-2.6)},{px(cy-3)} {px(cx+2.6)},{px(cy-3)} {px(cx+1.6)},{px(cy-6)} {px(cx-1.6)},{px(cy-6)}" '
      f'fill="{t["tarmachi"]}" stroke="{t["goldhi"]}" stroke-width="0.5"/>')
    for wx in (-1.2, 0, 1.2):
        A(f'<line x1="{px(cx+wx)}" y1="{px(cy-3.4)}" x2="{px(cx+wx)}" y2="{px(cy-5.6)}" stroke="{t["goldhi"]}" stroke-width="0.3"/>')
    A(f'<line x1="{px(cx)}" y1="{px(cy-6)}" x2="{px(cx)}" y2="{px(cy-8.5)}" stroke="{t["gold"]}" stroke-width="0.4"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy-8.8)}" r="{px(0.7)}" fill="{t["red"]}"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o=[]; A=o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # ══ RUNWAY down the centre (the through-line) — dark strip + dashed centreline + thresholds ══
    rw_w = 9.0
    rw_x = W/2 - rw_w/2
    A(f'<rect x="{px(rw_x)}" y="{px(TITLE_H)}" width="{px(rw_w)}" height="{px(FOOTER_Y-TITLE_H)}" '
      f'fill="{t["tarmac"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
    # centreline dashes
    ndash = 22
    for d in range(ndash):
        dy = TITLE_H + (FOOTER_Y-TITLE_H)*d/ndash + 1
        A(f'<rect x="{px(W/2-0.35)}" y="{px(dy)}" width="{px(0.7)}" height="{px(2.4)}" fill="{t["paint"]}" fill-opacity="0.8"/>')
    # threshold bars (piano keys) top & bottom
    for base_y, dirn in [(TITLE_H+1, 1), (FOOTER_Y-3, -1)]:
        for k in range(6):
            bx = rw_x + 0.8 + k*(rw_w-1.6)/6
            A(f'<rect x="{px(bx)}" y="{px(base_y)}" width="{px((rw_w-1.6)/6*0.55)}" height="{px(2.0)}" fill="{t["paint"]}" fill-opacity="0.7"/>')

    # ══ dense taxiway network: sweeping routes from the runway out to each jack cluster ══
    # (drawn before jacks so jacks sit on top like stands on taxiways)
    for bi in range(3):
        by = BAND_TOP + bi*(BAND_H+BAND_GAP) + BAND_H*0.5
        for side, sgn in ((0,-1),(1,1)):
            # a spur curving from runway edge out to the cluster
            x_run = W/2 + sgn*rw_w/2
            x_out = MARGIN+4 if side==0 else W-MARGIN-4
            taxiway(A, t, x_run, by-6, x_out, by-4, curve=sgn*10)
            taxiway(A, t, x_run, by+6, x_out, by+4, curve=sgn*10)
            # cross-connectors within the cluster
            taxiway(A, t, x_out, by-4, x_out, by+4, curve=sgn*3, w=1.0)
    # perimeter taxiway loop
    A(f'<rect x="{px(MARGIN-1)}" y="{px(TITLE_H+2)}" width="{px(W-2*MARGIN+2)}" height="{px(FOOTER_Y-TITLE_H-2)}" '
      f'fill="none" stroke="{t["goldfaint"]}" stroke-width="1.0" stroke-opacity="0.5"/>')

    # ══ control tower + planes in the title ══
    control_tower(A, t, W*0.5, 8.0)
    for i, pxp in enumerate([0.16, 0.30, 0.70, 0.84]):
        plane(A, t, W*pxp, 8.5, 1.5, t["gold"], ang=(0.3 if pxp<0.5 else -0.3))

    # ══ three jack bands (aircraft stands): 8 left | runway | 8 right ══
    JR = 3.1
    for label, kind, bi, colkey in BANDS:
        by = BAND_TOP + bi*(BAND_H+BAND_GAP)
        col = t[colkey]
        # band header strip with a holding-point bar
        A(f'<rect x="{px(MARGIN)}" y="{px(by)}" width="{px(W-2*MARGIN)}" height="{px(3.0)}" '
          f'fill="{t["apron"]}" stroke="{t["golddim"]}" stroke-width="0.3"/>')
        A(f'<line x1="{px(MARGIN)}" y1="{px(by+3.4)}" x2="{px(W-MARGIN)}" y2="{px(by+3.4)}" '
          f'stroke="{t["paintdim"]}" stroke-width="0.4" stroke-dasharray="{px(1)} {px(1)}"/>')
        rows, cps = 2, 4
        jy0 = by + 8.0
        row_h = (BAND_H-8.0)/rows
        side_w = (W/2 - rw_w/2 - MARGIN - 3)
        for side in (0, 1):
            x0 = MARGIN + 2.5 if side==0 else W/2 + rw_w/2 + 0.5
            cw = side_w/cps
            for r in range(rows):
                for c in range(cps):
                    v = (0 if side==0 else 8) + r*cps + c
                    cx = x0 + cw*(c+0.5)
                    cy = jy0 + row_h*(r+0.5)
                    jack(A, t, cx, cy, JR, col)
                    A(f'<circle id="output_{kind}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
        # a couple of taxiing planes per band
        plane(A, t, MARGIN+side_w*0.5, by+1.5, 1.2, t["goldhi"], ang=1.2)
        plane(A, t, W-MARGIN-side_w*0.5, by+1.5, 1.2, t["goldhi"], ang=-1.2)

    # ══ footer ══
    A(f'<line x1="{px(MARGIN)}" y1="{px(FOOTER_Y+1)}" x2="{px(W-MARGIN)}" y2="{px(FOOTER_Y+1)}" stroke="{t["golddim"]}" stroke-width="0.4"/>')
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
