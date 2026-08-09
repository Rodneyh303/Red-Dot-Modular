#!/usr/bin/env python3
"""Shophouse Micro — tuning+scale scene modulation for the Colonnades family (SHOPHOUSE_MICRO_SPEC).

DERIVED FROM gen_shophouse.py — KEEP THE BUILDING. The microtonal generalisation of Shophouse: the two
Peranakan shophouses (hipped tiled roofs, dormers, segmental-arched windows, majolica tile bands,
pilasters, the five-foot-way arcade + pintu-pagar gate) are UNCHANGED. Only the WINDOW CONTENTS change:
the 12 piano-keyboard shutters are replaced by an EMPTY recessed window well, and the widget fills it
with N equal-width ALTERNATING-COLOUR mask cells (SHOPHOUSE_MICRO_SPEC §224) that light/dim by the
loaded .dmtune's weight[] — no piano-key resemblance. The per-front SCALE knob and ROOT are DROPPED
(a front is just "which .dmtune"). The name band stays (abbreviated .dmtune name, doubles as load target).

STORY/MODE (spec §234): the building holds either FOUR 12-cell windows (12-mode, 2x2: houseA upper/lower,
houseB upper/lower) or TWO 24-cell windows (24-mode, one full-width per story). This SVG draws the
12-mode 2x2 layout; the widget MERGES each story's two windows into one 24-strip in 24-mode.

nanosvg-safe: solid fills + strokes + closed arc paths only (no gradients/masks/text/url). The mask
cells, names, wordmark and lantern glow are all widget-drawn.

Kit id markers (geometry single-sourced from the panel):
  window_<front>     the empty window well RECT — widget fills it with N mask cells (front 0..3)
  name_band_<front>  abbreviated .dmtune name readout / load click target
  lantern_<front>    active-front lantern anchor (above each window)
  param_indexcvatt   index CV attenuverter
  param_conservation Conservation toggle (Guide/Enforce)
  input_indexcv      scene-index CV in
  light_connect      host-connection mark
  wordmark           title anchor (widget-drawn text)
"""
import math
HP = 26
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

FACADE = {
    "dark":  ["#4a3f48", "#463b44", "#4a3a40", "#463640"],
    "light": ["#d8c4cc", "#d0bcc4", "#dcc0c4", "#d4b8bc"],
}
THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  winwell="#0f1620", winring="#5a7a78",
                  jackwell="#0c0e11", jackring="#4a4a4a", teal="#26a69a",
                  plaster="#5a6470", plasterhi="#727e8c", plastersh="#3c434c",
                  surround="#8a94a0", cornice="#7c8794", roof="#6b4a3e", roofhi="#8a6454",
                  roofsh="#4a3228", arch="#4a5058", namewell="#0f1114",
                  tileA="#c8960c", tileB="#26a69a", tileC="#b5546a", tilebg="#20242a"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  winwell="#c2ccd6", winring="#9ab4b2",
                  jackwell="#e2ddd2", jackring="#b0a898", teal="#1c7a70",
                  plaster="#c8cdd4", plasterhi="#e4e8ec", plastersh="#a4acb4",
                  surround="#d8d0c8", cornice="#c8b8b0", roof="#8a6858", roofhi="#a88878",
                  roofsh="#6a4838", arch="#c0c6cc", namewell="#e8e2d6",
                  tileA="#b07d00", tileB="#1c7a70", tileC="#a03a52", tilebg="#cdd2d8"),
}

# ── Layout (unchanged from gen_shophouse — the building geometry is preserved). ──
MARGIN = 6.0
GUTTER = 3.0
HOUSE_W = (W - 2*MARGIN - GUTTER) / 2
HOUSE_X = [MARGIN, MARGIN + HOUSE_W + GUTTER]

ROOF_TOP  = 9.0
ROOF_H    = 12.0
CORNICE_H = 2.6
FLOOR_TOP = ROOF_TOP + ROOF_H + CORNICE_H          # 23.6

WIN_TOP_PAD_  = 6.0
WIN_H_        = 13.5
BAND_GAP_     = 1.0
TILE_BAND_H_  = 5.8
NAME_GAP_     = 1.6
NAME_H_       = 3.6
FLOOR_BOT_PAD = 1.5
FLOOR_H   = (WIN_TOP_PAD_ + WIN_H_ + BAND_GAP_ + TILE_BAND_H_
             + NAME_GAP_ + NAME_H_ + FLOOR_BOT_PAD)                 # = 33.0
STRING_H  = 3.4
FOOT_TOP  = FLOOR_TOP + 2*FLOOR_H + STRING_H       # 93.0
FOOT_H    = 25.0
PILW = 2.6

def front_house(f): return f // 2
def front_floor(f): return f % 2
def front_y(f): return FLOOR_TOP + front_floor(f)*(FLOOR_H + STRING_H)

WIN_MARGIN_X = 4.6
WIN_TOP_PAD  = WIN_TOP_PAD_
WIN_H        = WIN_H_
TILE_BAND_H  = TILE_BAND_H_

def window_rect(f):
    hx = HOUSE_X[front_house(f)]
    fy = front_y(f)
    return hx + WIN_MARGIN_X, fy + WIN_TOP_PAD, HOUSE_W - 2*WIN_MARGIN_X, WIN_H

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

def tile_row(A, t, x, y, w, h):
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
    over = 1.8
    x0, x1 = hx - over, hx + w + over
    top_y, bot_y = ROOF_TOP, ROOF_TOP + ROOF_H
    inset = w * 0.22
    A(f'<polygon points="{px(x0)},{px(bot_y)} {px(hx+inset)},{px(top_y)} '
      f'{px(hx+w-inset)},{px(top_y)} {px(x1)},{px(bot_y)}" '
      f'fill="{t["roof"]}" stroke="{t["roofsh"]}" stroke-width="0.5"/>')
    for k in (1, 2):
        yy = top_y + (bot_y - top_y) * k/3
        ix = inset * (1 - k/3) - over*(k/3)
        A(f'<line x1="{px(hx+ix)}" y1="{px(yy)}" x2="{px(hx+w-ix)}" y2="{px(yy)}" '
          f'stroke="{t["roofsh"]}" stroke-width="0.3"/>')
    A(f'<line x1="{px(x0)}" y1="{px(bot_y)}" x2="{px(x1)}" y2="{px(bot_y)}" '
      f'stroke="{t["roofhi"]}" stroke-width="0.7"/>')
    dw = w * 0.26
    dx = hx + w/2 - dw/2
    A(f'<rect x="{px(dx)}" y="{px(top_y-4.2)}" width="{px(dw)}" height="{px(4.6)}" '
      f'rx="{px(0.3)}" fill="{t["roof"]}" stroke="{t["roofsh"]}" stroke-width="0.4"/>')
    A(f'<rect x="{px(dx+dw*0.16)}" y="{px(top_y-3.4)}" width="{px(dw*0.68)}" height="{px(2.4)}" '
      f'fill="{t["namewell"]}" stroke="{t["roofsh"]}" stroke-width="0.3"/>')
    A(f'<rect x="{px(dx-0.8)}" y="{px(top_y-4.8)}" width="{px(dw+1.6)}" height="{px(0.9)}" '
      f'fill="{t["roofhi"]}"/>')

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    facades = FACADE["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle id="wordmark" cx="{px(W/2)}" cy="{px(6.0)}" r="0.5" fill="none" stroke="none"/>')

    # ── The two houses (façades, roofs, cornices, string courses, pilasters) — UNCHANGED. ──
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

    # ── Per front: segmental arch + EMPTY window well (widget draws the mask cells) + majolica band +
    #    name display well. The piano shutters and the per-front scale knob are GONE. ──
    for f in range(4):
        house = front_house(f)
        hx = HOUSE_X[house]
        fy = front_y(f)
        wx, wy, ww, wh = window_rect(f)
        sur = 1.2
        acx = wx + ww/2
        asy = wy
        rise = ww * 0.11
        seg_r = (ww*ww/4 + rise*rise) / (2*rise)
        cap_top = asy - rise
        # segmental arched lintel above the window
        A(f'<path d="M {px(wx-sur)} {px(asy)} '
          f'A {px(seg_r)} {px(seg_r)} 0 0 1 {px(wx+ww+sur)} {px(asy)} '
          f'L {px(wx+ww+sur)} {px(asy-1.6)} '
          f'A {px(seg_r)} {px(seg_r)} 0 0 0 {px(wx-sur)} {px(asy-1.6)} Z" '
          f'fill="{t["surround"]}" stroke="{t["plastersh"]}" stroke-width="0.35"/>')
        # keystone
        A(f'<polygon points="{px(acx-1.2)},{px(cap_top-0.6)} {px(acx+1.2)},{px(cap_top-0.6)} '
          f'{px(acx+0.8)},{px(cap_top+2.0)} {px(acx-0.8)},{px(cap_top+2.0)}" '
          f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
        # window recess (EMPTY dark well; widget overlays N alternating mask cells)
        A(f'<rect x="{px(wx-0.4)}" y="{px(wy-0.4)}" width="{px(ww+0.8)}" height="{px(wh+0.8)}" '
          f'rx="{px(0.4)}" fill="{t["winwell"]}" stroke="{t["surround"]}" stroke-width="0.9"/>')
        # the marker rect the widget fills with mask cells
        A(f'<rect id="window_{f}" x="{px(wx)}" y="{px(wy)}" width="{px(ww)}" height="{px(wh)}" '
          f'fill="none" stroke="none"/>')
        # majolica band below the window (signature element — KEEP)
        band_y = wy + wh + BAND_GAP_
        tile_row(A, t, wx, band_y, ww, TILE_BAND_H)
        # name display well below the band (abbreviated .dmtune name / load target)
        nw_top = band_y + TILE_BAND_H + NAME_GAP_
        nw_x, nw_w = wx + 1.0, ww - 2.0
        A(f'<rect x="{px(nw_x-0.5)}" y="{px(nw_top-0.5)}" width="{px(nw_w+1.0)}" height="{px(NAME_H_+1.0)}" '
          f'rx="{px(0.6)}" fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.35"/>')
        A(f'<rect x="{px(nw_x)}" y="{px(nw_top)}" width="{px(nw_w)}" height="{px(NAME_H_)}" '
          f'rx="{px(0.4)}" fill="{t["namewell"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
        A(f'<rect id="name_band_{f}" x="{px(nw_x)}" y="{px(nw_top)}" width="{px(nw_w)}" '
          f'height="{px(NAME_H_)}" fill="none" stroke="none"/>')
        # active-front lantern anchor (above the arch crown)
        A(f'<circle id="lantern_{f}" cx="{px(acx)}" cy="{px(cap_top-1.4)}" r="0.5" fill="none" stroke="none"/>')

    # ══ Five-foot-way (ground floor colonnade) — UNCHANGED from gen_shophouse. ══
    fw_y = FOOT_TOP
    A(f'<rect x="{px(MARGIN)}" y="{px(fw_y)}" width="{px(W-2*MARGIN)}" height="{px(FOOT_H)}" '
      f'rx="{px(1.0)}" fill="{t["arch"]}" stroke="{t["plastersh"]}" stroke-width="0.5"/>')
    A(f'<rect x="{px(MARGIN)}" y="{px(fw_y)}" width="{px(W-2*MARGIN)}" height="{px(2.2)}" '
      f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
    n_arch = 5
    col_w = 3.0
    span = W - 2*MARGIN - 2.0
    arch_pitch = span / n_arch
    a_w = arch_pitch - col_w
    arch_top = fw_y + 3.0
    arch_h = 12.0
    ar = a_w/2
    for a in range(n_arch):
        ax = MARGIN + 1.0 + a*arch_pitch + col_w/2
        A(f'<path d="M {px(ax)} {px(arch_top+arch_h)} L {px(ax)} {px(arch_top+ar)} '
          f'A {px(ar)} {px(ar)} 0 0 1 {px(ax+a_w)} {px(arch_top+ar)} '
          f'L {px(ax+a_w)} {px(arch_top+arch_h)} Z" '
          f'fill="{t["bg"]}" stroke="{t["plaster"]}" stroke-width="0.5"/>')
        A(f'<rect x="{px(ax+a_w/2-0.7)}" y="{px(arch_top)}" width="{px(1.4)}" height="{px(2.0)}" '
          f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
    for c in range(n_arch+1):
        cxp = MARGIN + 1.0 + c*arch_pitch - col_w/2
        cxp = max(MARGIN+0.6, min(cxp, W-MARGIN-0.6-col_w))
        A(f'<rect x="{px(cxp)}" y="{px(arch_top-0.5)}" width="{px(col_w)}" height="{px(arch_h+2.5)}" '
          f'fill="{t["plaster"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        A(f'<line x1="{px(cxp+col_w/2)}" y1="{px(arch_top)}" x2="{px(cxp+col_w/2)}" '
          f'y2="{px(arch_top+arch_h+1)}" stroke="{t["plasterhi"]}" stroke-width="0.3"/>')

    plinth_y = arch_top + arch_h + 2.5
    A(f'<rect x="{px(MARGIN)}" y="{px(plinth_y)}" width="{px(W-2*MARGIN)}" height="{px(fw_y+FOOT_H-plinth_y)}" '
      f'fill="{t["plaster"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')

    # pintu pagar centre gate (signature)
    gate_cx = W/2
    gate_w = 11.0
    gate_top = arch_top + 1.5
    gate_bot = arch_top + arch_h
    gx0 = gate_cx - gate_w/2
    A(f'<rect x="{px(gx0-0.6)}" y="{px(gate_top-0.6)}" width="{px(gate_w+1.2)}" height="{px(gate_bot-gate_top+0.6)}" '
      f'fill="{t["namewell"]}" stroke="{t["plaster"]}" stroke-width="0.6"/>')
    for leaf in (0, 1):
        lx0 = gate_cx - gate_w/2 + leaf*(gate_w/2)
        lw = gate_w/2 - 0.3
        A(f'<rect x="{px(lx0)}" y="{px(gate_top)}" width="{px(lw)}" height="{px(gate_bot-gate_top)}" '
          f'fill="none" stroke="{t["cornice"]}" stroke-width="0.6"/>')
        A(f'<rect x="{px(lx0)}" y="{px(gate_top)}" width="{px(lw)}" height="{px(1.6)}" '
          f'fill="{t["cornice"]}" stroke="none"/>')
        nb = 4
        for b in range(1, nb+1):
            bx = lx0 + lw*b/(nb+1)
            A(f'<line x1="{px(bx)}" y1="{px(gate_top+1.8)}" x2="{px(bx)}" y2="{px(gate_bot)}" '
              f'stroke="{t["cornice"]}" stroke-width="0.5"/>')
        A(f'<line x1="{px(lx0)}" y1="{px((gate_top+gate_bot)/2)}" x2="{px(lx0+lw)}" '
          f'y2="{px((gate_top+gate_bot)/2)}" stroke="{t["cornice"]}" stroke-width="0.4"/>')

    ctl_y = plinth_y + (fw_y + FOOT_H - plinth_y) / 2
    for k in range(3):
        tile(A, t, 24.0 + k*3.0, ctl_y, 1.0)
    for k in range(3):
        tile(A, t, 91.0 + k*3.0, ctl_y, 1.0)

    # ── controls on the plinth, flanking the pintu-pagar gate (scale knob removed; these are all). ──
    cy_ctrl = plinth_y + (fw_y+FOOT_H-plinth_y)/2
    icx = MARGIN + 9
    A(f'<circle cx="{px(icx)}" cy="{px(cy_ctrl)}" r="{px(3.0)}" fill="{t["jackwell"]}" '
      f'stroke="{t["jackring"]}" stroke-width="0.5"/>')
    A(f'<circle id="input_indexcv" cx="{px(icx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    # attenuverter — left of the gate
    atx = gate_cx - gate_w/2 - 6.5
    A(f'<circle cx="{px(atx)}" cy="{px(cy_ctrl)}" r="{px(2.6)}" fill="{t["namewell"]}" '
      f'stroke="{t["plastersh"]}" stroke-width="0.4"/>')
    A(f'<circle id="param_indexcvatt" cx="{px(atx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    # conservation toggle — right of the gate
    ctx = gate_cx + gate_w/2 + 6.5
    A(f'<rect x="{px(ctx-2)}" y="{px(cy_ctrl-3)}" width="{px(4)}" height="{px(6)}" '
      f'rx="{px(0.5)}" fill="{t["namewell"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
    A(f'<circle id="param_conservation" cx="{px(ctx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    # connect light — far right
    lcx = W - MARGIN - 9
    A(f'<circle cx="{px(lcx)}" cy="{px(cy_ctrl)}" r="{px(1.6)}" fill="{t["namewell"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(cy_ctrl)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "ShophouseMicro_panel_dark.svg"), (False, "ShophouseMicro_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"ShophouseMicro {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
