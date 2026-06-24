"""Straits East Sands visual panel (40HP, native 75 DPI).
dot.modular panel system: Monsoon palette, title/footer DM mark, input-group
recess, editor recess w/ header + lane separators, MBS+waves background motif,
red corner screws. nanosvg-safe (no masks/gradients/text-for-controls)."""
import math
S = 75.0/25.4
def px(mm): return round(mm*S, 2)
W_MM, H_MM = 213.36, 128.5  # 42HP (40 + 2HP right strip for poly prob-out jacks)
PW, PH = px(W_MM), px(H_MM)

ROW_TOP, ROW_BOT, N = 14.0, 108.0, 4   # 4 lanes, one row each
def rowY(r): return ROW_TOP + (r+0.5)*(ROW_BOT-ROW_TOP)/N
# 4 CV jacks + 4 attens + spread base — columns match SandsMonoVisual, ED_X=88
JACK_X  = [6.0, 15.0, 24.0, 33.0]   # LEN/OFF/ROT/SPR-cv
ATTEN_X = [43.0, 52.0, 61.0, 70.0]  # LEN/OFF/ROT/SPR depth
SPREAD_X = 80.0                      # spread base trimpot
# Display order: row i → engine lane DISPLAY_ORDER[i] (MEL/OCT/REST/ACC top-to-bottom)
# matches SandsVisualEditorV4 lane order so left jacks align with editor rows.
DISPLAY_ORDER = [1, 2, 0, 3]   # row0=MEL(eng1), row1=OCT(eng2), row2=REST(eng0), row3=ACC(eng3)
LANE_NAMES_DISPLAY = ["MELODY","OCTAVE","REST","ACCENT"]

# Extra top margin so the voice tab row isn't crammed against the panel top edge.
# 0.5 cm = 5 mm. Adjust + rerun the generator (and mirror TAB_TOP_OFFSET_MM in
# StraitsEastSandsVisualWidget) to taste.
TAB_TOP_OFFSET_MM = 5.0
ED_X, ED_Y = 88.0, 18.0 + TAB_TOP_OFFSET_MM
PROB_OUT_X = 207.0  # right-strip jack column (matches hpp)
ED_W = PROB_OUT_X - ED_X - 8.0  # editor stops left of the prob-out jacks (matches hpp)
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
    for k in range(1,4):   # 4 lanes → 3 dividers
        ly=ED_Y+k*(ED_H/4.0)
        A(f'<line x1="{px(ED_X+1):.1f}" y1="{px(ly):.1f}" x2="{px(ED_X+ED_W-1):.1f}" y2="{px(ly):.1f}" stroke="{t["edborder"]}" stroke-width="0.75" opacity="0.7"/>')

    # ── below the editor + footer: Marina Bay water (waves) as an integrated
    #    base band spanning the full control-to-edge width ──
    A(waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=ED_W))
    # a crisp MBS as the identity mark, sat on the water, lower-right
    A(mbs(W_MM-66.0, 111.0, 52.0, 14.0, t, op=0.85))

    # ── control group recess (left) ──
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(SPREAD_X+6.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(f'<rect x="{px(gx):.1f}" y="{px(gy):.1f}" width="{px(gw):.1f}" height="{px(gh):.1f}" rx="{px(2):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
    sepx=0.5*(JACK_X[-1]+ATTEN_X[0])  # separator between jack cluster and atten cluster
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
    for row in range(4):
        y=rowY(row)
        for x in JACK_X:  jack(x,y)
        for x in ATTEN_X: trim(x,y,t["gold"])
        trim(SPREAD_X,y,t["teal"])

    # ── Macro/East blend controls — 3 labelled groups (REST / MELODY / OCTAVE),
    #    each a demarked box stacked as: lane-name header → owner latch (OWN) →
    #    a 2×2 MACRO-SEND trim grid (LEN OFF / ROT SPR), every control centred under
    #    its label. Labels are drawn by the widget in NanoVG (panel carries no baked
    #    text) using the IDENTICAL constants below — keep the two in lockstep. Only
    #    meaningful with a Macro visual attached; greyed otherwise.
    #
    #    SHARED LAYOUT CONSTANTS (mirror exactly in StraitsEastSandsVisual::draw):
    #      editor ends at ED_Y+ED_H=71; bottom art starts ~111. Post-inversion the
    #      Macro mix-in SENDS moved to the Macro panel — East keeps only the per-lane
    #      BASE opt-in latch (inherit Macro base / local East). So the band is short:
    #      a header + a centred latch per lane group.  BLEND_TOP=74  BLEND_H=22.
    BLEND_TOP = 74.0
    BLEND_H   = 22.0
    GAP       = 2.5               # tighter gap for 4 groups
    GROUP_W   = ED_W/4.0
    OWN_DY    = 13.0
    # "BASE" section header rule (full width, above the groups)
    A(f'<line x1="{px(ED_X):.1f}" y1="{px(BLEND_TOP-3.0):.1f}" x2="{px(ED_X+ED_W):.1f}" y2="{px(BLEND_TOP-3.0):.1f}" stroke="{t["accent"]}" stroke-width="1.0" opacity="0.6"/>')
    LANE_NAMES = LANE_NAMES_DISPLAY   # ["MELODY","OCTAVE","REST","ACCENT"]
    # Groups drawn in DISPLAY_ORDER (left-to-right: MEL/OCT/REST/ACC)
    # Group position g → engine lane DISPLAY_ORDER[g]
    OWN_XY = [None]*4  # indexed by engine lane
    for g in range(4):
        l = DISPLAY_ORDER[g]   # engine lane for this group position
        gx = ED_X + l*GROUP_W + GAP*0.5
        gw = GROUP_W - GAP
        gcx = gx + gw*0.5
        # demarked group box
        A(f'<rect x="{px(gx):.1f}" y="{px(BLEND_TOP):.1f}" width="{px(gw):.1f}" height="{px(BLEND_H):.1f}" rx="{px(1.4):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.92"/>')
        # header divider under the lane name
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(BLEND_TOP+7.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(BLEND_TOP+7.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        # opt-in (BASE) latch — centred under the header
        ox, oy = gcx, BLEND_TOP + OWN_DY
        A(f'<circle cx="{px(ox):.1f}" cy="{px(oy):.1f}" r="{px(2.6):.1f}" fill="{t["edrecess"]}" stroke="{t["accent"]}" stroke-width="1.2"/>')
        OWN_XY[l] = (ox, oy)
    A('</g>')
    # ── SvgPanelKit component layer. Indices mirror StraitsEastSandsVisual.hpp:
    #    cvId(r,c)=CV_START(0)+r*2+c  inputs 0..11;
    #    attenId(r,c)=ATTEN_START(3)+r*2+c  params 3..14;
    #    SPREAD_R/M/O = params 0/1/2. ──
    def kit_shape(kind, idx, x, y):
        A(f'<circle id="{kind}_{idx}" cx="{px(x):.2f}" cy="{px(y):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # Components placed in display order (row→engine lane via DISPLAY_ORDER).
    # cvId/attenDispId/SPREAD use engine lane index; rowY uses display row.
    for row in range(4):
        lane = DISPLAY_ORDER[row]   # engine lane
        y=rowY(row)
        for p,x in enumerate(JACK_X):  kit_shape("input", 0+lane*4+p, x, y)
        for p,x in enumerate(ATTEN_X): kit_shape("param", 4+lane*4+p, x, y)
        kit_shape("param", lane, SPREAD_X, y)  # SPREAD_R/M/O/A engine lane index
    # East opt-in (BASE) latch markers — engine lane indexed, at blend group position
    for g in range(4):
        l = DISPLAY_ORDER[g]
        ox,oy = OWN_XY[l]
        A(f'<circle id="param_owner_{l}" cx="{px(ox):.2f}" cy="{px(oy):.2f}" r="0.5" fill="none" stroke="none"/>')
    A(f'<circle id="light_connect" cx="{px(W_MM*0.5):.2f}" cy="{px(124.0):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for dark,name in [(True,"StraitsEastSandsVisual_40HP.svg"),(False,"StraitsEastSandsVisual_40HP_light.svg")]:
    svg=gen(dark)
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
