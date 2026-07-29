#!/usr/bin/env python3
"""Intertropical scene-sequencer panel generator (v4 -- wider, kit markers, legend, logo bottom).

res/panels/Intertropical_panel_{dark,light}.svg

The panel draws STATIC background only: screens, gridline lattices, wells, legend labels, brand,
and the expander connect mark. It does NOT draw knobs/jacks -- it emits invisible kit_shape
markers (param_/output_/light_connect) that the widget binds REAL SvgPanelKit assets to at those
centres (single-source-geometry, same pattern as every other dot.modular panel). All live state
(membership fill, active scene, repeat progress, routing fills, playhead, voice numbers) is
widget-drawn.

Layout (LEFT = per-scene arrangement, RIGHT = global output setup):
  LEFT : repeat strip (8 scenes x 4) above the 16-voice x 8-scene membership grid; voice# gutter
  RIGHT: 8x8 slot->output routing grid + SLOT/OUT legend; 8 per-output transpose knob wells
         (param_0..7); 5 poly output jacks (output_0..4) below
  BOTTOM-LEFT: dot.modular wordmark
  connect mark (light_connect): top-right, greyed when no Monsoon attached
Membership grid 6.0mm pitch (= Lantern laneH) so the two align.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dotmod_design as D
from dotmod_design import px, theme, svg_open, logo_embed, kit_shape

HP        = 34
PW_MM     = HP * 5.08          # 172.7mm
PH_MM     = 128.5
MARGIN    = 6.0

N_SCENES  = 8
N_VOICES  = 16
N_OUTPUTS = 8
N_SLOTS   = 8
MAX_REPEAT= 4
LANE_PITCH= 6.0                # = Lantern laneH

# vertical
REP_Y     = 14.0
REP_H     = 10.0
GRID_TOP  = REP_Y + REP_H + 3.5     # shared grid top (~27.5)
GRID_H    = N_VOICES * LANE_PITCH    # 96
GRID_BOT  = GRID_TOP + GRID_H        # ~123.5

# horizontal split
GUTTER    = 6.0
MEM_L     = MARGIN + GUTTER
MEM_W     = 78.0                     # roomy membership grid (8 scenes ~9.75mm each)
MEM_R     = MEM_L + MEM_W
COL_W     = MEM_W / N_SCENES

GAPX      = 10.0
RT_L      = MEM_R + GAPX
RT_W      = PW_MM - MARGIN - RT_L    # right block width
# routing grid: 8x8, cell height capped so the whole right column (routing+knobs+jacks) fits
ROUT_TOP  = GRID_TOP + 5.0           # room for SLOT->OUTPUT legend above
ROUT_CW   = RT_W / N_OUTPUTS
ROUT_ROWH = 5.2                      # capped (not square) so column fits vertically
ROUT_H    = N_SLOTS * ROUT_ROWH      # ~41.6mm

SCR="#101216"; GLINE="#2a2f37"

def screen(x,y,w,h,t):
    return (f'<rect x="{px(x):.1f}" y="{px(y):.1f}" width="{px(w):.1f}" height="{px(h):.1f}" '
            f'rx="{px(1.5):.1f}" fill="{SCR}" stroke="{t["edborder"]}" stroke-width="1"/>')

def vlines(x,y,w,h,cols,sw=0.6,op=0.7):
    return "".join(f'<line x1="{px(x+c*(w/cols)):.1f}" y1="{px(y+1):.1f}" x2="{px(x+c*(w/cols)):.1f}" y2="{px(y+h-1):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>' for c in range(1,cols))

def hlines(x,y,w,h,rows,sw=0.6,op=0.7):
    return "".join(f'<line x1="{px(x+1):.1f}" y1="{px(y+r*(h/rows)):.1f}" x2="{px(x+w-1):.1f}" y2="{px(y+r*(h/rows)):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>' for r in range(1,rows))

def well(x,y,r,t):
    return (f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(r):.1f}" fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="1.1"/>')

def jackwell(x,y,t):
    return (f'<circle cx="{px(x):.1f}" cy="{px(y):.1f}" r="{px(4.2):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1.1"/>')

def legend(x,y,txt,t,size=3.0,anchor="middle"):
    return (f'<text x="{px(x):.1f}" y="{px(y):.1f}" font-family="Barlow,sans-serif" '
            f'font-size="{px(size):.1f}" fill="{t["dim"]}" text-anchor="{anchor}">{txt}</text>')

def build(dark):
    t=theme(dark); PW,PH=px(PW_MM),px(PH_MM)
    s=[svg_open(PW,PH), f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>']
    comps=[]   # invisible kit markers, appended last in a components group

    # REPEAT strip (8 scenes x 4)
    s.append(screen(MEM_L, REP_Y, MEM_W, REP_H, t))
    s.append(vlines(MEM_L, REP_Y, MEM_W, REP_H, N_SCENES, sw=0.8, op=0.85))
    for c in range(N_SCENES):
        s.append(hlines(MEM_L+c*COL_W, REP_Y, COL_W, REP_H, MAX_REPEAT, sw=0.4, op=0.45))
    s.append(legend(MARGIN+GUTTER*0.4, REP_Y+REP_H*0.6, "REP", t, 2.6, "middle"))

    # MEMBERSHIP grid
    s.append(screen(MEM_L, GRID_TOP, MEM_W, GRID_H, t))
    s.append(vlines(MEM_L, GRID_TOP, MEM_W, GRID_H, N_SCENES))
    s.append(hlines(MEM_L, GRID_TOP, MEM_W, GRID_H, N_VOICES))
    # voice# gutter recess (numbers widget-drawn)
    s.append(f'<rect x="{px(MARGIN):.1f}" y="{px(GRID_TOP):.1f}" width="{px(GUTTER-1.0):.1f}" height="{px(GRID_H):.1f}" rx="{px(1.0):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="0.75"/>')
    # scene-number legend below the grid
    for c in range(N_SCENES):
        s.append(legend(MEM_L+(c+0.5)*COL_W, GRID_BOT+3.5, str(c+1), t, 2.8))
    s.append(legend(MEM_L+MEM_W*0.5, GRID_BOT+6.8, "SCENE", t, 2.6))

    # RIGHT: routing grid + legends
    s.append(legend(RT_L+RT_W*0.5, ROUT_TOP-2.0, "SLOT  \u2192  OUTPUT", t, 2.8))
    s.append(screen(RT_L, ROUT_TOP, RT_W, ROUT_H, t))
    s.append(vlines(RT_L, ROUT_TOP, RT_W, ROUT_H, N_OUTPUTS))
    s.append(hlines(RT_L, ROUT_TOP, RT_W, ROUT_H, N_SLOTS))
    for o in range(N_OUTPUTS):
        s.append(legend(RT_L+(o+0.5)*ROUT_CW, ROUT_TOP+ROUT_H+3.2, str(o+1), t, 2.6))
    for sl in range(N_SLOTS):
        s.append(legend(RT_L-2.2, ROUT_TOP+(sl+0.5)*ROUT_ROWH+1.0, str(sl+1), t, 2.6, "end"))

    # RIGHT: 8 transpose knob wells + markers (param_0..7), aligned to output columns
    kn_y = ROUT_TOP + ROUT_H + 12.0
    s.append(legend(RT_L+RT_W*0.5, kn_y-6.5, "OUTPUT TRANSPOSE  (\u00b124)", t, 2.8))
    for o in range(N_OUTPUTS):
        kx = RT_L + (o+0.5)*ROUT_CW
        s.append(well(kx, kn_y, 3.6, t))
        comps.append(kit_shape("param", o, kx, kn_y))

    # RIGHT: 5 poly output jacks + markers (output_0..4)
    names=["GATE","CV","ACC","LEG","SLG"]
    jy = kn_y + 16.0
    s.append(legend(RT_L+RT_W*0.5, jy-6.5, "POLY OUT", t, 2.8))
    for i,nm in enumerate(names):
        jx = RT_L + (i+0.5)*(RT_W/len(names))
        s.append(jackwell(jx, jy, t))
        s.append(legend(jx, jy+6.6, nm, t, 2.5))
        comps.append(kit_shape("output", i, jx, jy))

    # BOTTOM-LEFT: wordmark (clear of the right column)
    s.append(logo_embed(dark, MARGIN, PH_MM-10.0, 28.0))

    # connect mark (top-right), widget places makeConnectMark here
    ccx, ccy = PW_MM-MARGIN-6.0, 10.0
    comps.append(kit_shape("light", "connect", ccx, ccy))

    # components layer (invisible markers) last
    s.append('<g inkscape:label="components" inkscape:groupmode="layer">')
    s.extend(comps)
    s.append('</g>')
    s.append('</svg>')
    return "".join(s)

def main():
    root=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..")
    outdir=os.path.join(root,"res","panels"); os.makedirs(outdir,exist_ok=True)
    for dark,suf in [(True,"dark"),(False,"light")]:
        svg=build(dark); open(os.path.join(outdir,f"Intertropical_panel_{suf}.svg"),"w").write(svg)
        print(f"wrote Intertropical_panel_{suf}.svg ({len(svg)} bytes)")

if __name__=="__main__": main()
