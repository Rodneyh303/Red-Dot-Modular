#!/usr/bin/env python3
"""Intertropical scene-sequencer panel generator (v3 -- routing grid + transpose, Lantern-aligned).

res/panels/Intertropical_panel_{dark,light}.svg

Layout:
  brand strip (top)
  LEFT  block  = per-scene ARRANGEMENT:
     repeat strip (8 scenes x 4 segments, ABOVE the grid)   <- max repeats 4 (was 8)
     voice-number gutter + 16-voice x 8-scene MEMBERSHIP grid (6.0mm pitch = Lantern laneH)
  RIGHT block  = global SETUP (8 outputs + everything global about them):
     8x8 slot->output ROUTING grid (fan-out = >1 lit cell in a slot row)
     row of 8 per-output TRANSPOSE knobs (+/-24 semis, detented), aligned to output columns
  poly OUTPUT jacks (bottom)

Continuous-display style (dark screen both themes, bezel themes). Panel art is STATIC geometry;
all live state (membership fill via voiceColour, active scene, repeat count+progress, playhead,
routing cell fills, voice numbers) is widget-drawn. Store-backed, no params.

Alignment: the membership grid's 16 rows use 6.0mm pitch (= Lantern's laneH = 96/16). The repeat
strip pushes the grid top DOWN; Lantern's LCD is shifted down to the same top so the two grids
line up (Lantern has the headroom). GRID_TOP here is the shared top.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dotmod_design as D
from dotmod_design import px, theme, svg_open, logo_embed, jack, trim

# ---- panel ----
HP        = 25
PW_MM     = HP * 5.08          # 127.0mm
PH_MM     = 128.5
MARGIN    = 5.0

N_SCENES  = 8
N_VOICES  = 16
N_OUTPUTS = 8
N_SLOTS   = 8
MAX_REPEAT= 4                  # was 8 -- reduced for bigger/clearer controls

LANE_PITCH = 6.0               # = Lantern laneH (96/16); grids align on this

# ---- vertical bands ----
BRAND_Y   = 5.0
REP_Y     = 15.5               # repeat strip top
REP_H     = 9.0                # repeat strip height (4 segments per scene)
GRID_TOP  = REP_Y + REP_H + 3.0    # ~27.5mm -- SHARED top; Lantern LCD shifts here too
GRID_H    = N_VOICES * LANE_PITCH   # 96.0mm
GRID_BOT  = GRID_TOP + GRID_H       # ~123.5mm
OUT_Y     = PH_MM - 3.5             # jack row centre (bottom edge)

# ---- horizontal split: left membership block | right global block ----
GUTTER    = 5.5                     # voice-number gutter
MEM_L     = MARGIN + GUTTER
MEM_W     = 56.0                    # membership grid width (8 scenes)
MEM_R     = MEM_L + MEM_W
COL_W     = MEM_W / N_SCENES        # 7.0mm per scene -- clickable

GAPX      = 5.0
RT_L      = MEM_R + GAPX            # right block left edge
RT_W      = PW_MM - MARGIN - RT_L   # remaining width for routing grid
ROUT_CW   = RT_W / N_OUTPUTS        # routing cell width (output cols)
ROUT_ROWH = min(6.0, ROUT_CW)       # routing cell height (slot rows), keep near-square
ROUT_H    = N_SLOTS * ROUT_ROWH
ROUT_TOP  = GRID_TOP                # align routing grid top with membership grid top

SCR   = "#101216"
GLINE = "#2a2f37"

def screen(x, y, w, h, t):
    return (f'<rect x="{px(x):.1f}" y="{px(y):.1f}" width="{px(w):.1f}" height="{px(h):.1f}" '
            f'rx="{px(1.5):.1f}" fill="{SCR}" stroke="{t["edborder"]}" stroke-width="1"/>')

def vlines(x, y, w, h, cols, sw=0.6, op=0.7):
    out=[]
    for c in range(1, cols):
        lx=x+c*(w/cols)
        out.append(f'<line x1="{px(lx):.1f}" y1="{px(y+1):.1f}" x2="{px(lx):.1f}" y2="{px(y+h-1):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>')
    return "".join(out)

def hlines(x, y, w, h, rows, sw=0.6, op=0.7):
    out=[]
    for r in range(1, rows):
        ly=y+r*(h/rows)
        out.append(f'<line x1="{px(x+1):.1f}" y1="{px(ly):.1f}" x2="{px(x+w-1):.1f}" y2="{px(ly):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>')
    return "".join(out)

def build(dark):
    t=theme(dark); PW,PH=px(PW_MM),px(PH_MM)
    s=[svg_open(PW,PH)]
    s.append(f'<rect x="0" y="0" width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    s.append(logo_embed(dark, MARGIN, BRAND_Y, 32.0))

    # --- REPEAT strip: 8 scenes x 4 segments, above the grid ---
    s.append(screen(MEM_L, REP_Y, MEM_W, REP_H, t))
    s.append(vlines(MEM_L, REP_Y, MEM_W, REP_H, N_SCENES, sw=0.8, op=0.85))     # scene bounds
    for c in range(N_SCENES):
        s.append(hlines(MEM_L+c*COL_W, REP_Y, COL_W, REP_H, MAX_REPEAT, sw=0.4, op=0.45))

    # --- MEMBERSHIP grid: 16 voices x 8 scenes, 6.0mm pitch ---
    s.append(screen(MEM_L, GRID_TOP, MEM_W, GRID_H, t))
    s.append(vlines(MEM_L, GRID_TOP, MEM_W, GRID_H, N_SCENES))
    s.append(hlines(MEM_L, GRID_TOP, MEM_W, GRID_H, N_VOICES))
    # voice-number gutter recess (numbers widget-drawn)
    s.append(f'<rect x="{px(MARGIN):.1f}" y="{px(GRID_TOP):.1f}" width="{px(GUTTER-1.0):.1f}" height="{px(GRID_H):.1f}" rx="{px(1.0):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="0.75"/>')

    # --- RIGHT: global 8x8 slot->output routing grid ---
    s.append(screen(RT_L, ROUT_TOP, RT_W, ROUT_H, t))
    s.append(vlines(RT_L, ROUT_TOP, RT_W, ROUT_H, N_OUTPUTS))
    s.append(hlines(RT_L, ROUT_TOP, RT_W, ROUT_H, N_SLOTS))

    # --- RIGHT: row of 8 per-output TRANSPOSE knobs, aligned to output columns ---
    kn_y = ROUT_TOP + ROUT_H + 8.0
    for o in range(N_OUTPUTS):
        kx = RT_L + (o + 0.5) * ROUT_CW
        s.append(trim(kx, kn_y, t, t["gold"]))

    # --- poly OUTPUT jacks: bottom-RIGHT, under the routing/transpose block (near the outputs
    #     they define). Frees the membership grid's full height. 5 jacks across the right block. ---
    names=["gate","cv","accent","legato","sleg"]
    jack_y = kn_y + 12.0
    for i,nm in enumerate(names):
        jx = RT_L + (i+0.5)*(RT_W/len(names))
        s.append(jack(jx, jack_y, t))

    s.append('</svg>')
    return "".join(s)

def main():
    root=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..")
    outdir=os.path.join(root,"res","panels"); os.makedirs(outdir,exist_ok=True)
    for dark,suf in [(True,"dark"),(False,"light")]:
        svg=build(dark); p=os.path.join(outdir,f"Intertropical_panel_{suf}.svg")
        open(p,"w").write(svg); print(f"wrote {p} ({len(svg)} bytes)")

if __name__=="__main__": main()
