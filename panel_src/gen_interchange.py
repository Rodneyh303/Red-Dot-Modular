#!/usr/bin/env python3
"""Interchange (Tonal CV expander) panel generator — dot.modular.

FROM-SCRATCH parametric generator: constructs the peranakan hex-lattice panel entirely
from layout rules (no source SVG dependency), reverse-engineered from the hand-authored
"gemini" master panel. Produces res/panels/interchange_gemini_new2.svg + _light.svg,
nanosvg-safe, with the #components SvgPanelKit marker layer. Only Monsoon then remains
hand-authored.

PANEL 270x380 px (px-native, matches the widget's hard-coded coordinates).
GRID: cols x=48,102,168,222 ; rows y=80..280 step40 (6 semitone) + 320 (octave).
  per row L->R: CV-in, atten, atten, CV-in. left pair=semitone i / oct-lo, right=i+6 / oct-hi.

Layers (parametric rules reverse-engineered from the master's exact geometry):
  rails, faint per-cell hex tint (gold/teal flat — stands in for the master's
  radialGradient glows nanosvg can't draw), outer/mid/inner concentric flat-top hexes
  (half-widths 18/16/12), spokes (inner->outer vertices), gold vertex dots, horizontal
  connectors (3 ticks + diamond), centre nested diamonds, vertical connectors, teal edge
  chain (diamond+dot+drop-line per row down both rails), header bar+rule, control wells,
  and the invisible #components bind markers.

nanosvg-safe: solid strokes/fills only (no gradients/masks/patterns). The master used a
grain pattern bg + radialGradient glows; nanosvg can't render those, so the in-Rack look
IS this flat version.

Run from repo root:  python3 panel_src/gen_interchange.py
"""
import os

W, H = 270, 380
COLS = [48, 102, 168, 222]
ROWS = [80, 120, 160, 200, 240, 280, 320]
RAIL_X = [18, 252]

THEMES = {
    "dark":  dict(out="interchange_gemini_new2.svg",  bg="#141416", header="#141416",
                  outer="#6a6a6a", mid="#555555", inner="#888888", spoke="#999999",
                  hconn="#666666", vconn="#777777", rail="#555555",
                  gold="#d4af37", teal="#26a69a", wellGrey="#888888"),
    "light": dict(out="interchange_gemini_light.svg", bg="#f7f7f7", header="#f7f7f7",
                  outer="#b0b0b0", mid="#bdbdbd", inner="#999999", spoke="#aaaaaa",
                  hconn="#b5b5b5", vconn="#b0b0b0", rail="#cccccc",
                  gold="#c8960c", teal="#1f8f84", wellGrey="#999999"),
}

def hexpts(cx, cy, hw, top, sh):
    return [(cx, cy-top), (cx+hw, cy-sh), (cx+hw, cy+sh),
            (cx, cy+top), (cx-hw, cy+sh), (cx-hw, cy-sh)]

def poly(pts, **attr):
    p = " ".join(f"{x:g},{y:g}" for x, y in pts)
    a = "".join(f' {k.replace("_","-")}="{v}"' for k, v in attr.items())
    return f'<polygon points="{p}"{a} />'

def diamond(cx, cy, rx, ry, **attr):
    return poly([(cx-rx, cy), (cx, cy-ry), (cx+rx, cy), (cx, cy+ry)], **attr)

def gen(t):
    A = []; add = A.append
    add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    add(f'<rect width="{W}" height="{H}" fill="{t["bg"]}" />')
    add(f'<g stroke="{t["rail"]}" stroke-width="0.6" fill="none" opacity="0.30">')
    add('  <path d="M18,0 L18,380 M252,0 L252,380" />')
    add('</g>')

    add('<g opacity="1.0">')
    for cy in ROWS:
        for ci, cx in enumerate(COLS):
            tint = t["teal"] if ci in (0, 3) else t["gold"]
            op = 0.05 if ci in (0, 3) else 0.06
            add("  " + poly(hexpts(cx, cy, 18, 16, 10), fill=tint, fill_opacity=f"{op}", stroke="none"))
    add('</g>')

    for hw, top, sh, col, sw, op in [
        (18, 16, 10, t["outer"], 0.8, 0.55),
        (16, 14,  8, t["mid"],   0.5, 0.40),
        (12, 11,  6, t["inner"], 0.5, 0.45),
    ]:
        add(f'<g stroke="{col}" stroke-width="{sw}" fill="none" opacity="{op}">')
        for cy in ROWS:
            for cx in COLS:
                add("  " + poly(hexpts(cx, cy, hw, top, sh)))
        add('</g>')

    add(f'<g stroke="{t["spoke"]}" stroke-width="0.5" fill="none" opacity="0.35">')
    for cy in ROWS:
        for cx in COLS:
            ip = hexpts(cx, cy, 12, 11, 6)
            op_ = hexpts(cx, cy, 18, 16, 10)
            d = " ".join(f"M{ix:g},{iy:g} L{ox:g},{oy:g}" for (ix, iy), (ox, oy) in zip(ip, op_))
            add(f'  <path d="{d}" />')
    add('</g>')

    add(f'<g fill="{t["gold"]}" opacity="0.55">')
    for cy in ROWS:
        for cx in COLS:
            for (vx, vy) in hexpts(cx, cy, 12, 11, 6):
                add(f'  <circle cx="{vx:g}" cy="{vy:g}" r="1.5" />')
    add('</g>')

    add(f'<g stroke="{t["hconn"]}" stroke-width="0.5" fill="none" opacity="0.45">')
    for cy in ROWS:
        for (xa, xb) in [(COLS[0]+18, COLS[1]-18), (COLS[2]+18, COLS[3]-18)]:
            mx = (xa + xb) / 2
            add(f'  <line x1="{xa:g}" y1="{cy-3}" x2="{xb:g}" y2="{cy-3}" />')
            add(f'  <line x1="{xa:g}" y1="{cy+3}" x2="{xb:g}" y2="{cy+3}" />')
            add(f'  <line x1="{xa:g}" y1="{cy}" x2="{xb:g}" y2="{cy}" stroke-width="0.3" opacity="0.5" />')
            add("  " + diamond(mx, cy, 5, 6))
    add('</g>')

    add(f'<g stroke="{t["gold"]}" stroke-width="0.6" fill="none" opacity="0.50">')
    cxm = (COLS[1] + COLS[2]) / 2
    for cy in ROWS:
        add("  " + diamond(cxm, cy, 15, 10))
        add("  " + diamond(cxm, cy, 10, 6))
        add("  " + diamond(cxm, cy, 4, 4, stroke_width="0.4"))
    add('</g>')

    add(f'<g stroke="{t["vconn"]}" stroke-width="0.5" fill="none" opacity="0.45">')
    for ri in range(len(ROWS) - 1):
        gy = (ROWS[ri] + ROWS[ri+1]) / 2
        for cx in COLS:
            add("  " + diamond(cx, gy, 5, 5))
    add('</g>')

    add(f'<g fill="none" stroke="{t["teal"]}" opacity="0.40">')
    for ri, cy in enumerate(ROWS):
        for rx in RAIL_X:
            add("  " + diamond(rx, cy, 6, 8, stroke_width="0.6"))
            add(f'  <circle cx="{rx}" cy="{cy}" r="1.8" fill="{t["teal"]}" stroke="none" />')
            if ri < len(ROWS) - 1:
                add(f'  <line x1="{rx}" y1="{cy+8}" x2="{rx}" y2="{cy+32}" stroke-width="0.4" />')
    add('</g>')

    add('<g opacity="0.7">')
    add(f'  <rect x="0" y="0" width="{W}" height="55" fill="{t["header"]}" />')
    add(f'  <rect x="0" y="53" width="{W}" height="0.8" fill="{t["gold"]}" opacity="0.5" />')
    add('</g>')

    add('<g fill="none" stroke-width="1.0">')
    add(f'  <g stroke="{t["wellGrey"]}">')
    for cy in ROWS:
        for cx in (COLS[0], COLS[3]):
            add(f'    <circle cx="{cx}" cy="{cy}" r="9.0" />')
    add('  </g>')
    add(f'  <g stroke="{t["gold"]}">')
    for cy in ROWS:
        for cx in (COLS[1], COLS[2]):
            add(f'    <circle cx="{cx}" cy="{cy}" r="9.0" />')
    add('  </g>')
    add('</g>')

    def mark(idn, cx, cy):
        return f'  <circle id="{idn}" cx="{cx:.2f}" cy="{cy:.2f}" r="0.5" fill="none" stroke="none"/>'
    add('<g id="components">')
    for i in range(6):
        cy = ROWS[i]
        add(mark(f"input_SEMI_CV_{i}",    COLS[0], cy))
        add(mark(f"param_SEMI_ATT_{i}",   COLS[1], cy))
        add(mark(f"param_SEMI_ATT_{i+6}", COLS[2], cy))
        add(mark(f"input_SEMI_CV_{i+6}",  COLS[3], cy))
    oy = ROWS[6]
    add(mark("input_OCT_LO_CV",  COLS[0], oy))
    add(mark("param_OCT_LO_ATT", COLS[1], oy))
    add(mark("param_OCT_HI_ATT", COLS[2], oy))
    add(mark("input_OCT_HI_CV",  COLS[3], oy))
    add('</g>')

    add('</svg>')
    return "\n".join(A) + "\n"

def main():
    if not os.path.isdir("res/panels"):
        raise SystemExit("run from repo root (need res/panels/)")
    for key, t in THEMES.items():
        svg = gen(t)
        if "url(" in svg:
            raise SystemExit(f"{t['out']}: url() present — nanosvg-unsafe")
        path = os.path.join("res/panels", t["out"])
        with open(path, "w") as f:
            f.write(svg)
        markers = svg.count('id="param_') + svg.count('id="input_')
        print(f"wrote {path}  ({svg.count(chr(10))+1} lines, {markers} markers)")

if __name__ == "__main__":
    main()
