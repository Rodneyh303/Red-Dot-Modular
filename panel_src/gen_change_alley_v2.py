#!/usr/bin/env python3
"""Change Alley V2 -- ONE module: pin matrix + full transform controls. 48HP.

    INTRA controls  |  16x16 GRID  |  INTER controls
Mirrored, jacks to the outside. Rows: COLLAPSE ROTATE REFLECT SCATTER (R,M each).
Bottom-right cluster under the last REFLECT row: logo (left), legend + 2 poly mod jacks.

Widget draws all labels (nanosvg drops <text>): verb names both sides ("COLLAPSE INTRA"
left, "COLLAPSE INTER" right), column captions, grid numbers, legend.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, svg_open, logo_embed, jack, trim

HP     = 48
PW_MM  = HP * 5.08          # 243.84
PH_MM  = 128.5

# control block (per side); right mirrors
MARGIN  = 6.0
J_DOM   = MARGIN +  0.0
J_COD   = MARGIN +  9.5
KNOB1   = MARGIN + 18.0
KNOB2   = MARGIN + 26.0
BTN_D   = MARGIN + 33.5
BTN_C   = MARGIN + 40.0
J_BACK2 = MARGIN + 46.5
CTRL_W  = J_BACK2 + 4.45      # 57.0
GUTTER  = 8.0

GRID_X  = CTRL_W + GUTTER
GRID_W  = PW_MM - 2 * (CTRL_W + GUTTER)
CELL    = GRID_W / 16.0
GRID_Y  = 20.0
GRID_H  = CELL * 16.0

N_VERBS   = 4
ROW_H     = 9.0
GROUP_GAP = 6.8
ROW_TOP   = 21.0

def rowY(verb, sub):
    return ROW_TOP + verb * (2.0 * ROW_H + GROUP_GAP) + sub * ROW_H + ROW_H * 0.5

def lastRowBottom():
    return rowY(N_VERBS - 1, 1) + ROW_H * 0.5

def lx(x, flip): return (PW_MM - x) if flip else x

def pal(dark):
    base = dict(red="#d4001a", gold="#c8960c")
    if dark:
        return dict(base, body="#18181a", ink="#e8e2d0", dim="#8a8578",
                    frame="#2e2e33", jackwell="#0a0b0c", jackring="#46464c",
                    well="#0f1012", wellring="#3a3a40", edrecess="#101113",
                    edborder="#2e2e33", tabband="#181820", gridln="#26262b", booth="#141416")
    return dict(base, body="#e8e8ea", ink="#2a2a2e", dim="#888d96",
                frame="#a8aeb6", jackwell="#dadce0", jackring="#9298a0",
                well="#dcdee2", wellring="#a8aeb6", edrecess="#d8dade",
                edborder="#c0c4ca", tabband="#cdd4dc", gridln="#c4c8ce", booth="#d0d4da")

def gen(dark):
    t = pal(dark); els=[]; E=els.append
    E(f'<rect width="{px(PW_MM):.1f}" height="{px(PH_MM):.1f}" fill="{t["body"]}"/>')

    # grid
    E(f'<rect x="{px(GRID_X):.1f}" y="{px(GRID_Y):.1f}" width="{px(GRID_W):.1f}"'
      f' height="{px(GRID_H):.1f}" fill="{t["well"]}" stroke="{t["edborder"]}" stroke-width="{px(0.4):.2f}"/>')
    for i in range(1,16):
        gx=GRID_X+i*CELL; gy=GRID_Y+i*CELL
        E(f'<line x1="{px(gx):.1f}" y1="{px(GRID_Y):.1f}" x2="{px(gx):.1f}" y2="{px(GRID_Y+GRID_H):.1f}" stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')
        E(f'<line x1="{px(GRID_X):.1f}" y1="{px(gy):.1f}" x2="{px(GRID_X+GRID_W):.1f}" y2="{px(gy):.1f}" stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')
    for b in range(0,16,8):
        E(f'<rect x="{px(GRID_X+b*CELL):.1f}" y="{px(GRID_Y):.1f}" width="{px(8*CELL):.1f}" height="{px(GRID_H):.1f}" fill="{t["booth"]}" opacity="{0.3 if (b//8)%2 else 0.0:.2f}"/>')

    # controls
    for verb in range(N_VERBS):
        for sub in range(2):
            ry=rowY(verb,sub)
            for side in range(2):
                flip=(side==1)
                E(jack(lx(J_DOM,flip),ry,t)); E(jack(lx(J_COD,flip),ry,t))
                E(trim(lx(KNOB1,flip),ry,t,t["gold"]))
                if verb in (0,1): E(trim(lx(KNOB2,flip),ry,t,t["gold"]))
                elif verb==3:
                    E(jack(lx(KNOB2,flip),ry,t)); E(jack(lx(J_BACK2,flip),ry,t))
                for bx in (BTN_D,BTN_C):
                    E(f'<circle cx="{px(lx(bx,flip)):.1f}" cy="{px(ry):.1f}" r="{px(2.6):.1f}" fill="{t["frame"]}" stroke="{t["dim"]}" stroke-width="{px(0.5):.2f}"/>')

    # ── bottom-right cluster, under the last REFLECT row ──────────────────────
    by = lastRowBottom() + 8.0
    # logo LEFT (under last reflect row, left side)
    E(logo_embed(dark, MARGIN, by, 34.0))
    # legend + 2 poly mod jacks RIGHT (mirrored side)
    # poly mod jacks: GRAIN (16ch) and STEP (8ch)
    rx = PW_MM - MARGIN - 4.45
    E(jack(rx,        by + 2.0, t))     # STEP poly (ch 1-8)
    E(jack(rx - 9.5,  by + 2.0, t))     # GRAIN poly (ch 1-16)
    # (legend text is widget-drawn to the left of these jacks)

    out=os.path.join(os.path.dirname(__file__),"..","res","panels")
    os.makedirs(out,exist_ok=True)
    theme="dark" if dark else "light"
    fn=os.path.join(out,f"ChangeAlleyV2_panel_{theme}.svg")
    open(fn,"w").write(svg_open(px(PW_MM),px(PH_MM))+"\n"+"\n".join(els)+"\n</svg>\n")
    print(f"ChangeAlleyV2 {theme}: {HP}HP grid {GRID_W:.1f}mm cell {CELL:.2f}mm")

if __name__=="__main__":
    gen(True); gen(False)
