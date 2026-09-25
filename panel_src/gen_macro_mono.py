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
    N=5   # OPT-B: 5 lanes (Q-MIX at index 2), one row each
    ED_LANES=N   # explicit local so the draw/component loops below can't pick up a leaked module-
                 # scope ED_LANES (East's 7) — the "Macro missing a row / labels mixed up" root cause.
    # editor row → poly engine/spread lane (MEL->1 OCT->2 QMIX->4 REST->0 ACC->3). Mirrors
    # dotModular::EDITOR_TO_ENGINE_LANE_QMIX (dsp/LaneMapping.hpp). cv/atten/spread/prob ids are
    # engine-ordered, so emit them at the editor row via this table (using `el` put REST's spread on
    # the melody row etc). Defined HERE (was implicitly leaked) so gen_macro is self-contained.
    EDITOR_TO_ENGINE=[1,2,4,0,3]
    assert len(EDITOR_TO_ENGINE)==ED_LANES, "EDITOR_TO_ENGINE must have one entry per editor lane"
    # Extra top margin so the view-tab row isn't crammed against the panel top
    # edge. 0.5 cm = 5 mm. Mirror TAB_TOP_OFFSET_MM in StraitsSandsMacroVisualWidget.
        # Mirrors src/ui/SandsGrid.hpp — tabs sit ABOVE the grid (3..13mm), lane 0 starts at 14.
    TAB_TOP, TAB_ROW_H = 3.0, 5.0
    TAB_TOP_OFFSET_MM = 5.0
    # Mirrors src/ui/SandsGrid.hpp: lane 0 at 14mm, 4 lanes x 14mm = 56 (tabs live above, 3..13).
    ED_X=88.; ED_W=111.; OWNER_X=205.; DIR_X=212.; DIR_MOD_X=220.; PROB_OUT_X=236.; ED_Y=14.; ED_H=65.   # OPT-B: 5 lanes x 13mm, editor 14->79
    ED_LANE_H=ED_H/N
    # Left-control rows align with the EDITOR lane centres (must match the hpp's rowY).
    def rowY(r): return ED_Y+(r+0.5)*ED_LANE_H
    ctrlY = rowY   # alias: a few sites below use ctrlY (as gen_mono does); same lane-centre.
    # (Removed the stale 4-entry DISPLAY_ORDER / LANE_NAMES_D — pre-q-mix leftovers. Rows are
    #  editor lanes 0..4 directly (MEL/OCT/QMIX/REST/ACC); no display remap, no local label table.)
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
    A(D.helix_sands(4.0, 82.0, 74.0, 33.0, t, op=0.95))   # Sands Helix hero mark, bottom-left pocket (moved down 6mm so its MBS motif reads lower; wordmark moved the same amount)
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
    # 5 editor lanes (MEL/OCT/QMIX/REST/ACC), q-mix a PLAIN lane at row 2. Row == editor lane
    # (no ESLOT/DISPLAY_ORDER remap); 4 CV jacks + 4 attens + spread base each.
    for el in range(ED_LANES):
        y=rowY(el)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
        A(D.trim(SPREAD_X,y,t,t["wellring"]))
    # ── Macro→voice MIX-IN send groups (relocated from East under the control
    #    inversion). 3 demarked groups (REST/MEL/OCT) below the editor, each a 2×2
    #    Len/Off/Rot/Spr send grid. "per voice, how much of Macro's global CV reaches
    #    this voice." Geometry shared with the widget labels in
    #    StraitsSandsMacroVisual::draw. GEOMETRY IS OWNED HERE: the generator emits a
    #    label_mixin_<editorLane> anchor per group (+ reuses the param_send_/taplor/tapspr
    #    anchors for the item labels), and draw() derives every label position from
    #    centerOf(findNamed(...)) — NOT by recomputing GROUP_W/BLEND_*. So these constants
    #    live in ONE place; re-running the generator can no longer drift the labels off the
    #    boxes (the ED_W/4-vs-ED_W/5 bug that recurred 3×).
    BLEND_TOP=85.0; BLEND_H=35.0; BGAP=2.5; GROUP_W=ED_W/float(ED_LANES)  # 5 groups (q-mix is a full lane)
    SEND_Y0=10.0; SEND_DY=9.0; SEND_DX=6.0                   # DX 7→6 for narrower groups
    TAP_ROW_DY=9.0                                            # row 3 (taps) below the 2 send rows
    A(f'<line x1="{px(ED_X):.1f}" y1="{px(BLEND_TOP-3.0):.1f}" x2="{px(ED_X+ED_W):.1f}" y2="{px(BLEND_TOP-3.0):.1f}" stroke="{t["accent"]}" stroke-width="1.0" opacity="0.6"/>')
    # Blend groups drawn in EDITOR order (left-to-right: MEL/OCT/QMIX/REST/ACC). Group index g
    # IS the editor lane — send/tap markers below are editor-lane indexed to match the C++ binds
    # (param_send_<editorLane>_<item>, param_taplor/tapspr_<editorLane>). No engine remap.
    MIX_XY=[None]*ED_LANES   # indexed by EDITOR lane
    TAP_XY=[None]*ED_LANES   # P9b: [LOR tap, spread tap] per EDITOR lane
    LABEL_MIXIN_XY=[None]*ED_LANES   # group-header label anchor per EDITOR lane
    for el in range(ED_LANES):
        gx=ED_X+el*GROUP_W+BGAP*0.5; gw=GROUP_W-BGAP; gcx=gx+gw*0.5
        A(f'<rect x="{px(gx):.1f}" y="{px(BLEND_TOP):.1f}" width="{px(gw):.1f}" height="{px(BLEND_H):.1f}" rx="{px(1.4):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.92"/>')
        LABEL_MIXIN_XY[el]=(gcx, BLEND_TOP+4.0)   # group-name label centre (matches old draw() gcx, BLEND_TOP+4)
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(BLEND_TOP+7.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(BLEND_TOP+7.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        lane_sends=[]
        for item in range(4):
            cxs=gcx+(-SEND_DX if (item%2)==0 else SEND_DX)
            cys=BLEND_TOP+SEND_Y0+(item//2)*SEND_DY
            A(D.trim(cxs,cys,t,t["gold"]))
            lane_sends.append((cxs,cys))
        MIX_XY[el]=lane_sends
        # P9b: row 3 = the two CV taps for this lane group — LOR (left) + SPREAD (right).
        tap_y = BLEND_TOP+SEND_Y0+2*TAP_ROW_DY
        A(f'<line x1="{px(gx+2):.1f}" y1="{px(tap_y-5.5):.1f}" x2="{px(gx+gw-2):.1f}" y2="{px(tap_y-5.5):.1f}" stroke="{t["edborder"]}" stroke-width="0.6" opacity="0.6"/>')
        A(D.trim(gcx-SEND_DX, tap_y, t, t["wellring"]))   # LOR tap
        A(D.trim(gcx+SEND_DX, tap_y, t, t["wellring"]))   # spread tap
        TAP_XY[el]=[(gcx-SEND_DX,tap_y),(gcx+SEND_DX,tap_y)]
    A('</g>')
    # ── SvgPanelKit component layer. ALL ids EDITOR-lane indexed, matching StraitsSandsMacroVisual
    #    .cpp binds exactly (editor lane el: 0 MEL,1 OCT,2 QMIX,3 REST,4 ACC — no DISPLAY_ORDER remap):
    #      cvId(el,c)   = CV_START(0)    + el*4 + c   inputs 0..19
    #      attenId(el,c)= ATTEN_START(5) + el*4 + c   params 5..24   (SPREAD_REST..QMIX = 0..4)
    #      spread base  = param el (SPREAD_REST..QMIX positional by editor lane)
    #      prob out     = output_{el}   (C++ binds "output_"+std::to_string(PROB_OUT_REST+el))
    #      param_send_<el>_<item>, param_taplor_/tapspr_<el>, param_dir_<el>, input_dir_mod_<el>. ──
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # ENGINE-lane-indexed groups (cv/atten/spread/prob) sit at the EDITOR row `el` but carry the
    # id for engine lane `eng` = EDITOR_TO_ENGINE[el] — because the C++ store accessors
    # (getGlobalAtten/Spread, PROB_OUT_REST+lane) are engine-indexed. Using `el` here put REST's
    # spread on the melody row etc. (the reported bug).
    for el in range(ED_LANES):
        y=rowY(el); eng=EDITOR_TO_ENGINE[el]
        for p,x in enumerate(JACK_X):  A(D.kit_shape("input", 0 + eng*4 + p, x, y))   # cvId(eng,c)
        for p,x in enumerate(ATTEN_X): A(D.kit_shape("param", 5 + eng*4 + p, x, y))   # attenId(eng,c)=ATTEN_START(5)+..
        A(D.kit_shape("param", eng, SPREAD_X, y))    # SPREAD_REST..QMIX = param eng (engine lane)
        A(D.kit_shape("output", eng, PROB_OUT_X, y)) # output_{eng} (PROB_OUT_REST+eng, engine lane)
    # Macro→voice mix-in send markers + PRE/POST taps — also ENGINE-lane indexed
    # (getMacroSend(slot, lane, item) / getGlobalTap(lane,..) are engine order), at editor row.
    for el in range(ED_LANES):
        eng=EDITOR_TO_ENGINE[el]
        for item in range(4):
            cxs,cys = MIX_XY[el][item]
            A(f'<circle id="param_send_{eng}_{item}" cx="{px(cxs):.2f}" cy="{px(cys):.2f}" r="0.5" fill="none" stroke="none"/>')
        (lx,ly),(sx,sy) = TAP_XY[el]
        A(f'<circle id="param_taplor_{eng}" cx="{px(lx):.2f}" cy="{px(ly):.2f}" r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="param_tapspr_{eng}" cx="{px(sx):.2f}" cy="{px(sy):.2f}" r="0.5" fill="none" stroke="none"/>')
    # Group-header label anchors (EDITOR order — the group name MEL/OCT/QMIX/REST/ACC). draw()
    # reads centerOf(findNamed("label_mixin_<el>")) instead of recomputing ED_X+el*GROUP_W, so the
    # header + its item labels can never drift off the boxes when GROUP_W/BLEND_* change here.
    for el in range(ED_LANES):
        gcx, gy = LABEL_MIXIN_XY[el]
        A(f'<circle id="label_mixin_{el}" cx="{px(gcx):.2f}" cy="{px(gy):.2f}" r="0.5" fill="none" stroke="none"/>')
    # Direction cells (param_dir_<editorLane>) + gate-mod jacks (input_dir_mod_<editorLane>) —
    # these ARE editor-lane indexed in the C++ (getGlobalDir(editorLane)), so keep `el`.
    for el in range(ED_LANES):
        A(f'<circle id="param_dir_{el}" cx="{px(DIR_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="input_dir_mod_{el}" cx="{px(DIR_MOD_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

def gen_mono(dark):
    t=theme(dark); W_MM,H_MM=243.84,128.5; PW,PH=px(W_MM),px(H_MM)   # 48HP (44 + 4HP for mod + prob_out jacks)
    # Mirrors src/ui/SandsGrid.hpp: 6 lanes x 14mm from 14 → bottom 98 (was 108, laneH 15.667).
    ROW_TOP,ROW_BOT,N=14.,105.,7   # OPT-B: 7 lanes x 13mm (Q-MIX at index 2); editor 14->105, into the space above MBS
    def laneY(l): return ROW_TOP+(l+0.5)*(ROW_BOT-ROW_TOP)/N
    ctrlY = laneY   # alias: control/marker rows use ctrlY; identical to the lane centre.
    # Geometry MUST match MonsoonSandsVisualExpander.hpp:
    #   JACK_X={6,15,24}  ATTEN_X={34,43,52}  (all 6 lanes)
    #   spread (lanes 0-2 REST/MEL/OCT): SPR_BASE_X=62, SPR_CV_X=71, SPR_ATTEN_X=80
    JACK_X=[6.,15.,24.]; ATTEN_X=[34.,43.,52.]
    SPR_BASE_X,SPR_CV_X,SPR_ATTEN_X=62.,71.,80.
    N_SPREAD=5                                   # REST/MEL/OCT/ACC/QMIX (poly lanes)
    SPR_TO_EDITOR=[3,0,1,4,2]                    # spread idx (poly engine REST/MEL/OCT/ACC/QMIX) → editor lane; matches cpp ENGINE_LANE_TO_EDITOR_QMIX
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
    # 7 editor lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG): 3 CV jacks + 3 attens each. q-mix a PLAIN
    # lane at row 2 (no ESLOT gap, no separate special-case block).
    for lane in range(N):
        y=ctrlY(lane)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
    # 5 spread lanes (REST/MEL/OCT/ACC/QMIX) placed on their editor rows via SPR_TO_EDITOR.
    for sidx in range(N_SPREAD):
        y=ctrlY(SPR_TO_EDITOR[sidx])
        A(D.trim(SPR_BASE_X,y,t,t["wellring"]))
        A(D.jack(SPR_CV_X,y,t))
        A(D.trim(SPR_ATTEN_X,y,t,t["gold"]))
    A('</g>')
    # ── SvgPanelKit component (anchor) layer — THE SINGLE GEOMETRY SOURCE. ────────
    # The widget binds every control by name via SvgPanelKit (loadPanel + findNamed);
    # it no longer places anything with mm2px. Anchors are DESCRIPTIVE (not bare-
    # numeric) so StoreKnobs — which carry no paramId — can be bound by bindWidget.
    # All editor-lane indexed, all visible (fill/stroke none, never display:none).
    # The anchor-vs-bind audit (test/audit_anchor_bind.py) enforces 1:1 anchor↔bind.
    def named(kind_name, x, y):
        A(f'<circle id="{kind_name}" cx="{px(x):.2f}" cy="{px(y):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # LOR CV jacks + attenuverters — 7 editor lanes × 3 (LEN/OFF/ROT).
    #   input_cv_<el>_<col>      = cvId(el,col)   (bindInput)
    #   param_atten_<el>_<col>   StoreKnob        (bindWidget)
    for el in range(N):
        y=ctrlY(el)
        for col,x in enumerate(JACK_X):  named(f"input_cv_{el}_{col}", x, y)
        for col,x in enumerate(ATTEN_X): named(f"param_atten_{el}_{col}", x, y)
    # Spread group — 5 poly lanes (REST/MEL/OCT/ACC/QMIX) on their editor rows.
    #   param_spr_<sidx>     spread base StoreKnob (bindWidget)
    #   input_sprcv_<sidx>   spread CV jack        (bindInput, id sprCvId(sidx))
    #   param_spratten_<sidx> spread atten StoreKnob (bindWidget)
    for sidx in range(N_SPREAD):
        y=ctrlY(SPR_TO_EDITOR[sidx])
        named(f"param_spr_{sidx}",      SPR_BASE_X,  y)
        named(f"input_sprcv_{sidx}",    SPR_CV_X,    y)
        named(f"param_spratten_{sidx}", SPR_ATTEN_X, y)
    # V1 ownership cells — poly lanes 0..4 (bare OwnerCell, bindChild). Previously had
    # NO anchor at all (the widget placed them by mm2px only) — now a real anchor.
    for lane in range(N_SPREAD):
        named(f"param_owner_{lane}", OWNER_X, ctrlY(lane))
    # Direction cells + gate-mod jacks — one per editor lane 0..6.
    #   param_dir_<lane>     DirCell bare widget   (bindChild)
    #   input_dir_mod_<lane> gate-mod jack         (bindInput, dirModId)
    for lane in range(N):
        named(f"param_dir_{lane}",     DIR_X,     ctrlY(lane))
        named(f"input_dir_mod_{lane}", DIR_MOD_X, ctrlY(lane))
    # Delegation gate-mod jacks — poly lanes 0..4 (bindInput, delegModId).
    for lane in range(N_SPREAD):
        named(f"input_deleg_mod_{lane}", DELEG_MOD_X, ctrlY(lane))
    # Probability-out jacks — 7 mono prob CV outs (bindOutput, PROB_OUT_START).
    for lane in range(N):
        named(f"output_prob_{lane}", PROB_OUT_X, ctrlY(lane))
    # Editor recess box anchor — the live SandsVisualEditorV4 is sized/placed from
    # boundsOf(findNamed("param_editor_recess")). Radius encodes half-extents so the
    # widget can recover box.size from the anchor bounds (cx±rx, cy±ry via nanosvg).
    A(f'<rect id="param_editor_recess" x="{px(ED_X):.2f}" y="{px(ED_Y):.2f}" '
      f'width="{px(ED_W):.2f}" height="{px(ED_H):.2f}" fill="none" stroke="none"/>')
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
