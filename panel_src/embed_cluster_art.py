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
    IX, IP = 15.0, 17.0
    OX, OP = 114.0, 17.0

    # ── Lower-panel group boxes are drawn fairly opaque to suppress the old
    #    hand-authored circles beneath, while the supertrees still read in the
    #    gaps between groups (matching the original aesthetic). No full-width
    #    cover-all rect — that would bury the Gardens-by-the-Bay motif.

    # ── Control row recess (slots 0..7: slew/dice/trial/mix) ──
    gx0, gx1 = rx(0) - 6.0, rx(7) + 6.0
    a(f'<rect x="{px(gx0):.1f}" y="{px(81):.1f}" width="{px(gx1-gx0):.1f}" height="{px(12):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["recess"]}" stroke="{t["line"]}" stroke-width="1"/>')
    for i in (0, 1):   # SLEW trims (neutral)
        a(f'<circle cx="{px(rx(i)):.1f}" cy="{px(87):.1f}" r="{px(3.4):.1f}" fill="{t["well"]}" stroke="{t["line"]}" stroke-width="1"/>')
    for i in (2, 3):   # MAIN dice seats (red)
        a(f'<rect x="{px(rx(i)-3):.1f}" y="{px(87-3):.1f}" width="{px(6):.1f}" height="{px(6):.1f}" rx="{px(1.2):.1f}" fill="{t["well"]}" stroke="{t["red"]}" stroke-width="0.9"/>')
    for i in (4, 5):   # TRIAL dice seats (gold)
        a(f'<rect x="{px(rx(i)-3):.1f}" y="{px(87-3):.1f}" width="{px(6):.1f}" height="{px(6):.1f}" rx="{px(1.2):.1f}" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="0.9"/>')
    for i in (6, 7):   # MIX knob wells (teal)
        a(f'<circle cx="{px(rx(i)):.1f}" cy="{px(87):.1f}" r="{px(3.6):.1f}" fill="{t["well"]}" stroke="{t["teal"]}" stroke-width="1"/>')
    # Utility button wells (slots 8..11: LOCK/MUTE/RESET/RUN) — small, no recess
    for i in (8, 9, 10, 11):
        a(f'<circle cx="{px(rx(i)):.1f}" cy="{px(87):.1f}" r="{px(2.6):.1f}" fill="{t["well"]}" stroke="{t["line"]}" stroke-width="0.9"/>')

    # ── Input group recess (left) + output group recess (right) ──
    # Fairly opaque so the old artwork circles beneath don't show through.
    a(f'<rect x="{px(IX-7):.1f}" y="{px(99):.1f}" width="{px((IX+5*IP)-(IX-7)+7):.1f}" height="{px(27):.1f}" '
      f'rx="{px(2.5):.1f}" fill="{t["recess"]}" fill-opacity="0.92" stroke="{t["line"]}" stroke-width="1"/>')
    a(f'<rect x="{px(OX-7):.1f}" y="{px(99):.1f}" width="{px((OX+4*OP)-(OX-7)+7):.1f}" height="{px(27):.1f}" '
      f'rx="{px(2.5):.1f}" fill="{t["recess"]}" fill-opacity="0.86" stroke="{t["outline"]}" stroke-width="1"/>')

    # ── Jack-row wells at the WIDGET's exact positions ──
    # Inputs: 6 + 6 at IX=15/IP=17, y=105/120. Outputs: 5 (top) + 4 (bottom) at OX=114/OP=17.
    for i in range(6):   # input row 1 (y105)
        a(f'<circle cx="{px(IX+i*IP):.1f}" cy="{px(105):.1f}" r="{px(4.4):.1f}" fill="{t["well"]}" stroke="{t["line"]}" stroke-width="1"/>')
    for i in range(6):   # input row 2 (y120)
        a(f'<circle cx="{px(IX+i*IP):.1f}" cy="{px(120):.1f}" r="{px(4.4):.1f}" fill="{t["well"]}" stroke="{t["line"]}" stroke-width="1"/>')
    for i in range(5):   # output row 1 (y105) — 5 jacks
        a(f'<circle cx="{px(OX+i*OP):.1f}" cy="{px(105):.1f}" r="{px(4.4):.1f}" fill="{t["well"]}" stroke="{t["outline"]}" stroke-width="1"/>')
    for i in range(4):   # output row 2 (y120) — 4 jacks
        a(f'<circle cx="{px(OX+i*OP):.1f}" cy="{px(120):.1f}" r="{px(4.4):.1f}" fill="{t["well"]}" stroke="{t["outline"]}" stroke-width="1"/>')

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
