#!/usr/bin/env python3
"""Shophouse — scale expander panel (20HP portrait), styled as a row of Emerald Hill
Peranakan shophouse fronts.

Four shophouse fronts stacked as a terrace. Each front is a colour-washed Peranakan
facade with: pilasters (plaster columns + capitals) framing it, a moulded window
surround + cornice around the shutter "window" (the 12 piano-keyboard shutters that
are the live scale display + root selector), and a name band. A five-foot-way
colonnade (arches) runs along the street level at the bottom, housing the CV in,
attenuverter, and Conservation toggle.

Palette: Peranakan pastels tuned to the dot.modular theme (muted, dark shutters so the
lit scale notes read). nanosvg-safe: solid fills + strokes only, no gradients/masks/
text/url.

Kit id markers (positions UNCHANGED from the functional layout, so widget bindings hold):
  shutter_<front>_<semi>   click target per scale degree (0..11)
  name_band_<front>        live scale-name readout position
  param_scale_<front>      scale knob per front
  param_indexcvatt         index CV attenuverter
  param_conservation       Conservation toggle
  input_indexcv            list-index CV in
  light_connect
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

# Peranakan pastel facade washes per front (muted, dot.modular-toned). Four units = a terrace.
FACADE = {
    "dark": ["#2b4a48", "#3a4038", "#3f3236", "#2e3b44"],   # teal, ochre-grey, dusty rose, slate-blue
    "light": ["#bcd6cf", "#d8cba8", "#d8b8b8", "#bcc8d4"],
}
# Pilaster / trim (plaster) per theme
THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  shutterwell="#0d0f12", shutterblack="#0a0b0d", wellring="#454a52",
                  jackwell="#0c0e11", jackring="#4a4a4a", teal="#26a69a",
                  plaster="#5a6470", plasterhi="#727e8c", plastersh="#3c434c",
                  surround="#6b7580", cornice="#7c8794", roof="#8a1414", arch="#4a5058",
                  namewell="#0f1114"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  shutterwell="#2a2e33", shutterblack="#1c1f24", wellring="#8a8a8a",
                  jackwell="#e2ddd2", jackring="#b0a898", teal="#1c7a70",
                  plaster="#c8cdd4", plasterhi="#e4e8ec", plastersh="#a4acb4",
                  surround="#b8bec6", cornice="#d0d6dc", roof="#c89090", arch="#c0c6cc",
                  namewell="#e8e2d6"),
}

NUM_FRONTS = 4
FRONT_TOP  = 22.0
FRONT_H    = 20.0
FRONT_GAP  = 2.2
FRONT_X    = 6.0
FRONT_W    = W - 12.0
PIL_W      = 3.2      # pilaster width


def shutter_geometry(fx, fy, fw, fh):
    """Return {semi: (cx, cy)} shutter centres — UNCHANGED geometry (widget hit-tests these)."""
    pad = 2.0
    bx, by = fx + pad, fy + pad + 4.0
    bw, bh = fw - 2*pad, fh - pad - 6.0
    nW = len(WHITE_ORDER)
    wgap = 0.6
    ww = (bw - (nW-1)*wgap) / nW
    wh = bh
    cen = {}
    for i, semi in enumerate(WHITE_ORDER):
        sx = bx + i*(ww+wgap)
        cen[semi] = (sx + ww/2, by + wh*0.66)
    bwd = ww * 0.62
    bhh = wh * 0.55
    for semi, after in BLACK_AFTER.items():
        sx = bx + (after+1)*(ww+wgap) - wgap - bwd/2
        cen[semi] = (sx + bwd/2, by + bhh*0.5)
    return cen, (bx, by, bw, bh, ww, wh, wgap, bwd, bhh)


def gen(dark):
    t = THEMES["dark" if dark else "light"]
    facades = FACADE["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')

    # ── Ornate pediment gable (top crest of the terrace) ──
    cxm = W*0.5
    # stepped plaster gable
    A(f'<polygon points="{px(cxm-16)},{px(18)} {px(cxm-13)},{px(9)} {px(cxm-5)},{px(4.5)} '
      f'{px(cxm)},{px(3)} {px(cxm+5)},{px(4.5)} {px(cxm+13)},{px(9)} {px(cxm+16)},{px(18)}" '
      f'fill="{t["plaster"]}" stroke="{t["plastersh"]}" stroke-width="0.75"/>')
    # gable moulding highlight
    A(f'<polygon points="{px(cxm-13)},{px(9)} {px(cxm)},{px(4)} {px(cxm+13)},{px(9)}" '
      f'fill="none" stroke="{t["plasterhi"]}" stroke-width="0.75"/>')
    # cartouche (Lissajous mark sits here — light_connect)
    A(f'<circle cx="{px(cxm)}" cy="{px(10.5)}" r="{px(3.4)}" fill="{t["bg"]}" stroke="{t["red"]}" stroke-width="1"/>')

    for f in range(NUM_FRONTS):
        fy = FRONT_TOP + f*(FRONT_H+FRONT_GAP)
        face = facades[f % len(facades)]

        # ── Facade wash (colour-washed unit) ──
        A(f'<rect x="{px(FRONT_X)}" y="{px(fy)}" width="{px(FRONT_W)}" height="{px(FRONT_H)}" '
          f'rx="{px(1.0)}" fill="{face}"/>')

        # ── Pilasters framing the front (both sides) with moulded capitals ──
        for side in (0, 1):
            px_ = FRONT_X if side == 0 else FRONT_X + FRONT_W - PIL_W
            # shaft
            A(f'<rect x="{px(px_)}" y="{px(fy+2)}" width="{px(PIL_W)}" height="{px(FRONT_H-2)}" '
              f'fill="{t["plaster"]}" stroke="{t["plastersh"]}" stroke-width="0.5"/>')
            # flute highlight
            A(f'<line x1="{px(px_+PIL_W*0.5)}" y1="{px(fy+3)}" x2="{px(px_+PIL_W*0.5)}" '
              f'y2="{px(fy+FRONT_H-1)}" stroke="{t["plasterhi"]}" stroke-width="0.5"/>')
            # capital (moulded block at top)
            A(f'<rect x="{px(px_-0.6)}" y="{px(fy+0.5)}" width="{px(PIL_W+1.2)}" height="{px(2.2)}" '
              f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')

        # ── Name band (between the capitals) ──
        nb_x = FRONT_X + PIL_W + 1.0
        nb_w = FRONT_W - 2*PIL_W - 9.0
        A(f'<rect x="{px(nb_x)}" y="{px(fy+1.0)}" width="{px(nb_w)}" height="{px(3.6)}" '
          f'rx="{px(0.6)}" fill="{t["namewell"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        A(f'<circle id="name_band_{f}" cx="{px(nb_x+nb_w/2)}" cy="{px(fy+2.8)}" r="0.5" fill="none" stroke="none"/>')

        # ── Scale knob (right, over the capital area) ──
        kx, ky = FRONT_X + FRONT_W - 7.0, fy + 3.4
        A(f'<circle cx="{px(kx)}" cy="{px(ky)}" r="{px(3.0)}" fill="{t["shutterwell"]}" stroke="{t["gold"]}" stroke-width="1.25"/>')
        A(f'<line x1="{px(kx)}" y1="{px(ky)}" x2="{px(kx)}" y2="{px(ky-2.4)}" stroke="{t["gold"]}" stroke-width="1"/>')
        A(f'<circle id="param_scale_{f}" cx="{px(kx)}" cy="{px(ky)}" r="0.5" fill="none" stroke="none"/>')

        cen, geo = shutter_geometry(FRONT_X, fy, FRONT_W, FRONT_H)
        bx, by, bw, bh, ww, wh, wgap, bwd, bhh = geo

        # ── Moulded window surround + cornice around the shutter "window" ──
        sur = 1.1
        # cornice cap above the window
        A(f'<rect x="{px(bx-sur)}" y="{px(by-sur-1.2)}" width="{px(bw+2*sur)}" height="{px(1.4)}" '
          f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.4"/>')
        # surround frame
        A(f'<rect x="{px(bx-sur)}" y="{px(by-sur)}" width="{px(bw+2*sur)}" height="{px(bh+2*sur)}" '
          f'rx="{px(0.6)}" fill="none" stroke="{t["surround"]}" stroke-width="1.4"/>')
        # dark reveal behind the shutters (window recess)
        A(f'<rect x="{px(bx-0.4)}" y="{px(by-0.4)}" width="{px(bw+0.8)}" height="{px(bh+0.8)}" '
          f'rx="{px(0.4)}" fill="{t["shutterwell"]}"/>')

        # ── Shutters (dark, widget re-colours the lit dots live) ──
        for i, semi in enumerate(WHITE_ORDER):
            sx = bx + i*(ww+wgap)
            A(f'<rect x="{px(sx)}" y="{px(by)}" width="{px(ww)}" height="{px(wh)}" rx="{px(0.5)}" '
              f'fill="{t["shutterwell"]}" stroke="{t["wellring"]}" stroke-width="0.4"/>')
        for semi, after in BLACK_AFTER.items():
            sx = bx + (after+1)*(ww+wgap) - wgap - bwd/2
            A(f'<rect x="{px(sx)}" y="{px(by)}" width="{px(bwd)}" height="{px(bhh)}" rx="{px(0.4)}" '
              f'fill="{t["shutterblack"]}" stroke="{t["wellring"]}" stroke-width="0.4"/>')
        for semi in range(12):
            cx, cy = cen[semi]
            A(f'<circle id="shutter_{f}_{semi}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    # ── Five-foot-way colonnade at street level ──
    street_y = FRONT_TOP + NUM_FRONTS*(FRONT_H+FRONT_GAP) + 1.0
    street_h = H - street_y - 2.0
    # plaster street band
    A(f'<rect x="{px(FRONT_X)}" y="{px(street_y)}" width="{px(FRONT_W)}" height="{px(street_h)}" '
      f'rx="{px(1.0)}" fill="{t["plaster"]}" stroke="{t["plastersh"]}" stroke-width="0.5"/>')
    # arch openings (recesses that house the controls)
    n_arch = 3
    arch_gap = 2.0
    arch_w = (FRONT_W - (n_arch+1)*arch_gap) / n_arch
    arch_top = street_y + 2.5
    for a in range(n_arch):
        ax0 = FRONT_X + arch_gap + a*(arch_w+arch_gap)
        # arch recess: rounded-top opening (rect + semicircle via path)
        rr = arch_w*0.5
        top = arch_top + rr
        A(f'<path d="M {px(ax0)} {px(street_y+street_h-1.5)} '
          f'L {px(ax0)} {px(top)} '
          f'A {px(rr)} {px(rr)} 0 0 1 {px(ax0+arch_w)} {px(top)} '
          f'L {px(ax0+arch_w)} {px(street_y+street_h-1.5)} Z" '
          f'fill="{t["arch"]}" stroke="{t["plastersh"]}" stroke-width="0.5"/>')
        # keystone
        A(f'<rect x="{px(ax0+arch_w/2-0.9)}" y="{px(arch_top-0.5)}" width="{px(1.8)}" height="{px(2.4)}" '
          f'fill="{t["cornice"]}" stroke="{t["plastersh"]}" stroke-width="0.3"/>')

    # controls, one per arch (centres)
    def arch_cx(a): return FRONT_X + arch_gap + a*(arch_w+arch_gap) + arch_w/2
    cy_ctl = street_y + street_h*0.62
    # arch 0: CV in
    jx = arch_cx(0)
    A(f'<circle cx="{px(jx)}" cy="{px(cy_ctl)}" r="{px(3.9)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
    A(f'<circle id="input_indexcv" cx="{px(jx)}" cy="{px(cy_ctl)}" r="0.5" fill="none" stroke="none"/>')
    # arch 1: attenuverter
    ax = arch_cx(1)
    A(f'<circle cx="{px(ax)}" cy="{px(cy_ctl)}" r="{px(3.0)}" fill="{t["shutterwell"]}" stroke="{t["gold"]}" stroke-width="1.25"/>')
    A(f'<line x1="{px(ax)}" y1="{px(cy_ctl)}" x2="{px(ax)}" y2="{px(cy_ctl-2.4)}" stroke="{t["gold"]}" stroke-width="1"/>')
    A(f'<circle id="param_indexcvatt" cx="{px(ax)}" cy="{px(cy_ctl)}" r="0.5" fill="none" stroke="none"/>')
    # arch 2: Conservation toggle
    tx = arch_cx(2)
    A(f'<rect x="{px(tx-4)}" y="{px(cy_ctl-4)}" width="{px(8)}" height="{px(8)}" rx="{px(1)}" '
      f'fill="{t["shutterwell"]}" stroke="{t["teal"]}" stroke-width="1.25"/>')
    A(f'<circle id="param_conservation" cx="{px(tx)}" cy="{px(cy_ctl)}" r="0.5" fill="none" stroke="none"/>')

    A(f'<circle id="light_connect" cx="{px(cxm)}" cy="{px(10.5)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Shophouse_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Shophouse {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
