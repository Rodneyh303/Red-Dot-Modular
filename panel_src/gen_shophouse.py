#!/usr/bin/env python3
"""Shophouse — scale expander panel (20HP portrait, functional; art refined later).

Four shophouse fronts (the scale-list street), each with 12 piano-keyboard shutters
(the scale display AND the root selector — clicking a shutter sets that front's root),
a scale knob, and a name band. Street level: CV-index IN jack + Conservation toggle.

Direct, menu-free — the whole point is beating the 3-right-click context menu.

nanosvg-safe. Kit id markers the widget binds / hit-tests:
  shutter_<front>_<semi>   invisible marker at each shutter centre (0..11) — click target for root
  param_scale_<front>      scale knob per front
  param_conservation       Conservation toggle
  input_indexcv            list-index CV in
  light_connect
The shutter DISPLAY (lit/dim/root-red) is drawn live by the widget from the active
scale mask; here we only lay out geometry + markers + a neutral facade.
"""
HP = 20
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

BLACK = {1, 3, 6, 8, 10}
WHITE_ORDER = [0, 2, 4, 5, 7, 9, 11]
BLACK_AFTER = {1: 0, 3: 1, 6: 3, 8: 4, 10: 5}

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  well="#0f1114", wellring="#3a3a3a", jackwell="#0c0e11", jackring="#4a4a4a",
                  group="#1c1f24", groupline="#33373d", teal="#26a69a", roof="#8a1414"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  well="#e8e2d6", wellring="#c0b8a8", jackwell="#e2ddd2", jackring="#b0a898",
                  group="#d0d0d0", groupline="#bcbcbc", teal="#1c7a70", roof="#c89090"),
}

NUM_FRONTS = 4
# Four fronts stacked as a vertical street (portrait). Each front = a horizontal band.
FRONT_TOP  = 20.0
FRONT_H    = 20.0
FRONT_GAP  = 2.0
FRONT_X    = 6.0
FRONT_W    = W - 12.0


def shutter_geometry(fx, fy, fw, fh):
    """Return {semi: (cx, cy)} shutter centres for one front (piano-keyboard layout)."""
    pad = 2.0
    bx, by = fx + pad, fy + pad + 4.0     # leave room for scale-name band on top
    bw, bh = fw - 2*pad, fh - pad - 6.0
    nW = len(WHITE_ORDER)
    wgap = 0.6
    ww = (bw - (nW-1)*wgap) / nW
    wh = bh
    cen = {}
    for i, semi in enumerate(WHITE_ORDER):
        sx = bx + i*(ww+wgap)
        cen[semi] = (sx + ww/2, by + wh*0.66)   # click centre lower (white part)
    bwd = ww * 0.62
    bhh = wh * 0.55
    for semi, after in BLACK_AFTER.items():
        sx = bx + (after+1)*(ww+wgap) - wgap - bwd/2
        cen[semi] = (sx + bwd/2, by + bhh*0.5)
    return cen, (bx, by, bw, bh, ww, wh, wgap, bwd, bhh)


def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    # pediment / roof + wordmark space
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<polygon points="{px(W*0.5)},{px(4)} {px(W*0.5-10)},{px(13)} {px(W*0.5+10)},{px(13)}" fill="{t["roof"]}" opacity="0.7"/>')
    A(f'<circle cx="{px(W-4)}" cy="{px(6)}" r="{px(1.6)}" fill="{t["red"]}"/>')

    for f in range(NUM_FRONTS):
        fy = FRONT_TOP + f*(FRONT_H+FRONT_GAP)
        # facade band
        A(f'<rect x="{px(FRONT_X)}" y="{px(fy)}" width="{px(FRONT_W)}" height="{px(FRONT_H)}" '
          f'rx="{px(1.5)}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
        # scale-name band (top strip of the front)
        A(f'<rect x="{px(FRONT_X+1)}" y="{px(fy+1)}" width="{px(FRONT_W-18)}" height="{px(3.5)}" '
          f'rx="{px(0.8)}" fill="{t["well"]}" opacity="0.6"/>')
        # name-band marker (widget draws the live scale name here)
        A(f'<circle id="name_band_{f}" cx="{px(FRONT_X+1+(FRONT_W-18)/2)}" cy="{px(fy+2.75)}" r="0.5" fill="none" stroke="none"/>')
        # scale knob (right end of the name band)
        kx, ky = FRONT_X + FRONT_W - 7.0, fy + 3.5
        A(f'<circle cx="{px(kx)}" cy="{px(ky)}" r="{px(3.0)}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="1.25"/>')
        A(f'<line x1="{px(kx)}" y1="{px(ky)}" x2="{px(kx)}" y2="{px(ky-2.4)}" stroke="{t["gold"]}" stroke-width="1"/>')
        A(f'<circle id="param_scale_{f}" cx="{px(kx)}" cy="{px(ky)}" r="0.5" fill="none" stroke="none"/>')

        cen, geo = shutter_geometry(FRONT_X, fy, FRONT_W, FRONT_H)
        bx, by, bw, bh, ww, wh, wgap, bwd, bhh = geo
        # draw neutral shutters (widget re-colours live) + emit click markers
        for i, semi in enumerate(WHITE_ORDER):
            sx = bx + i*(ww+wgap)
            A(f'<rect x="{px(sx)}" y="{px(by)}" width="{px(ww)}" height="{px(wh)}" rx="{px(0.6)}" '
              f'fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="0.5"/>')
        for semi, after in BLACK_AFTER.items():
            sx = bx + (after+1)*(ww+wgap) - wgap - bwd/2
            A(f'<rect x="{px(sx)}" y="{px(by)}" width="{px(bwd)}" height="{px(bhh)}" rx="{px(0.5)}" '
              f'fill="{t["jackwell"]}" stroke="{t["wellring"]}" stroke-width="0.5"/>')
        for semi in range(12):
            cx, cy = cen[semi]
            A(f'<circle id="shutter_{f}_{semi}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    # street level: CV index in + attenuverter + Conservation toggle
    sy = FRONT_TOP + NUM_FRONTS*(FRONT_H+FRONT_GAP) + 6.0
    jx = W*0.22
    A(f'<circle cx="{px(jx)}" cy="{px(sy)}" r="{px(3.9)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
    A(f'<circle id="input_indexcv" cx="{px(jx)}" cy="{px(sy)}" r="0.5" fill="none" stroke="none"/>')
    # index CV attenuverter
    ax = W*0.45
    A(f'<circle cx="{px(ax)}" cy="{px(sy)}" r="{px(3.0)}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="1.25"/>')
    A(f'<line x1="{px(ax)}" y1="{px(sy)}" x2="{px(ax)}" y2="{px(sy-2.4)}" stroke="{t["gold"]}" stroke-width="1"/>')
    A(f'<circle id="param_indexcvatt" cx="{px(ax)}" cy="{px(sy)}" r="0.5" fill="none" stroke="none"/>')
    tx = W*0.68
    A(f'<rect x="{px(tx-4)}" y="{px(sy-4)}" width="{px(8)}" height="{px(8)}" rx="{px(1)}" '
      f'fill="{t["well"]}" stroke="{t["teal"]}" stroke-width="1.25"/>')
    A(f'<circle id="param_conservation" cx="{px(tx)}" cy="{px(sy)}" r="0.5" fill="none" stroke="none"/>')

    A(f'<circle id="light_connect" cx="{px(W*0.5)}" cy="{px(9)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Shophouse_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Shophouse {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
