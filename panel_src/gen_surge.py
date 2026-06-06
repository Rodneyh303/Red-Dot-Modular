"""Generate the Surge expander panel (8HP, 120x380px) — dark + light.

Surge = the big-5 pattern-knob modulation expander: CV (+ attenuverter) for
NOTE VALUE, VARIATION, LEGATO, REST, ACCENT. Sibling to Interchange (pitch) and
Causeway (draw generation); Surge + Interchange shape how we REACT to the draw.

Layout: 5 rows of  jack | atten  (one CV pair per big-5 knob). 8HP is plenty.

nanosvg-safe: solid per-shape fills, no gradients/masks/text. Labels are runtime
nvgText in the widget. Brand palette from res/logo/README.md.
"""
import re
S = 75.0 / 25.4
def px(mm): return round(mm * S, 2)

W_PX, H_PX = 120.0, 380.0          # 8HP x 3U
W_MM = W_PX / S

THEMES = {
    "dark":  dict(bg="#070707", rail="#111111", stripe="#d4001a", ink="#f0ede8",
                  well="#0f1114", line="#2a2f37", teal="#26a69a", neutral="#6a6e76",
                  screw="#555", screwln="#222"),
    "light": dict(bg="#ece9e2", rail="#dcd9d2", stripe="#d4001a", ink="#15140f",
                  well="#d4d6d9", line="#c0c4ca", teal="#1a8276", neutral="#8a8e86",
                  screw="#bbb", screwln="#888"),
}

# 5 rows: jack (left) | attenuverter (right). One per big-5 knob.
ROWS = [60.0, 74.0, 88.0, 102.0, 116.0]
JACK_X, ATT_X = 14.0, 30.0

def header(t):
    o = [f'<rect width="{W_PX}" height="{H_PX}" fill="{t["bg"]}"/>',
         f'<rect x="0" y="0" width="{W_PX}" height="14.5" fill="{t["rail"]}"/>',
         f'<rect x="0" y="366" width="{W_PX}" height="14.5" fill="{t["rail"]}"/>',
         f'<rect x="0" y="0" width="{W_PX}" height="2" fill="{t["stripe"]}"/>']
    for cx, cy in [(7.5,14.5),(7.5,365.5),(112.5,14.5),(112.5,365.5)]:
        o.append(f'<circle cx="{cx}" cy="{cy}" r="4.5" fill="{t["screw"]}" stroke="{t["screwln"]}" stroke-width="0.8"/>'
                 f'<line x1="{cx-2.7}" y1="{cy}" x2="{cx+2.7}" y2="{cy}" stroke="{t["screwln"]}" stroke-width="1.2"/>')
    return o

def logo(theme_name):
    src = open(f"res/logo/dot-modular-logo-{theme_name}.svg").read()
    inner = re.sub(r'^.*?<svg[^>]*>', '', src, flags=re.DOTALL).replace('</svg>', '')
    inner = re.sub(r'<rect width="717" height="190"[^>]*/>', '', inner)
    inner = re.sub(r'<rect x="0" y="0" width="717" height="2.5"[^>]*/>', '', inner)
    sc = 100.0 / 717.0
    x = (W_PX - 100.0) / 2.0
    return [f'<g transform="translate({x:.2f},20.0) scale({sc:.5f})">{inner}</g>']

def wells(t):
    o = []
    a = o.append
    a(f'<rect x="{px(4):.1f}" y="{px(52):.1f}" width="{px(W_MM-8):.1f}" height="{px(72):.1f}" '
      f'rx="{px(2):.1f}" fill="{t["well"]}" fill-opacity="0.5" stroke="{t["line"]}" stroke-width="1"/>')
    for ry in ROWS:
        a(f'<circle cx="{px(JACK_X):.1f}" cy="{px(ry):.1f}" r="{px(4.0):.1f}" fill="{t["well"]}" stroke="{t["teal"]}" stroke-width="1"/>')
        a(f'<circle cx="{px(ATT_X):.1f}" cy="{px(ry):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{t["neutral"]}" stroke-width="1"/>')
    return o

def gen(theme_name):
    t = THEMES[theme_name]
    body = header(t) + logo(theme_name) + wells(t)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W_PX:.0f} {H_PX:.0f}" '
            f'width="{W_PX:.0f}" height="{H_PX:.0f}">\n' + "\n".join(body) + "\n</svg>")

if __name__ == "__main__":
    for v in ("dark","light"):
        open(f"res/panels/Surge_panel_{v}.svg","w").write(gen(v))
        print(f"Surge {v}: written")
