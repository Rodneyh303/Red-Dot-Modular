"""Macro (26HP) and Mono (40HP) Sands visual panels — dot.modular design language.
Uses shared dotmod_design helpers (palette, logo, MBS+waves motif, recesses).
nanosvg-safe: per-shape paint, no gradients/masks/text-for-controls."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

def gen_macro(dark, W_MM=243.84):   # 48HP (44 + 4HP for dir_mod + prob_out jack columns)
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
    # QMIX GEOMETRY (Option B): +1 editor lane (q-mix at slot 2, after MEL/OCT), LANE_H 14→13
    # (POLY_LANES 4→5 → editor bottom 14+5*13=79). Mirrors src/ui/SandsGrid.hpp. q-mix has no
    # left controls yet — the 4 existing lanes occupy editor slots ESLOT=[0,1,3,4]; slot 2 empty.
    ED_X=88.; ED_W=111.; OWNER_X=205.; DIR_X=212.; DIR_MOD_X=220.; PROB_OUT_X=236.; ED_Y=14.
    LANE_H=13.; ED_LANES=5; ED_H=ED_LANES*LANE_H   # 65
    ED_LANE_H=LANE_H
    ESLOT=[0,1,3,4]                                 # existing display lane k → editor slot (skip slot 2)
    # Left-control rows align with the EDITOR lane centres (must match the hpp's rowY).
    def rowY(r): return ED_Y+(r+0.5)*LANE_H
    def ctrlY(k): return rowY(ESLOT[k])             # control/marker row for existing lane k
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
    A(D.helix_sands(4.0, 90.0, 74.0, 35.0, t, op=0.95))   # moved down below the 5-lane editor   # Sands Helix hero mark, bottom-left pocket (moved down 6mm so its MBS motif reads lower; wordmark moved the same amount)
    # (MBS identity mark removed — the Helix already carries an MBS motif in its background,
    #  and it collided with the bottom-left wordmark. The Helix alone is the identity art here.)
    A(D.accent_rules(PW,t))
    gx,gy=1.5,ctrlY(0)-ED_LANE_H*0.5-3.0; gw,gh=(SPREAD_X+6.0)-gx,(ctrlY(N-1)+ED_LANE_H*0.5+3.0)-gy  # gx clears leftmost jack
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(JACK_X[-1]+ATTEN_X[0])))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=5))
    A(D.owner_block(OWNER_X, [ctrlY(r) for r in range(N)], ED_X+ED_W, t, cell_w_mm=6.0))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=200.0, y_mm=122.0, target_w_mm=40.0))   # bottom-RIGHT (opposite the helix)
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for row in range(4):
        y=ctrlY(row)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
        A(D.trim(SPREAD_X,y,t,t["wellring"]))
    # q-mix — a FULL lane (5th) at editor slot 2: 3 jacks + 3 attens + spread, like MEL/OCT.
    yq=rowY(2)
    for x in JACK_X:  A(D.jack(x,yq,t))
    for x in ATTEN_X: A(D.trim(x,yq,t,t["gold"]))
    A(D.trim(SPREAD_X,yq,t,t["wellring"]))
    # ── Macro→voice MIX-IN send groups (relocated from East under the control
    #    inversion). 3 demarked groups (REST/MEL/OCT) below the editor, each a 2×2
    #    Len/Off/Rot/Spr send grid. "per voice, how much of Macro's global CV reaches
    #    this voice." Geometry shared with the widget labels in
    #    StraitsSandsMacroVisual::draw — keep in lockstep:
    #      BLEND_TOP=72 BLEND_H=36 GAP=3.5 SEND_Y0=12 SEND_DY=11 SEND_DX=7
    BLEND_TOP=82.0; BLEND_H=38.0; BGAP=2.5; GROUP_W=ED_W/5.0  # 5 groups (q-mix is a full lane); taller for the tap row 3
    SEND_Y0=10.0; SEND_DY=9.0; SEND_DX=6.0                   # DX 7→6 for narrower groups
    TAP_ROW_DY=9.0                                            # row 3 (taps) below the 2 send rows
    A(f'<line x1="{px(ED_X):.1f}" y1="{px(BLEND_TOP-3.0):.1f}" x2="{px(ED_X+ED_W):.1f}" y2="{px(BLEND_TOP-3.0):.1f}" stroke="{t["accent"]}" stroke-width="1.0" opacity="0.6"/>')
    # Blend groups drawn in display order (left-to-right: MEL/OCT/REST/ACC)
    MIX_XY=[None]*4   # indexed by engine lane
    TAP_XY=[None]*4   # P9b: [LOR tap, spread tap] per engine lane
    DISPLAY_ORDER_5=[1,2,-1,0,3]  # editor MEL/OCT/QMIX/REST/ACC -> engine lane (-1 = q-mix; markers TODO)
    for g in range(5):
        l=DISPLAY_ORDER_5[g]   # engine lane (-1 = q-mix)
        gx=ED_X+g*GROUP_W+BGAP*0.5; gw=GROUP_W-BGAP; gcx=gx+gw*0.5
        A(f'<rect x="{px(gx):.1f}" y="{px(BLEND_TOP):.1f}" width="{px(gw):.1f}" height="{px(BLEND_H):.1f}" rx="{px(1.4):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.92"/>')
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(BLEND_TOP+7.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(BLEND_TOP+7.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        lane_sends=[]
        for item in range(4):
            cxs=gcx+(-SEND_DX if (item%2)==0 else SEND_DX)
            cys=BLEND_TOP+SEND_Y0+(item//2)*SEND_DY
            A(D.trim(cxs,cys,t,t["gold"]))
            lane_sends.append((cxs,cys))
        if l>=0: MIX_XY[l]=lane_sends
        # P9b: row 3 = the two CV taps for this lane group — LOR (left) + SPREAD (right),
        # full-size trimpots. A faint divider separates them from the sends above.
        tap_y = BLEND_TOP+SEND_Y0+2*TAP_ROW_DY
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(tap_y-5.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(tap_y-5.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        A(D.trim(gcx-SEND_DX, tap_y, t, t["wellring"]))   # LOR tap
        A(D.trim(gcx+SEND_DX, tap_y, t, t["wellring"]))   # spread tap
        if l>=0: TAP_XY[l]=[(gcx-SEND_DX,tap_y),(gcx+SEND_DX,tap_y)]
    A('</g>')
    # ── SvgPanelKit component layer: named markers at every control centre, so a
    #    widget can bind by id later. Indices mirror StraitsSandsMacroVisual.hpp:
    #    cvId(r,c)=CV_START(0)+r*2+c (inputs), attenId(r,c)=ATTEN_START(3)+r*2+c
    #    (params), SPREAD_REST/MEL/OCT = 0/1/2 (params). ──
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # Components in display order; engine lane from DISPLAY_ORDER.
    for row in range(4):
        lane=DISPLAY_ORDER[row]   # engine lane
        y=ctrlY(row)
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
    # Direction cells (param_dir_<lane>) — per-lane direction toggle, at DIR_X, one per lane.
    # Uses EDITOR lane order (row 0..3 = MEL/OCT/REST/ACC), matching East's convention
    # and the C++ dirDispId(editorLane). NOT engine lane order — avoids the conversion
    # that other kit markers (cvId/attenId) require via DISPLAY_ORDER.
    for row in range(4):
        A(f'<circle id="param_dir_{row}" cx="{px(DIR_X):.2f}" cy="{px(ctrlY(row)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Direction gate-mod jacks (input_dir_mod_<lane>) — mono, gate cycles direction.
    for row in range(4):
        A(f'<circle id="input_dir_mod_{row}" cx="{px(DIR_MOD_X):.2f}" cy="{px(ctrlY(row)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Probability-out jacks (output_prob_<lane>) — 4 poly prob CV outs.
    for row in range(4):
        A(f'<circle id="output_prob_{row}" cx="{px(PROB_OUT_X):.2f}" cy="{px(rowY(row)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

def gen_mono(dark):
    t=theme(dark); W_MM,H_MM=243.84,128.5; PW,PH=px(W_MM),px(H_MM)   # 48HP (44 + 4HP for mod + prob_out jacks)
    # QMIX GEOMETRY (Option B): +1 editor lane (q-mix at slot 2, after MEL/OCT), LANE_H 14→13,
    # editor extends DOWN into the band above the MBS mark. Mirrors src/ui/SandsGrid.hpp
    # (MONO_LANES 6→7, LANE_H 14→13 → monoBottom 105). q-mix has no left controls yet — the 6
    # existing lanes occupy editor slots ESLOT=[0,1,3,4,5,6]; slot 2 is the empty q-mix band.
    ROW_TOP,LANE_H,N=14.,13.,7
    ROW_BOT=ROW_TOP+N*LANE_H                 # 105
    ESLOT=[0,1,3,4,5,6]                       # existing lane k → editor slot (skip slot 2 = q-mix)
    def laneY(l): return ROW_TOP+(l+0.5)*LANE_H
    def ctrlY(k): return laneY(ESLOT[k])      # control/marker row for existing lane k
    # Geometry MUST match MonsoonSandsVisualExpander.hpp:
    #   JACK_X={6,15,24}  ATTEN_X={34,43,52}  (all 6 lanes)
    #   spread (lanes 0-2 REST/MEL/OCT): SPR_BASE_X=62, SPR_CV_X=71, SPR_ATTEN_X=80
    JACK_X=[6.,15.,24.]; ATTEN_X=[34.,43.,52.]
    SPR_BASE_X,SPR_CV_X,SPR_ATTEN_X=62.,71.,80.
    N_SPREAD=4                                  # REST/MEL/OCT + ACCENT (poly lanes)
    SPR_TO_EDITOR=[2,0,1,3]                      # spread index (REST/MEL/OCT/ACCENT) → editor lane; matches cpp ENGINE_LANE_TO_EDITOR
    # Jack columns follow the TOGGLE order left->right (owner cell at OWNER_X, then dir cell
    # at DIR_X), so deleg_mod sits under the owner cell and dir_mod under the dir cell instead
    # of crossing over.
    ED_X=88.; ED_W=111.; OWNER_X=205.; DIR_X=212.; DELEG_MOD_X=220.; DIR_MOD_X=228.; PROB_OUT_X=236.  # +4HP mod+prob_out columns
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
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=7))
    A(D.owner_block(OWNER_X, [ctrlY(l) for l in range(4)], ED_X+ED_W, t, cell_w_mm=(ED_W-2*6.0)/16.0, draw_cells=False))
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=200.0, y_mm=122.0, target_w_mm=40.0))   # bottom-RIGHT (opposite the helix)
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    for lane in range(6):
        y=ctrlY(lane)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
    for sidx in range(N_SPREAD):
        y=ctrlY(SPR_TO_EDITOR[sidx])
        A(D.trim(SPR_BASE_X,y,t,t["wellring"]))
        A(D.jack(SPR_CV_X,y,t))
        A(D.trim(SPR_ATTEN_X,y,t,t["gold"]))
    # q-mix — a FULL lane (same complement as MEL/OCT): 3 CV jacks + 3 attens + spread, at
    # editor slot 2. Component markers use NEW q-mix param/CV ids that land with the engine
    # strand (LaneMapping.hpp) — visual controls drawn here so the geometry is provisioned for
    # the real lane, not an empty gap.
    yq=laneY(2)
    for x in JACK_X:  A(D.jack(x,yq,t))
    for x in ATTEN_X: A(D.trim(x,yq,t,t["gold"]))
    A(D.trim(SPR_BASE_X,yq,t,t["wellring"]))
    A(D.jack(SPR_CV_X,yq,t))
    A(D.trim(SPR_ATTEN_X,yq,t,t["gold"]))
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
        y=ctrlY(row)
        for p,x in enumerate(JACK_X):  A(D.kit_shape("input", 0+row*3+p, x, y))
        for p,x in enumerate(ATTEN_X): A(D.kit_shape("param", 22+row*3+p, x, y))
    for sidx in range(N_SPREAD):
        y=ctrlY(SPR_TO_EDITOR[sidx])
        A(D.kit_shape("param", 18+sidx, SPR_BASE_X, y))   # SPR_REST/MEL/OCT/ACCENT (18..21, engine order)
        A(D.kit_shape("input", 18+sidx, SPR_CV_X, y))     # SPR_CV (18..21)
        A(D.kit_shape("param", 40+sidx, SPR_ATTEN_X, y))  # SPR_ATTEN (40..43)
    # Direction cells (param_dir_<lane>) — per-lane direction toggle, at DIR_X, one per lane (0..5).
    for lane in range(6):
        A(f'<circle id="param_dir_{lane}" cx="{px(DIR_X):.2f}" cy="{px(ctrlY(lane)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Direction gate-mod jacks (input_dir_mod_<lane>) — mono, gate cycles direction. 6 lanes.
    for lane in range(6):
        A(f'<circle id="input_dir_mod_{lane}" cx="{px(DIR_MOD_X):.2f}" cy="{px(ctrlY(lane)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Delegation gate-mod jacks (input_deleg_mod_<lane>) — mono, gate flips delegation. Lanes 0..3.
    for lane in range(4):
        A(f'<circle id="input_deleg_mod_{lane}" cx="{px(DELEG_MOD_X):.2f}" cy="{px(ctrlY(lane)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Probability-out jacks (output_prob_<lane>) — 6 mono prob CV outs.
    for lane in range(6):
        A(f'<circle id="output_prob_{lane}" cx="{px(PROB_OUT_X):.2f}" cy="{px(ctrlY(lane)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

import os as _os
_outdir = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "res", "panels")
for fn,base in [(gen_macro,"StraitsSandsMacroVisual_48HP"),(gen_mono,"SandsMonoVisual_48HP")]:
    for dark,suf in [(True,""),(False,"_light")]:
        svg=fn(dark); name=f"{base}{suf}.svg"
        with open(_os.path.join(_outdir, name),"w") as f: f.write(svg)
        print(f"{name}: {len(svg):,} bytes")
