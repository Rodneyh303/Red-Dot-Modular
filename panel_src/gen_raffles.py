#!/usr/bin/env python3
"""Raffles panel (renamed from Causeway) — fanning raffle-tickets motif.

Hero graphic: a fan of numbered raffle tickets spreading from a point, in red —
evoking the random DRAW that this module performs (it modulates the dice/rhythm
stochastic generation). "Raffles" doubles as the iconic Singapore name, keeping
the place-name family. Black panel, gold trim, red detailing, white text.

Geometry matches the in-use Raffles panel: 180x380 px (mm coords, 75 DPI).
Controls live at y>=38mm; the ticket fan occupies the header (y ~ 6..34mm).
R/L sectioning (rhythm left / melody right) preserved as faint tints.
nanosvg-safe: solid fills, no gradients/masks/text/url. Screws via C++ RedScrew.
"""
import math

W, H = 180, 380
S = 75/25.4
def mm(v): return v*S

THEMES = {
    "dark":  dict(bg="#18181a", red="#d4001a", redsoft="#dc2626", gold="#c8960c",
                  ticket="#d4001a", ticketln="#18181a", line="#3a3a3e",
                  well="#0f1012", wellring="#46464c", tintR="#d4001a", tintM="#d4001a",
                  text="#f0f0f0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", redsoft="#c0001a", gold="#b07d00",
                  ticket="#d4001a", ticketln="#dcdcdc", line="#bcbcbc",
                  well="#e8e2d6", wellring="#c0b8a8", tintR="#d4001a", tintM="#d4001a",
                  text="#1a1a1a"),
}


def ticket_fan(t, cx_mm, pivot_mm):
    """A fan of numbered raffle tickets spreading from a pivot near the top
    centre. Each ticket = a rounded rectangle rotated about the pivot, with a
    perforation line + a couple of number 'ticks'."""
    o = ['<g>']
    cx = mm(cx_mm); py = mm(pivot_mm)
    tw, th = mm(8.5), mm(15)          # ticket size: wider + SHORTER (fits tight header)
    angles = [-54, -36, -18, 0, 18, 36, 54]   # 7 tickets, wider spread
    for i, a in enumerate(angles):
        rad = math.radians(a)
        # rotate the ticket rectangle about the pivot; build its 4 corners
        # ticket extends downward from pivot
        def rot(dx, dy):
            return (cx + dx*math.cos(rad) - dy*math.sin(rad),
                    py + dx*math.sin(rad) + dy*math.cos(rad))
        x0,y0 = rot(-tw/2, mm(3))
        x1,y1 = rot( tw/2, mm(3))
        x2,y2 = rot( tw/2, mm(3)+th)
        x3,y3 = rot(-tw/2, mm(3)+th)
        # slight alpha variation so the fan reads as layered
        op = 0.78 + 0.03*abs(i-3)
        o.append(f'<path d="M {x0:.1f} {y0:.1f} L {x1:.1f} {y1:.1f} '
                 f'L {x2:.1f} {y2:.1f} L {x3:.1f} {y3:.1f} Z" '
                 f'fill="{t["ticket"]}" fill-opacity="{op:.2f}" '
                 f'stroke="{t["ticketln"]}" stroke-width="0.8"/>')
        # perforation line near the top of each ticket (dashed look via short segs)
        pxa,pya = rot(-tw/2+mm(0.6), mm(3)+mm(5))
        pxb,pyb = rot( tw/2-mm(0.6), mm(3)+mm(5))
        o.append(f'<line x1="{pxa:.1f}" y1="{pya:.1f}" x2="{pxb:.1f}" y2="{pyb:.1f}" '
                 f'stroke="{t["ticketln"]}" stroke-width="0.7" stroke-dasharray="1.5,1.5"/>')
        # two number 'ticks' (abstract digits) on the stub
        for ddy in (mm(9), mm(12)):
            qxa,qya = rot(-mm(1.6), mm(3)+ddy)
            qxb,qyb = rot( mm(1.6), mm(3)+ddy)
            o.append(f'<line x1="{qxa:.1f}" y1="{qya:.1f}" x2="{qxb:.1f}" y2="{qyb:.1f}" '
                     f'stroke="{t["ticketln"]}" stroke-width="0.9" stroke-opacity="0.6"/>')
    # pivot hub
    o.append(f'<circle cx="{cx:.1f}" cy="{py:.1f}" r="{mm(1.8):.1f}" fill="{t["gold"]}"/>')
    o.append('</g>')
    return o


def jack_well(t, x_mm, y_mm, ring=None, cid=None):
    x,y = mm(x_mm), mm(y_mm)
    r = ring or t["red"]
    idattr = f' id="{cid}"' if cid else ''
    return (f'<circle{idattr} cx="{x:.1f}" cy="{y:.1f}" r="{mm(3.6):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="1.2"/>'
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{mm(2.0):.1f}" fill="none" stroke="{r}" stroke-width="0.7" opacity="0.5"/>')


def trim_well(t, x_mm, y_mm, cid=None):
    x,y = mm(x_mm), mm(y_mm)
    idattr = f' id="{cid}"' if cid else ''
    return (f'<circle{idattr} cx="{x:.1f}" cy="{y:.1f}" r="{mm(3.0):.1f}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="1.3"/>'
            f'<line x1="{x:.1f}" y1="{y-mm(0.9):.1f}" x2="{x:.1f}" y2="{y-mm(2.4):.1f}" stroke="{t["red"]}" stroke-width="1.2" stroke-linecap="round"/>')


def panel(theme):
    t = THEMES[theme]
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">']
    o.append(f'<rect width="{W}" height="{H}" fill="{t["bg"]}"/>')
    o.append(f'<rect x="0" y="0" width="{W}" height="3" fill="{t["red"]}"/>')
    o.append(f'<rect x="0" y="{H-3}" width="{W}" height="3" fill="{t["red"]}"/>')
    # ── branding band: logo mark (red dot) left + module-name space ("RAFFLES"
    #    drawn by the widget at runtime) + underline. ──
    # o.append(f'<circle cx="{mm(6):.1f}" cy="{mm(9):.1f}" r="{mm(2.4):.1f}" fill="{t["red"]}"/>')
    # o.append(f'<line x1="{mm(2):.1f}" y1="{mm(15):.1f}" x2="{mm(W/S-2):.1f}" y2="{mm(15):.1f}" stroke="{t["line"]}" stroke-width="0.8" stroke-opacity="0.6"/>')
    # R/L sides separated by OUTLINED recess boxes (no fill) — a grey highlight
    # outline reads cleaner than a red wash. Rhythm left / melody right.
    rx0, ry, rw, rh = mm(3), mm(34), mm(26), mm(88)
    o.append(f'<rect x="{rx0:.1f}" y="{ry:.1f}" width="{rw:.1f}" height="{rh:.1f}" rx="{mm(2):.1f}" '
             f'fill="none" stroke="{t["line"]}" stroke-width="1.0" stroke-opacity="0.8"/>')
    o.append(f'<rect x="{mm(31):.1f}" y="{ry:.1f}" width="{rw:.1f}" height="{rh:.1f}" rx="{mm(2):.1f}" '
             f'fill="none" stroke="{t["line"]}" stroke-width="1.0" stroke-opacity="0.8"/>')
    # tiny red tab on each box top-centre as a subtle R/M side cue (not a fill)
    o.append(f'<rect x="{rx0+rw/2-mm(3):.1f}" y="{ry-mm(0.6):.1f}" width="{mm(6):.1f}" height="{mm(1.2):.1f}" fill="{t["red"]}" opacity="0.7"/>')
    o.append(f'<rect x="{mm(31)+rw/2-mm(3):.1f}" y="{ry-mm(0.6):.1f}" width="{mm(6):.1f}" height="{mm(1.2):.1f}" fill="{t["redsoft"]}" opacity="0.7"/>')
    # header motif: fanning raffle tickets — pivot just under the branding band
    o += ticket_fan(t, cx_mm=W/2/S, pivot_mm=13.0)

    # control wells with KIT ID markers, driven by the SAME source of truth as
    # the module (panel_src/layouts/raffles.json). Each control's id becomes the
    # SVG marker id "<kind>_<CONTROL_ID>" so the widget binds by name via the kit.
    import json, os
    _here = os.path.dirname(os.path.abspath(__file__))
    controls = json.load(open(os.path.join(_here, "layouts", "raffles.json")))["controls"]
    o.append('<g id="components">')
    for c in controls:
        cid = f'{c["kind"]}_{c["id"]}'           # e.g. param_RAFFLES_SLEW_R_ATT
        if c["kind"] == "param":
            o.append(trim_well(t, c["x_mm"], c["y_mm"], cid=cid))
        else:
            o.append(jack_well(t, c["x_mm"], c["y_mm"], cid=cid))
    # dot.modular connect mark anchor (footer-centre; reposition here).
    o.append(f'<circle id="light_connect" cx="{mm(W/S/2.0):.1f}" cy="{mm(124.0):.1f}" r="0.5" fill="#000000" fill-opacity="0"/>')
    o.append('</g>')
    o.append('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for theme in ("dark","light"):
        out = f"res/panels/Raffles_panel_{theme}.svg"
        open(out,"w").write(panel(theme))
        print(f"wrote {out}")
