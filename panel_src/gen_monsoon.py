#!/usr/bin/env python3
"""Monsoon — main sequencer panel (40HP), FROM-SCRATCH generator.  [WIP]

Replaces the hand-authored res/panels/Monsoon_panel_*_monsoon.svg, which is unfixable in place:
it uses <pattern> + 2 <linearGradient> + 1 <radialGradient> behind 4 `fill="url(#…)"` refs, and
**nanosvg renders none of them** — so in Rack the Supertrees, sky, grain and the knob "shading"
(ringGlow) are all drawn wrong. See docs/design/MONSOON_PANEL_CLEANUP.md.

Structure (the macro; detail iterates later):
  VERTICAL   = supertree trunks — bay dividers between control groups
  HORIZONTAL = skyway rails — which ARE the note-fader guide lines, emitted from the SAME loop
               as the fader positions, so they cannot drift (kills the old alignment bug)
  Singapore Flyer = the 16-step LED ring (a 16-point ring should be a wheel), at its EXISTING
               position (162, 30) r=14mm — nothing relocates. FLYER=False falls back to a bare ring.

Everything is solid fills/strokes: no gradient, mask, text, pattern or url(). Supertrees come from
panel_src/supertree.py (rules measured off master; one free param `rx` per tree).

WRITES TO *_GENERATED.svg by default so it can never clobber the hand-made panel. Compare renders,
then switch the widget over.

Kit id markers: param_*, input_*, output_*, light_* — widget binds by name.
"""
import math, random, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from supertree import supertree, THEME_DARK as TREE_DARK, THEME_LIGHT as TREE_LIGHT

HP, H_MM = 40, 128.5
W_MM = HP * 5.08                       # 203.2
S    = 767.99 / W_MM                   # 3.7795 px/mm (master canvas)
PW, PH = round(W_MM*S,2), round(H_MM*S,2)
def px(v): return round(v*S, 2)

FLYER = True
FCX, FCY, FR = 162.0, 30.0, 14.0       # existing 16-step ring (mm) — UNMOVED
FADER_X = [7.5 + i*9.0 for i in range(12)]
OCT_X   = [119.0, 128.0]
FADER_Y, FADER_H = 59.75, 34.0
ROWS    = [88.5, 97.5, 106.5]
GROUND  = 122.0
MODE_XY = (197.0, 12.0)

# grove: (cx_mm, rx_mm) — trunks divide the bottom rows; kept clear of the fader field
GROVE = [(18.0, 5.6), (44.0, 4.0), (76.0, 6.2), (140.0, 4.4), (186.0, 5.0)]

TH = {
 "dark": dict(bg="#0a0e14", sky="#101725", band="#161f2e", ground="#0c1017",
   steel="#5b7490", steel_hi="#93aec8", cable="#44596e", rain="#3a4d61",
   rail="#6f8aa6", rail_hi="#a3bcd4", post="#5b7490",
   well="#05070a", ring="#8ba2b8", ochre="#d99a2b",
   red="#ff1030", teal="#2fd4c4", topbar="#d4001a", tree=TREE_DARK),
 "light": dict(bg="#e9e7dd", sky="#dedcd1", band="#cfcdc0", ground="#b6b4a6",
   steel="#5a6672", steel_hi="#2f3841", cable="#8d99a4", rain="#9aa5b0",
   rail="#4a5661", rail_hi="#2e3840", post="#5a6672",
   well="#f6f4ed", ring="#4b5764", ochre="#a06a12",
   red="#c1001a", teal="#12796f", topbar="#d4001a", tree=TREE_LIGHT),
}

def gen(dark, ph=5):
    t = TH["dark" if dark else "light"]
    o=[]; A=o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    # ── strata (weather section) ──
    for y0,y1,c in [(0,20,t["sky"]),(20,44,t["band"]),(44,84,t["sky"]),(84,GROUND,t["band"])]:
        A(f'<rect x="0" y="{px(y0)}" width="{PW}" height="{px(y1-y0)}" fill="{c}"/>')
    A(f'<rect x="0" y="{px(GROUND)}" width="{PW}" height="{px(H_MM-GROUND)}" fill="{t["ground"]}"/>')

    # ── rain: steep dashed strokes, master's angle; sky band only (not a wash) ──
    random.seed(11)
    for _ in range(30):
        x=random.random()*W_MM; y=random.random()*40
        A(f'<line x1="{px(x)}" y1="{px(y)}" x2="{px(x+2.4)}" y2="{px(y+15)}" stroke="{t["rain"]}" '
          f'stroke-width="0.4" stroke-opacity="0.35" stroke-dasharray="{px(1.3)},{px(1.0)}"/>')

    # ── Singapore Flyer = the 16-step ring ──
    if FLYER:
        for sx in (-1,1):
            xt,xb = FCX+sx*8.5, FCX+sx*17.0
            yt,yb = FCY+FR*0.86, 78.0
            n=14
            for k in range(n):
                f0,f1=k/n,(k+1)/n
                op=max(0.0, 0.95-(f0**1.35)*1.1)
                if op<=0.03: break
                A(f'<line x1="{px(xt+(xb-xt)*f0)}" y1="{px(yt+(yb-yt)*f0)}" '
                  f'x2="{px(xt+(xb-xt)*f1)}" y2="{px(yt+(yb-yt)*f1)}" '
                  f'stroke="{t["steel"]}" stroke-width="1.2" stroke-opacity="{op:.2f}"/>')
        for i in range(32):
            a=i/32*2*math.pi-math.pi/2
            A(f'<line x1="{px(FCX)}" y1="{px(FCY)}" x2="{px(FCX+FR*0.965*math.cos(a))}" '
              f'y2="{px(FCY+FR*0.965*math.sin(a))}" stroke="{t["cable"]}" stroke-width="0.3" stroke-opacity="0.75"/>')
        for rr,wd in ((FR,0.85),(FR*0.93,0.4)):
            A(f'<circle cx="{px(FCX)}" cy="{px(FCY)}" r="{px(rr)}" fill="none" stroke="{t["steel_hi"]}" stroke-width="{wd}"/>')
        A(f'<circle cx="{px(FCX)}" cy="{px(FCY)}" r="{px(1.9)}" fill="{t["steel"]}" stroke="{t["steel_hi"]}" stroke-width="0.4"/>')
        for i in range(16):
            a=i/16*2*math.pi-math.pi/2
            A(f'<circle cx="{px(FCX+FR*math.cos(a))}" cy="{px(FCY+FR*math.sin(a))}" r="{px(1.9)}" '
              f'fill="{t["well"]}" stroke="{t["steel_hi"]}" stroke-width="0.4"/>')
    else:
        A(f'<circle cx="{px(FCX)}" cy="{px(FCY)}" r="{px(FR)}" fill="none" stroke="{t["ring"]}" stroke-width="0.6"/>')
    for i in range(16):
        a=i/16*2*math.pi-math.pi/2
        A(f'<circle id="light_step_{i}" cx="{px(FCX+FR*math.cos(a))}" cy="{px(FCY+FR*math.sin(a))}" '
          f'r="0.5" fill="none" stroke="none"/>')

    # ── supertree grove (parametric; from measured master rules) ──
    for cx,rx in GROVE:
        for el in supertree(px(cx), px(GROUND), px(rx), t["tree"]):
            A(el)

    # ── skyway rails ACROSS the fader field = the note-fader guide lines (same loop → no drift) ──
    top,bot = FADER_Y-FADER_H/2, FADER_Y+FADER_H/2
    sx0,sx1 = FADER_X[0]-4.5, FADER_X[-1]+4.5
    for pxp in (sx0,sx1):
        A(f'<line x1="{px(pxp)}" y1="{px(top-3)}" x2="{px(pxp)}" y2="{px(bot+3)}" stroke="{t["post"]}" stroke-width="1.0"/>')
    for lvl in range(6):
        gy=top+(bot-top)*lvl/5
        edge = lvl in (0,5)
        for a,b in ((sx0,sx1),(OCT_X[0]-4.0,OCT_X[-1]+4.0)):
            A(f'<line x1="{px(a)}" y1="{px(gy)}" x2="{px(b)}" y2="{px(gy)}" stroke="{t["rail"]}" '
              f'stroke-width="{0.55 if edge else 0.38}" stroke-opacity="{0.95 if edge else 0.5}"/>')
    for k in range(int((sx1-sx0)/4.0)+1):
        bx=sx0+k*4.0
        A(f'<line x1="{px(bx)}" y1="{px(top)}" x2="{px(bx)}" y2="{px(top+2.2)}" stroke="{t["rail"]}" stroke-width="0.28" stroke-opacity="0.45"/>')

    # ── fader wells + markers ──
    for i,fx in enumerate(FADER_X):
        A(f'<rect x="{px(fx-1.6)}" y="{px(top)}" width="{px(3.2)}" height="{px(bot-top)}" rx="{px(0.6)}" '
          f'fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<circle id="param_semi_{i}" cx="{px(fx)}" cy="{px(FADER_Y)}" r="0.5" fill="none" stroke="none"/>')
    for i,fx in enumerate(OCT_X):
        A(f'<rect x="{px(fx-1.6)}" y="{px(top)}" width="{px(3.2)}" height="{px(bot-top)}" rx="{px(0.6)}" '
          f'fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<circle id="param_oct_{"lo" if i==0 else "hi"}" cx="{px(fx)}" cy="{px(FADER_Y)}" r="0.5" fill="none" stroke="none"/>')

    # ── bottom control rows, in the bays between trunks (NO recess boxes) ──
    bays=[(8,34),(52,68),(84,132),(146,180)]
    n=0
    for ry in ROWS:
        for bx0,bx1 in bays:
            cnt=max(2,int((bx1-bx0)/13))
            for k in range(cnt):
                cx=bx0+(bx1-bx0)*(k+0.5)/cnt
                A(f'<circle cx="{px(cx)}" cy="{px(ry)}" r="{px(3.2)}" fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.55"/>')
                A(f'<circle id="ctl_{n}" cx="{px(cx)}" cy="{px(ry)}" r="0.5" fill="none" stroke="none"/>'); n+=1

    # ── mode row: A B C D E  (E was MISSING on the old panel) ──
    for i,lab in enumerate("ABCDE"):
        mx = MODE_XY[0]-  (4-i)*0.0
        my = MODE_XY[1] + i*4.6
        A(f'<circle cx="{px(MODE_XY[0])}" cy="{px(my)}" r="{px(1.5)}" fill="{t["well"]}" stroke="{t["ochre"]}" stroke-width="0.45"/>')
        A(f'<circle id="light_mode_{lab}" cx="{px(MODE_XY[0])}" cy="{px(my)}" r="0.5" fill="none" stroke="none"/>')
    A(f'<circle id="param_mode" cx="{px(MODE_XY[0])}" cy="{px(MODE_XY[1]-5.0)}" r="0.5" fill="none" stroke="none"/>')

    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["topbar"]}"/>')
    A('</svg>')
    return "\n".join(o)

def main():
    out=os.path.join(os.path.dirname(__file__),"..","res","panels")
    for dark,name in [(True,"Monsoon_panel_dark_GENERATED.svg"),(False,"Monsoon_panel_light_GENERATED.svg")]:
        svg=gen(dark)
        open(os.path.join(out,name),"w").write(svg)
        bad=sum(svg.count(x) for x in ['gradient','<mask','<text','url(','<pattern'])
        print(f"{name}  ({HP}HP {PW}x{PH}px)  forbidden={bad}")

if __name__=="__main__":
    main()
