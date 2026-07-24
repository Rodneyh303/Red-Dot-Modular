#!/usr/bin/env python3
"""Change Alley V2 -- ONE module: pin matrix + full transform controls.

Replaces Change Alley (29HP) + Temasek (24HP) = 53HP with a single 42HP panel.

Layout, per Rodney's original intent:
    INTRA controls  |  16x16 GRID  |  INTER controls
    (within-block)     (centre)       (across-block)
Mirrored, with jacks to the outside so cables never cross the knobs.

Rows top-to-bottom: COLLAPSE, ROTATE, REFLECT, SCATTER -- two rows each (Rhythm, Melody).
Per row, OUTER -> INNER: domain jack, codomain jack, grain knob,
  leader knob (Collapse) / step knob (Rotate) / domain-BACK jack (Scatter),
  domain button, codomain button, pending light,
  and for Scatter only a codomain-BACK jack.

The old 8-row control column is GONE: Temasek's verbs strictly superset it
(domain/codomain, intra/inter, leader/step, reversible scatter).

Labels/pins/crosshair/tooltip are widget-drawn (nanosvg drops <text>).
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, svg_open, logo_embed, jack, trim

HP     = 42
PW_MM  = HP * 5.08          # 213.36
PH_MM  = 128.5

# ── control block (identical geometry both sides; right side mirrored) ────────
MARGIN  = 6.0
J_DOM   = MARGIN +  0.0
J_COD   = MARGIN +  9.5
KNOB1   = MARGIN + 18.0
KNOB2   = MARGIN + 26.0     # leader / step / Scatter domain-BACK
BTN_D   = MARGIN + 33.5
BTN_C   = MARGIN + 40.0
J_BACK2 = MARGIN + 46.5     # Scatter only: codomain-BACK
CTRL_W  = J_BACK2 + 4.45    # 57.0mm per side

# ── grid (centre) ────────────────────────────────────────────────────────────
GRID_X  = CTRL_W
GRID_W  = PW_MM - 2 * CTRL_W
CELL    = GRID_W / 16.0
GRID_Y  = 16.0
GRID_H  = CELL * 16.0

# ── rows: same pitch as the old Change Alley control column ──────────────────
N_VERBS   = 4
ROW_H     = 9.0
GROUP_GAP = 6.8
ROW_TOP   = 21.0

def rowY(verb, sub):
    return ROW_TOP + verb * (2.0 * ROW_H + GROUP_GAP) + sub * ROW_H + ROW_H * 0.5

def lx(x_mm, flip):
    return (PW_MM - x_mm) if flip else x_mm

def pal(dark):
    grid = dict(well="#0b0c0d", gridln="#26262b", booth="#141416",
                red="#d4001a", gold="#c8960c")
    if dark:
        return dict(grid, body="#18181a", ink="#e8e2d0", dim="#8a8578",
                    frame="#2e2e33", jackwell="#0a0b0c", jackring="#46464c",
                    well="#0f1012", wellring="#3a3a40", edrecess="#101113",
                    edborder="#2e2e33", tabband="#181820")
    return dict(grid, body="#e8e8ea", ink="#2a2a2e", dim="#888d96",
                frame="#a8aeb6", jackwell="#dadce0", jackring="#9298a0",
                well="#dcdee2", wellring="#a8aeb6", edrecess="#d8dade",
                edborder="#c0c4ca", tabband="#cdd4dc")

def gen(dark):
    t = pal(dark); els = []; E = els.append
    E(f'<rect width="{px(PW_MM):.1f}" height="{px(PH_MM):.1f}" fill="{t["body"]}"/>')

    # grid well + lines
    E(f'<rect x="{px(GRID_X):.1f}" y="{px(GRID_Y):.1f}" width="{px(GRID_W):.1f}"'
      f' height="{px(GRID_H):.1f}" fill="{t["well"]}" stroke="{t["edborder"]}"'
      f' stroke-width="{px(0.4):.2f}"/>')
    for i in range(1, 16):
        gx = GRID_X + i * CELL
        gy = GRID_Y + i * CELL
        E(f'<line x1="{px(gx):.1f}" y1="{px(GRID_Y):.1f}" x2="{px(gx):.1f}"'
          f' y2="{px(GRID_Y+GRID_H):.1f}" stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')
        E(f'<line x1="{px(GRID_X):.1f}" y1="{px(gy):.1f}" x2="{px(GRID_X+GRID_W):.1f}"'
          f' y2="{px(gy):.1f}" stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')
    # 4-cell booth bands (block structure, echoes the default grain)
    for b in range(0, 16, 4):
        E(f'<rect x="{px(GRID_X + b*CELL):.1f}" y="{px(GRID_Y):.1f}"'
          f' width="{px(4*CELL):.1f}" height="{px(GRID_H):.1f}"'
          f' fill="{t["booth"]}" opacity="{0.35 if (b//4)%2 else 0.0:.2f}"/>')

    # controls, both sides
    for verb in range(N_VERBS):
        for sub in range(2):
            ry = rowY(verb, sub)
            for side in range(2):
                flip = (side == 1)
                E(jack(lx(J_DOM, flip), ry, t))
                E(jack(lx(J_COD, flip), ry, t))
                E(trim(lx(KNOB1, flip), ry, t, t["gold"]))
                if verb in (0, 1):
                    E(trim(lx(KNOB2, flip), ry, t, t["gold"]))
                elif verb == 3:
                    E(jack(lx(KNOB2,   flip), ry, t))
                    E(jack(lx(J_BACK2, flip), ry, t))
                for bx in (BTN_D, BTN_C):
                    E(f'<circle cx="{px(lx(bx,flip)):.1f}" cy="{px(ry):.1f}" r="{px(2.6):.1f}"'
                      f' fill="{t["frame"]}" stroke="{t["dim"]}" stroke-width="{px(0.5):.2f}"/>')

    E(logo_embed(dark, PW_MM/2 - 17.0, PH_MM - 11.0, 34.0))

    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    os.makedirs(out, exist_ok=True)
    theme = "dark" if dark else "light"
    fn = os.path.join(out, f"ChangeAlleyV2_panel_{theme}.svg")
    with open(fn, "w") as f:
        f.write(svg_open(px(PW_MM), px(PH_MM)) + "\n" + "\n".join(els) + "\n</svg>\n")
    print(f"ChangeAlleyV2 {theme}: {HP}HP  grid {GRID_W:.1f}mm cell {CELL:.2f}mm  ctrl {CTRL_W:.1f}mm/side")

if __name__ == "__main__":
    gen(True); gen(False)
