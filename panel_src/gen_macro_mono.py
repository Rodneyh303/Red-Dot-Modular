"""Macro (26HP) and Mono (40HP) Sands visual panels — dot.modular design language.
Uses shared dotmod_design helpers (palette, logo, MBS+waves motif, recesses).
nanosvg-safe: per-shape paint, no gradients/masks/text-for-controls."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

def gen_macro(dark, W_MM=294.64):   # 58HP (48HP + 10HP widened on the RIGHT for the 6-col send row per lane)
    # Macro mirrors the East visual's 7-lane geometry EXACTLY (same lane tops/heights); it does
    # the same spread job but GLOBAL rather than per-lane. Must match StraitsSandsMacroVisual.hpp
    # (W_MM, ROW_BOT=105, ED_H=91) and src/ui/SandsGrid.hpp (LANE_TOP=14, LANE_H=13, POLY_LANES=7).
    # SANDS CONSOLIDATION Step 3: widened 48→52HP; the 6 mix-in send knobs per lane MOVED from
    # below-editor groups to a 6-column × 7-row RHS grid (row-aligned per lane). Left/center
    # geometry (jacks/attens/spread/editor/owner/dir/dir_mod/prob_out) is UNCHANGED.
    t=theme(dark); H_MM=128.5; PW,PH=px(W_MM),px(H_MM)
    N=7   # 7 editor lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG), one row each — matches East
    ED_LANES=N   # explicit local so the draw/component loops below can't pick up a leaked module-
                 #scope ED_LANES (East's 7) — the "Macro missing a row / labels mixed up" root cause.
    # editor row → poly engine/spread lane. Mirrors dotModular::EDITOR_TO_ENGINE_LANE_QMIX
    # (dsp/LaneMapping.hpp): MEL->1 OCT->2 QMIX->4 REST->0 ACC->3 VAR->5 LEG->6. cv/atten/spread/
    # prob ids are engine-ordered, so emit them at the editor row via this table. Defined HERE
    # (was implicitly leaked) so gen_macro is self-contained.
    EDITOR_TO_ENGINE=[1,2,4,0,3,5,6]
    assert len(EDITOR_TO_ENGINE)==ED_LANES, "EDITOR_TO_ENGINE must have one entry per editor lane"
    # Extra top margin so the view-tab row isn't crammed against the panel top
    # edge. 0.5 cm = 5 mm. Mirror TAB_TOP_OFFSET_MM in StraitsSandsMacroVisualWidget.
        # Mirrors src/ui/SandsGrid.hpp — tabs sit ABOVE the grid (3..13mm), lane 0 starts at 14.
    TAB_TOP, TAB_ROW_H = 3.0, 5.0
    TAB_TOP_OFFSET_MM = 5.0
    # Geometry: 7 lanes × 13mm, editor 14→105 (matches East). Left/center/right columns are
    # UNCHANGED from the 5-lane layout; the extra 20mm is added on the RIGHT for the send grid.
    ED_X=88.; ED_W=111.; OWNER_X=205.; DIR_X=212.; DIR_MOD_X=220.; PROB_OUT_X=230.; ED_Y=14.; ED_H=91.   # 7 lanes x 13mm, editor 14->105; PROB_OUT moved L to clear send col
    ED_LANE_H=ED_H/N
    # Left-control rows align with the EDITOR lane centres (must match the hpp's rowY).
    def rowY(r): return ED_Y+(r+0.5)*ED_LANE_H
    ctrlY = rowY   # alias: a few sites below use ctrlY (as gen_mono does); same lane-centre.
    # 4 CV jacks + 4 attens + spread base — columns match SandsMonoVisual, ED_X=88
    JACK_X=[6.,15.,24.,33.]            # LEN/OFF/ROT/SPR-cv
    ATTEN_X=[43.,52.,61.,70.]          # LEN/OFF/ROT/SPR depth
    SPREAD_X=80.                       # per-lane spread base trimpot
    # RHS mix-in SEND row: 6 knobs in a SINGLE horizontal row per lane, 9mm pitch (matches
    # left-side modulation knobs). Row-aligned to each LED lane (Y = ctrlY(el)). Cols 0..3 =
    # LEN/OFF/ROT/SPR sends (gold), col 4 = LOR tap, col 5 = SPR tap (wellring). Anchor NAMES
    # are UNCHANGED — only X/Y move — so widget name-binds still resolve.
    SEND_COL_X=[240.,249.,258.,267.,276.,285.]   # 6 columns at 9mm pitch
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    # Identity artwork: Sands Helix hero mark, bottom-left pocket. With 7 lanes the left controls
    # now extend to Y=105, so the helix is scaled into the free bottom strip (105→128.5) to avoid
    # overlapping the VAR/LEG control rows. Still bottom-left (vs East's lower-right), just lower
    # + shorter. A full helix reposition is deferred to Step 4.
    A(D.helix_sands(4.0, 107.0, 60.0, 20.0, t, op=0.95))
    A(D.accent_rules(PW,t))
    gx,gy=1.5,ctrlY(0)-ED_LANE_H*0.5-3.0; gw,gh=(SPREAD_X+6.0)-gx,(ctrlY(N-1)+ED_LANE_H*0.5+3.0)-gy  # gx clears leftmost jack
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(JACK_X[-1]+ATTEN_X[0])))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=7))
    A(D.owner_block(OWNER_X, [ctrlY(r) for r in range(N)], ED_X+ED_W, t, cell_w_mm=6.0))
    # RHS send-grid recess: a faint panel-coloured frame behind the 3×2 send knob blocks.
    sg_x=SEND_COL_X[0]-3.0; sg_y=ED_Y; sg_w=(SEND_COL_X[-1]-SEND_COL_X[0])+6.0; sg_h=ED_H
    A(f'<rect x="{px(sg_x):.1f}" y="{px(sg_y):.1f}" width="{px(sg_w):.1f}" height="{px(sg_h):.1f}" rx="{px(1.4):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="0.9" opacity="0.55"/>')
    A('</g>')
    A('<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(D.logo_embed(dark, x_mm=W_MM-44.0, y_mm=122.0, target_w_mm=40.0))   # bottom-RIGHT (opposite the helix), tracks the wider panel
    A('</g>')
    A('<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    # 7 editor lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG), q-mix a PLAIN lane at row 2. Row == editor
    # lane (no ESLOT/DISPLAY_ORDER remap); 4 CV jacks + 4 attens + spread base each.
    for el in range(ED_LANES):
        y=rowY(el)
        for x in JACK_X:  A(D.jack(x,y,t))
        for x in ATTEN_X: A(D.trim(x,y,t,t["gold"]))
        A(D.trim(SPREAD_X,y,t,t["wellring"]))
    # ── Macro→voice MIX-IN send row: 6 knobs in a single horizontal row per lane, 9mm pitch.
    #    Cols 0..3 = LEN/OFF/ROT/SPR sends (gold), col 4 = LOR tap, col 5 = SPR tap (wellring).
    #    Row-aligned to each LED lane (Y = ctrlY(el)), mirroring the left-side modulation knobs.
    for el in range(ED_LANES):
        y=rowY(el)
        for item in range(4):
            A(D.trim(SEND_COL_X[item], y, t, t["gold"]))   # LEN/OFF/ROT/SPR send
        A(D.trim(SEND_COL_X[4], y, t, t["wellring"]))      # LOR tap (PRE/POST)
        A(D.trim(SEND_COL_X[5], y, t, t["wellring"]))      # SPR tap (PRE/POST)
    A('</g>')
    # ── SvgPanelKit component (anchor) layer — THE SINGLE GEOMETRY SOURCE. ────────
    # DESCRIPTIVE, EDITOR-lane-indexed names (matches Sands Mono + East). The widget binds each
    # by name; the anchor-vs-bind audit enforces 1:1. Send/tap anchors now sit in the RHS grid;
    # their NAMES are unchanged, so the StoreKnob binds (param_send_<el>_<item> etc.) resolve
    # automatically — only X/Y changed. editor lane el: 0 MEL,1 OCT,2 QMIX,3 REST,4 ACC,5 VAR,6 LEG.
    def named(name, x, y):
        A(f'<circle id="{name}" cx="{px(x):.2f}" cy="{px(y):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # Left section: CV jacks (LEN/OFF/ROT/SPR) + attens + spread base + prob out, per editor lane.
    #   input_cv_<el>_<c> / param_atten_<el>_<c> / param_spr_<el> / output_prob_<el>
    for el in range(ED_LANES):
        y=rowY(el)
        for c,x in enumerate(JACK_X):  named(f"input_cv_{el}_{c}",    x, y)
        for c,x in enumerate(ATTEN_X): named(f"param_atten_{el}_{c}", x, y)
        named(f"param_spr_{el}",  SPREAD_X,   y)
        named(f"output_prob_{el}", PROB_OUT_X, y)
    # Macro→voice mix-in send + PRE/POST tap anchors — single row per lane, 6 columns at 9mm pitch.
    for el in range(ED_LANES):
        y=rowY(el)
        named(f"param_send_{el}_0", SEND_COL_X[0], y)   # LEN send
        named(f"param_send_{el}_1", SEND_COL_X[1], y)   # OFF send
        named(f"param_send_{el}_2", SEND_COL_X[2], y)   # ROT send
        named(f"param_send_{el}_3", SEND_COL_X[3], y)   # SPR send
        named(f"param_taplor_{el}", SEND_COL_X[4], y)   # LOR tap
        named(f"param_tapspr_{el}", SEND_COL_X[5], y)   # SPR tap
    # Direction cells (param_dir_<editorLane>) + gate-mod jacks (input_dir_mod_<editorLane>) —
    # these ARE editor-lane indexed in the C++ (getGlobalDir(editorLane)), so keep `el`.
    for el in range(ED_LANES):
        named(f"param_dir_{el}",     DIR_X,     rowY(el))
        named(f"input_dir_mod_{el}", DIR_MOD_X, rowY(el))
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
