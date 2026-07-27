#!/usr/bin/env python3
"""Intertropical scene-sequencer panel generator (SCAFFOLD v3 -- unified grid idiom).

Produces res/panels/Intertropical_panel_{dark,light}.svg.

Grid style: CONTINUOUS DISPLAY, matching Lantern / the Sands visual editor -- a single
recessed DARK screen with a thin gridline lattice, NOT discrete wells. The SCREEN stays dark
on both themes (voiceColour hues are tuned against dark; on light they'd fall below readable
contrast and the display would lie about the data). Only the BEZEL follows the panel.

REPEATS use the SAME grid idiom as the voice cells (Rodney): the repeat row is one cell per
scene, each subdivided HORIZONTALLY into 8 sub-segments. The widget uses colour-DEPTH to show
both COUNT (N sub-segments lit = N repeats) and live PROGRESS (the fill deepens/advances
through them as the scene plays -- 'repeat 3 of 5' reads as 3 done + 2 pending). One cell
carries count + progress in the grid's own language -- no separate meter, no text.

Left gutter: just VOICE NUMBERS 1..16 (widget-drawn text; the panel reserves the space, draws
no swatch -- identity-by-colour lives in the CELLS, where a filled cell is that voice's hue).

Layout: brand | repeat row (8 scenes, each 8-subdivided) | main grid (8 scenes x 16 voices) |
poly outputs. Panel art is STATIC geometry; all live state (membership fill, active scene,
repeat count+progress, playhead, voice numbers) is widget-drawn. Store-backed, no params.
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
N_SUBSEG  = 8            # horizontal subdivisions of each repeat cell (max repeats)

BRAND_Y   = 6.0
REP_Y     = 16.0
REP_H     = 9.0          # one grid-row tall-ish; the repeat "row"
GAP       = 3.0
GRID_Y    = REP_Y + REP_H + GAP
OUT_Y     = PH_MM - 11.0
GRID_BOT  = OUT_Y - 8.0

GUTTER    = 6.0          # left gutter for widget-drawn voice numbers 1..16
SCR_L     = MARGIN + GUTTER
SCR_R     = PW_MM - MARGIN
SCR_W     = SCR_R - SCR_L
COL_W     = SCR_W / N_SCENES
GRID_H    = GRID_BOT - GRID_Y
ROW_H     = GRID_H / N_VOICES

SCR   = "#101216"        # display screen: dark on both themes
GLINE = "#2a2f37"        # gridline


def screen(x, y, w, h, t):
    return (f'<rect x="{px(x):.1f}" y="{px(y):.1f}" width="{px(w):.1f}" height="{px(h):.1f}" '
            f'rx="{px(1.5):.1f}" fill="{SCR}" stroke="{t["edborder"]}" stroke-width="1"/>')


def vlines(x, y, w, h, cols, sw=0.6, op=0.7):
    out = []
    for c in range(1, cols):
        lx = x + c * (w / cols)
        out.append(f'<line x1="{px(lx):.1f}" y1="{px(y+1):.1f}" x2="{px(lx):.1f}" '
                   f'y2="{px(y+h-1):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>')
    return "".join(out)


def hlines(x, y, w, h, rows, sw=0.6, op=0.7):
    out = []
    for r in range(1, rows):
        ly = y + r * (h / rows)
        out.append(f'<line x1="{px(x+1):.1f}" y1="{px(ly):.1f}" x2="{px(x+w-1):.1f}" '
                   f'y2="{px(ly):.1f}" stroke="{GLINE}" stroke-width="{sw}" stroke-opacity="{op}"/>')
    return "".join(out)


def build(dark):
    t = theme(dark)
    PW, PH = px(PW_MM), px(PH_MM)
    s = [svg_open(PW, PH)]
    s.append(f'<rect x="0" y="0" width="{PW}" height="{PH}" fill="{t["bg"]}"/>')

    s.append(logo_embed(dark, MARGIN, BRAND_Y, 34.0))

    # REPEAT row: one screen, 8 scene columns; EACH column subdivided into 8 finer sub-segments.
    # Scene boundaries drawn slightly stronger than the sub-segment ticks, so it reads as
    # "8 cells, each split into 8" rather than "64 equal cells".
    s.append(screen(SCR_L, REP_Y, SCR_W, REP_H, t))
    s.append(vlines(SCR_L, REP_Y, SCR_W, REP_H, N_SCENES, sw=0.8, op=0.85))       # scene bounds
    for c in range(N_SCENES):                                                     # sub-segments
        s.append(vlines(SCR_L + c*COL_W, REP_Y, COL_W, REP_H, N_SUBSEG, sw=0.4, op=0.45))

    # MAIN grid: one screen, 8 scenes x 16 voices, continuous lattice.
    s.append(screen(SCR_L, GRID_Y, SCR_W, GRID_H, t))
    s.append(vlines(SCR_L, GRID_Y, SCR_W, GRID_H, N_SCENES))
    s.append(hlines(SCR_L, GRID_Y, SCR_W, GRID_H, N_VOICES))

    # Left gutter: NO swatches. Voice numbers 1..16 are widget-drawn text in this reserved band.
    # (A faint recess strip hints the label lane without competing with the grid.)
    s.append(f'<rect x="{px(MARGIN):.1f}" y="{px(GRID_Y):.1f}" width="{px(GUTTER-1.0):.1f}" '
             f'height="{px(GRID_H):.1f}" rx="{px(1.0):.1f}" fill="{t["group"]}" '
             f'stroke="{t["groupline"]}" stroke-width="0.75"/>')

    # poly outputs
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
