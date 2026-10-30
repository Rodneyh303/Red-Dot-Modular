"""Macro (26HP) and Mono (40HP) Sands visual panels — dot.modular design language.
Uses shared dotmod_design helpers (palette, logo, MBS+waves motif, recesses).
nanosvg-safe: per-shape paint, no gradients/masks/text-for-controls."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dotmod_design as D
from dotmod_design import px, theme

def gen_macro(dark, W_MM=259.08):  # 51HP exactly (51 x 5.08mm = 765px) — the old 58HP+18mm minus the MOVED send strip, re-widened from 258mm: Rack's ModuleWidget::setPanel rounds the widget box to WHOLE HP (761.81px art in a 765px box left a 3.19px rack-bg sliver at the seam the subpanels seat against — CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC "seamless abutment")
    # Macro mirrors the East visual's 7-lane geometry EXACTLY (same lane tops/heights); it does
    # the same spread job but GLOBAL rather than per-lane. Must match StraitsSandsMacroVisual.hpp
    # (W_MM, ROW_BOT=105, ED_H=91) and src/ui/SandsGrid.hpp (LANE_TOP=14, LANE_H=13, POLY_LANES=7).
    # SANDS CONSOLIDATION Step 3: widened 48→52HP; the 6 mix-in send knobs per lane MOVED from
    # below-editor groups to a 6-column × 7-row RHS grid (row-aligned per lane). Left/center
    # geometry (jacks/attens/spread/editor/owner/dir/dir_mod/prob_out) is UNCHANGED.
    # THE SEND STRIP MOVED OFF (the spec's §"MACRO PER-VOICE SENDS → their own subpanel"):
    # the 7x4 mix-in sends + 7x2 PRE/POST taps draw on the SandsMacroSendSubpanel now
    # (panel_src/gen_sands_macro_send.py) — this panel is cut back to 258mm; the anchors
    # param_send_*/param_taplor_*/param_tapspr_* MOVED with the strip (they bind the same
    # editor stores there).
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
    # Geometry: 7 lanes × 13mm, editor 14→105 (matches East). Left/center columns are
    # UNCHANGED from the 5-lane layout; the send strip's 10HP is GONE (moved to the
    # SandsMacroSendSubpanel — the spec's "Macro is already big").
    WLEVEL_X=88.   # w-level knob column (CC_PROMPT_04 STEP 1) — the OLD ED_X slot (9mm left-cluster rhythm)
    WARPEDIT_X=229.   # CC_PROMPT_04 STEP 2: warp curve-editor trigger button column, right of the dir cell
    # UNIFIED ADDRESSING Stage 3: +5mm — the voice-range strip (RANGE_MM, the thin constant-block
    # lane at the TOP of the grid, the Macro tier's authoring surface). The editor BOX grows
    # DOWN; the 7 probability lanes keep their full 13mm and start one strip lower. Mirrors
    # src/ui/SandsGrid.hpp RANGE_LANE_H/polyBoxHeight().
    RANGE_MM=5.0
    ED_X=97.; ED_W=111.; OWNER_X=214.; DIR_X=221.; DIR_MOD_X=238.; PROB_OUT_X=248.; ED_Y=14.; ED_H=96.   # +9mm (w-level col) +9mm (CRV col); 7 lanes x 13mm + 5mm range strip, editor 14->110
    ED_LANE_H=(ED_H-RANGE_MM)/N   # 13mm — the LANE area below the range strip
    # Left-control rows align with the EDITOR lane centres (must match the hpp's rowY).
    # Stage 3: lanes start below the voice-range strip — every row shifts down by RANGE_MM.
    def rowY(r): return ED_Y+RANGE_MM+(r+0.5)*ED_LANE_H
    ctrlY = rowY   # alias: a few sites below use ctrlY (as gen_mono does); same lane-centre.
    # 4 CV jacks + 4 attens + spread base — columns match SandsMonoVisual, ED_X=88
    JACK_X=[6.,15.,24.,33.]            # LEN/OFF/ROT/SPR-cv
    ATTEN_X=[43.,52.,61.,70.]          # LEN/OFF/ROT/SPR depth
    SPREAD_X=80.                       # per-lane spread base trimpot
    L=[]; A=L.append
    A(D.svg_open(PW,PH))
    A('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    A(D.bg_rect(PW,PH,t))
    # Identity artwork: Sands Helix hero mark, bottom-left pocket. With 7 lanes the left controls
    # now extend to Y=105, so the helix is scaled into the free bottom strip (105→128.5) to avoid
    # overlapping the VAR/LEG control rows. Still bottom-left (vs East's lower-right), just lower
    # + shorter. A full helix reposition is deferred to Step 4.
    # Stage 3: the control rows (+RANGE_MM) push the group recess's bottom to ~113, so the helix
    # shrinks into the remaining pocket (113.5→128) — the full mark stays visible, just flatter.
    A(D.helix_sands(4.0, 113.5, 60.0, 14.5, t, op=0.95))
    A(D.accent_rules(PW,t))
    gx,gy=1.5,ctrlY(0)-ED_LANE_H*0.5-3.0; gw,gh=(WLEVEL_X+6.0)-gx,(ctrlY(N-1)+ED_LANE_H*0.5+3.0)-gy  # gx clears leftmost jack; recess wraps the w-level column too (CC_PROMPT_04)
    A(D.input_group(gx,gy,gw,gh,t,sep_mm=0.5*(JACK_X[-1]+ATTEN_X[0])))
    A(D.editor_recess(ED_X,ED_Y,ED_W,ED_H,t,lanes=7,range_h=RANGE_MM))
    A(D.owner_block(OWNER_X, [ctrlY(r) for r in range(N)], ED_X+ED_W, t, cell_w_mm=6.0))
    # CC_PROMPT_04 STEP 2: faint backing for the warp curve-editor ("CRV") button column,
    # right of the dir cell (header text is widget-drawn on Macro, matching its other headers).
    A(f'<rect x="{px(WARPEDIT_X-4.85):.1f}" y="{px(ED_Y):.1f}" width="{px(9.7):.1f}" height="{px(ED_H):.1f}" rx="{px(1.0):.1f}" fill="#ffffff" fill-opacity="0.05"/>')
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
        A(D.trim(WLEVEL_X,y,t,t["wellring"]))   # CC_PROMPT_04: w-level (warp depth) — Macro authors the DELEGATED DEFAULT (globalWarpW)
    # (the RHS mix-in send row MOVED to the SandsMacroSendSubpanel — the spec's "Macro is
    #  already big"; the knobs bind the same editor.macroSend/macroTap stores there.)
    A('</g>')
    # ── SvgPanelKit component (anchor) layer — THE SINGLE GEOMETRY SOURCE. ────────
    # DESCRIPTIVE, EDITOR-lane-indexed names (matches Sands Mono + East). The widget binds each
    # by name; the anchor-vs-bind audit enforces 1:1. The send/tap anchors (param_send_* /
    # param_taplor_* / param_tapspr_*) MOVED with the strip to gen_sands_macro_send.py.
    # editor lane el: 0 MEL,1 OCT,2 QMIX,3 REST,4 ACC,5 VAR,6 LEG.
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
        # w-level knobs (param_wlevel_<editorLane>) — CC_PROMPT_04 STEP 1: Macro's delegated
        # DEFAULT depth per lane (globalWarpW[lane]), same delegation pattern as the spread
        # base above. EVERY lane gets one (booleans too — inert until their consumer warps).
        named(f"param_wlevel_{el}", WLEVEL_X, y)
        # warp curve-editor trigger buttons (param_warpedit_<editorLane>) — CC_PROMPT_04 STEP 2:
        # Macro's button is ALWAYS available (edits Macro's own delegated-default curve).
        named(f"param_warpedit_{el}", WARPEDIT_X, y)
        named(f"output_prob_{el}", PROB_OUT_X, y)
    # Direction cells (param_dir_<editorLane>) + gate-mod jacks (input_dir_mod_<editorLane>) —
    # these ARE editor-lane indexed in the C++ (getGlobalDir(editorLane)), so keep `el`.
    for el in range(ED_LANES):
        named(f"param_dir_{el}",     DIR_X,     rowY(el))
        named(f"input_dir_mod_{el}", DIR_MOD_X, rowY(el))
    A('</g>')
    A('</svg>')
    return "\n".join(L)

import os as _os
_outdir = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "res", "panels")
for fn,base in [(gen_macro,"StraitsSandsMacroVisual_48HP")]:
    for dark,suf in [(True,""),(False,"_light")]:
        svg=fn(dark); name=f"{base}{suf}.svg"
        with open(_os.path.join(_outdir, name),"w") as f: f.write(svg)
        print(f"{name}: {len(svg):,} bytes")
