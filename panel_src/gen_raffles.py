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

# 18HP (was 12HP / W=180 when there were only TWO streams). Three 26mm stream
# columns + two 2mm gaps + margins = 91.44mm = 18HP.
S = 75/25.4
def mm(v): return v*S
HP       = 18
W_MM     = HP * 5.08                      # 91.44
W, H     = round(W_MM * S), 380

# ── Stream columns: ONE source of truth for the layout ───────────────────────────
# Previously the control positions lived in panel_src/layouts/raffles.json. That was
# an earlier experiment, superseded by "the generator emits the components layer and
# the widget binds by name via SvgPanelKit" — so the table now lives here, and every
# position is DERIVED from the column origin. Adding a fourth stream would be one
# entry in STREAMS.
COL_W_MM, COL_GAP_MM = 26.0, 2.0
COL_MARGIN_MM = (W_MM - (3*COL_W_MM + 2*COL_GAP_MM)) / 2.0      # 4.72
STREAMS = [("R", "red"), ("Q", "gold"), ("M", "redsoft")]        # left, centre, right
BOX_Y_MM, BOX_H_MM = 34.0, 88.0

def col_x0(i):  return COL_MARGIN_MM + i * (COL_W_MM + COL_GAP_MM)
def col_cx(i):  return col_x0(i) + COL_W_MM / 2.0

# Per-stream controls, as offsets from the COLUMN CENTRE. The two-sided mirror the
# old 2-column layout used does not generalise to three columns, so all columns now
# share one identical arrangement — easier to learn and one rule to maintain.
ROW_SLEW_Y, ROW_MIX_Y, ROW_GATE_Y, ROW_LIVE_Y = 44.0, 56.0, 72.0, 92.0
PAIR_DX, GATE_DX = 4.5, 5.5
STREAM_CONTROLS = [                        # (kind, id-suffix template, dx, y)
    ("input", "RAFFLES_SLEW_{s}_CV",         -PAIR_DX, ROW_SLEW_Y),
    ("param", "RAFFLES_SLEW_{s}_ATT",        +PAIR_DX, ROW_SLEW_Y),
    ("input", "RAFFLES_MIX_{s}_CV",          -PAIR_DX, ROW_MIX_Y),
    ("param", "RAFFLES_MIX_{s}_ATT",         +PAIR_DX, ROW_MIX_Y),
    ("input", "RAFFLES_GATE_REDICE_{s}",     -GATE_DX, ROW_GATE_Y),
    ("input", "RAFFLES_GATE_LASTDICE_{s}",   +GATE_DX, ROW_GATE_Y),
    ("input", "RAFFLES_GATE_LIVESTATIC_{s}",      0.0, ROW_LIVE_Y),
]
# Globals (not per stream), centred on the panel.
GLOBAL_CONTROLS = [
    ("input", "RAFFLES_GATE_RESEED_ROLL",    -16.0, 116.0),
    ("input", "RAFFLES_GATE_RESEED_RESTART", +16.0, 116.0),
]

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
    ry, rw, rh = mm(BOX_Y_MM), mm(COL_W_MM), mm(BOX_H_MM)
    for i, (sname, tint) in enumerate(STREAMS):
        bx = mm(col_x0(i))
        o.append(f'<rect x="{bx:.1f}" y="{ry:.1f}" width="{rw:.1f}" height="{rh:.1f}" rx="{mm(2):.1f}" '
                 f'fill="none" stroke="{t["line"]}" stroke-width="1.0" stroke-opacity="0.8"/>')
        # tiny tab on each box top-centre as a subtle per-stream cue (not a fill)
        o.append(f'<rect x="{mm(col_cx(i))-mm(3):.1f}" y="{ry-mm(0.6):.1f}" width="{mm(6):.1f}" '
                 f'height="{mm(1.2):.1f}" fill="{t[tint]}" opacity="0.7"/>')
    # header motif: fanning raffle tickets — pivot just under the branding band
    o += ticket_fan(t, cx_mm=W/2/S, pivot_mm=13.0)

    # control wells with KIT ID markers, driven by the SAME source of truth as
    # the module (panel_src/layouts/raffles.json). Each control's id becomes the
    # SVG marker id "<kind>_<CONTROL_ID>" so the widget binds by name via the kit.
    placed = []
    for i, (sname, _tint) in enumerate(STREAMS):
        for kind, tmpl, dx, y in STREAM_CONTROLS:
            placed.append((kind, tmpl.format(s=sname), col_cx(i) + dx, y))
    for kind, cid, dx, y in GLOBAL_CONTROLS:
        placed.append((kind, cid, W_MM / 2.0 + dx, y))

    o.append('<g id="components">')
    for kind, cid, x_mm, y_mm in placed:
        marker = f'{kind}_{cid}'            # e.g. param_RAFFLES_SLEW_R_ATT — the kit binds by name
        if kind == "param":
            o.append(trim_well(t, x_mm, y_mm, cid=marker))
        else:
            o.append(jack_well(t, x_mm, y_mm, cid=marker))
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
