#!/usr/bin/env python3
"""Overlay an SvgPanelKit #components marker layer onto the EXISTING hand-authored
Interchange panels, without touching their artwork.

This is the "option A" alternative to gen_interchange.py: instead of regenerating
the panel (and changing its look), we keep res/panels/interchange_gemini_*.svg
exactly as drawn and only inject invisible bind anchors so SvgPanelKit's findNamed
can resolve them. Idempotent — re-running replaces any prior injected layer.

Marker convention matches the working kit-composed panels (Junction/Raffles/…): a
VISIBLE <g id="components"> with r=0.5, fill=none, stroke=none circles. (We avoid
the display:none style some embed_ scripts use, because nanosvg may drop
display:none shapes from the parsed shape list, which would make findNamed fail.)

Positions are the widget's ground-truth px coordinates
(MonsoonInterchangeExpander): the panel is px-native (270x380), so markers are in
px directly. Run from repo root:  python3 panel_src/embed_interchange_markers.py
"""
import re

PANELS = [
    "res/panels/interchange_gemini_new2.svg",   # dark
    "res/panels/interchange_gemini_light.svg",  # light
]

# Control layout (px) — must match the widget + gen_interchange.py.
COL_X = [48.0, 102.0, 168.0, 222.0]   # CV-in L, atten L, atten R, CV-in R
SEMI_ROWS_Y = [80.0 + i * 40.0 for i in range(6)]
OCT_Y = 320.0
W, H = 270.0, 380.0


def markers():
    m = []
    def add(kind, label, x, y):
        m.append(f'    <circle id="{kind}_{label}" cx="{x:.2f}" cy="{y:.2f}" '
                 f'r="0.5" fill="none" stroke="none"/>')
    for i in range(6):
        y = SEMI_ROWS_Y[i]
        add("input", f"SEMI_CV_{i}",    COL_X[0], y)   # semitone i
        add("param", f"SEMI_ATT_{i}",   COL_X[1], y)
        add("param", f"SEMI_ATT_{i+6}", COL_X[2], y)   # semitone i+6
        add("input", f"SEMI_CV_{i+6}",  COL_X[3], y)
    add("input", "OCT_LO_CV",  COL_X[0], OCT_Y)
    add("param", "OCT_LO_ATT", COL_X[1], OCT_Y)
    add("param", "OCT_HI_ATT", COL_X[2], OCT_Y)
    add("input", "OCT_HI_CV",  COL_X[3], OCT_Y)
    add("light", "connect", W * 0.5, H - 20.0)         # dot.modular connect mark
    return "\n".join(m)


def inject(path):
    with open(path) as f:
        svg = f.read()
    # Remove any previously-injected components layer (idempotent).
    svg = re.sub(r'\s*<g id="components">.*?</g>\s*(?=</svg>)', '\n', svg, flags=re.DOTALL)
    layer = '  <g id="components">\n' + markers() + '\n  </g>\n'
    svg = svg.replace('</svg>', layer + '</svg>')
    with open(path, "w") as f:
        f.write(svg)
    n = svg.count('id="input_') + svg.count('id="param_') + svg.count('id="light_')
    return n


if __name__ == "__main__":
    for p in PANELS:
        n = inject(p)
        print(f"injected {n} markers into {p}")
