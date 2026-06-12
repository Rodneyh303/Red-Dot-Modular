"""Generate the Causeway expander panel (12HP, 180x380px) — dark + light.

Causeway = the dice/draw-generation modulation expander. It carries CV (with
attenuverters) for the 2 slews + 2 mixes, and 10 dedicated gate jacks for the
die actions. Reacts INTO Monsoon's draw generation; the sibling expanders
Interchange (pitch) and Surge (the big-5 pattern knobs) shape the REACTION to
the draw.

Layout (mm; VCV 1mm = 75/25.4 px):
  CV section  : Interchange-style  jack | atten | atten | jack
  Gate section: two columns of gate jacks (rhythm left, melody right)

nanosvg-safe: solid per-shape fills, no gradients/masks/filters. Control labels
are drawn at RUNTIME in the widget (nvgText); the SVG carries wells + the logo.
Brand palette from res/logo/README.md.
"""
S = 75.0 / 25.4
def px(mm): return round(mm * S, 2)

W_PX, H_PX = 180.0, 380.0          # 12HP x 3U
W_MM = W_PX / S

THEMES = {
    "dark":  dict(bg="#070707", rail="#111111", stripe="#d4001a", ink="#f0ede8",
                  well="#0f1114", line="#2a2f37", red="#dc2626", gold="#c8960c",
                  teal="#26a69a", neutral="#6a6e76", screw="#555", screwln="#222"),
    "light": dict(bg="#ece9e2", rail="#dcd9d2", stripe="#d4001a", ink="#15140f",
                  well="#d4d6d9", line="#c0c4ca", red="#c0202a", gold="#a07808",
                  teal="#1a8276", neutral="#8a8e86", screw="#bbb", screwln="#888"),
}

# ── Control geometry (mm) ────────────────────────────────────────────────────
# Fit 2 CV rows + 5 gate rows in the y=30..120mm band (366px rail = ~123.9mm).
# CV section: 4 columns jack|att|att|jack at these x (mm); two rows (slew, mix).
CV_COLS = [9.0, 21.0, 39.0, 51.0]     # within 60mm width (12HP=~60.96mm)
CV_ROWS = [38.0, 50.0]                # slew row, mix row
# Gate section: 2 columns (rhythm left, melody right), 5 rows.
G_COLS = [16.0, 44.0]
G_ROWS = [66.0, 78.0, 90.0, 102.0, 114.0]

def header(t):
    o = [f'<rect width="{W_PX}" height="{H_PX}" fill="{t["bg"]}"/>',
         f'<rect x="0" y="0" width="{W_PX}" height="14.5" fill="{t["rail"]}"/>',
         f'<rect x="0" y="366" width="{W_PX}" height="14.5" fill="{t["rail"]}"/>',
         f'<rect x="0" y="0" width="{W_PX}" height="2" fill="{t["stripe"]}"/>']
    # Screws are drawn in C++ (ui/RedScrew.hpp), NOT painted here — avoids the
    # double-draw and keeps screw style/size/position consistent across modules.
    return o

def sectioning(t):
    # Rhythm (left) / Melody (right) visual split. Faint tinted column backings
    # spanning the control area, plus a centre divider. Causeway places rhythm
    # controls in the left column of each pair, melody in the right.
    o = []
    a = o.append
    y0, y1 = 30.0, 120.0
    h = y1 - y0
    mid = W_MM / 2.0
    # left (rhythm) tint
    a(f'<rect x="{px(2):.1f}" y="{px(y0):.1f}" width="{px(mid-3):.1f}" height="{px(h):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["red"]}" fill-opacity="0.05"/>')
    # right (melody) tint
    a(f'<rect x="{px(mid+1):.1f}" y="{px(y0):.1f}" width="{px(mid-3):.1f}" height="{px(h):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["teal"]}" fill-opacity="0.05"/>')
    # centre divider
    a(f'<line x1="{px(mid):.1f}" y1="{px(y0):.1f}" x2="{px(mid):.1f}" y2="{px(y1):.1f}" '
      f'stroke="{t["line"]}" stroke-width="0.8" stroke-opacity="0.5"/>')
    return o

def wells(t):
    o = []
    a = o.append
    # CV section recess
    a(f'<rect x="{px(4):.1f}" y="{px(33):.1f}" width="{px(W_MM-8):.1f}" height="{px(24):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["well"]}" fill-opacity="0.5" stroke="{t["line"]}" stroke-width="1"/>')
    # CV jacks (outer cols) + attenuverter wells (inner cols), 2 rows
    for ry in CV_ROWS:
        # jacks outer
        for cx in (CV_COLS[0], CV_COLS[3]):
            a(f'<circle cx="{px(cx):.1f}" cy="{px(ry):.1f}" r="{px(4.0):.1f}" fill="{t["well"]}" stroke="{t["teal"]}" stroke-width="1"/>')
        # attenuverters inner
        for cx in (CV_COLS[1], CV_COLS[2]):
            a(f'<circle cx="{px(cx):.1f}" cy="{px(ry):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{t["neutral"]}" stroke-width="1"/>')
    # Gate section: 10 gate jacks (2 cols x 5 rows). Ring colour hints action group.
    ring = [(t["red"],t["red"]), (t["red"],t["red"]), (t["gold"],t["gold"]),
            (t["teal"],t["teal"]), (t["neutral"],t["neutral"])]
    for r, ry in enumerate(G_ROWS):
        for cx in G_COLS:
            col = ring[r][0]
            a(f'<circle cx="{px(cx):.1f}" cy="{px(ry):.1f}" r="{px(4.0):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1"/>')
    return o

import re

def logo(t, theme_name):
    """Embed the real nanosvg-safe wordmark (res/logo/dot-modular-logo-*.svg),
    stripped of its <svg> wrapper + background, scaled to fit the 12HP header."""
    src = open(f"res/logo/dot-modular-logo-{theme_name}.svg").read()
    # inner content = everything between the opening <svg ...> and </svg>,
    # minus the full-bleed background rect and the red top stripe (we have our own).
    inner = re.sub(r'^.*?<svg[^>]*>', '', src, flags=re.DOTALL)
    inner = inner.replace('</svg>', '')
    inner = re.sub(r'<rect width="717" height="190"[^>]*/>', '', inner)
    inner = re.sub(r'<rect x="0" y="0" width="717" height="2.5"[^>]*/>', '', inner)
    # wordmark native 717x190; scale to ~150px wide, placed under the top rail.
    sc = 150.0 / 717.0
    x = (W_PX - 150.0) / 2.0
    y = 20.0
    return [f'<g transform="translate({x:.2f},{y:.2f}) scale({sc:.5f})">{inner}</g>']

def gen(theme_name):
    t = THEMES[theme_name]
    body = header(t) + sectioning(t) + logo(t, theme_name) + wells(t)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W_PX:.0f} {H_PX:.0f}" '
           f'width="{W_PX:.0f}" height="{H_PX:.0f}">\n' + "\n".join(body) + "\n</svg>")
    return svg

if __name__ == "__main__":
    for v in ("dark","light"):
        open(f"res/panels/Causeway_panel_{v}.svg","w").write(gen(v))
        print(f"Causeway {v}: written")
