#!/usr/bin/env python3
"""Monsoon (Supertree) panel generator — dot.modular.

FROM-SCRATCH parametric generation of the Monsoon panel ART, reverse-engineered from the
hand-authored super-tree panel (res/panels/Monsoon_panel_{dark,light}_monsoon.svg) and
IMPROVED for the in-Rack (nanosvg) look.

WHY IMPROVED, not just reproduced: the original draws the super-tree canopies with a
`treeGrad` <linearGradient> and the moon glow with a radial gradient — nanosvg can't render
either, so in Rack the trees collapse to flat grey blobs. This generator rebuilds the trees
as nanosvg-safe flat silhouettes (layered flat fills that fade by stacking, plus the
radiating branch armature) so they actually read as Gardens-by-the-Bay supertrees in Rack,
and keeps cloud/rain SUBTLE — only a few wisps, low opacity, in a couple of places.

WHAT IS GENERATED (the art, in the 768x485.67 internal frame, scaled 0.78125 → 600x379.43):
  • flat dark background + faint grain-substitute
  • faint structural grid (the "rain field" — very low opacity)
  • SUPER-TREES: a varied grove of 6, each = tapered trunk + canopy disc + radiating
    branch armature + flat canopy silhouette (nanosvg-safe, no gradient)
  • canopy moon-discs (the upper ring motifs) + the gold moon with a flat glow ring
  • SUBTLE cloud/rain: a few low-opacity cloud arcs + sparse rain streaks, not a wall
WHAT IS PRESERVED VERBATIM (functional, hand-tuned — embedded from a saved fragment):
  • the dot.modular Barlow wordmark with the Lissajous "o" glyph
  • the control-bank boxes + indicator strip
  • the #components SvgPanelKit marker layer (43 bindings)

Run from repo root:  python3 panel_src/gen_monsoon.py
"""
import os, math, random

W, H = 600.0, 379.4291            # panel size (px)
IW, IH = 767.99, 485.67           # internal art frame (scaled by S below)
S = 0.78125                       # IW*S = W

FRAG = "panel_src/monsoon_fragments"

THEMES = {
    "dark": dict(
        out="Monsoon_panel_dark_monsoon.svg", tail="function_tail_dark.svgfrag",
        bg="#0c0e11", grain="#1e2228", grid="#1a2535",
        trunk="#26303d", canopy="#2c3a4a", canopyEdge="#3a4a5e",
        branch="#dc2626", moon="#c8960c", moonGlow="#1f6f5f",
        cloud="#2a3340", rain="#2f3a48"),
    "light": dict(
        out="Monsoon_panel_light_monsoon.svg", tail="function_tail_light.svgfrag",
        bg="#eceef1", grain="#dfe3e8", grid="#c7d0db",
        trunk="#c2ccd8", canopy="#cdd6e0", canopyEdge="#b4c0cd",
        branch="#b5341f", moon="#a07808", moonGlow="#7fb8ad",
        cloud="#c4ccd6", rain="#b8c0cc"),
}

# Super-tree grove — (canopy cx, canopy cy, canopy radius). Lifted so canopies read in the
# band between the disc row and the control banks (panel y ~230-290); trunks descend behind
# the banks to the floor. Varied heights/sizes for a natural grove.
TREES = [
    (68.03, 320.0, 30.0),
    (166.30, 345.0, 21.0),
    (287.24, 300.0, 36.0),
    (444.0, 332.0, 25.0),
    (574.48, 312.0, 31.0),
    (702.99, 340.0, 23.0),
]
# Upper canopy moon-discs (the ring motifs across the top band)
DISCS = [60.47, 158.74, 257.01, 355.27, 453.54]   # cx; cy=83.15
DISC_CY, DISC_R1, DISC_R2 = 83.15, 41.57, 28.35
MOON = (612.28, 113.39, 75.59)                      # the gold moon (cx, cy, r)

def supertree(t, cx, cyTop, r):
    """One supertree, nanosvg-safe: tapered trunk + flat canopy disc (stacked flats for a
    soft edge, no gradient) + radiating branch armature fanning from the canopy underside."""
    o = []
    floor = IH
    halfBase = r * 0.72                 # trunk half-width at floor
    halfTop  = r * 0.18                 # trunk half-width at canopy
    # tapered trunk (flat fill)
    o.append(f'<polygon points="{cx-halfBase:.1f},{floor:.1f} {cx+halfBase:.1f},{floor:.1f} '
             f'{cx+halfTop:.1f},{cyTop:.1f} {cx-halfTop:.1f},{cyTop:.1f}" '
             f'fill="{t["trunk"]}" stroke="none"/>')
    # canopy: 3 stacked flat ellipses (largest faint, smallest solid) → soft edge w/o gradient
    for k, (rr, op) in enumerate([(1.12, 0.30), (1.0, 0.55), (0.84, 0.85)]):
        o.append(f'<ellipse cx="{cx:.1f}" cy="{cyTop:.1f}" rx="{r*rr:.1f}" ry="{r*rr*0.28:.1f}" '
                 f'fill="{t["canopy"]}" fill-opacity="{op:.2f}" stroke="none"/>')
    o.append(f'<ellipse cx="{cx:.1f}" cy="{cyTop:.1f}" rx="{r:.1f}" ry="{r*0.28:.1f}" '
             f'fill="none" stroke="{t["canopyEdge"]}" stroke-width="0.6" opacity="0.5"/>')
    # radiating branch armature — short red strokes fanning up/out from the canopy rim
    nb = 9
    o.append(f'<g stroke="{t["branch"]}" stroke-width="0.45" fill="none" opacity="0.40">')
    for i in range(nb):
        ang = math.pi * (0.15 + 0.70 * i / (nb - 1))     # fan across the top
        x0 = cx + math.cos(ang) * r * 0.9
        y0 = cyTop - abs(math.sin(ang)) * r * 0.20
        x1 = cx + math.cos(ang) * r * 1.28
        y1 = cyTop - abs(math.sin(ang)) * r * 0.62
        o.append(f'  <line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}"/>')
        # mirror to the other side
        x0m = cx - math.cos(ang) * r * 0.9
        x1m = cx - math.cos(ang) * r * 1.28
        o.append(f'  <line x1="{x0m:.1f}" y1="{y0:.1f}" x2="{x1m:.1f}" y2="{y1:.1f}"/>')
    o.append('</g>')
    return o

def clouds_and_rain(t):
    """SUBTLE: a few cloud wisps + sparse rain, low opacity, only in a couple of places."""
    o = []
    rng = random.Random(7)            # deterministic
    # 3 cloud wisps (flat layered arcs), upper-left and upper-mid only — not a blanket
    o.append(f'<g fill="{t["cloud"]}" stroke="none" opacity="0.16">')
    for (cx, cy, w) in [(120, 70, 70), (300, 55, 90), (660, 60, 60)]:
        # a wisp = a few overlapping flat ellipses
        for dx, dy, rx, ry in [(-w*0.3, 4, w*0.45, 9), (0, 0, w*0.55, 12), (w*0.32, 5, w*0.4, 8)]:
            o.append(f'  <ellipse cx="{cx+dx:.0f}" cy="{cy+dy:.0f}" rx="{rx:.0f}" ry="{ry:.0f}"/>')
    o.append('</g>')
    # sparse rain — a few short streaks under two of the clouds only
    o.append(f'<g stroke="{t["rain"]}" stroke-width="0.5" opacity="0.18">')
    for cx0, cy0, n in [(120, 84, 5), (300, 70, 7)]:
        for _ in range(n):
            x = cx0 + rng.uniform(-55, 55)
            y = cy0 + rng.uniform(0, 14)
            o.append(f'  <line x1="{x:.0f}" y1="{y:.0f}" x2="{x-2:.0f}" y2="{y+11:.0f}"/>')
    o.append('</g>')
    return o

def build_art(t):
    o = [f'<g transform="scale({S})">']
    # background
    o.append(f'<rect width="{IW:.2f}" height="{IH:.2f}" fill="{t["bg"]}"/>')
    # faint grain substitute (a couple of low-opacity scan rects, flat)
    o.append(f'<g fill="{t["grain"]}" opacity="0.25">')
    for y in range(0, int(IH), 6):
        o.append(f'  <rect x="0" y="{y}" width="{IW:.0f}" height="0.5"/>')
    o.append('</g>')
    # faint structural grid (the rain field — very subtle)
    o.append(f'<g stroke="{t["grid"]}" stroke-width="0.5" opacity="0.30">')
    for x in range(0, int(IW) + 1, 49):
        o.append(f'  <line x1="{x}" y1="0" x2="{x}" y2="{IH:.0f}"/>')
    for y in range(0, int(IH) + 1, 49):
        o.append(f'  <line x1="0" y1="{y}" x2="{IW:.0f}" y2="{y}"/>')
    o.append('</g>')
    # subtle cloud/rain (behind trees)
    o += clouds_and_rain(t)
    # upper canopy moon-discs (ring motifs)
    o.append(f'<g fill="none" stroke="{t["branch"]}" stroke-width="0.6" opacity="0.45">')
    for cx in DISCS:
        o.append(f'  <circle cx="{cx:.2f}" cy="{DISC_CY}" r="{DISC_R1}"/>')
        o.append(f'  <circle cx="{cx:.2f}" cy="{DISC_CY}" r="{DISC_R2}" stroke-width="0.4" opacity="0.6"/>')
        o.append(f'  <circle cx="{cx:.2f}" cy="158.74" r="1.8" fill="{t["branch"]}" stroke="none"/>')
    o.append('</g>')
    # the gold moon — flat concentric rings stand in for the radial glow
    mx, my, mr = MOON
    o.append(f'<g fill="none" stroke="{t["moon"]}" opacity="0.70">')
    o.append(f'  <circle cx="{mx}" cy="{my}" r="{mr}" stroke-width="0.9"/>')
    o.append(f'  <circle cx="{mx}" cy="{my}" r="{mr*1.2:.1f}" stroke-width="0.4" opacity="0.5"/>')
    o.append(f'  <circle cx="{mx}" cy="{my}" r="{mr*0.6:.1f}" stroke="{t["moonGlow"]}" stroke-width="0.5" opacity="0.5"/>')
    o.append('</g>')
    # the super-tree grove
    for (cx, cy, r) in TREES:
        o += supertree(t, cx, cy, r)
    o.append('</g>')   # close scale group
    return "\n".join(o)

def main():
    if not os.path.isdir("res/panels"):
        raise SystemExit("run from repo root")
    for key, t in THEMES.items():
        tailpath = os.path.join(FRAG, t["tail"])
        with open(tailpath) as f:
            tail = f.read().rstrip("\n")
        svg = []
        svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" '
                   f'xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
                   f'width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
        svg.append('<g>')               # outer content wrapper — the verbatim tail's final
                                        # </g> closes this (in the original it wrapped the
                                        # whole body; the art's own scale group is separate)
        svg.append(build_art(t))
        svg.append('  <circle cx="752.88" cy="15.12" r="4" fill="#dc2626"/>')  # corner status dot
        svg.append(tail)                # verbatim wordmark + control boxes + components
        if not tail.rstrip().endswith("</svg>"):
            svg.append("</svg>")
        out = "\n".join(svg)
        if "url(" in build_art(t):
            raise SystemExit(f"{t['out']}: art has url() — nanosvg-unsafe")
        path = os.path.join("res/panels", t["out"])
        with open(path, "w") as f:
            f.write(out)
        markers = out.count('id="param_') + out.count('id="input_') + out.count('id="output_')
        print(f"wrote {path}  ({out.count(chr(10))+1} lines, {markers} component markers preserved)")

if __name__ == "__main__":
    main()
