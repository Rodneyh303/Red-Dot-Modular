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
# Extra top margin so the voice tab row isn't crammed against the panel top edge.
# 0.5 cm = 5 mm. Adjust + rerun the generator (and mirror TAB_TOP_OFFSET_MM in
# StraitsEastSandsVisualWidget) to taste.
TAB_TOP_OFFSET_MM = 5.0
ED_X, ED_Y = 58.0, 18.0 + TAB_TOP_OFFSET_MM
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
    # Marina Bay Sands: three leaning tower-pairs (curved-lean outer + straight
    # inner leg) carrying the SkyPark boat deck, plus a fountain spray fan. Mapped
    # from the reference content box (x[90..630], y[150..500]) into the panel box.
    # nanosvg-safe. Window-grid + fountain only when large (h>55px). Kept in sync
    # with dotmod_design.mbs (same art).
    x,y,w,h = px(x_mm),px(y_mm),px(w_mm),px(h_mm)
    fill=t["motif"]; acc=t["accent"]; bright=t["ink"]
    big=(h>55)
    RX0,RX1,RY0,RY1 = 90.0,630.0,150.0,500.0
    def MX(rx): return x+(rx-RX0)/(RX1-RX0)*w
    def MY(ry): return y+(ry-RY0)/(RY1-RY0)*h
    L=[]; A=L.append
    lean=[(210,500,260,400,280,190,295,190,270,400,250,500),
          (340,500,390,400,410,190,425,190,400,400,380,500),
          (470,500,520,400,540,190,555,190,530,400,510,500)]
    for (a1,a2,b1,b2,c1,c2,d1,d2,e1,e2,f1,f2) in lean:
        A(f'<path d="M {MX(a1):.1f} {MY(a2):.1f} Q {MX(b1):.1f} {MY(b2):.1f} {MX(c1):.1f} {MY(c2):.1f} '
          f'L {MX(d1):.1f} {MY(d2):.1f} Q {MX(e1):.1f} {MY(e2):.1f} {MX(f1):.1f} {MY(f2):.1f} Z" '
          f'fill="{fill}" fill-opacity="{op}"/>')
    for (a1,a2,b1,b2,c1,c2,d1,d2) in [(290,500,305,190,330,190,330,500),(420,500,435,190,460,190,460,500),(550,500,565,190,590,190,590,500)]:
        A(f'<polygon points="{MX(a1):.1f},{MY(a2):.1f} {MX(b1):.1f},{MY(b2):.1f} {MX(c1):.1f},{MY(c2):.1f} {MX(d1):.1f},{MY(d2):.1f}" fill="{fill}" fill-opacity="{op}"/>')
    if big:
        for (sx0,sx1) in [(290,330),(420,460),(550,590)]:
            for f_ in (0.18,0.34,0.50,0.66,0.82):
                ry=190+(500-190)*f_
                A(f'<line x1="{MX(sx0):.1f}" y1="{MY(ry):.1f}" x2="{MX(sx1):.1f}" y2="{MY(ry):.1f}" stroke="{acc}" stroke-width="0.4" stroke-opacity="{op*0.3:.3f}"/>')
    A(f'<path d="M {MX(90):.1f} {MY(150):.1f} Q {MX(365):.1f} {MY(160):.1f} {MX(630):.1f} {MY(150):.1f} '
      f'C {MX(640):.1f} {MY(150):.1f} {MX(640):.1f} {MY(190):.1f} {MX(620):.1f} {MY(190):.1f} '
      f'L {MX(280):.1f} {MY(190):.1f} Q {MX(160):.1f} {MY(190):.1f} {MX(90):.1f} {MY(150):.1f} Z" fill="{fill}" fill-opacity="{op}"/>')
    A(f'<path d="M {MX(90):.1f} {MY(150):.1f} Q {MX(365):.1f} {MY(160):.1f} {MX(630):.1f} {MY(150):.1f}" fill="none" stroke="{bright}" stroke-width="0.8" stroke-opacity="{op*0.7:.3f}" stroke-linecap="round"/>')
    if big:
        fb=MY(500)
        for (tx,ty) in [(70,390),(100,360),(140,340),(180,370),(210,400)]:
            A(f'<line x1="{MX(140):.1f}" y1="{fb:.1f}" x2="{MX(tx):.1f}" y2="{MY(ty):.1f}" stroke="{bright}" stroke-width="0.5" stroke-opacity="{op*0.45:.3f}" stroke-linecap="round"/>')
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
    # red accent rule under the branding strip + footer rule
    A(f'<line x1="0" y1="{px(11):.1f}" x2="{PW}" y2="{px(11):.1f}" stroke="{t["accent"]}" stroke-width="1.5" opacity="0.55"/>')
    A(f'<line x1="0" y1="{px(120):.1f}" x2="{PW}" y2="{px(120):.1f}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')

    # ── editor recess: a framed "display window", with a large faint Marina Bay
    #    Sands watermark INSIDE it so the empty state reads as Marina Bay, not a
    #    void (the live editor widget draws on top). ──
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y-6):.1f}" width="{px(ED_W):.1f}" height="{px(5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" opacity="0.6"/>')
    A(f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>')
    # MBS watermark centred in the editor, large + faint (reads as Marina Bay)
    A(mbs(ED_X+ED_W*0.28, ED_Y+ED_H-5.0, ED_W*0.46, ED_H*0.68, t, op=0.16))
    for k in range(1,3):
        ly=ED_Y+k*(ED_H/3.0)
        A(f'<line x1="{px(ED_X+1):.1f}" y1="{px(ly):.1f}" x2="{px(ED_X+ED_W-1):.1f}" y2="{px(ly):.1f}" stroke="{t["edborder"]}" stroke-width="0.75" opacity="0.7"/>')

    # ── below the editor + footer: Marina Bay water (waves) as an integrated
    #    base band spanning the full control-to-edge width ──
    A(waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=ED_W))
    # a crisp MBS as the identity mark, sat on the water, lower-right
    A(mbs(W_MM-66.0, 111.0, 52.0, 14.0, t, op=0.85))

    # ── control group recess (left) ──
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(SPREAD_X+5.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(f'<rect x="{px(gx):.1f}" y="{px(gy):.1f}" width="{px(gw):.1f}" height="{px(gh):.1f}" rx="{px(2):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
    sepx=0.5*(COL_J2+COL_A1)
    A(f'<line x1="{px(sepx):.1f}" y1="{px(gy+2):.1f}" x2="{px(sepx):.1f}" y2="{px(gy+gh-2):.1f}" stroke="{t["groupline"]}" stroke-width="0.75" opacity="0.6"/>')
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

    # ── Macro/East blend controls — 3 labelled groups (REST / MELODY / OCTAVE),
    #    each a boxed cluster: an owner on/off button (top-left) + a 2×2 grid of
    #    MACRO CV SEND trimpots (Len/Off/Rot/Spr). Group boxes + a "MACRO BLEND"
    #    header give the previously-loose row a clear structure. Labels (group
    #    names + item names) are drawn by the widget in NanoVG (the panel carries
    #    no baked text), positioned to match these coordinates. Only meaningful
    #    with a Macro visual attached; the module greys them out otherwise. ──
    BLEND_TOP = 74.0               # below the editor (ED_Y+ED_H = 66)
    BLEND_H   = 30.0
    GROUP_W   = ED_W/3.0
    GAP       = 3.0
    # section header rule
    A(f'<line x1="{px(ED_X):.1f}" y1="{px(BLEND_TOP-3.0):.1f}" x2="{px(ED_X+ED_W):.1f}" y2="{px(BLEND_TOP-3.0):.1f}" stroke="{t["edborder"]}" stroke-width="0.8" opacity="0.7"/>')
    LANE_NAMES = ["REST","MELODY","OCTAVE"]
    OWN_XY = []     # owner button centres, per lane
    SEND_XY = []    # send trim centres, per (lane,item)
    for l in range(3):
        gx = ED_X + l*GROUP_W + GAP*0.5
        gw = GROUP_W - GAP
        # group box
        A(f'<rect x="{px(gx):.1f}" y="{px(BLEND_TOP):.1f}" width="{px(gw):.1f}" height="{px(BLEND_H):.1f}" rx="{px(1.2):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.9"/>')
        # owner button: top-left of the box
        oy = BLEND_TOP + 6.5
        ox = gx + 5.5
        A(f'<circle cx="{px(ox):.1f}" cy="{px(oy):.1f}" r="{px(2.4):.1f}" fill="{t["edrecess"]}" stroke="{t["accent"]}" stroke-width="1.1"/>')
        OWN_XY.append((ox,oy))
        # 2×2 send grid in the right ~2/3 of the box
        sx0 = gx + gw*0.42
        sxs = gw*0.26
        sy0 = BLEND_TOP + 8.5
        sys = 12.0
        lane_sends=[]
        for item in range(4):
            cxs = sx0 + (item % 2)*sxs
            cys = sy0 + (item // 2)*sys
            trim(cxs, cys, t["gold"])
            lane_sends.append((cxs,cys))
        SEND_XY.append(lane_sends)
    A('</g>')
    # ── SvgPanelKit component layer. Indices mirror StraitsEastSandsVisual.hpp:
    #    cvId(r,c)=CV_START(0)+r*2+c  inputs 0..11;
    #    attenId(r,c)=ATTEN_START(3)+r*2+c  params 3..14;
    #    SPREAD_R/M/O = params 0/1/2. ──
    def kit_shape(kind, idx, x, y):
        A(f'<circle id="{kind}_{idx}" cx="{px(x):.2f}" cy="{px(y):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    for r in range(6):
        y=rowY(r)
        kit_shape("input", 0+r*2+0, COL_J1, y); kit_shape("input", 0+r*2+1, COL_J2, y)
        kit_shape("param", 3+r*2+0, COL_A1, y); kit_shape("param", 3+r*2+1, COL_A2, y)
    for lane in range(3):
        y=0.5*(rowY(lane*2)+rowY(lane*2+1))
        kit_shape("param", lane, SPREAD_X, y)   # SPREAD_R/M/O = 0/1/2
    # Macro/East blend control markers (named; bound to display proxies). Use the
    # grouped coordinates computed above (OWN_XY, SEND_XY).
    for l in range(3):
        ox,oy = OWN_XY[l]
        A(f'<circle id="param_owner_{l}" cx="{px(ox):.2f}" cy="{px(oy):.2f}" r="0.5" fill="none" stroke="none"/>')
        for item in range(4):
            cxs,cys = SEND_XY[l][item]
            A(f'<circle id="param_send_{l}_{item}" cx="{px(cxs):.2f}" cy="{px(cys):.2f}" r="0.5" fill="none" stroke="none"/>')
    A(f'<circle id="light_connect" cx="{px(W_MM*0.5):.2f}" cy="{px(124.0):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"StraitsEastSandsVisual_40HP.svg"),(False,"StraitsEastSandsVisual_40HP_light.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
