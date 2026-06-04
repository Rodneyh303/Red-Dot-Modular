"""Straits East Sands visual panel (40HP, native 75 DPI).
dot.modular panel system: Monsoon palette, title/footer DM mark, input-group
recess, editor recess w/ header + lane separators, MBS+waves background motif,
red corner screws. nanosvg-safe (no masks/gradients/text-for-controls)."""
import math
S = 75.0/25.4
def px(mm): return round(mm*S, 2)
W_MM, H_MM = 203.2, 128.5
PW, PH = px(W_MM), px(H_MM)

ROW_TOP, ROW_BOT, N = 14.0, 108.0, 6
def rowY(r): return ROW_TOP + (r+0.5)*(ROW_BOT-ROW_TOP)/N
COL_J1, COL_J2, COL_A1, COL_A2, SPREAD_X = 8.0, 18.0, 30.0, 39.0, 49.0
ED_X, ED_Y = 58.0, 18.0
ED_W = W_MM - ED_X - 4.0
ED_H = 48.0

def theme(dark):
    if dark: return dict(
        bg="#16181c", ink="#d8d8dc", dim="#8a8f98", teal="#26a69a", gold="#c8960c",
        accent="#dc2626", jackwell="#0b0d10", jackring="#4a5058", well="#0f1114",
        wellring="#3a4048", edrecess="#101216", edborder="#2a2f37", tabband="#202833",
        group="#13151a", groupline="#262b33", motif="#262a32", motifwave="#2c313a")
    return dict(
        bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", teal="#1a8276", gold="#a07808",
        accent="#c0202a", jackwell="#dadce0", jackring="#9298a0", well="#d4d6d9",
        wellring="#a8aeb6", edrecess="#d8dade", edborder="#c0c4ca", tabband="#cdd4dc",
        group="#dfe0e3", groupline="#c8ccd2", motif="#d8d9dc", motifwave="#d2d4d8")

def dm_mark(cx_mm, cy_mm, r_mm, t):
    cx, cy, r = px(cx_mm), px(cy_mm), px(r_mm); inner=r*0.84
    L=[]; A=L.append
    A(f'<g opacity="0.95">')
    A(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{t["well"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
    A(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{inner:.1f}" fill="{t["edrecess"]}"/>')
    A(f'<g stroke="{t["accent"]}" stroke-width="1" opacity="0.85">')
    A(f'<line x1="{cx-inner:.1f}" y1="{cy:.1f}" x2="{cx+inner:.1f}" y2="{cy:.1f}"/>')
    A(f'<line x1="{cx:.1f}" y1="{cy-inner:.1f}" x2="{cx:.1f}" y2="{cy+inner:.1f}"/></g>')
    A(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r*0.26:.1f}" fill="{t["accent"]}"/></g>')
    return "".join(L)

def mbs(x_mm, y_mm, w_mm, h_mm, t, op):
    x,y,w,h = px(x_mm),px(y_mm),px(w_mm),px(h_mm)
    tw=w*0.13; gap=(w-3*tw)/2.0; th=h*0.86; by=y+h
    L=[]; A=L.append
    A(f'<g fill="{t["motif"]}" opacity="{op}">')
    for tx in [x, x+tw+gap, x+2*(tw+gap)]:
        A(f'<rect x="{tx:.1f}" y="{by-th:.1f}" width="{tw:.1f}" height="{th:.1f}" rx="{tw*0.18:.1f}"/>')
    deckY=by-th-h*0.04
    A(f'<path d="M {x-w*0.02:.1f} {deckY:.1f} L {x+w+w*0.10:.1f} {deckY-h*0.10:.1f} '
      f'L {x+w+w*0.10:.1f} {deckY-h*0.10+h*0.05:.1f} L {x-w*0.02:.1f} {deckY+h*0.05:.1f} Z"/></g>')
    return "".join(L)

def waves(x_mm, y_mm, t, op, rows=3, span_mm=138.0):
    x0=px(x_mm); L=[]; A=L.append
    A(f'<g stroke="{t["motifwave"]}" stroke-width="1.5" fill="none" opacity="{op}">')
    for i in range(rows):
        yb=px(y_mm+i*3.2); amp=px(1.6); seg=px(span_mm)/8.0
        d=f'M {x0:.1f} {yb:.1f} '
        for k in range(8):
            cx1=x0+seg*k+seg*0.25; cx2=x0+seg*k+seg*0.75; ex=x0+seg*(k+1)
            ud=-amp if k%2==0 else amp
            d+=f'C {cx1:.1f} {yb+ud:.1f} {cx2:.1f} {yb+ud:.1f} {ex:.1f} {yb:.1f} '
        A(f'<path d="{d}"/>')
    A('</g>')
    return "".join(L)

def gen(dark):
    t=theme(dark); L=[]; A=L.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(mbs(120.0, 92.0, 70.0, 24.0, t, op=0.55))
    A(waves(60.0, 118.0, t, op=0.55, rows=3, span_mm=138.0))
    A(f'<line x1="0" y1="{px(11):.1f}" x2="{PW}" y2="{px(11):.1f}" stroke="{t["accent"]}" stroke-width="1.5" opacity="0.55"/>')
    A(f'<line x1="0" y1="{px(120):.1f}" x2="{PW}" y2="{px(120):.1f}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(SPREAD_X+5.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(f'<rect x="{px(gx):.1f}" y="{px(gy):.1f}" width="{px(gw):.1f}" height="{px(gh):.1f}" rx="{px(2):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
    A(f'<rect x="{px(gx):.1f}" y="{px(gy-5):.1f}" width="{px(gw):.1f}" height="{px(5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" opacity="0.6"/>')
    sepx=0.5*(COL_J2+COL_A1)
    A(f'<line x1="{px(sepx):.1f}" y1="{px(gy+2):.1f}" x2="{px(sepx):.1f}" y2="{px(gy+gh-2):.1f}" stroke="{t["groupline"]}" stroke-width="0.75" opacity="0.6"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y-6):.1f}" width="{px(ED_W):.1f}" height="{px(5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" opacity="0.6"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>')
    for k in range(1,3):
        ly=ED_Y+k*(ED_H/3.0)
        A(f'<line x1="{px(ED_X+1):.1f}" y1="{px(ly):.1f}" x2="{px(ED_X+ED_W-1):.1f}" y2="{px(ly):.1f}" stroke="{t["edborder"]}" stroke-width="0.75" opacity="0.7"/>')
    A('</g>')
    A(f'<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(dm_mark(14.0, 6.0, 4.0, t))
    A(dm_mark(W_MM-8.0, 124.5, 3.0, t))
    A('</g>')
    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    def jack(x,y):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.7):.1f}" fill="{t["edrecess"]}"/>')
    def trim(x,y,col):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>')
        A(f'<line x1="{px(x):.1f}" y1="{px(y):.1f}" x2="{px(x):.1f}" y2="{px(y-2.4):.1f}" stroke="{col}" stroke-width="1"/>')
    for r in range(6):
        y=rowY(r); jack(COL_J1,y); jack(COL_J2,y)
        trim(COL_A1,y,t["gold"]); trim(COL_A2,y,t["gold"])
    for lane in range(3):
        y=0.5*(rowY(lane*2)+rowY(lane*2+1)); trim(SPREAD_X,y,t["teal"])
    A('</g>')
    A(f'<g inkscape:label="screws" inkscape:groupmode="layer">')
    def screw(x,y):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(2.2):.1f}" fill="{t["accent"]}" stroke="{t["bg"]}" stroke-width="0.5"/>')
        A(f'<line x1="{px(x-1.4):.1f}" y1="{px(y):.1f}" x2="{px(x+1.4):.1f}" y2="{px(y):.1f}" stroke="{t["bg"]}" stroke-width="0.6" opacity="0.7"/>')
    for sx in (5.0, W_MM-5.0):
        for sy in (5.0, H_MM-5.0): screw(sx,sy)
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"StraitsEastSandsVisual_40HP.svg"),(False,"StraitsEastSandsVisual_40HP_light.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
