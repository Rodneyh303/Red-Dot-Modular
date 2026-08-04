#!/usr/bin/env python3
"""Intertropical panel generator (v5 -- Lantern-aligned grid top, repeats below, no overlap).

res/panels/Intertropical_panel_{dark,light}.svg

HARD CONSTRAINTS (from the screenshot feedback):
- Membership grid TOP = 16mm, HEIGHT = 96mm  == Lantern's LCD exactly (Lantern.cpp:1087-88),
  so the two grids' 16 rows line up when placed side by side. Nothing sits above the grid except
  the brand row, so the REPEAT strip moved BELOW the grid (it can't push the grid down).
- 36HP wide with a clear gap so the membership grid and the right-hand routing block DO NOT
  overlap (the v4 bug).
- Panel draws STATIC background + WELLS + invisible kit_shape markers only. The widget binds real
  SvgPanelKit assets (knobs, jacks, connect mark) to the markers -- the panel never draws them.
  The OLD widget jack/knob positions are replaced by binding to THESE markers.

Layout:
  LEFT : membership grid (16 voices x 8 scenes, 16-112mm, 6.0mm pitch) + voice# gutter;
         REPEAT strip (8 scenes x 4) directly BELOW it.
  RIGHT: SLOT->OUTPUT routing grid (8x8) + legend; 8 output-transpose knob wells (param_0..7);
         5 poly-out jack wells (output_0..4) + names.
  brand wordmark bottom-left; light_connect marker top-right.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dotmod_design as D
from dotmod_design import px, theme, svg_open, logo_embed, kit_shape

# --- nanosvg-safe outlined text ----------------------------------------------------------------
# nanosvg does NOT render <text> (no font engine), so any STATIC label baked into the panel must be
# real path objects. We outline the few coordinate labels with fontTools (same technique as the
# logo). Widget-drawn labels still use <text> (NanoVG has a font engine at runtime) -- only the
# equatorial-band markers are outlined here because they're part of the static panel art.
_FONT_CACHE = {}
def _load_font(path):
    if path not in _FONT_CACHE:
        from fontTools.ttLib import TTFont
        f = TTFont(path)
        _FONT_CACHE[path] = (f, f.getGlyphSet(), f["cmap"].getBestCmap(),
                             f["head"].unitsPerEm)
    return _FONT_CACHE[path]

def outline_text(s, x_px, y_px, size_px, fill, anchor="start", opacity=1.0,
                 font="/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", letter=0.0):
    """Return SVG <path> objects tracing `s` at (x_px,y_px) baseline. size_px = cap size in px.
    anchor: start|middle|end. letter = extra tracking in px. nanosvg-safe (solid paths only)."""
    from fontTools.pens.svgPathPen import SVGPathPen
    ttf, gs, cmap, upm = _load_font(font)
    scale = size_px / upm
    # measure advance
    total = 0.0
    for ch in s:
        gname = cmap.get(ord(ch))
        if gname is None: continue
        total += gs[gname].width * scale + letter
    if s: total -= letter
    ox = x_px - (total if anchor == "end" else total/2 if anchor == "middle" else 0.0)
    out, pen_x = [], ox
    for ch in s:
        gname = cmap.get(ord(ch))
        if gname is None:
            pen_x += size_px*0.5 + letter; continue
        pen = SVGPathPen(gs)
        gs[gname].draw(pen)
        d = pen.getCommands()
        if d:
            # place glyph: translate to pen_x, flip Y (font y-up -> svg y-down) about baseline
            out.append(f'<path transform="translate({pen_x:.2f},{y_px:.2f}) scale({scale:.5f},{-scale:.5f})" '
                       f'd="{d}" fill="{fill}" fill-opacity="{opacity:.2f}"/>')
        pen_x += gs[gname].width * scale + letter
    return "".join(out)

HP=42; PW_MM=HP*5.08; PH_MM=128.5; MARGIN=6.0
N_SCENES=8; N_VOICES=16; N_OUTPUTS=8; N_SLOTS=8; MAX_REPEAT=4
LANE_PITCH=6.0

# vertical  grid matches Lantern exactly
GRID_TOP=16.0; GRID_H=N_VOICES*LANE_PITCH; GRID_BOT=GRID_TOP+GRID_H   # 16 -> 112
REP_H=6.0; REP_Y=GRID_TOP-REP_H-2.5                                    # repeats ABOVE grid

# horizontal
GUTTER=6.0
MEM_L=MARGIN+GUTTER; MEM_W=92.0; MEM_R=MEM_L+MEM_W                     # 12 -> 82
COL_W=MEM_W/N_SCENES
GAPX=8.0
RT_L=MEM_R+GAPX; RT_W=PW_MM-MARGIN-RT_L

# VOICE->SLOT grid (top of right block) -- read-only visualiser, smaller pitch
VS_TOP=GRID_TOP+4.0
VS_ROWH=4.1                    # increased from 3.5 -- more readable row pitch
VS_H=N_SLOTS*VS_ROWH           # 32.8mm

# SLOT->OUTPUT grid (below voice->slot) -- reference grid
VS_GAP=5.0                     # slightly larger gap above routing grid
ROUT_TOP=VS_TOP+VS_H+VS_GAP    # pushed down to give VS more room
ROUT_CW=RT_W/N_OUTPUTS
ROUT_ROWH=5.0                  # slightly larger than VS, still compact
ROUT_H=N_SLOTS*ROUT_ROWH       # 40mm

SCR="#101216"; GLINE="#2a2f37"

def screen(x,y,w,h,t):
    return f'<rect x="{px(x):.1f}" y="{px(y):.1f}" width="{px(w):.1f}" height="{px(h):.1f}" rx="{px(1.5):.1f}" fill="{SCR}" stroke="{t["edborder"]}" stroke-width="1"/>'
def vlines(x,y,w,h,cols,sw=0.6,op=0.7):
    return "".join(f'<line x1="{px(x+c*(w/cols)):.1f}" y1="{px(y+1):.1f}" x2="{px(x+c*(w/cols)):.1f}" y2="{px(y+h-1):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>' for c in range(1,cols))
def hlines(x,y,w,h,rows,sw=0.6,op=0.7):
    return "".join(f'<line x1="{px(x+1):.1f}" y1="{px(y+r*(h/rows)):.1f}" x2="{px(x+w-1):.1f}" y2="{px(y+r*(h/rows)):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>' for r in range(1,rows))
def well(x,y,r,t):
    return f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(r):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="1.1"/>'
def jackwell(x,y,t):
    return f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(4.4):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1.1"/><circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(1.9):.1f}" fill="{SCR}"/>'
def lab(x,y,txt,t,size=2.8,anchor="middle"):
    return f'<text x="{px(x):.1f}" y="{px(y):.1f}" font-family="Barlow,sans-serif" font-size="{px(size):.1f}" fill="{t["dim"]}" text-anchor="{anchor}">{txt}</text>'

def build(dark):
    t=theme(dark); PW,PH=px(PW_MM),px(PH_MM)
    s=[svg_open(PW,PH), f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>']
    comps=[]

    # MEMBERSHIP grid (Lantern-aligned)
    s.append(screen(MEM_L,GRID_TOP,MEM_W,GRID_H,t))
    s.append(vlines(MEM_L,GRID_TOP,MEM_W,GRID_H,N_SCENES))
    s.append(hlines(MEM_L,GRID_TOP,MEM_W,GRID_H,N_VOICES))
    s.append(f'<rect x="{px(MARGIN):.1f}" y="{px(GRID_TOP):.1f}" width="{px(GUTTER-1.0):.1f}" height="{px(GRID_H):.1f}" rx="{px(1.0):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="0.75"/>')

    # REPEAT strip BELOW the grid
    s.append(screen(MEM_L,REP_Y,MEM_W,REP_H,t))
    s.append(vlines(MEM_L,REP_Y,MEM_W,REP_H,N_SCENES,sw=0.8,op=0.85))
    for c in range(N_SCENES):
        s.append(hlines(MEM_L+c*COL_W,REP_Y,COL_W,REP_H,MAX_REPEAT,sw=0.4,op=0.45))
    s.append(lab(MARGIN+GUTTER*0.4,REP_Y+REP_H*0.62,"REP",t,2.4,"middle"))
    for c in range(N_SCENES):
        s.append(lab(MEM_L+(c+0.5)*COL_W,REP_Y-2.2,str(c+1),t,2.4))
    s.append(lab(MEM_L+MEM_W*0.5,REP_Y-4.8,"SCENE",t,2.3))

    # RIGHT: VOICE->SLOT visualiser (top, smaller pitch -- read-only, widget draws live fills)
    # For the active scene: which global voice (coloured+numbered) is in each slot row.
    s.append(lab(RT_L+RT_W*0.5,VS_TOP-2.0,"VOICE \u2192 SLOT",t,2.8))
    s.append(screen(RT_L,VS_TOP,RT_W,VS_H,t))
    s.append(vlines(RT_L,VS_TOP,RT_W,VS_H,N_SLOTS))
    s.append(hlines(RT_L,VS_TOP,RT_W,VS_H,N_SLOTS,sw=0.4,op=0.5))
    for sl in range(N_SLOTS):
        s.append(lab(RT_L-2.2,VS_TOP+(sl+0.5)*VS_ROWH+0.8,str(sl+1),t,2.2,"end"))

    # RIGHT: routing grid
    s.append(lab(RT_L+RT_W*0.5,ROUT_TOP-2.0,"SLOT  \u2192  OUTPUT",t,3.0))
    s.append(screen(RT_L,ROUT_TOP,RT_W,ROUT_H,t))
    s.append(vlines(RT_L,ROUT_TOP,RT_W,ROUT_H,N_OUTPUTS))
    s.append(hlines(RT_L,ROUT_TOP,RT_W,ROUT_H,N_SLOTS))
    for o in range(N_OUTPUTS):
        s.append(lab(RT_L+(o+0.5)*ROUT_CW,ROUT_TOP+ROUT_H+3.4,str(o+1),t,2.6))
    for sl in range(N_SLOTS):
        s.append(lab(RT_L-2.4,ROUT_TOP+(sl+0.5)*ROUT_ROWH+1.0,str(sl+1),t,2.6,"end"))

    # RIGHT: transpose knob wells (param_0..7)
    kn_y=ROUT_TOP+ROUT_H+8.0   # tightened from 13 (saved 5mm)
    s.append(lab(RT_L+RT_W*0.5,kn_y-6.5,"OUTPUT TRANSPOSE  (\u00b124)",t,3.0))
    for o in range(N_OUTPUTS):
        kx=RT_L+(o+0.5)*ROUT_CW
        s.append(well(kx,kn_y,4.2,t)); comps.append(kit_shape("param",o,kx,kn_y))

    # RIGHT: poly out jacks (output_0..4)
    names=["GATE","CV","ACC","LEG","SLG"]
    jy=kn_y+13.0               # tightened from 17 (saved 4mm)
    s.append(lab(RT_L+RT_W*0.5,jy-6.5,"POLY OUT",t,3.0))
    for i,nm in enumerate(names):
        jx=RT_L+(i+0.5)*(RT_W/len(names))
        s.append(jackwell(jx,jy,t)); s.append(lab(jx,jy+7.0,nm,t,2.5))
        comps.append(kit_shape("output",i,jx,jy))

    # brand bottom-left
    # --- Equatorial-band theme (INTERTROPICAL) ---------------------------------------------------
    # The panel content lives in the intertropical band: a red 1degN line (with the brand red DOT,
    # Singapore's latitude / the "red dot") across the TOP, and the gold 0deg EQUATOR line (with a
    # marker dot) across the BOTTOM. Content sits between them = the zone. Labels are OUTLINED
    # (fontTools) so nanosvg renders them; the degree ring "o" is drawn as a small circle object.
    RED = "#d4001a"; GOLD = t["gold"]
    LINE_L = px(MARGIN); LINE_R = px(PW_MM - MARGIN)
    def deg_label(x_mm, y_base_mm, txt, fill, anchor="start"):
        # Outline the WHOLE label (including the actual degree glyph) via fontTools -> real path
        # objects nanosvg renders. No <text>, no hand-placed ring.
        return outline_text(txt, px(x_mm), px(y_base_mm), px(2.6), fill,
                            anchor=anchor, opacity=0.85, letter=px(0.12))
    # TOP: red 1degN line + centre red dot (the brand "red dot" at Singapore's latitude)
    y1 = px(13.0)
    s.append(f'<line x1="{LINE_L:.1f}" y1="{y1:.1f}" x2="{LINE_R:.1f}" y2="{y1:.1f}" '
             f'stroke="{RED}" stroke-width="1.1" stroke-opacity="0.9"/>')
    s.append(f'<circle cx="{px(PW_MM/2):.1f}" cy="{y1:.1f}" r="{px(0.9):.1f}" fill="{RED}"/>')
    s.append(deg_label(PW_MM/2 + 3.0, 13.0 + 0.9, "1\u00b0N", RED))
    # BOTTOM: gold 0deg equator line + marker dot
    y0 = px(115.0)
    s.append(f'<line x1="{LINE_L:.1f}" y1="{y0:.1f}" x2="{LINE_R:.1f}" y2="{y0:.1f}" '
             f'stroke="{GOLD}" stroke-width="0.9" stroke-opacity="0.8"/>')
    s.append(f'<circle cx="{px(PW_MM/2):.1f}" cy="{y0:.1f}" r="{px(0.8):.1f}" fill="{GOLD}"/>')
    s.append(deg_label(MARGIN + 1.0, 115.0 + 3.4, "0\u00b0", GOLD))

    s.append(logo_embed(dark,MARGIN,PH_MM-9.5,28.0))    # connect mark top-right
    comps.append(kit_shape("light","connect",PW_MM-MARGIN-6.0,9.0))

    s.append('<g inkscape:label="components" inkscape:groupmode="layer">'); s.extend(comps); s.append('</g></svg>')
    return "".join(s)

def main():
    root=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..")
    outdir=os.path.join(root,"res","panels"); os.makedirs(outdir,exist_ok=True)
    for dark,suf in [(True,"dark"),(False,"light")]:
        svg=build(dark); open(os.path.join(outdir,f"Intertropical_panel_{suf}.svg"),"w").write(svg)
        print(f"wrote {suf} ({len(svg)}b)")

if __name__=="__main__": main()
