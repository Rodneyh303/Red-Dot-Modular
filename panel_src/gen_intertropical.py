#!/usr/bin/env python3
"""Intertropical scene-sequencer panel generator (SCAFFOLD v2 -- continuous grid).

Produces res/panels/Intertropical_panel_{dark,light}.svg.

Grid style: CONTINUOUS DISPLAY, matching Lantern / the Sands visual editor -- a single
recessed DARK screen with a thin gridline lattice, NOT a field of discrete wells. Cells are
divisions of one continuous surface. Per the Sands rule (SandsVisualEditorV4.hpp): the SCREEN
stays dark on both themes (per-voice hues are tuned against dark; on a light ground they fall
below readable contrast and the display would lie about the data); only the BEZEL follows the
light/dark panel, so the screen reads as inset into the panel.

Layout (top -> bottom):
  brand strip | repeat-select screen (8 cols x 8 segments) | main grid screen
  (8 scenes x 16 voices, continuous) | poly outputs

Panel art is STATIC geometry only. All live state -- membership fill (voiceColour), active
scene highlight, current-repeat emphasis, playhead -- is widget-drawn OVER the screen.
Store-backed cells, no params (Intertropical is de-parammed from the start).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dotmod_design as D
from dotmod_design import px, theme, svg_open, logo_embed, jack

HP        = 22
PW_MM     = HP * 5.08
PH_MM     = 128.5
MARGIN    = 6.0

N_SCENES  = 8
N_VOICES  = 16
N_REPEATS = 8

BRAND_Y   = 6.0
REP_Y     = 15.0
REP_H     = 14.0
GRID_Y    = REP_Y + REP_H + 5.0
OUT_Y     = PH_MM - 11.0
GRID_BOT  = OUT_Y - 8.0

SCR_L     = MARGIN + 7.0
SCR_R     = PW_MM - MARGIN
SCR_W     = SCR_R - SCR_L
COL_W     = SCR_W / N_SCENES
GRID_H    = GRID_BOT - GRID_Y
ROW_H     = GRID_H / N_VOICES


def screen(x, y, w, h, t):
    """Recessed dark display screen. Screen fill ALWAYS dark; bezel follows the panel."""
    scr = "#101216"
    bez = t["edborder"]
    return (f'<rect x="{px(x):.1f}" y="{px(y):.1f}" width="{px(w):.1f}" height="{px(h):.1f}" '
            f'rx="{px(1.5):.1f}" fill="{scr}" stroke="{bez}" stroke-width="1"/>')


def lattice(x, y, w, h, cols, rows):
    """Thin gridlines dividing a screen into cols x rows -- the continuous grid look."""
    gl = "#2a2f37"
    out = []
    for c in range(1, cols):
        lx = x + c * (w / cols)
        out.append(f'<line x1="{px(lx):.1f}" y1="{px(y+1):.1f}" x2="{px(lx):.1f}" '
                   f'y2="{px(y+h-1):.1f}" stroke="{gl}" stroke-width="0.6" stroke-opacity="0.7"/>')
    for r in range(1, rows):
        ly = y + r * (h / rows)
        out.append(f'<line x1="{px(x+1):.1f}" y1="{px(ly):.1f}" x2="{px(x+w-1):.1f}" '
                   f'y2="{px(ly):.1f}" stroke="{gl}" stroke-width="0.6" stroke-opacity="0.7"/>')
    return "".join(out)


def build(dark):
    t = theme(dark)
    PW, PH = px(PW_MM), px(PH_MM)
    s = [svg_open(PW, PH)]
    s.append(f'<rect x="0" y="0" width="{PW}" height="{PH}" fill="{t["bg"]}"/>')

    s.append(logo_embed(dark, MARGIN, BRAND_Y, 34.0))

    s.append(screen(SCR_L, REP_Y, SCR_W, REP_H, t))
    s.append(lattice(SCR_L, REP_Y, SCR_W, REP_H, N_SCENES, N_REPEATS))

    s.append(screen(SCR_L, GRID_Y, SCR_W, GRID_H, t))
    s.append(lattice(SCR_L, GRID_Y, SCR_W, GRID_H, N_SCENES, N_VOICES))

    for r in range(N_VOICES):
        cy = GRID_Y + (r + 0.5) * ROW_H
        s.append(screen(MARGIN + 1.5, cy - ROW_H*0.32, 4.0, ROW_H*0.64, t))

    names = ["gate", "cv", "accent", "legato", "sleg"]
    n = len(names)
    for i, nm in enumerate(names):
        jx = SCR_L + (i + 0.5) * (SCR_W / n)
        s.append(jack(jx, OUT_Y, t))

    s.append('</svg>')
    return "".join(s)


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    outdir = os.path.join(root, "res", "panels")
    os.makedirs(outdir, exist_ok=True)
    for dark, suffix in [(True, "dark"), (False, "light")]:
        svg = build(dark)
        p = os.path.join(outdir, f"Intertropical_panel_{suffix}.svg")
        open(p, "w").write(svg)
        print(f"wrote {p}  ({len(svg)} bytes)")


if __name__ == "__main__":
    main()
