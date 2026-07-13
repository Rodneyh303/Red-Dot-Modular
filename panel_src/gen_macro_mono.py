"""Macro (26HP) and Mono (40HP) Sands visual panels — dot.modular design language.
Uses shared dotmod_design helpers (palette, logo, MBS+waves motif, recesses).
nanosvg-safe: per-shape paint, no gradients/masks/text-for-controls."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

def gen_macro(dark, W_MM=218.44):   # 43HP (42 + 1HP for the per-lane owner-source block)
    # Macro mirrors the East visual's 40HP geometry exactly (same columns); it does
    # the same spread job but GLOBAL (3 lanes) rather than per-lane. Must match
    # StraitsSandsMacroVisual.hpp: COL_J1=8 J2=18 A1=30 A2=39 SPREAD_X=49 ED_X=58.
    t=theme(dark); H_MM=128.5; PW,PH=px(W_MM),px(H_MM)
    N=4   # 4 lanes, one row each
    # Extra top margin so the view-tab row isn't crammed against the panel top
    # edge. 0.5 cm = 5 mm. Mirror TAB_TOP_OFFSET_MM in StraitsSandsMacroVisualWidget.
        # Mirrors src/ui/SandsGrid.hpp — tabs sit ABOVE the grid (3..13mm), lane 0 starts at 14.
    TAB_TOP, TAB_ROW_H = 3.0, 5.0
    TAB_TOP_OFFSET_MM = 5.0
    # Mirrors src/ui/SandsGrid.hpp: lane 0 at 14mm, 4 lanes x 14mm = 56 (tabs live above, 3..13).
    ED_X=88.; ED_W=111.; OWNER_X=205.; PROB_OUT_X=212.; ED_Y=14.; ED_H=56.
    ED_LANE_H=ED_H/N
    # Left-control rows align with the EDITOR lane centres (must match the hpp's rowY).
    def rowY(r): return ED_Y+(r+0.5)*ED_LANE_H
    # Display order: row i → engine lane (MEL/OCT/REST/ACC top-to-bottom)
    DISPLAY_ORDER=[1,2,0,3]   # row0=MEL(eng1), row1=OCT(eng2), row2=REST(eng0), row3=ACC(eng3)
    LANE_NAMES_D=["MELODY","OCTAVE","REST","ACCENT"]
    # 4 CV jacks + 4 attens + spread base — columns match SandsMonoVisual, ED_X=88
    JACK_X=[6.,15.,24.,33.]            # LEN/OFF/ROT/SPR-cv
    ATTEN_X=[43.,52.,61.,70.]          # LEN/OFF/ROT/SPR depth
    SPREAD_X=80.                       # per-lane spread base trimpot
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    # Identity artwork in the BOTTOM-LEFT corner (vs East's lower-right) so the
    # two near-identical 42HP panels read apart at a glance. Bottom-left is free
    # on Macro (send grids live in the right section).
    A(D.helix_sands(4.0, 70.0, 74.0, 52.0, t, op=0.95))   # Sands Helix hero mark, bottom-left pocket
    A(D.mbs(6.0, 110.0, 60.0, 14.0, t, op=0.85))
    A(D.accent_rules(PW,t))
    gx,gy=1.5,rowY(0)-ED_LANE_H*0.5-3.0; gw,gh=(SPREAD_X+6.0)-gx,(rowY(N-1)+ED_LANE_H*0.5+3.0)-gy  # gx clears leftmost jack
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(JACK_X[-1]+ATTEN_X[0])))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=4))
    A(D.owner_block(OWNER_X, [rowY(r) for r in range(N)], ED_X+ED_W, t, cell_w_mm=6.0))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=11.0, y_mm=4.5, target_w_mm=42.0))
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for row in range(4):
        y=rowY(row)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
        A(D.trim(SPREAD_X,y,t,t["wellring"]))
    # ── Macro→voice MIX-IN send groups (relocated from East under the control
    #    inversion). 3 demarked groups (REST/MEL/OCT) below the editor, each a 2×2
    #    Len/Off/Rot/Spr send grid. "per voice, how much of Macro's global CV reaches
    #    this voice." Geometry shared with the widget labels in
    #    StraitsSandsMacroVisual::draw — keep in lockstep:
    #      BLEND_TOP=72 BLEND_H=36 GAP=3.5 SEND_Y0=12 SEND_DY=11 SEND_DX=7
    BLEND_TOP=72.0; BLEND_H=47.0; BGAP=2.5; GROUP_W=ED_W/4.0  # 4 groups; taller for the tap row 3
    SEND_Y0=12.0; SEND_DY=11.0; SEND_DX=6.0                   # DX 7→6 for narrower groups
    TAP_ROW_DY=11.0                                            # row 3 (taps) below the 2 send rows
    A(f'<line x1="{px(ED_X):.1f}" y1="{px(BLEND_TOP-3.0):.1f}" x2="{px(ED_X+ED_W):.1f}" y2="{px(BLEND_TOP-3.0):.1f}" stroke="{t["accent"]}" stroke-width="1.0" opacity="0.6"/>')
    # Blend groups drawn in display order (left-to-right: MEL/OCT/REST/ACC)
    MIX_XY=[None]*4   # indexed by engine lane
    TAP_XY=[None]*4   # P9b: [LOR tap, spread tap] per engine lane
    for g in range(4):
        l=DISPLAY_ORDER[g]   # engine lane
        gx=ED_X+g*GROUP_W+BGAP*0.5; gw=GROUP_W-BGAP; gcx=gx+gw*0.5
        A(f'<rect x="{px(gx):.1f}" y="{px(BLEND_TOP):.1f}" width="{px(gw):.1f}" height="{px(BLEND_H):.1f}" rx="{px(1.4):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.92"/>')
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(BLEND_TOP+7.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(BLEND_TOP+7.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        lane_sends=[]
        for item in range(4):
            cxs=gcx+(-SEND_DX if (item%2)==0 else SEND_DX)
            cys=BLEND_TOP+SEND_Y0+(item//2)*SEND_DY
            A(D.trim(cxs,cys,t,t["gold"]))
            lane_sends.append((cxs,cys))
        MIX_XY[l]=lane_sends
        # P9b: row 3 = the two CV taps for this lane group — LOR (left) + SPREAD (right),
        # full-size trimpots. A faint divider separates them from the sends above.
        tap_y = BLEND_TOP+SEND_Y0+2*TAP_ROW_DY
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(tap_y-5.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(tap_y-5.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        A(D.trim(gcx-SEND_DX, tap_y, t, t["wellring"]))   # LOR tap
        A(D.trim(gcx+SEND_DX, tap_y, t, t["wellring"]))   # spread tap
        TAP_XY[l]=[(gcx-SEND_DX,tap_y),(gcx+SEND_DX,tap_y)]
    A('</g>')
    # ── SvgPanelKit component layer: named markers at every control centre, so a
    #    widget can bind by id later. Indices mirror StraitsSandsMacroVisual.hpp:
    #    cvId(r,c)=CV_START(0)+r*2+c (inputs), attenId(r,c)=ATTEN_START(3)+r*2+c
    #    (params), SPREAD_REST/MEL/OCT = 0/1/2 (params). ──
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # Components in display order; engine lane from DISPLAY_ORDER.
    for row in range(4):
        lane=DISPLAY_ORDER[row]   # engine lane
        y=rowY(row)
        for p,x in enumerate(JACK_X):  A(D.kit_shape("input", 0+lane*4+p, x, y))
        for p,x in enumerate(ATTEN_X): A(D.kit_shape("param", 4+lane*4+p, x, y))
        A(D.kit_shape("param", lane, SPREAD_X, y))  # SPREAD engine lane
        # (P9b: the in-row per-lane tap was removed — taps now live as a 3rd row in the
        # send groups below the lanes; see param_taplor_/param_tapspr_ markers there.)
        # poly probability CV out — right strip, at this lane's row (engine lane = PROB_OUT_REST+lane)
        A(D.kit_shape("output", lane, PROB_OUT_X, y))
    # Macro→voice mix-in send markers (bound to sendDispId display proxies).
    for g in range(4):
        l=DISPLAY_ORDER[g]   # engine lane
        for item in range(4):
            cxs,cys = MIX_XY[l][item]
            A(f'<circle id="param_send_{l}_{item}" cx="{px(cxs):.2f}" cy="{px(cys):.2f}" r="0.5" fill="none" stroke="none"/>')
        # P9b: the two CV-tap markers for this lane group (LOR, spread).
        (lx,ly),(sx,sy) = TAP_XY[l]
        A(f'<circle id="param_taplor_{l}" cx="{px(lx):.2f}" cy="{px(ly):.2f}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="param_tapspr_{l}" cx="{px(sx):.2f}" cy="{px(sy):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

def gen_mono(dark):
    t=theme(dark); W_MM,H_MM=218.44,128.5; PW,PH=px(W_MM),px(H_MM)   # 43HP (42 + 1HP owner block)
    # Mirrors src/ui/SandsGrid.hpp: 6 lanes x 14mm from 14 → bottom 98 (was 108, laneH 15.667).
    ROW_TOP,ROW_BOT,N=14.,98.,6
    def laneY(l): return ROW_TOP+(l+0.5)*(ROW_BOT-ROW_TOP)/N
    # Geometry MUST match MonsoonSandsVisualExpander.hpp:
    #   JACK_X={6,15,24}  ATTEN_X={34,43,52}  (all 6 lanes)
    #   spread (lanes 0-2 REST/MEL/OCT): SPR_BASE_X=62, SPR_CV_X=71, SPR_ATTEN_X=80
    JACK_X=[6.,15.,24.]; ATTEN_X=[34.,43.,52.]
    SPR_BASE_X,SPR_CV_X,SPR_ATTEN_X=62.,71.,80.
    N_SPREAD=4                                  # REST/MEL/OCT + ACCENT (poly lanes)
    SPR_TO_EDITOR=[2,0,1,3]                      # spread index (REST/MEL/OCT/ACCENT) → editor lane; matches cpp ENGINE_LANE_TO_EDITOR
    ED_X=88.; ED_W=111.; OWNER_X=205.; PROB_OUT_X=212.  # +1HP owner block; ED_W decoupled from PROB_OUT_X
    # Editor recess spans the SAME band the left controls (laneY) divide, so the
    # live editor lanes (zero internal padding, even division) line up with the
    # left jacks/attens and the painted lanes.
    ED_Y=ROW_TOP; ED_H=ROW_BOT-ROW_TOP
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    A(D.mbs(W_MM-72.0, 110.0, 60.0, 14.0, t, op=0.85))
    A(D.waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=W_MM-ED_X-2))
    A(D.accent_rules(PW,t))
    # Input group box framing the LOR jacks + attenuverters (x 6..52), with a
    # separator between the jack cluster and the attenuverter cluster.
    gx,gy=1.5,ROW_TOP-4.0; gw,gh=(ATTEN_X[-1]+6.0)-gx,(ROW_BOT+2.0)-(ROW_TOP-4.0)  # gx clears leftmost jack
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(JACK_X[-1]+ATTEN_X[0])))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=6))
    A(D.owner_block(OWNER_X, [laneY(l) for l in range(4)], ED_X+ED_W, t, cell_w_mm=(ED_W-2*6.0)/16.0, draw_cells=False))
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
    # Physical rows are laid out in EDITOR display order (MELODY/OCTAVE/REST/
    # ACCENT/VARIATION/LEGATO = editor lanes 0..5), matching the editor lanes +
    # labels that share these rows. The LOR params are now EDITOR-ordered
    # (LEN_MELODY=0, OCT, REST, ACC, VAR, LEG) — same as the display rows — so we
    # bind each row's jacks/attens at the editor index directly (no remap).
    for row in range(6):
        y=laneY(row)
        for p,x in enumerate(JACK_X):  A(D.kit_shape("input", 0+row*3+p, x, y))
        for p,x in enumerate(ATTEN_X): A(D.kit_shape("param", 22+row*3+p, x, y))
    for sidx in range(N_SPREAD):
        y=laneY(SPR_TO_EDITOR[sidx])
        A(D.kit_shape("param", 18+sidx, SPR_BASE_X, y))   # SPR_REST/MEL/OCT/ACCENT (18..21, engine order)
        A(D.kit_shape("input", 18+sidx, SPR_CV_X, y))     # SPR_CV (18..21)
        A(D.kit_shape("param", 40+sidx, SPR_ATTEN_X, y))  # SPR_ATTEN (40..43)
    A('</g>')
    A('</svg>')
    return "\n".join(L)

for fn,base in [(gen_macro,"StraitsSandsMacroVisual_40HP"),(gen_mono,"SandsMonoVisual_40HP")]:
    for dark,suf in [(True,""),(False,"_light")]:
        svg=fn(dark); name=f"{base}{suf}.svg"
        with open(f"res/panels/{name}","w") as f: f.write(svg)
        print(f"{name}: {len(svg):,} bytes")
