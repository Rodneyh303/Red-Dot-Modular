"""Clean 40HP Straits East panel (native 75 DPI).
Artwork: jack wells, atten/spread trimpot wells, 2-row tab band guide,
editor recess. No <text> (nanosvg ignores it; the widget/editor draw labels)."""
import math
S = 75.0/25.4
def px(mm): return round(mm*S, 2)
W_MM, H_MM = 203.2, 128.5
PW, PH = px(W_MM), px(H_MM)

# Match StraitsEastVisualIds layout constants
ROW_TOP, ROW_BOT, N = 14.0, 108.0, 6
def rowY(r): return ROW_TOP + (r+0.5)*(ROW_BOT-ROW_TOP)/N
COL_J1, COL_J2, COL_A1, COL_A2, SPREAD_X = 8.0, 18.0, 30.0, 39.0, 49.0
ED_X, ED_Y = 58.0, 18.0
ED_W = W_MM - ED_X - 4.0
ED_H = 48.0   # 3 poly lanes × 16mm (matches StraitsEastSandsVisual.hpp ED_H)

def theme(dark):
    if dark: return dict(bg="#1a1d22", ink="#d8dade", dim="#8a9098", teal="#26a69a",
        accent="#dc2626", jackwell="#0e1013", jackring="#4a5058", well="#15181c",
        wellring="#3a4048", edrecess="#101216", edborder="#2a3038", tabband="#202833")
    return dict(bg="#e7e8ea", ink="#2a2d32", dim="#7a808a", teal="#1a8276",
        accent="#c0202a", jackwell="#dadce0", jackring="#9298a0", well="#d6d8dc",
        wellring="#a8aeb6", edrecess="#dde0e4", edborder="#c0c4ca", tabband="#cdd4dc")

def gen(dark):
    t=theme(dark); L=[]; A=L.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    # Title strip accent
    A(f'<line x1="0" y1="{px(10):.1f}" x2="{PW}" y2="{px(10):.1f}" stroke="{t["accent"]}" stroke-width="1" opacity="0.4"/>')
    # 2-row tab band guide (just a recess where the TabButtonGroup sits)
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y-12):.1f}" width="{px(ED_W):.1f}" height="{px(10):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" opacity="0.5"/>')
    # Editor recess
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>')
    A('</g>')

    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    def jack(x,y):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.7):.1f}" fill="{t["edrecess"]}"/>')
    def trim(x,y,col):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1"/>')
    # jack + atten columns per row
    for r in range(6):
        y=rowY(r)
        jack(COL_J1,y); jack(COL_J2,y)
        trim(COL_A1,y,t["wellring"]); trim(COL_A2,y,t["wellring"])
    # per-lane spread trimpots (centre of each lane's 2-row band), teal ring
    for lane in range(3):
        y=0.5*(rowY(lane*2)+rowY(lane*2+1))
        trim(SPREAD_X,y,t["teal"])
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"StraitsEastSandsVisual_40HP.svg"),(False,"StraitsEastSandsVisual_40HP_light.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
