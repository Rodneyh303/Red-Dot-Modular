#!/usr/bin/env python3
"""Temasek -- transform companion for Change Alley. 40HP plain panel.
All labels/controls drawn by the widget. Art deferred until functionality is proven.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, svg_open, logo_embed, jack, trim

HP     = 40
PW_MM  = HP * 5.08
PH_MM  = 128.5

def pal(dark):
    if dark:
        return dict(body="#18181a", ink="#e8e2d0", dim="#8a8578",
                    frame="#2e2e33", jackwell="#0a0b0c", jackring="#46464c",
                    well="#0f1012", wellring="#3a3a40", edrecess="#101113",
                    edborder="#2e2e33", tabband="#181820",
                    red="#d4001a", gold="#c8960c")
    return  dict(body="#e8e8ea", ink="#2a2a2e", dim="#888d96",
                 frame="#a8aeb6", jackwell="#dadce0", jackring="#9298a0",
                 well="#dcdee2", wellring="#a8aeb6", edrecess="#d8dade",
                 edborder="#c0c4ca", tabband="#cdd4dc",
                 red="#d4001a", gold="#a07808")

MARGIN  = 5.0
CX      = PW_MM / 2
N_VERBS = 4
N_ROWS  = N_VERBS * 2
ROW_H   = (PH_MM - 16.0) / N_ROWS
ROW_TOP = 8.0

J_OUTER = MARGIN
BTN_D   = MARGIN + 8.5
KNOB1   = MARGIN + 17.5
KNOB2   = MARGIN + 27.0
BTN_C   = MARGIN + 36.0
J_INNER = MARGIN + 44.5

def rowY(verb, sub):
    return ROW_TOP + (verb * 2 + sub + 0.5) * ROW_H

def lx(x_mm, flip):
    return (PW_MM - x_mm) if flip else x_mm

def gen(dark):
    t = pal(dark)
    els = []
    E = els.append

    E(f'<rect width="{px(PW_MM):.1f}" height="{px(PH_MM):.1f}" fill="{t["body"]}"/>')

    # centre divider
    E(f'<line x1="{px(CX):.1f}" y1="{px(4.0):.1f}" x2="{px(CX):.1f}" y2="{px(PH_MM-4.0):.1f}"')
    E(f'  stroke="{t["dim"]}" stroke-width="{px(0.3):.2f}" opacity="0.5"/>')

    for verb in range(N_VERBS):
        for sub in range(2):
            ry = rowY(verb, sub)
            for side in range(2):
                flip = (side == 1)
                E(jack(lx(J_OUTER, flip), ry, t))
                E(jack(lx(J_INNER, flip), ry, t))
                for bx in [BTN_D, BTN_C]:
                    E(f'<circle cx="{px(lx(bx,flip)):.1f}" cy="{px(ry):.1f}" r="{px(2.8):.1f}"')
                    E(f'  fill="{t["frame"]}" stroke="{t["dim"]}" stroke-width="{px(0.5):.2f}"/>')
                E(trim(lx(KNOB1, flip), ry, t, t["gold"]))
                if verb in (0, 1):
                    E(trim(lx(KNOB2, flip), ry, t, t["gold"]))
                if verb == 3:
                    fwd_y = ry - ROW_H * 0.22
                    bk_y  = ry + ROW_H * 0.22
                    E(jack(lx(J_OUTER - 0.0, flip), fwd_y, t))
                    E(jack(lx(J_OUTER - 0.0, flip), bk_y,  t))

    E(logo_embed(dark, CX - 17.0, PH_MM - 11.0, 34.0))

    out_dir = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    os.makedirs(out_dir, exist_ok=True)
    theme = "dark" if dark else "light"
    fname = os.path.join(out_dir, f"Temasek_panel_{theme}.svg")
    with open(fname, "w") as f2:
        f2.write(svg_open(px(PW_MM), px(PH_MM)) + "\n" + "\n".join(els) + "\n</svg>\n")
    print(f"Temasek {theme}: {fname}")

if __name__ == "__main__":
    gen(dark=True)
    gen(dark=False)
