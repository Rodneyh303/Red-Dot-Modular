#!/usr/bin/env python3
"""Lantern panel — note-output visualiser (read-only observer).

12HP (180x380px @ 75 DPI). dot.modular dark/light, nanosvg-safe (solid fills,
no gradients/masks/filters/text-for-labels). Layout:
  - header (y 6..13mm): title band + connect dot
  - display recess (y ~14..92mm): the 16-lane bar view (widget draws contents)
  - controls (y ~96..120mm): VIEW (Notes/Velocity/Prob) + ZOOM knob + FOLLOW
  - footer: dot.modular wordmark

The C++ widget (LanternDisplay) draws all lane content, labels, bars, and the
phase line — the panel only provides the recess + control wells + branding.
Screws via C++ RedScrew. Hero motif: a lantern glow (concentric arcs) in the
header, echoing "lights up what's happening".

nanosvg rules: every shape carries its own paint; no <text> for control labels.
"""
import math, os

W, H = 180, 380
S = 75 / 25.4
def mm(v): return v * S

THEMES = {
    "dark":  dict(bg="#16181c", ink="#d8d8dc", dim="#8a8f98", red="#d4001a",
                  redsoft="#dc2626", gold="#c8960c", line="#3a3a3e",
                  recess="#101216", recessring="#2a2f37",
                  well="#0f1114", wellring="#3a4048", text="#f0f0f0"),
    "light": dict(bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", red="#d4001a",
                  redsoft="#c0001a", gold="#a07808", line="#c8ccd2",
                  recess="#d8dade", recessring="#c0c4ca",
                  well="#d4d6d9", wellring="#a8aeb6", text="#1a1a1a"),
}


def lantern_glow(t, cx_mm, cy_mm):
    """Header motif: concentric arcs suggesting a lantern's light — a small
    red core with widening dim rings. nanosvg-safe (stroked circles)."""
    o = ['<g>']
    cx, cy = mm(cx_mm), mm(cy_mm)
    o.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{mm(1.4):.1f}" fill="{t["red"]}" fill-opacity="0.9"/>')
    for i, r in enumerate((3.0, 4.6, 6.2)):
        op = 0.5 - i * 0.14
        o.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{mm(r):.1f}" fill="none" '
                 f'stroke="{t["red"]}" stroke-width="{mm(0.4):.1f}" stroke-opacity="{op:.2f}"/>')
    o.append('</g>')
    return "\n".join(o)


def recess(t, x_mm, y_mm, w_mm, h_mm, cid=None):
    """The display recess the C++ widget renders into. Optional marker id so the
    widget can bind its box by name via SvgPanelKit."""
    x, y, w, h = mm(x_mm), mm(y_mm), mm(w_mm), mm(h_mm)
    idattr = f' id="{cid}"' if cid else ""
    return (f'<rect{idattr} x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" '
            f'rx="{mm(1.5):.1f}" fill="{t["recess"]}" stroke="{t["recessring"]}" '
            f'stroke-width="{mm(0.4):.1f}"/>')


def trim_well(t, x_mm, y_mm, cid, r_mm=5.0):
    x, y = mm(x_mm), mm(y_mm)
    return (f'<circle id="{cid}" cx="{x:.1f}" cy="{y:.1f}" r="{mm(r_mm):.1f}" '
            f'fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="{mm(0.4):.1f}"/>')


def button_well(t, x_mm, y_mm, w_mm, h_mm, cid):
    x, y, w, h = mm(x_mm), mm(y_mm), mm(w_mm), mm(h_mm)
    return (f'<rect id="{cid}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" '
            f'rx="{mm(1.0):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" '
            f'stroke-width="{mm(0.4):.1f}"/>')


def panel(theme):
    t = THEMES[theme]
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W*S:.1f}" height="{H*S:.1f}" '
         f'viewBox="0 0 {W*S:.1f} {H*S:.1f}">']
    # background
    o.append(f'<rect x="0" y="0" width="{W*S:.1f}" height="{H*S:.1f}" fill="{t["bg"]}"/>')
    # header branding band
    o.append(f'<rect x="0" y="0" width="{W*S:.1f}" height="{mm(12):.1f}" fill="{t["red"]}" opacity="0.06"/>')
    o.append(f'<line x1="0" y1="{mm(12):.1f}" x2="{W*S:.1f}" y2="{mm(12):.1f}" '
             f'stroke="{t["red"]}" stroke-width="{mm(0.5):.1f}" opacity="0.5"/>')
    o.append(lantern_glow(t, cx_mm=W/2, cy_mm=6.5))

    # display recess — the 16-lane bar view (widget draws contents)
    o.append('<g id="components">')
    o.append(recess(t, 6, 14, W - 12, 78, cid="display_lantern"))

    # controls: VIEW buttons (Notes/Velocity/Prob), ZOOM knob, FOLLOW button
    o.append(button_well(t, 14, 98,  22, 7, cid="param_VIEW_NOTES"))
    o.append(button_well(t, 14, 107, 22, 7, cid="param_VIEW_VELOCITY"))
    o.append(button_well(t, 14, 116, 22, 7, cid="param_VIEW_PROB"))
    o.append(trim_well(t, W/2, 108, cid="param_ZOOM", r_mm=7.0))
    o.append(button_well(t, W-36, 104, 22, 10, cid="param_FOLLOW"))

    # dot.modular connect mark anchor (footer)
    o.append(f'<circle id="light_connect" cx="{mm(W/2):.1f}" cy="{mm(124):.1f}" '
             f'r="0.5" fill="#000000" fill-opacity="0"/>')
    o.append('</g>')
    o.append('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for theme in ("dark", "light"):
        out = os.path.join(root, f"res/panels/Lantern_panel_{theme}.svg")
        open(out, "w").write(panel(theme))
        print(f"wrote {out}")
