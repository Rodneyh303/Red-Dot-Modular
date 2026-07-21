#!/usr/bin/env python3
"""Change Alley — 29HP pin matrix + restructure-transform control column.
Built on the shared dotmod_design kit (nanosvg-safe, native 75dpi px).
DARK BODY IN BOTH THEMES: the grid is a dark instrument surface (Lantern LCD rule)
and the cream body read terribly in-rack; light theme differs only in trim tones.
Labels/pins/crosshair/tooltip are widget-drawn (nanosvg drops <text>).
Layout: control column LEFT (4 transforms x [knob | jack | button+light] with R/M rows),
16x16 grid RIGHT (height-driven square cells), logo large under the control column.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dotmod_design import px, jack, trim, svg_open, logo_embed

HP    = 29
PW_MM = HP * 5.08          # 147.32
PH_MM = 128.5
PW, PH = px(PW_MM), px(PH_MM)

# ── Palette: Raffles house, dark body BOTH themes ────────────────────────────
def pal(dark):
    base = dict(
        bg="#131316", body="#18181a", red="#d4001a", ink="#e8e2d0", dim="#8a8578",
        well="#0b0c0d", gridln="#26262b", booth="#141416", frame="#2e2e33",
        jackwell="#0a0b0c", jackring="#46464c", edrecess="#101113",
        knobwell="#0f1012", knobring="#c8960c", knobtick="#e8e2d0",
        btnwell="#0f1012", btnring="#46464c", ledoff="#3a2226",
        rhythm="#f0f0ee", melody="#d4001a", gold="#c8960c",
    )
    if not dark:
        # Light theme: SAME dark body (deliberate); only lift trims slightly
        base.update(dim="#9a9588", jackring="#54545c", frame="#36363c")
    return base

# ── Shared geometry (single source of truth — widget mirrors via these) ──────
GUTTER_T = 9.0
GUTTER_B = 8.0
MY_MM    = GUTTER_T + 8.0                    # grid top
AVAIL_H  = PH_MM - MY_MM - GUTTER_B - 2.0    # 101.5 -> cell 6.34
CELL     = AVAIL_H / 16.0
MW_MM    = CELL * 16.0
GRID_R_MARGIN = 4.0
MX_MM    = PW_MM - GRID_R_MARGIN - MW_MM     # grid claims the RIGHT
# Control column
CTRL_X_KNOB = 10.0     # knob centre x
CTRL_X_JACK = 20.5
CTRL_X_BTN  = 29.0
CTRL_X_LED  = 34.5
CTRL_TOP    = MY_MM + 2.0
CTRL_ROW_H  = (MH := MW_MM) / 8.0 * 0.78     # 8 rows packed in upper ~78% of grid height
LABEL_GAP   = 6.5                            # widget row-number labels sit left of grid

def transform_rows():
    """8 rows: (Collapse,Rotate,Scatter,Reflect) x (R,M). Returns [(y_mm, tIdx, isR)]."""
    rows = []
    for tIdx in range(4):
        for r in range(2):
            i = tIdx*2 + r
            rows.append((CTRL_TOP + i*CTRL_ROW_H + CTRL_ROW_H*0.5, tIdx, r == 0))
    return rows

def gen(dark):
    t = pal(dark)
    o = [svg_open(PW, PH)]
    E = o.append
    # Body — dark both themes
    E(f'<rect width="{PW}" height="{PH}" fill="{t["body"]}"/>')
    # Top brand stripe
    E(f'<rect width="{PW}" height="{px(2.0)}" fill="{t["red"]}"/>')

    # ── Grid recess ──────────────────────────────────────────────────────────
    E(f'<rect x="{px(MX_MM-1.2)}" y="{px(MY_MM-1.2)}" width="{px(MW_MM+2.4)}" height="{px(MW_MM+2.4)}" rx="{px(1.2)}" fill="{t["frame"]}"/>')
    E(f'<rect x="{px(MX_MM)}" y="{px(MY_MM)}" width="{px(MW_MM)}" height="{px(MW_MM)}" fill="{t["well"]}"/>')
    for row in range(16):                     # booth bands
        if row % 2 == 1:
            E(f'<rect x="{px(MX_MM)}" y="{px(MY_MM+row*CELL)}" width="{px(MW_MM)}" height="{px(CELL)}" fill="{t["booth"]}" opacity="0.85"/>')
    for i in range(17):                       # grid lines
        v = px(MX_MM + i*CELL); h = px(MY_MM + i*CELL)
        E(f'<line x1="{v}" y1="{px(MY_MM)}" x2="{v}" y2="{px(MY_MM+MW_MM)}" stroke="{t["gridln"]}" stroke-width="0.5"/>')
        E(f'<line x1="{px(MX_MM)}" y1="{h}" x2="{px(MX_MM+MW_MM)}" y2="{h}" stroke="{t["gridln"]}" stroke-width="0.5"/>')

    # ── Control column: 4 transform groups x (R,M) rows ──────────────────────
    for (y, tIdx, isR) in transform_rows():
        # hairline separator above each transform group (between groups)
        if isR and tIdx > 0:
            sy = px(y - CTRL_ROW_H*0.5)
            E(f'<line x1="{px(4.0)}" y1="{sy}" x2="{px(38.5)}" y2="{sy}" stroke="{t["frame"]}" stroke-width="0.8"/>')
        if isR:  # block-size knob spans the R row (shared placement; per-type knobs stacked)
            pass
        col = t["rhythm"] if isR else t["melody"]
        # knob (block size) — per transform PER TYPE (8 knobs): gold ring, type tick
        E(f'<circle cx="{px(CTRL_X_KNOB)}" cy="{px(y)}" r="{px(3.6)}" fill="{t["knobwell"]}" stroke="{t["knobring"]}" stroke-width="1.2"/>')
        E(f'<line x1="{px(CTRL_X_KNOB)}" y1="{px(y)}" x2="{px(CTRL_X_KNOB)}" y2="{px(y-2.7)}" stroke="{t["knobtick"]}" stroke-width="1"/>')
        # gate jack
        E(jack(CTRL_X_JACK, y, dict(jackwell=t["jackwell"], jackring=t["jackring"], edrecess=t["edrecess"])))
        # manual button (small round)
        E(f'<circle cx="{px(CTRL_X_BTN)}" cy="{px(y)}" r="{px(2.5)}" fill="{t["btnwell"]}" stroke="{t["btnring"]}" stroke-width="1"/>')
        # pending light bezel (widget draws the lit state)
        E(f'<circle cx="{px(CTRL_X_LED)}" cy="{px(y)}" r="{px(1.6)}" fill="{t["ledoff"]}" stroke="{t["btnring"]}" stroke-width="0.7"/>')
        # type marker: small dot in the type colour left of the knob
        E(f'<circle cx="{px(4.6)}" cy="{px(y)}" r="{px(1.1)}" fill="{col}"/>')

    # ── Logo: LARGE, under the control column (real estate it deserves) ─────
    E(logo_embed(True, 4.0, PH_MM - 13.5, 34.0))   # dark wordmark on dark body, 34mm wide

    E('</svg>')
    return "\n".join(o)

if __name__ == "__main__":
    os.makedirs("res/panels", exist_ok=True)
    for dark in (True, False):
        th = "dark" if dark else "light"
        out = f"res/panels/ChangeAlley_panel_{th}.svg"
        open(out, "w").write(gen(dark))
        print(f"ChangeAlley {th}: {out}  ({HP}HP {PW}x{PH}px, cell {CELL:.2f}mm, grid x={MX_MM:.1f})")
