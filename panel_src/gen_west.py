"""Straits West Sands visual panel (36HP) — dot.modular design language.
West mirrors East's layout (shares StraitsEastVisualIds constants) at 36HP width.
Uses shared dotmod_design helpers. nanosvg-safe."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

# 36HP. East layout constants (shared via StraitsEastVisualIds in C++).
W_MM, H_MM = 182.88, 128.5
PW, PH = px(W_MM), px(H_MM)
ROW_TOP, ROW_BOT, N = 14.0, 108.0, 6
def rowY(r): return ROW_TOP + (r+0.5)*(ROW_BOT-ROW_TOP)/N
COL_J1, COL_J2, COL_A1, COL_A2, SPREAD_X = 8.0, 18.0, 30.0, 39.0, 49.0
ED_X, ED_Y = 58.0, 18.0
ED_W = W_MM - ED_X - 4.0
ED_H = 48.0

def gen(dark):
    t=theme(dark); L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    A(D.mbs(W_MM-58.0, 96.0, 52.0, 18.0, t, op=0.8))
    A(D.waves(ED_X, 116.0, t, op=0.7, rows=3, span_mm=W_MM-ED_X-2))
    A(D.accent_rules(PW,t))
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(SPREAD_X+5.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(COL_J2+COL_A1)))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=3))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=11.0, y_mm=4.5, target_w_mm=40.0))
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for r in range(6):
        y=rowY(r)
        A(D.jack(COL_J1,y,t)); A(D.jack(COL_J2,y,t))
        A(D.trim(COL_A1,y,t,t["gold"])); A(D.trim(COL_A2,y,t,t["gold"]))
    for lane in range(3):
        y=0.5*(rowY(lane*2)+rowY(lane*2+1)); A(D.trim(SPREAD_X,y,t,t["teal"]))
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for dark,suf in [(True,""),(False,"_light")]:
    svg=gen(dark); name=f"StraitsWestSandsVisual_36HP{suf}.svg"
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
