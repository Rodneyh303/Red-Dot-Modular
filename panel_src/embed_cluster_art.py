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
    # Single control row at y=87. Slots (rx(i)=12+i*16.7):
    #  0 SLEW_R 1 SLEW_M | 2 DICE_R 3 DICE_M | 4 TRIAL_R 5 TRIAL_M | 6 MIX_R 7 MIX_M
    #  | 8 LOCK 9 MUTE 10 RESET 11 RUN
    def rx(i): return 12.0 + i * 16.7
    # ── Recess binding the slew/dice/trial/mix group (slots 0..7) ──
    gx0 = rx(0) - 6.0
    gx1 = rx(7) + 6.0
    a(f'<rect x="{px(gx0):.1f}" y="{px(81):.1f}" width="{px(gx1-gx0):.1f}" height="{px(12):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["recess"]}" stroke="{t["line"]}" stroke-width="1"/>')
    # SLEW trim wells (neutral)
    for i in (0, 1):
        a(f'<circle cx="{px(rx(i)):.1f}" cy="{px(87):.1f}" r="{px(3.4):.1f}" fill="{t["well"]}" stroke="{t["line"]}" stroke-width="1"/>')
    # MAIN dice seats (red)
    for i in (2, 3):
        a(f'<rect x="{px(rx(i)-3):.1f}" y="{px(87-3):.1f}" width="{px(6):.1f}" height="{px(6):.1f}" rx="{px(1.2):.1f}" fill="{t["well"]}" stroke="{t["red"]}" stroke-width="0.9"/>')
    # TRIAL dice seats (gold)
    for i in (4, 5):
        a(f'<rect x="{px(rx(i)-3):.1f}" y="{px(87-3):.1f}" width="{px(6):.1f}" height="{px(6):.1f}" rx="{px(1.2):.1f}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="0.9"/>')
    # MIX knob wells (teal)
    for i in (6, 7):
        a(f'<circle cx="{px(rx(i)):.1f}" cy="{px(87):.1f}" r="{px(3.6):.1f}" fill="{t["well"]}" stroke="{t["teal"]}" stroke-width="1"/>')
    # ── Output group recess (in/out accent), behind the output jacks ──
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
