"""Macro (26HP) and Mono (40HP) Sands visual panels — dot.modular design language.
Uses shared dotmod_design helpers (palette, logo, MBS+waves motif, recesses).
nanosvg-safe: per-shape paint, no gradients/masks/text-for-controls."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

def gen_macro(dark, W_MM=213.36):   # 42HP (40 + 2HP right strip for poly prob-out jacks)
    # Macro mirrors the East visual's 40HP geometry exactly (same columns); it does
    # the same spread job but GLOBAL (3 lanes) rather than per-lane. Must match
    # StraitsSandsMacroVisual.hpp: COL_J1=8 J2=18 A1=30 A2=39 SPREAD_X=49 ED_X=58.
    t=theme(dark); H_MM=128.5; PW,PH=px(W_MM),px(H_MM)
    ROW_TOP,ROW_BOT,N=14.,108.,6
    def rowY(r): return ROW_TOP+(r+0.5)*(ROW_BOT-ROW_TOP)/N
    COL_J1,COL_J2,COL_A1,COL_A2=8.,18.,30.,39.
    SPREAD_X=49.                      # per-lane global spread trimpot column
    # Extra top margin so the view-tab row isn't crammed against the panel top
    # edge. 0.5 cm = 5 mm. Mirror TAB_TOP_OFFSET_MM in StraitsSandsMacroVisualWidget.
    TAB_TOP_OFFSET_MM = 5.0
    ED_X=58.; PROB_OUT_X=207.; ED_W=PROB_OUT_X-ED_X-8.; ED_Y=18.+TAB_TOP_OFFSET_MM; ED_H=48.
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    A(D.mbs(W_MM-72.0, 110.0, 60.0, 14.0, t, op=0.85))
    A(D.waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=W_MM-ED_X-2))
    A(D.accent_rules(PW,t))
    gx,gy=4.0,ROW_TOP-4.0; gw,gh=(SPREAD_X+5.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(COL_J2+COL_A1)))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=3))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=11.0, y_mm=4.5, target_w_mm=42.0))
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for r in range(6):
        y=rowY(r)
        A(D.jack(COL_J1,y,t)); A(D.jack(COL_J2,y,t))
        A(D.trim(COL_A1,y,t,t["gold"])); A(D.trim(COL_A2,y,t,t["gold"]))
    # Per-lane global SPREAD trimpots (REST/MEL/OCT), lane-centred, at SPREAD_X.
    for lane in range(3):
        y=0.5*(rowY(lane*2)+rowY(lane*2+1))
        A(D.trim(SPREAD_X,y,t,t["wellring"]))
    # ── Macro→voice MIX-IN send groups (relocated from East under the control
    #    inversion). 3 demarked groups (REST/MEL/OCT) below the editor, each a 2×2
    #    Len/Off/Rot/Spr send grid. "per voice, how much of Macro's global CV reaches
    #    this voice." Geometry shared with the widget labels in
    #    StraitsSandsMacroVisual::draw — keep in lockstep:
    #      BLEND_TOP=72 BLEND_H=36 GAP=3.5 SEND_Y0=12 SEND_DY=11 SEND_DX=7
    BLEND_TOP=72.0; BLEND_H=36.0; BGAP=3.5; GROUP_W=ED_W/3.0
    SEND_Y0=12.0; SEND_DY=11.0; SEND_DX=7.0
    A(f'<line x1="{px(ED_X):.1f}" y1="{px(BLEND_TOP-3.0):.1f}" x2="{px(ED_X+ED_W):.1f}" y2="{px(BLEND_TOP-3.0):.1f}" stroke="{t["accent"]}" stroke-width="1.0" opacity="0.6"/>')
    MIX_XY=[]
    for l in range(3):
        gx=ED_X+l*GROUP_W+BGAP*0.5; gw=GROUP_W-BGAP; gcx=gx+gw*0.5
        A(f'<rect x="{px(gx):.1f}" y="{px(BLEND_TOP):.1f}" width="{px(gw):.1f}" height="{px(BLEND_H):.1f}" rx="{px(1.4):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.92"/>')
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(BLEND_TOP+7.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(BLEND_TOP+7.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        lane_sends=[]
        for item in range(4):
            cxs=gcx+(-SEND_DX if (item%2)==0 else SEND_DX)
            cys=BLEND_TOP+SEND_Y0+(item//2)*SEND_DY
            A(D.trim(cxs,cys,t,t["gold"]))
            lane_sends.append((cxs,cys))
        MIX_XY.append(lane_sends)
    A('</g>')
    # ── SvgPanelKit component layer: named markers at every control centre, so a
    #    widget can bind by id later. Indices mirror StraitsSandsMacroVisual.hpp:
    #    cvId(r,c)=CV_START(0)+r*2+c (inputs), attenId(r,c)=ATTEN_START(3)+r*2+c
    #    (params), SPREAD_REST/MEL/OCT = 0/1/2 (params). ──
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    for r in range(6):
        y=rowY(r)
        A(D.kit_shape("input", 0+r*2+0, COL_J1, y))   # cvId(r,0)
        A(D.kit_shape("input", 0+r*2+1, COL_J2, y))   # cvId(r,1)
        A(D.kit_shape("param", 3+r*2+0, COL_A1, y))   # attenId(r,0)
        A(D.kit_shape("param", 3+r*2+1, COL_A2, y))   # attenId(r,1)
    for lane in range(3):
        y=0.5*(rowY(lane*2)+rowY(lane*2+1))
        A(D.kit_shape("param", lane, SPREAD_X, y))    # SPREAD_REST/MELODY/OCTAVE = 0/1/2
    # Macro→voice mix-in send markers (bound to sendDispId display proxies).
    for l in range(3):
        for item in range(4):
            cxs,cys = MIX_XY[l][item]
            A(f'<circle id="param_send_{l}_{item}" cx="{px(cxs):.2f}" cy="{px(cys):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

def gen_mono(dark):
    t=theme(dark); W_MM,H_MM=213.36,128.5; PW,PH=px(W_MM),px(H_MM)   # 42HP (+2HP prob-out strip)
    ROW_TOP,ROW_BOT,N=14.,108.,6
    def laneY(l): return ROW_TOP+(l+0.5)*(ROW_BOT-ROW_TOP)/N
    # Geometry MUST match MonsoonSandsVisualExpander.hpp:
    #   JACK_X={6,15,24}  ATTEN_X={34,43,52}  (all 6 lanes)
    #   spread (lanes 0-2 REST/MEL/OCT): SPR_BASE_X=62, SPR_CV_X=71, SPR_ATTEN_X=80
    JACK_X=[6.,15.,24.]; ATTEN_X=[34.,43.,52.]
    SPR_BASE_X,SPR_CV_X,SPR_ATTEN_X=62.,71.,80.
    N_SPREAD=4                                  # REST/MEL/OCT + ACCENT (poly lanes)
    SPR_TO_EDITOR=[0,1,2,4]                      # spread index → editor lane (accent skips LEG)
    ED_X=88.; PROB_OUT_X=207.; ED_W=PROB_OUT_X-ED_X-8.; ED_Y=18.; ED_H=ROW_BOT-ED_Y
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
    for sidx in range(N_SPREAD):
        y=laneY(SPR_TO_EDITOR[sidx])
        A(D.trim(SPR_BASE_X,y,t,t["wellring"]))
        A(D.jack(SPR_CV_X,y,t))
        A(D.trim(SPR_ATTEN_X,y,t,t["gold"]))
    A('</g>')
    # ── SvgPanelKit component layer. Indices mirror MonsoonSandsVisualExpander.hpp:
    #    CV jacks   cvId(lane,p)   = CV_START(0)  + lane*3 + p   inputs 0..17
    #    attens     attenId(lane,p)= ATTEN_START(21)+ lane*3 + p  params 21..38
    #    spread base SPR_REST/MEL/OCT = params 18..20
    #    spread CV   SPR_CV_START(18) + l         = inputs 18..20
    #    spread atten SPR_ATTEN_START(39) + l     = params 39..41
    #    (LEN/OFF/ROT params 0-17 have no physical knob — editor-driven — so no marker.) ──
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    for lane in range(6):
        y=laneY(lane)
        for p,x in enumerate(JACK_X):  A(D.kit_shape("input", 0+lane*3+p, x, y))
        for p,x in enumerate(ATTEN_X): A(D.kit_shape("param", 21+lane*3+p, x, y))
    for sidx in range(N_SPREAD):
        y=laneY(SPR_TO_EDITOR[sidx])
        A(D.kit_shape("param", 18+sidx, SPR_BASE_X, y))   # SPR_REST/MEL/OCT/ACCENT (18..21)
        A(D.kit_shape("input", 18+sidx, SPR_CV_X, y))     # SPR_CV (18..21)
        A(D.kit_shape("param", 39+sidx, SPR_ATTEN_X, y))  # SPR_ATTEN (39..42)
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for fn,base in [(gen_macro,"StraitsSandsMacroVisual_40HP"),(gen_mono,"SandsMonoVisual_40HP")]:
    for dark,suf in [(True,""),(False,"_light")]:
        svg=fn(dark); name=f"{base}{suf}.svg"
        with open(f"res/panels/{name}","w") as f: f.write(svg)
        print(f"{name}: {len(svg):,} bytes")
