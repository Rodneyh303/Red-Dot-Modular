#!/usr/bin/env python3
"""Junction panel (renamed from Surge) — Bugis pinisi schooner motif.

Hero graphic: a stylized red pinisi (two-masted Bugis/Makassar schooner) with a
fan of sails and a triangular bowsprit, sailing on red wave lines — honouring the
Buginese maritime heritage referenced at Bugis (the area's name traces to the
seafaring Bugis people). Black panel, gold knob trim, red detailing, white text.

Geometry matches the in-use Surge panel: 120x380 px (mm coords at 75 DPI).
Controls live at y>=60mm; the pinisi occupies the header (y ~ 8..50mm).
Screws drawn in C++ (RedScrew) — not painted here. nanosvg-safe: solid fills,
no gradients/masks/text/url.
"""
import math

W, H = 120, 380            # px
S = 75 / 25.4              # px per mm
def mm(v): return v * S

THEMES = {
    "dark":  dict(bg="#18181a", red="#d4001a", redsoft="#dc2626", gold="#c8960c",
                  hull="#d4001a", sail="#dc2626", wave="#8a1414", line="#2a2a2a",
                  well="#0f1114", wellring="#3a3a3a", text="#f0f0f0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", redsoft="#c0001a", gold="#b07d00",
                  hull="#d4001a", sail="#c0001a", wave="#c89090", line="#bcbcbc",
                  well="#e8e2d6", wellring="#c0b8a8", text="#1a1a1a"),
}


def pinisi(t, cx_mm, top_mm):
    """Stylized two-masted pinisi with a fan of sails, centred at cx, hull near
    bottom of the header band. All coords in mm, converted at emit."""
    o = ['<g>']
    cx = mm(cx_mm)
    hullY = mm(top_mm + 28)        # waterline (compact so it clears the controls)
    # ── hull: a long shallow curved crescent (dhow-like) ──
    hw = mm(17)                    # half hull width
    o.append(f'<path d="M {cx-hw:.1f} {hullY:.1f} '
             f'Q {cx:.1f} {hullY+mm(7):.1f} {cx+hw:.1f} {hullY:.1f} '
             f'L {cx+hw-mm(3):.1f} {hullY-mm(2.2):.1f} '
             f'L {cx-hw+mm(3):.1f} {hullY-mm(2.2):.1f} Z" '
             f'fill="{t["hull"]}"/>')
    # ── two masts (front/right taller per pinisi; tripod hint) ──
    mastFootR = cx + mm(4)
    mastFootL = cx - mm(7)
    mastTopR  = top_mm + 2
    mastTopL  = top_mm + 6
    o.append(f'<line x1="{mastFootR:.1f}" y1="{hullY-mm(2):.1f}" x2="{cx+mm(4):.1f}" y2="{mm(mastTopR):.1f}" stroke="{t["line"]}" stroke-width="1.4"/>')
    o.append(f'<line x1="{mastFootL:.1f}" y1="{hullY-mm(2):.1f}" x2="{cx-mm(7):.1f}" y2="{mm(mastTopL):.1f}" stroke="{t["line"]}" stroke-width="1.2"/>')
    # bowsprit (triangular spar forward of the bow, lower-left)
    o.append(f'<line x1="{cx-hw:.1f}" y1="{hullY-mm(1):.1f}" x2="{cx-hw-mm(7):.1f}" y2="{hullY-mm(5):.1f}" stroke="{t["line"]}" stroke-width="1.2"/>')

    # ── fan of sails: big main + smaller fore + jibs (≈7 sails total) ──
    def sail(x1,y1, x2,y2, x3,y3, fill, op=1.0):
        o.append(f'<path d="M {x1:.1f} {y1:.1f} L {x2:.1f} {y2:.1f} L {x3:.1f} {y3:.1f} Z" fill="{fill}" fill-opacity="{op}"/>')
    # main sail (tall, on right mast) — triangle filling the mast
    sail(cx+mm(4), mm(mastTopR), cx+mm(4), hullY-mm(3), cx+mm(15), hullY-mm(3), t["sail"], 0.95)
    # fore sail (on left mast)
    sail(cx-mm(7), mm(mastTopL), cx-mm(7), hullY-mm(3), cx+mm(2), hullY-mm(3), t["sail"], 0.9)
    # topsail (small, above main)
    sail(cx+mm(4), mm(mastTopR), cx+mm(4), mm(top_mm+12), cx+mm(11), mm(top_mm+12), t["redsoft"], 0.8)
    # jibs (three small forward triangles off the bowsprit)
    bx = cx-hw-mm(7)
    for k,(sx,sy) in enumerate([(mm(6),mm(10)),(mm(9),mm(14)),(mm(12),mm(18))]):
        sail(cx-mm(7), mm(mastTopL), bx, hullY-mm(5), bx+sx, hullY-mm(5)+0, t["sail"], 0.55+0.12*k)
    o.append('</g>')
    return o


def waves(t, y0_mm, n=3):
    o = ['<g>']
    for i in range(n):
        yy = mm(y0_mm + i*3.0)
        # a row of shallow sine humps across the panel
        d = f'M 0 {yy:.1f}'
        x = 0
        while x < W:
            d += f' q {mm(3):.1f} {-mm(1.6):.1f} {mm(6):.1f} 0 q {mm(3):.1f} {mm(1.6):.1f} {mm(6):.1f} 0'
            x += mm(12)
        o.append(f'<path d="{d}" fill="none" stroke="{t["wave"]}" stroke-width="1.0" stroke-opacity="{0.7-0.15*i}"/>')
    o.append('</g>')
    return o


def jack_well(t, x_mm, y_mm, cid=None):
    x,y = mm(x_mm), mm(y_mm)
    idattr = f' id="{cid}"' if cid else ''
    return (f'<circle{idattr} cx="{x:.1f}" cy="{y:.1f}" r="{mm(3.6):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="1.2"/>'
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{mm(2.0):.1f}" fill="none" stroke="{t["red"]}" stroke-width="0.7" opacity="0.5"/>')


def trim_well(t, x_mm, y_mm, cid=None):
    x,y = mm(x_mm), mm(y_mm)
    idattr = f' id="{cid}"' if cid else ''
    return (f'<circle{idattr} cx="{x:.1f}" cy="{y:.1f}" r="{mm(3.2):.1f}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="1.4"/>'
            f'<line x1="{x:.1f}" y1="{y-mm(1):.1f}" x2="{x:.1f}" y2="{y-mm(2.6):.1f}" stroke="{t["red"]}" stroke-width="1.3" stroke-linecap="round"/>')


def panel(theme):
    t = THEMES[theme]
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">']
    o.append(f'<rect width="{W}" height="{H}" fill="{t["bg"]}"/>')
    o.append(f'<rect x="0" y="0" width="{W}" height="3" fill="{t["red"]}"/>')
    o.append(f'<rect x="0" y="{H-3}" width="{W}" height="3" fill="{t["red"]}"/>')
    # ── branding band (logo mark left + module-name space) at the very top ──
    #    The module name "JUNCTION" is drawn by the widget at runtime (nvgText);
    #    here we reserve the band + a subtle underline. Logo mark = red dot + DM hint.
    o.append(f'<circle cx="{mm(6):.1f}" cy="{mm(10):.1f}" r="{mm(2.4):.1f}" fill="{t["red"]}"/>')
    o.append(f'<line x1="{mm(2):.1f}" y1="{mm(17):.1f}" x2="{mm(W/S-2):.1f}" y2="{mm(17):.1f}" stroke="{t["line"]}" stroke-width="0.8" stroke-opacity="0.6"/>')
    # header motif: pinisi over waves — below branding band, sized to clear y=60 controls
    o += waves(t, 47.0)
    o += pinisi(t, cx_mm=W/2/S, top_mm=20.0)
    # control wells with KIT ID markers, matching the Surge module's controls
    # (same geometry). Row order = NOTEVAL, VARIATION, LEGATO, REST, ACCENT;
    # per row: input=<NAME>_CV jack, param=<NAME>_ATT trim.
    ROWS=[60.,74.,88.,102.,116.]; JACK_X=14.; ATT_X=30.
    NAMES=["NOTEVAL","VARIATION","LEGATO","REST","ACCENT"]
    o.append('<g id="components">')
    for i, y in enumerate(ROWS):
        o.append(jack_well(t, JACK_X, y, cid=f"input_SURGE_{NAMES[i]}_CV"))
        o.append(trim_well(t, ATT_X, y,  cid=f"param_SURGE_{NAMES[i]}_ATT"))
    # dot.modular connect light anchor (brand red dot). Position via this marker;
    # the RedDotLight widget binds to it by id. Footer-centre by default.
    # dot.modular connect mark anchor. fill-opacity 0 (not fill=none) so nanosvg
    # reliably keeps this no-paint shape in the parsed list for kit findNamed.
    o.append(f'<circle id="light_connect" cx="{mm(20.0):.1f}" cy="{mm(124.0):.1f}" r="0.5" fill="#000000" fill-opacity="0"/>')
    o.append('</g>')
    o.append('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for theme in ("dark","light"):
        out = f"res/panels/Junction_panel_{theme}.svg"
        open(out,"w").write(panel(theme))
        print(f"wrote {out}")
