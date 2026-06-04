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
        group="#13151a", groupline="#262b33", motif="#2e333c", motifwave="#333944")
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

# ── Embed the real dot.modular wordmark (nanosvg-safe logo-{dark,light}.svg) ──
# The logo is 717x190 in its own coords with a full bg rect + red top-rule we
# must strip (the panel has its own). We keep only the wordmark body (the <g>
# groups) and place it via translate+scale into the title strip.
import re as _re
def logo_embed(dark, x_mm, y_mm, target_w_mm):
    path = "res/logo/dot-modular-logo-dark.svg" if dark else "res/logo/dot-modular-logo-light.svg"
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]   # drop the two leading bg/accent rects
    LOGO_W, LOGO_H = 717.0, 190.0
    scale = px(target_w_mm) / LOGO_W            # uniform scale to target width (px)
    tx, ty = px(x_mm), px(y_mm)
    return f'<g transform="translate({tx:.2f},{ty:.2f}) scale({scale:.5f})">{body}</g>'

def mbs(x_mm, y_mm, w_mm, h_mm, t, op):
    # Marina Bay Sands: three slanted trapezoidal towers + connecting skypark
    # deck, with faint window lines and antenna dots — geometry adapted from the
    # earlier panel concept, but rendered nanosvg-safe (solid per-shape fills,
    # no gradient, no group-inherited paint).
    x,y,w,h = px(x_mm),px(y_mm),px(w_mm),px(h_mm)
    fill=t["motif"]; acc=t["accent"]
    tw=w*0.165; gap=(w-3*tw)/2.0; th=h; by=y+h; lean=tw*0.06
    L=[]; A=L.append
    xs=[x, x+tw+gap, x+2*(tw+gap)]
    for tx in xs:
        # trapezoid: slightly wider at base (towers lean), as real MBS
        x1,x2 = tx+lean, tx+tw-lean      # top
        x3,x4 = tx+tw, tx                # base
        A(f'<polygon points="{x1:.1f},{y:.1f} {x2:.1f},{y:.1f} {x3:.1f},{by:.1f} {x4:.1f},{by:.1f}" '
          f'fill="{fill}" fill-opacity="{op}"/>')
        # two faint window bands per tower
        for f_ in (0.36, 0.64):
            wy=y+th*f_
            A(f'<line x1="{tx+lean*0.6:.1f}" y1="{wy:.1f}" x2="{tx+tw-lean*0.6:.1f}" y2="{wy:.1f}" '
              f'stroke="{acc}" stroke-width="0.5" stroke-opacity="{op*0.4:.3f}"/>')
        # antenna + dot atop each tower
        cxk=tx+tw*0.5
        A(f'<line x1="{cxk:.1f}" y1="{y:.1f}" x2="{cxk:.1f}" y2="{y-h*0.10:.1f}" stroke="{acc}" stroke-width="0.6" stroke-opacity="{op*0.6:.3f}"/>')
        A(f'<circle cx="{cxk:.1f}" cy="{y-h*0.12:.1f}" r="1.1" fill="{acc}" fill-opacity="{op*0.6:.3f}"/>')
    # skypark deck spanning the three tops (slight lift to the right, MBS-like)
    dy=y-h*0.06
    A(f'<polygon points="{x-w*0.02:.1f},{dy:.1f} {x+w+w*0.06:.1f},{dy-h*0.05:.1f} '
      f'{x+w+w*0.06:.1f},{dy-h*0.05+h*0.06:.1f} {x-w*0.02:.1f},{dy+h*0.06:.1f}" '
      f'fill="{fill}" fill-opacity="{op}"/>')
    return "".join(L)

def waves(x_mm, y_mm, t, op, rows=3, span_mm=138.0):
    x0=px(x_mm); col=t["motifwave"]; L=[]; A=L.append
    for i in range(rows):
        yb=px(y_mm+i*3.2); amp=px(1.6); seg=px(span_mm)/8.0
        d=f'M {x0:.1f} {yb:.1f} '
        for k in range(8):
            cx1=x0+seg*k+seg*0.25; cx2=x0+seg*k+seg*0.75; ex=x0+seg*(k+1)
            ud=-amp if k%2==0 else amp
            d+=f'C {cx1:.1f} {yb+ud:.1f} {cx2:.1f} {yb+ud:.1f} {ex:.1f} {yb:.1f} '
        # inline stroke + opacity per path (nanosvg ignores group-level values)
        A(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.5" stroke-opacity="{op}"/>')
    return "".join(L)

def gen(dark):
    t=theme(dark); L=[]; A=L.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(mbs(116.0, 96.0, 78.0, 18.0, t, op=0.8))
    A(waves(58.0, 116.0, t, op=0.7, rows=3, span_mm=140.0))
    A(f'<line x1="0" y1="{px(11):.1f}" x2="{PW}" y2="{px(11):.1f}" stroke="{t["accent"]}" stroke-width="1.5" opacity="0.55"/>')
    A(f'<line x1="0" y1="{px(120):.1f}" x2="{PW}" y2="{px(120):.1f}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(SPREAD_X+5.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(f'<rect x="{px(gx):.1f}" y="{px(gy):.1f}" width="{px(gw):.1f}" height="{px(gh):.1f}" rx="{px(2):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
    # (no header tab — the title strip above serves as the section header)
    sepx=0.5*(COL_J2+COL_A1)
    A(f'<line x1="{px(sepx):.1f}" y1="{px(gy+2):.1f}" x2="{px(sepx):.1f}" y2="{px(gy+gh-2):.1f}" stroke="{t["groupline"]}" stroke-width="0.75" opacity="0.6"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y-6):.1f}" width="{px(ED_W):.1f}" height="{px(5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" opacity="0.6"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>')
    for k in range(1,3):
        ly=ED_Y+k*(ED_H/3.0)
        A(f'<line x1="{px(ED_X+1):.1f}" y1="{px(ly):.1f}" x2="{px(ED_X+ED_W-1):.1f}" y2="{px(ly):.1f}" stroke="{t["edborder"]}" stroke-width="0.75" opacity="0.7"/>')
    A('</g>')
    A(f'<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(logo_embed(dark, x_mm=11.0, y_mm=4.5, target_w_mm=42.0))
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
    # (screws are drawn by the C++ RedScrew widget, not the panel SVG)
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"StraitsEastSandsVisual_40HP.svg"),(False,"StraitsEastSandsVisual_40HP_light.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
