"""Inject a VISIBLE 'cluster-art' layer into the rich Monsoon panels.

This adds the static panel furniture for the dice/slew/mix control cluster and
the output group — recess rectangles, control wells/seats, and the output-group
recess — WITHOUT touching the hand-authored 568-element artwork. Like
embed_components.emit(), it strips any prior 'cluster-art' layer first, so it is
idempotent and safe to re-run.

nanosvg-safe: solid per-shape fill/stroke, no gradients/masks, no <text>
(control labels stay as widget nvgText — nanosvg ignores <text>).

Run AFTER embed_monsoon.py (order doesn't matter since both manage their own
layer, but conventionally: gen base -> embed_monsoon -> embed_cluster_art).
"""
import re

S = 75.0 / 25.4
def px(mm): return round(mm * S, 2)

# Cluster geometry — MUST match MonsoonWidget.cpp (CXR=13, CXM=27; rows
# SLEW=79, ROLL/main=86, TRIAL=93, MIX=100) and the output group.
THEMES = {
    "dark":  dict(recess="#101216", line="#2a2f37", well="#0f1114",
                  red="#dc2626", gold="#c8960c", teal="#26a69a",
                  outaccent="#c8ccd4", outline="#2a2f37"),
    "light": dict(recess="#d8dade", line="#c0c4ca", well="#d4d6d9",
                  red="#c0202a", gold="#a07808", teal="#1a8276",
                  outaccent="#202228", outline="#c0c4ca"),
}

def cluster_layer(t):
    o = []
    a = o.append
    # ── Dice/Slew/Mix cluster recess ──
    a(f'<rect x="{px(6):.1f}" y="{px(74):.1f}" width="{px(30):.1f}" height="{px(30):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["recess"]}" stroke="{t["line"]}" stroke-width="1"/>')
    for cx in (13.0, 27.0):
        # SLEW trim well
        a(f'<circle cx="{px(cx):.1f}" cy="{px(79):.1f}" r="{px(3.4):.1f}" fill="{t["well"]}" stroke="{t["line"]}" stroke-width="1"/>')
        # MAIN dice seat (red)
        a(f'<rect x="{px(cx-3):.1f}" y="{px(86-3):.1f}" width="{px(6):.1f}" height="{px(6):.1f}" rx="{px(1.2):.1f}" fill="{t["well"]}" stroke="{t["red"]}" stroke-width="0.9"/>')
        # TRIAL dice seat (gold)
        a(f'<rect x="{px(cx-3):.1f}" y="{px(93-3):.1f}" width="{px(6):.1f}" height="{px(6):.1f}" rx="{px(1.2):.1f}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="0.9"/>')
        # MIX knob well (teal)
        a(f'<circle cx="{px(cx):.1f}" cy="{px(100):.1f}" r="{px(3.6):.1f}" fill="{t["well"]}" stroke="{t["teal"]}" stroke-width="1"/>')
    # ── Output group recess (in/out accent), behind the output jacks ──
    # Outputs occupy x 114..182, y 105 & 120; frame with a margin.
    a(f'<rect x="{px(106):.1f}" y="{px(97):.1f}" width="{px(88):.1f}" height="{px(31):.1f}" '
      f'rx="{px(2.5):.1f}" fill="{t["outaccent"]}" fill-opacity="0.10" '
      f'stroke="{t["outline"]}" stroke-width="1" stroke-opacity="0.6"/>')
    return ("<g inkscape:label=\"cluster-art\" inkscape:groupmode=\"layer\" id=\"cluster-art\">\n"
            + "\n".join(o) + "\n</g>")

def inject(panel_path, theme_name):
    with open(panel_path) as f: svg = f.read()
    if 'xmlns:inkscape' not in svg:
        svg = svg.replace('<svg ', '<svg xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" ', 1)
    # strip prior cluster-art layer (idempotent)
    svg = re.sub(r'<g inkscape:label="cluster-art".*?</g>\s*', '', svg, flags=re.DOTALL)
    layer = cluster_layer(THEMES[theme_name])
    # Insert BEFORE the hidden components layer if present (so cluster-art renders
    # under nothing problematic), else before </svg>.
    if 'inkscape:label="components"' in svg:
        svg = svg.replace('<g inkscape:label="components"', layer + '\n<g inkscape:label="components"', 1)
    else:
        svg = svg.replace('</svg>', layer + '\n</svg>')
    with open(panel_path, 'w') as f: f.write(svg)

if __name__ == "__main__":
    for v in ("dark", "light"):
        inject(f"res/panels/Monsoon_panel_{v}_monsoon.svg", v)
        print(f"Monsoon {v}: cluster-art layer injected")
