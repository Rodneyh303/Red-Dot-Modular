#!/usr/bin/env python3
"""Containment -- articulation-envelope routing (the Change Alley SIBLING). 40HP.

Rodney 2026-10-26 (panel correction): "It should produce a 16 by 16 panel like
change alley (square near full module height with controls jacks to the side not
underneath.)  Full Polyphony is 16 voices.  Need to support up to 16 vertices
depending on active Polyphony level."

So: the matrix is the SAME SQUARE as CA V2 (99.6mm, cells 6.225mm) at the same
top (GRID_Y 16.0) -- near full module height (99.6 of 128.5mm), always 16x16.
Rows = voices (v), columns = parents (u); cell (v,u) ON = voice v contained in
parent u.  Rows/cols >= activeCount are WIDGET-dimmed at step 4 (the generator
always emits all 16 -- full polyphony is the geometry's ground state).

Controls/jacks to the SIDES, never underneath:
  LEFT  = one CV trig jack per verb (input_trig_<verb>), 7 rows
  RIGHT = one fire button + pending lamp per verb (param_btn_/light_pending_),
          mirrored via lx() so each verb row reads as one left-right pair.
Two groups: ROLES trio, then FOLD trio + RANDOM (the 2026-10-27 ruling: the seed
verb as row 7 -- the gesture vocabulary unclosed for a genuinely new decision
class; Rodney: "Random needs to unclose the 6 gesture rows. It needs to be on
panel with jack and button"). ROW_H tightened 14.0 -> 12.0 to keep all seven
rows inside the grid band (one group gap only, after ROLES):
  ROLES     : ROTATE  REFLECT  INVERT                  (role conjugations + duality)
  FOLD/RAND : COLLAPSE  COLLAPSE-ALL  EXPAND  RANDOM  (structure folding + restore +
              generation; the row label speaks for itself)

ALL lettering is WIDGET-DRAWN (nanosvg -- the parser Rack uses -- silently
drops <text>); this file emits art + the #components anchor layer ONLY.
MonsoonContainment.hpp mirrors these constants in ContainmentPanelMm
("MUST MATCH gen_containment.py" -- the gen_change_alley_v2.py rule).

Column order per side, OUTER -> INNER:
  LEFT : J_X (trig jack)
  RIGHT: BTN_X = lx(J_X) (fire button)   L_X (pending lamp)
HP is COMPUTED from the control strips then bumped to the round number
(Rodney: land on the round number -- the gen_change_alley_v2.py rule).
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, svg_open, jack, logo_embed

PH_MM = 128.5
MARGIN = 6.0
J_HALF = 4.25
JACK_P = 8.5

# ── Matrix: SAME square as CA V2, same top -- "square near full module height" ──
GRID_W = 99.6                  # cells 6.225mm x 16 (== CA V2)
CELL   = GRID_W / 16.0
GRID_Y = 16.0                  # matrix top (== CA V2)
GRID_H = CELL * 16.0

# ── Control strips (mm, from panel edge) ─────────────────────────────────────
J_X    = MARGIN + J_HALF + JACK_P     # 18.75  left trig-jack column (CA J_DOM spacing)
L_X    = J_X + 9.0                    # 27.75  right pending-lamp column (button at lx(J_X))
CTRL_W = L_X + 4.0                    # strip width incl. breathing pad
PW_RAW = 2 * CTRL_W + GRID_W
# Round UP to full HP then bump to at least 40HP (round number, the CA rule);
# slack is absorbed SYMMETRICALLY into the two gutters (grid stays centred).
HP     = max(40, int(math.ceil(PW_RAW / 5.08)))
PW_MM  = HP * 5.08
GUTTER = (PW_MM - 2 * CTRL_W - GRID_W) / 2.0
GRID_X = CTRL_W + GUTTER

# ── Verb rows: ROLES trio, then FOLD trio + RANDOM, spread down the grid band ──────────
ROW_TOP   = GRID_Y + 8.0
ROW_H     = 12.0
GROUP_GAP = 5.0
VERBS = [("rotate", "ROTATE"), ("reflect", "REFLECT"), ("invert", "INVERT"),
         ("collapse", "COLLAPSE"), ("collapseall", "COLLAPSE-ALL"), ("expand", "EXPAND"),
         ("random", "RANDOM")]

def rowY(k):
    return ROW_TOP + k * ROW_H + (GROUP_GAP if k >= 3 else 0.0) + ROW_H * 0.5

def lx(x, flip): return (PW_MM - x) if flip else x

# ── Widget-drawn label geometry (mirrored in MonsoonContainment.hpp) ──────────
NUM_X   = GRID_X - 1.4     # voice numbers, right-aligned to the grid's left edge
LABEL_X = NUM_X - 3.0      # verb labels, right-aligned inboard of the numbers
TITLE_Y = 9.8              # CONTAINMENT, centred (widget-drawn)
CAP1_Y  = 121.0            # bottom caption line 1 (centred)
CAP2_Y  = 125.2            # bottom caption line 2 (centred)
SIDE_Y  = GRID_Y - 1.2     # TRIG / FIRE side captions (level with parent numbers)

def pal(dark):
    b = dict(red="#d4001a", gold="#c8960c")
    if dark:
        return dict(b, body="#18181a", ink="#e8e2d0", dim="#8a8578", frame="#2e2e33",
                    jackwell="#0a0b0c", jackring="#46464c", well="#0f1012", wellring="#3a3a40",
                    edrecess="#101113", edborder="#2e2e33", gridln="#26262b")
    return dict(b, body="#e8e8ea", ink="#2a2a2e", dim="#888d96", frame="#a8aeb6",
                jackwell="#dadce0", jackring="#9298a0", well="#dcdee2", wellring="#a8aeb6",
                edrecess="#d8dade", edborder="#c0c4ca", gridln="#c4c8ce")

def gen(dark):
    t = pal(dark); els = []; E = els.append
    anchors = []
    def A(i, x, y):
        anchors.append(f'<circle id="{i}" cx="{px(x):.2f}" cy="{px(y):.2f}" r="1" fill="none" stroke="none"/>')

    E(f'<rect width="{px(PW_MM):.1f}" height="{px(PH_MM):.1f}" fill="{t["body"]}"/>')

    # Matrix well + 15 inner gridlines each way (CA V2 drawing weights).
    E(f'<rect x="{px(GRID_X):.1f}" y="{px(GRID_Y):.1f}" width="{px(GRID_W):.1f}" height="{px(GRID_H):.1f}" '
      f'fill="{t["well"]}" stroke="{t["jackring"]}" stroke-width="{px(0.4):.2f}"/>')
    for i in range(1, 16):
        gx = GRID_X + i * CELL; gy = GRID_Y + i * CELL
        E(f'<line x1="{px(gx):.1f}" y1="{px(GRID_Y):.1f}" x2="{px(gx):.1f}" y2="{px(GRID_Y + GRID_H):.1f}" '
          f'stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')
        E(f'<line x1="{px(GRID_X):.1f}" y1="{px(gy):.1f}" x2="{px(GRID_X + GRID_W):.1f}" y2="{px(gy):.1f}" '
          f'stroke="{t["gridln"]}" stroke-width="{px(0.2):.2f}"/>')

    # Diagonal cells: self-containment is FORBIDDEN (no-self-bit invariant) --
    # shade the diagonal so the illegal cells are visible in the art itself.
    diag_op = "0.30" if dark else "0.12"
    for k in range(16):
        dx = GRID_X + k * CELL; dy = GRID_Y + k * CELL
        E(f'<rect x="{px(dx + 0.3):.1f}" y="{px(dy + 0.3):.1f}" width="{px(CELL - 0.6):.1f}" '
          f'height="{px(CELL - 0.6):.1f}" fill="#000000" opacity="{diag_op}"/>')

    # dot.modular wordmark, top band, left of the (widget-drawn) centred title.
    E(logo_embed(dark, GRID_X, 3.0, 30.0))

    # ── Verb rows: LEFT trig jack | RIGHT fire button + pending lamp ───────────
    for k, (verb, _label) in enumerate(VERBS):
        ry = rowY(k)
        E(jack(J_X, ry, t));                A(f"input_trig_{verb}",  J_X,          ry)
        E(f'<circle cx="{px(lx(J_X, True)):.1f}" cy="{px(ry):.1f}" r="{px(2.6):.1f}" '
          f'fill="{t["frame"]}" stroke="{t["dim"]}" stroke-width="{px(0.5):.2f}"/>')
        A(f"param_btn_{verb}", lx(J_X, True), ry)
        E(f'<circle cx="{px(lx(L_X, True)):.1f}" cy="{px(ry):.1f}" r="{px(1.3):.1f}" '
          f'fill="{t["well"]}" stroke="{t["dim"]}" stroke-width="{px(0.3):.2f}"/>')
        A(f"light_pending_{verb}", lx(L_X, True), ry)

    # ── Geometry report + clearance asserts (the gen_change_alley_v2.py discipline) ──
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    os.makedirs(out, exist_ok=True)
    th = "dark" if dark else "light"
    comps = ('<g inkscape:label="components" inkscape:groupmode="layer" id="components">\n'
             + "\n".join(anchors) + "\n</g>")
    open(os.path.join(out, f"Containment_panel_{th}.svg"), "w").write(
        svg_open(px(PW_MM), px(PH_MM)) + "\n" + "\n".join(els) + "\n" + comps + "\n</svg>\n")
    print(f"Containment {th}: {HP}HP grid {GRID_W:.1f}mm cell {CELL:.3f}mm "
          f"ctrl {CTRL_W:.2f}mm/side gutter {GUTTER:.2f}mm  anchors={len(anchors)}")

    # exact-mirror assertion (button/lamp columns are lx() images of the jack column)
    assert abs(lx(J_X, True) - (PW_MM - J_X)) < 1e-9, "button column not an exact mirror"
    # label clearance: longest on-panel label must clear the jack rim by >= 1mm
    est_w = 0.66 * 2.8 * max(len(l) for _, l in VERBS)
    label_start = LABEL_X - est_w
    assert label_start >= J_X + J_HALF + 1.0, f"verb labels collide with jack rim ({label_start:.2f})"
    # rows live inside the grid band with breathing room
    assert rowY(0) - 4.5 >= GRID_Y + 1.5 and rowY(6) + 4.5 <= GRID_Y + GRID_H - 1.5, "verb rows leave the grid band"
    print(f"  label zone: LABEL_X={LABEL_X:.2f} start>={label_start:.2f} vs jack rim "
          f"{J_X + J_HALF:.2f} (clear)  rows {rowY(0):.1f}..{rowY(6):.1f} in band "
          f"{GRID_Y + 1.5:.1f}..{GRID_Y + GRID_H - 1.5:.1f}")

if __name__ == "__main__":
    gen(True); gen(False)
