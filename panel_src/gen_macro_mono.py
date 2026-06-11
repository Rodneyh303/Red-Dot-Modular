"""Macro (26HP) and Mono (40HP) Sands visual panels — dot.modular design language.
Uses shared dotmod_design helpers (palette, logo, MBS+waves motif, recesses).
nanosvg-safe: per-shape paint, no gradients/masks/text-for-controls."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

def gen_macro(dark, W_MM=132.08):
    t=theme(dark); H_MM=128.5; PW,PH=px(W_MM),px(H_MM)
    ROW_TOP,ROW_BOT,N=14.,108.,6
    def rowY(r): return ROW_TOP+(r+0.5)*(ROW_BOT-ROW_TOP)/N
    COL_J1,COL_J2,COL_A1,COL_A2=6.,14.,23.,32.
    ED_X=39.; ED_W=W_MM-ED_X-4.; ED_Y=18.; ED_H=48.
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    A(D.mbs(W_MM-60.0, 110.0, 50.0, 14.0, t, op=0.85))
    A(D.waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=W_MM-ED_X-2))
    A(D.accent_rules(PW,t))
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(COL_A2+5.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(COL_J2+COL_A1)))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=3))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=10.0, y_mm=4.5, target_w_mm=34.0))
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for r in range(6):
        y=rowY(r)
        A(D.jack(COL_J1,y,t)); A(D.jack(COL_J2,y,t))
        A(D.trim(COL_A1,y,t,t["gold"])); A(D.trim(COL_A2,y,t,t["gold"]))
    A('</g>')
    A('</svg>')
    return "\n".join(L)

def gen_mono(dark):
    t=theme(dark); W_MM,H_MM=203.2,128.5; PW,PH=px(W_MM),px(H_MM)
    ROW_TOP,ROW_BOT,N=14.,108.,6
    def laneY(l): return ROW_TOP+(l+0.5)*(ROW_BOT-ROW_TOP)/N
    # Geometry MUST match MonsoonSandsVisualExpander.hpp:
    #   JACK_X={6,15,24}  ATTEN_X={34,43,52}  (all 6 lanes)
    #   spread (lanes 0-2 REST/MEL/OCT): SPR_BASE_X=62, SPR_CV_X=71, SPR_ATTEN_X=80
    JACK_X=[6.,15.,24.]; ATTEN_X=[34.,43.,52.]
    SPR_BASE_X,SPR_CV_X,SPR_ATTEN_X=62.,71.,80.
    N_SPREAD=3
    ED_X=88.; ED_W=W_MM-ED_X-4.; ED_Y=18.; ED_H=ROW_BOT-ED_Y
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    A(D.mbs(W_MM-72.0, 110.0, 60.0, 14.0, t, op=0.85))
    A(D.waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=W_MM-ED_X-2))
    A(D.accent_rules(PW,t))
    # Input group box framing the LOR jacks + attenuverters (x 6..52), with a
    # separator between the jack cluster and the attenuverter cluster.
    gx,gy=2.0,ROW_TOP-4.0; gw,gh=(ATTEN_X[-1]+6.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(JACK_X[-1]+ATTEN_X[0])))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=6))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=11.0, y_mm=4.5, target_w_mm=42.0))
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for lane in range(6):
        y=laneY(lane)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
    for lane in range(N_SPREAD):
        y=laneY(lane)
        A(D.trim(SPR_BASE_X,y,t,t["wellring"]))
        A(D.jack(SPR_CV_X,y,t))
        A(D.trim(SPR_ATTEN_X,y,t,t["gold"]))
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for fn,base in [(gen_macro,"StraitsSandsMacroVisual_26HP"),(gen_mono,"SandsMonoVisual_40HP")]:
    for dark,suf in [(True,""),(False,"_light")]:
        svg=fn(dark); name=f"{base}{suf}.svg"
        with open(f"res/panels/{name}","w") as f: f.write(svg)
        print(f"{name}: {len(svg):,} bytes")

# Enlarged macro matching the East visual's width (40HP / 203.2mm), so the macro
# editor gets the same generous footprint as East — they share controls and only
# differ global-vs-per-channel.
for dark,suf in [(True,""),(False,"_light")]:
    svg=gen_macro(dark, W_MM=203.2); name=f"StraitsSandsMacroVisual_40HP{suf}.svg"
    with open(f"res/panels/{name}","w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes (wide, East-sized)")
