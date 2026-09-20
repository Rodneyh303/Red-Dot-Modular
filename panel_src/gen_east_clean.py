"""Straits East Sands visual panel (40HP, native 75 DPI).
dot.modular panel system: Monsoon palette, title/footer DM mark, input-group
recess, editor recess w/ header + lane separators, MBS+waves background motif,
red corner screws. nanosvg-safe (no masks/gradients/text-for-controls)."""
import math, os
S = 75.0/25.4
def px(mm): return round(mm*S, 2)
W_MM, H_MM = 243.84, 128.5  # 48HP (44 + 4HP for dir_mod + deleg_mod + prob_out jack columns)
PW, PH = px(W_MM), px(H_MM)

N = 7   # 7 editor lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG), one row each
# Extra top margin so the voice tab row isn't crammed against the panel top edge.
# 0.5 cm = 5 mm. Adjust + rerun the generator (and mirror TAB_TOP_OFFSET_MM in
# StraitsEastSandsVisualWidget) to taste.
# Mirrors src/ui/SandsGrid.hpp — tabs sit ABOVE the grid (3..13mm), lane 0 starts at 14.
TAB_TOP, TAB_ROW_H = 3.0, 5.0
TAB_TOP_OFFSET_MM = 5.0
ED_X, ED_Y = 88.0, 14.0
ED_W = 111.0        # editor width (fixed; no longer tied to PROB_OUT_X — matches hpp)
OWNER_X = 205.0     # owner-source cell column, right of the editor (matches hpp)
DIR_X = 212.0       # direction cell column (Fwd/Rev/Pend/PingPong), right of owner (matches hpp)
# Jack columns follow the TOGGLE order left->right (owner/delegation cell, then dir cell),
# so each jack sits under the control it modulates instead of crossing over.
DELEG_MOD_X = 220.0 # delegation gate-mod jack column (gate flips local/delegated)
DIR_MOD_X = 228.0   # direction gate-mod jack column (gate cycles Fwd→Rev→Pend→PingPong)
PROB_OUT_X = 236.0  # right-strip jack column, pushed right by the mod jack columns (matches hpp)
ED_H = 91.0   # OPT-B: 7 lanes x 13mm — East adds Q-MIX at index 2 (MEL/OCT/QMIX/REST/ACC/VAR/LEG); editor 14->105, ~6mm above MBS
ED_LANES = 7  # OPT-B: editor lanes drawn (Q-MIX added at index 2); control rows still N=4 spread lanes (q-mix control wiring = follow-up)
ED_LANE_H = ED_H / ED_LANES
# Left-control rows align with the EDITOR lane centres (must match the hpp's rowY):
# each lane's CV jacks + attens sit beside the visual lane they modulate. Row == editor lane
# (no ESLOT remap — q-mix is a real lane, not a preview gap).
def rowY(r): return ED_Y + (r+0.5)*ED_LANE_H
def ctrlY(k): return rowY(k)   # control/marker row for editor lane k (identity)
# editor row -> ENGINE/spread poly lane (REST0/MEL1/OCT2/ACC3/QMIX4). Mirrors
# dotModular::EDITOR_TO_ENGINE_LANE_QMIX (dsp/LaneMapping.hpp). East's SPREAD knobs bind by
# SPREAD/engine-lane id (sprPid[] + getSpread(slot, spreadLane), REST=0) while cv/atten bind by
# EDITOR lane (cvId/attenDispId + getMacroAtten by editor lane). So spread markers carry the
# ENGINE id at the editor row; cv/atten stay editor-indexed. Emitting editor ids for spread put
# REST's spread on the MEL row (the reported East mixup).
EDITOR_TO_ENGINE=[1,2,4,0,3]   # MEL->1 OCT->2 QMIX->4 REST->0 ACC->3
# 4 CV jacks + 4 attens + spread base — columns match SandsMonoVisual, ED_X=88
JACK_X  = [6.0, 15.0, 24.0, 33.0]   # LEN/OFF/ROT/SPR-cv
ATTEN_X = [43.0, 52.0, 61.0, 70.0]  # LEN/OFF/ROT/SPR depth
SPREAD_X = 80.0                      # spread base trimpot
# (DISPLAY_ORDER removed — rows are EDITOR lanes 0..6 directly (MEL/OCT/QMIX/REST/ACC/VAR/LEG);
#  component ids are editor-lane indexed to match the C++ binds via LaneMapping, no remap.)

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
    _dir = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(_dir, "..", "res", "logo", "dot-modular-logo-dark.svg" if dark else "dot-modular-logo-light.svg")
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]   # drop the two leading bg/accent rects
    # Nondestructive wordmark crop (viewBox-style zoom): strip the two <polyline>
    # positioning brackets (top-left & bottom-right registration marks for physical module
    # placement) and zoom to the wordmark bbox (matches dot-modular-logo-*-tight.svg:
    # x26 y65 w657 h95) so the wordmark fills target_w_mm with no blank margins. Done as a
    # translate/scale transform — nanosvg-safe (no clip / nested <svg>); source untouched.
    body = _re.sub(r'<polyline\b[^>]*/>', '', body)
    CX, CY, CW = 26.0, 65.0, 657.0
    sc = px(target_w_mm) / CW
    tx, ty = px(x_mm), px(y_mm)
    return f'<g transform="translate({tx:.2f},{ty:.2f}) scale({sc:.5f}) translate({-CX:.2f},{-CY:.2f})">{body}</g>'

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
    for k in range(1,ED_LANES):   # 6 lanes → 5 dividers (was /4, a 4-lane leftover)
        ly=ED_Y+k*ED_LANE_H
        A(f'<line x1="{px(ED_X+1):.1f}" y1="{px(ly):.1f}" x2="{px(ED_X+ED_W-1):.1f}" y2="{px(ly):.1f}" stroke="{t["edborder"]}" stroke-width="0.75" opacity="0.7"/>')

    # ── per-lane owner-source block, right of the editor before the prob-outs.
    #    The faint backing + separator + "SRC" label form the container; the LIVE
    #    OwnerCell widget draws each per-lane cell itself (filled = global/Macro,
    #    outline in lane colour = East/per-voice), so no static outline cells here.
    _ed_right = ED_X + ED_W
    _lane_ys = [ctrlY(r) for r in range(N)]   # all 7 editor lanes
    _ch = ED_LANE_H * 0.9                  # match the live OwnerCell (≈ lane step cell)
    _cw = (ED_W - 2*6.0) / 16.0            # one editor step wide (padding=6, 16 steps)
    _pad = 1.6
    _by = min(_lane_ys) - _ch*0.5 - _pad
    _bh = (max(_lane_ys)-min(_lane_ys)) + _ch + 2*_pad
    A(f'<rect x="{px(OWNER_X-_cw*0.5-_pad):.1f}" y="{px(_by):.1f}" width="{px(_cw+2*_pad):.1f}" height="{px(_bh):.1f}" rx="{px(1.0):.1f}" fill="#ffffff" fill-opacity="0.05"/>')
    A(f'<line x1="{px(_ed_right+1.4):.1f}" y1="{px(_by):.1f}" x2="{px(_ed_right+1.4):.1f}" y2="{px(_by+_bh):.1f}" stroke="{t["edborder"]}" stroke-width="1.2" opacity="0.8"/>')
    A(f'<text x="{px(OWNER_X):.1f}" y="{px(_by-1.4):.1f}" font-family="sans-serif" font-size="{px(2.0):.1f}" fill="{t["edborder"]}" text-anchor="middle">SRC</text>')

    # ── below the editor + footer: Marina Bay water (waves) as an integrated
    #    base band spanning the full control-to-edge width ──
    A(waves(ED_X, 112.0, t, op=0.6, rows=3, span_mm=ED_W))
    # a crisp MBS as the identity mark, sat on the water, lower-right
    A(mbs(W_MM-66.0, 111.0, 52.0, 14.0, t, op=0.85))

    # ── control group recess (left) — wraps the editor-aligned rows ──
    # box left edge clears the leftmost jack (JACK_X[0]=6, r=3.9 → left extent 2.1mm)
    gx,gy=1.5,ctrlY(0)-ED_LANE_H*0.5-3.0
    # recess spans all 7 control rows (5 spread lanes + VAR/LEG rows 5,6)
    gw,gh=(SPREAD_X+6.0)-gx,(ctrlY(N-1)+ED_LANE_H*0.5+3.0)-gy
    A(f'<rect x="{px(gx):.1f}" y="{px(gy):.1f}" width="{px(gw):.1f}" height="{px(gh):.1f}" rx="{px(2):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
    sepx=0.5*(JACK_X[-1]+ATTEN_X[0])  # separator between jack cluster and atten cluster
    A(f'<line x1="{px(sepx):.1f}" y1="{px(gy+2):.1f}" x2="{px(sepx):.1f}" y2="{px(gy+gh-2):.1f}" stroke="{t["groupline"]}" stroke-width="0.75" opacity="0.6"/>')
    A('</g>')
    A(f'<g inkscape:label="branding" inkscape:groupmode="layer">')
    A(logo_embed(dark, x_mm=11.0, y_mm=113.0, target_w_mm=42.0))
    A('</g>')
    A(f'<g inkscape:label="control-graphics" inkscape:groupmode="layer">')
    def jack(x,y):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.7):.1f}" fill="{t["edrecess"]}"/>')
    def trim(x,y,col):
        A(f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>')
        A(f'<line x1="{px(x):.1f}" y1="{px(y):.1f}" x2="{px(x):.1f}" y2="{px(y-2.4):.1f}" stroke="{col}" stroke-width="1"/>')
    # 5 SPREAD lanes at editor rows 0..4 (MEL/OCT/QMIX/REST/ACC): 4 CV jacks + 4 attens + spread.
    # q-mix is a PLAIN lane at editor row 2 — identical complement, no special-casing (ESLOT gone).
    for el in range(POLY_LANES):
        y=rowY(el)
        for x in JACK_X:  jack(x,y)
        for x in ATTEN_X: trim(x,y,t["gold"])
        trim(SPREAD_X,y,t["teal"])
    # VARIATION (row 5) / LEGATO (row 6): LEN/OFF/ROT only — no SPR jack, no spread knob
    # (spread does not apply to these mono-strand lanes). Attens in gold like the others.
    for el in range(POLY_LANES, ED_LANES):
        y=rowY(el)
        for x in JACK_X[:3]:  jack(x,y)
        for x in ATTEN_X[:3]: trim(x,y,t["gold"])

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
    # ── (REMOVED) The old below-lanes "BASE" owner section. Ownership now lives in
    #    the per-lane SRC owner cells right of the editor (param_owner_{lane} bound
    #    to OwnerCell). The Macro mix-in sends had already moved to the Macro panel,
    #    so this band held nothing but the old owner latch and is gone entirely.
    A('</g>')
    # ── SvgPanelKit component layer. ALL ids are EDITOR-lane indexed, matching the C++ binds in
    #    StraitsEastSandsVisual.cpp exactly (no engine-order DISPLAY_ORDER remap — that was the
    #    "shape not found" root cause). editor lane el: 0 MEL,1 OCT,2 QMIX,3 REST,4 ACC,5 VAR,6 LEG.
    #      cvId(el,c)        = CV_START(0)      + el*4 + c   inputs 0..19   (5 spread lanes × 4)
    #      attenDispId(el,c) = ATTEN_START(4)   + el*4 + c   params 4..23
    #      SPREAD_R/M/O/A/Q  = params 0..4  (positional by editor lane — sprPid[] in the cpp)
    #      varlegCvId(vl,c)  = VARLEG_CV_START(20)        + vl*3 + c  inputs 20..25 (vl 0=VAR,1=LEG)
    #      varlegAttDispId   = VARLEG_ATTEN_DISP_START(20)+ vl*3 + c  params 20..25
    #      dirModId(el)=DIR_MOD_START(26)+el  delegModId(el)=DELEG_MOD_START(33)+el  (el 0..6)
    #      output_prob_<el>, param_owner_<el>, param_dir_<el> — all editor lane. ──
    def kit_shape(kind, idx, x, y):
        A(f'<circle id="{kind}_{idx}" cx="{px(x):.2f}" cy="{px(y):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('<g inkscape:label="components" inkscape:groupmode="layer">')
    # 5 spread lanes (editor 0..4 incl QMIX at 2): CV jacks + attens + spread base.
    # cv/atten are EDITOR-lane indexed (cvId(el,c)/attenDispId(el,c) + getMacroAtten by editor
    # lane) — keep el. SPREAD is ENGINE/spread-lane indexed (sprPid[]/getSpread(slot,spreadLane),
    # REST=0) — emit param_<eng> at the editor row so the REST-spread knob lands on the REST row.
    for el in range(POLY_LANES):
        y=rowY(el); eng=EDITOR_TO_ENGINE[el]
        for p,x in enumerate(JACK_X):  kit_shape("input", 0 + el*4 + p, x, y)   # cvId(el,c) — editor lane
        for p,x in enumerate(ATTEN_X): kit_shape("param", 4 + el*4 + p, x, y)   # attenDispId(el,c) — editor lane
        kit_shape("param", eng, SPREAD_X, y)   # SPREAD_R..Q = param <engine lane> (positional at editor row)
    # VARIATION (editor 5) / LEGATO (editor 6): VAR/LEG CV jacks + depth attens (LEN/OFF/ROT).
    for vl in range(2):
        y=rowY(POLY_LANES+vl)   # editor rows 5,6
        for c in range(3):
            kit_shape("input", 20 + vl*3 + c, JACK_X[c], y)   # varlegCvId(vl,c)
            kit_shape("param", 20 + vl*3 + c, ATTEN_X[c], y)  # varlegAttDispId(vl,c)
    # Owner cells (param_owner_<editorLane>) — 5 poly (0..4) + VAR/LEG (5,6). All editor lane.
    for el in range(ED_LANES):
        A(f'<circle id="param_owner_{el}" cx="{px(OWNER_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Direction cells (param_dir_<editorLane>) — one per editor lane 0..6.
    for el in range(ED_LANES):
        A(f'<circle id="param_dir_{el}" cx="{px(DIR_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Direction + delegation gate-mod jacks — one per editor lane 0..6.
    for el in range(ED_LANES):
        A(f'<circle id="input_dir_mod_{el}" cx="{px(DIR_MOD_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
        A(f'<circle id="input_deleg_mod_{el}" cx="{px(DELEG_MOD_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    # Probability-out jacks (output_prob_<editorLane>) — 5 poly prob CV outs (incl QMIX at 2).
    for el in range(POLY_LANES):
        A(f'<circle id="output_prob_{el}" cx="{px(PROB_OUT_X):.2f}" cy="{px(rowY(el)):.2f}" '
          f'r="0.5" fill="none" stroke="none"/>')
    A(f'<circle id="light_connect" cx="{px(W_MM*0.5):.2f}" cy="{px(124.0):.2f}" r="0.5" fill="none" stroke="none"/>')
    A('</g>')
    A('</svg>')
    return "\n".join(L)

_outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
for dark,name in [(True,"StraitsEastSandsVisual_48HP.svg"),(False,"StraitsEastSandsVisual_48HP_light.svg")]:
    svg=gen(dark)
    with open(os.path.join(_outdir, name),"w") as f: f.write(svg)
    print(f"{name}: {len(svg):,} bytes ({PW} x {PH})")
