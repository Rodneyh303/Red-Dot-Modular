#!/usr/bin/env python3
"""Shophouse — scale expander panel (26HP), styled as TWO Emerald Hill Peranakan
shophouses side by side.

Two houses, each two storeys, two scale-fronts (one window per floor):
  House A (left)  = fronts 0 (upper), 1 (lower)
  House B (right) = fronts 2 (upper), 3 (lower)

Each front is an arched louvred window — the 12 piano-keyboard shutters that are the live
scale display + root selector — framed by pilasters, with Peranakan majolica tile panels
(lifted from the Interchange hex/diamond motif) flanking the windows. A hipped tiled roof
with a dormer caps each house; a string course divides the storeys; a five-foot-way colonnade
runs along the street level housing CV in / attenuverter / Conservation.

Active-scale indicator: a lit red lantern (widget-drawn) hangs over whichever front is the
committed-active scale — resolving both house (left/right) and floor (upper/lower). Panel
emits a lantern_<front> marker per window for the widget to position it.

nanosvg-safe: solid fills + strokes + closed arc paths only (no gradients/masks/text/url).

Kit id markers (geometry single-sourced from the panel; widget reads shutter RECTS via boundsOf):
  shutter_<front>_<semi>  the drawn key rect IS the marker (0..11)
  name_band_<front>       live scale-name readout position
  param_scale_<front>     scale knob per front
  lantern_<front>         active-scale lantern anchor (above each window)
  param_indexcvatt        index CV attenuverter
  param_conservation      Conservation toggle
  input_indexcv           list-index CV in
  light_connect
"""
import math
HP = 26
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

BLACK = {1, 3, 6, 8, 10}
WHITE_ORDER = [0, 2, 4, 5, 7, 9, 11]
BLACK_AFTER = {1: 0, 3: 1, 6: 3, 8: 4, 10: 5}

FACADE = {
    "dark":  ["#2b4a48", "#284440", "#3f3236", "#3a2e33"],
    "light": ["#bcd6cf", "#b2ccc5", "#d8b8b8", "#d0aeae"],
}
THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  shutterwell="#0d0f12", shutterblack="#0a0b0d", wellring="#454a52",
                  jackwell="#0c0e11", jackring="#4a4a4a", teal="#26a69a",
                  plaster="#5a6470", plasterhi="#727e8c", plastersh="#3c434c",
                  surround="#6b7580", cornice="#7c8794", roof="#7a3a30", roofhi="#9a5040",
                  roofsh="#5a2820", arch="#4a5058", namewell="#0f1114",
                  tileA="#c8960c", tileB="#26a69a", tileC="#b5546a", tilebg="#20242a"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  shutterwell="#2a2e33", shutterblack="#1c1f24", wellring="#8a8a8a",
                  jackwell="#e2ddd2", jackring="#b0a898", teal="#1c7a70",
                  plaster="#c8cdd4", plasterhi="#e4e8ec", plastersh="#a4acb4",
                  surround="#b8bec6", cornice="#d0d6dc", roof="#c08878", roofhi="#d8a898",
                  roofsh="#a06858", arch="#c0c6cc", namewell="#e8e2d6",
                  tileA="#b07d00", tileB="#1c7a70", tileC="#a03a52", tilebg="#cdd2d8"),
}

MARGIN = 4.0
GUTTER = 3.0
HOUSE_W = (W - 2*MARGIN - GUTTER) / 2
HOUSE_X = [MARGIN, MARGIN + HOUSE_W + GUTTER]

ROOF_TOP  = 5.0
ROOF_H    = 14.0
CORNICE_H = 3.0
FLOOR_TOP = ROOF_TOP + ROOF_H + CORNICE_H
FLOOR_H   = 33.0
STRING_H  = 4.0
FOOT_TOP  = FLOOR_TOP + 2*FLOOR_H + STRING_H
FOOT_H    = 20.0
PILW = 2.6

def front_house(f): return f // 2
def front_floor(f): return f % 2
def front_y(f): return FLOOR_TOP + front_floor(f)*(FLOOR_H + STRING_H)

WIN_MARGIN_X = 4.6      # inset from house edge (past pilasters)
WIN_TOP_PAD  = 8.5      # below floor top (arch springs here)
WIN_H        = 15.5     # shorter window → room for a majolica band + name band below
TILE_BAND_H  = 7.5      # horizontal majolica band under each window
WGAP = 0.5

def window_rect(f):
    hx = HOUSE_X[front_house(f)]
    fy = front_y(f)
    return hx + WIN_MARGIN_X, fy + WIN_TOP_PAD, HOUSE_W - 2*WIN_MARGIN_X, WIN_H

def shutter_geometry(f):
    wx, wy, ww, wh = window_rect(f)
    kw = (ww - 6*WGAP) / 7.0
    kh = wh
    bwd = kw * 0.60
    bhh = wh * 0.58
    cen = {}
    for i, semi in enumerate(WHITE_ORDER):
        sx = wx + i*(kw+WGAP)
        cen[semi] = (sx + kw/2, wy + kh/2)
    for semi, after in BLACK_AFTER.items():
        sx = wx + (after+1)*(kw+WGAP) - WGAP - bwd/2
        cen[semi] = (sx + bwd/2, wy + bhh/2)
    return cen, (wx, wy, ww, wh, kw, kh, bwd, bhh)

def tile(A, t, cx, cy, r):
    x0, y0 = cx - r, cy - r
    A(f'<rect x="{px(x0)}" y="{px(y0)}" width="{px(2*r)}" height="{px(2*r)}" '
      f'fill="{t["tilebg"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
    for sx in (-1, 1):
        for sy in (-1, 1):
            pxc, pyc = cx + sx*r*0.6, cy + sy*r*0.6
            A(f'<polygon points="{px(pxc)},{px(pyc-r*0.22)} {px(pxc+r*0.22)},{px(pyc)} '
              f'{px(pxc)},{px(pyc+r*0.22)} {px(pxc-r*0.22)},{px(pyc)}" fill="{t["tileA"]}" stroke="none"/>')
    A(f'<polygon points="{px(cx)},{px(cy-r*0.66)} {px(cx+r*0.34)},{px(cy)} '
      f'{px(cx)},{px(cy+r*0.66)} {px(cx-r*0.34)},{px(cy)}" fill="{t["tileC"]}" stroke="none"/>')
    A(f'<polygon points="{px(cx-r*0.66)},{px(cy)} {px(cx)},{px(cy-r*0.34)} '
      f'{px(cx+r*0.66)},{px(cy)} {px(cx)},{px(cy+r*0.34)}" fill="{t["tileB"]}" fill-opacity="0.85" stroke="none"/>')
    A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r*0.18)}" fill="{t["gold"]}"/>')

def tile_strip(A, t, x, y, w, h, n):
    if n <= 0: return
    step = h / n
    r = min(w, step) * 0.42
    for i in range(n):
        tile(A, t, x + w/2, y + step*(i+0.5), r)

def tile_row(A, t, x, y, w, h):
    """A horizontal majolica band of square floral tiles across (x,y,w,h) — the signature element."""
    # recessed band ground
    A(f'<rect x="{px(x)}" y="{px(y)}" width="{px(w)}" height="{px(h)}" '
      f'fill="{t["tilebg"]}" stroke="{t["plastersh"]}" stroke-width="0.35"/>')
    r = h*0.42
    step = 2*r + 0.6
    n = max(1, int(w / step))
    total = n*step - 0.6
    x0 = x + (w - total)/2 + r
    for i in range(n):
        tile(A, t, x0 + i*step, y + h/2, r)

def hipped_roof(A, t, hx, w):
    x0, x1 = hx, hx + w
    top_y, bot_y = ROOF_TOP, ROOF_TOP + ROOF_H
    inset = w * 0.16
    A(f'<polygon points="{px(x0)},{px(bot_y)} {px(x0+inset)},{px(top_y)} '
      f'{px(x1-inset)},{px(top_y)} {px(x1)},{px(bot_y)}" '
      f'fill="{t["roof"]}" stroke="{t["roofsh"]}" stroke-width="0.5"/>')
    for k in range(1, 4):
        yy = top_y + (bot_y - top_y) * k/4
        ix = inset * (1 - k/4)
        A(f'<line x1="{px(x0+ix)}" y1="{px(yy)}" x2="{px(x1-ix)}" y2="{px(yy)}" '
          f'stroke="{t["roofsh"]}" stroke-width="0.35"/>')
    A(f'<line x1="{px(x0)}" y1="{px(bot_y)}" x2="{px(x1)}" y2="{px(bot_y)}" '
      f'stroke="{t["roofhi"]}" stroke-width="0.6"/>')
    dw = w * 0.20
    dx = hx + w/2 - dw/2
    A(f'<rect x="{px(dx)}" y="{px(top_y-3.2)}" width="{px(dw)}" height="{px(4.0)}" '
      f'rx="{px(0.4)}" fill="{t["plaster"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    facades = FACADE["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    for house in (0, 1):
        hx = HOUSE_X[house]
        fac = facades[house*2]
        A(f'<rect x="{px(hx)}" y="{px(FLOOR_TOP-0.5)}" width="{px(HOUSE_W)}" '
          f'height="{px(2*FLOOR_H+STRING_H+1)}" rx="{px(1.2)}" fill="{fac}"/>')
        hipped_roof(A, t, hx, HOUSE_W)
        cy = FLOOR_TOP - CORNICE_H/2
        A(f'<rect x="{px(hx)}" y="{px(FLOOR_TOP-CORNICE_H)}" width="{px(HOUSE_W)}" '
          f'height="{px(CORNICE_H)}" fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        for k in range(5):
            rx = hx + HOUSE_W*(k+0.5)/5
            A(f'<circle cx="{px(rx)}" cy="{px(cy)}" r="{px(0.7)}" fill="{t["plastersh"]}"/>')
        scy = FLOOR_TOP + FLOOR_H
        A(f'<rect x="{px(hx)}" y="{px(scy)}" width="{px(HOUSE_W)}" height="{px(STRING_H)}" '
          f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        for side in (0, 1):
            pxl = hx if side == 0 else hx + HOUSE_W - PILW
            A(f'<rect x="{px(pxl)}" y="{px(FLOOR_TOP)}" width="{px(PILW)}" '
              f'height="{px(2*FLOOR_H+STRING_H)}" fill="{t["plaster"]}" '
              f'stroke="{t["plastersh"]}" stroke-width="0.4"/>')

    for f in range(4):
        house = front_house(f)
        hx = HOUSE_X[house]
        fy = front_y(f)
        cen, geo = shutter_geometry(f)
        wx, wy, ww, wh, kw, kh, bwd, bhh = geo
        sur = 1.2
        arch_r = ww/2
        acx = wx + ww/2
        asy = wy            # arch springs at window top
        # ── Arched tympanum (filled fan above the window) so the arch reads strongly ──
        A(f'<path d="M {px(wx)} {px(asy)} '
          f'A {px(arch_r)} {px(arch_r)} 0 0 1 {px(wx+ww)} {px(asy)} Z" '
          f'fill="{t["surround"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        # radiating fan ribs
        for rr in range(1, 6):
            ang = 3.14159 * rr/6
            ex = acx - arch_r*math.cos(ang)
            ey = asy - arch_r*math.sin(ang)
            A(f'<line x1="{px(acx)}" y1="{px(asy)}" x2="{px(ex)}" y2="{px(ey)}" '
              f'stroke="{t["plastersh"]}" stroke-width="0.3"/>')
        # keystone at the crown
        A(f'<polygon points="{px(acx-1.3)},{px(asy-arch_r)} {px(acx+1.3)},{px(asy-arch_r)} '
          f'{px(acx+0.9)},{px(asy-arch_r+2.6)} {px(acx-0.9)},{px(asy-arch_r+2.6)}" '
          f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
        # arch outline over the tympanum
        A(f'<path d="M {px(wx-sur)} {px(asy)} '
          f'A {px(arch_r+sur)} {px(arch_r+sur)} 0 0 1 {px(wx+ww+sur)} {px(asy)}" '
          f'fill="none" stroke="{t["surround"]}" stroke-width="1.2"/>')
        # ── window recess + shutters ──
        A(f'<rect x="{px(wx-0.4)}" y="{px(wy-0.4)}" width="{px(ww+0.8)}" height="{px(wh+0.8)}" '
          f'rx="{px(0.4)}" fill="{t["shutterwell"]}" stroke="{t["surround"]}" stroke-width="0.8"/>')
        for i, semi in enumerate(WHITE_ORDER):
            sx = wx + i*(kw+WGAP)
            A(f'<rect id="shutter_{f}_{semi}" x="{px(sx)}" y="{px(wy)}" width="{px(kw)}" height="{px(kh)}" rx="{px(0.5)}" '
              f'fill="{t["shutterwell"]}" stroke="{t["wellring"]}" stroke-width="0.4"/>')
            for sl in range(1, 4):
                ly = wy + kh*sl/4
                A(f'<line x1="{px(sx+0.6)}" y1="{px(ly)}" x2="{px(sx+kw-0.6)}" y2="{px(ly)}" '
                  f'stroke="{t["wellring"]}" stroke-width="0.3"/>')
        for semi, after in BLACK_AFTER.items():
            sx = wx + (after+1)*(kw+WGAP) - WGAP - bwd/2
            A(f'<rect id="shutter_{f}_{semi}" x="{px(sx)}" y="{px(wy)}" width="{px(bwd)}" height="{px(bhh)}" rx="{px(0.4)}" '
              f'fill="{t["shutterblack"]}" stroke="{t["wellring"]}" stroke-width="0.4"/>')
        # ── horizontal majolica band below the window (the signature element) ──
        band_y = wy + wh + 1.2
        tile_row(A, t, wx, band_y, ww, TILE_BAND_H)
        # ── name band (live scale readout) below the majolica band ──
        nb_y = band_y + TILE_BAND_H + 2.0
        A(f'<rect x="{px(wx)}" y="{px(nb_y-1.4)}" width="{px(ww)}" height="{px(2.8)}" '
          f'rx="{px(0.5)}" fill="{t["namewell"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
        A(f'<circle id="name_band_{f}" cx="{px(wx+ww/2)}" cy="{px(nb_y)}" r="0.5" fill="none" stroke="none"/>')
        kx = hx + HOUSE_W - 3.0
        ky = fy + 4.0
        A(f'<circle cx="{px(kx)}" cy="{px(ky)}" r="{px(2.6)}" fill="{t["plaster"]}" '
          f'stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        A(f'<circle id="param_scale_{f}" cx="{px(kx)}" cy="{px(ky)}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="lantern_{f}" cx="{px(acx)}" cy="{px(wy-arch_r-sur-1.0)}" r="0.5" fill="none" stroke="none"/>')

    fw_y = FOOT_TOP
    A(f'<rect x="{px(MARGIN)}" y="{px(fw_y)}" width="{px(W-2*MARGIN)}" height="{px(FOOT_H)}" '
      f'rx="{px(1.0)}" fill="{t["arch"]}" stroke="{t["plastersh"]}" stroke-width="0.5"/>')
    n_arch = 4
    a_gap = 1.6
    a_w = (W - 2*MARGIN - (n_arch+1)*a_gap) / n_arch
    for a in range(n_arch):
        ax = MARGIN + a_gap + a*(a_w+a_gap)
        ar = a_w/2
        acy = fw_y + 2.0
        A(f'<path d="M {px(ax)} {px(acy+7)} L {px(ax)} {px(acy+ar)} '
          f'A {px(ar)} {px(ar)} 0 0 1 {px(ax+a_w)} {px(acy+ar)} '
          f'L {px(ax+a_w)} {px(acy+7)} Z" fill="{t["bg"]}" stroke="{t["plaster"]}" stroke-width="0.6"/>')
    cy_ctrl = fw_y + FOOT_H - 4.5
    icx = MARGIN + 8
    A(f'<circle cx="{px(icx)}" cy="{px(cy_ctrl)}" r="{px(3.0)}" fill="{t["jackwell"]}" '
      f'stroke="{t["jackring"]}" stroke-width="0.5"/>')
    A(f'<circle id="input_indexcv" cx="{px(icx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    atx = MARGIN + 20
    A(f'<circle cx="{px(atx)}" cy="{px(cy_ctrl)}" r="{px(2.6)}" fill="{t["plaster"]}" '
      f'stroke="{t["plastersh"]}" stroke-width="0.4"/>')
    A(f'<circle id="param_indexcvatt" cx="{px(atx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    ctx = W - MARGIN - 12
    A(f'<rect x="{px(ctx-2)}" y="{px(cy_ctrl-3)}" width="{px(4)}" height="{px(6)}" '
      f'rx="{px(0.5)}" fill="{t["namewell"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
    A(f'<circle id="param_conservation" cx="{px(ctx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    lcx = W - MARGIN - 4
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Shophouse_panel_dark.svg"), (False, "Shophouse_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Shophouse {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
