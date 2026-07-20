#!/usr/bin/env python3
"""
Change Alley pin matrix panel — 16×16, EMS VCS3 lineage.
Rows = consuming voices (which currency is exchanging).
Columns = source voices (whose pool to borrow from).
Pin colours: white = rhythm, red = melody, concentric when both occupy same cell.
Dark grid forced on BOTH themes (same rule as Lantern LCD — state encoded in
pin position/colour; a light background would wash out the red/white contrast).
"""
import os, math

PW_MM = 18 * 5.08   # 91.44 mm (18HP)
PH_MM = 128.5       # standard Rack height

HP = 18

# All coordinates in mm; SVG uses the same unit via viewBox.
SVG_W, SVG_H = PW_MM, PH_MM

DARK_GRID = dict(
    bg      = "#080a07",
    red     = "#d4001a",
    ink     = "#e8e2d0",
    inkdim  = "#7a7060",
    well    = "#0d100b",        # cell background
    gridln  = "#1e2420",        # grid lines
    booth   = "#111410",        # alternate row tint
    # Pin colours
    rhythm  = "#f0f0ee",        # white — rhythm source
    melody  = "#d4001a",        # red — melody source
    rhythmD = "#404040",        # ghost outline — rhythm
    melodyD = "#4a0008",        # ghost outline — melody
    both    = "#d4001a",        # outer ring when concentric
    # Active voice indicator
    active  = "#d4001a",        # red tick for active poly rows
    inactive= "#282c28",
    mono    = "#c8900c",        # amber for row 0 (mono)
)

# Light mode: keep the grid dark (forced, same as Lantern)
LIGHT_GRID = dict(**DARK_GRID)
LIGHT_GRID.update(dict(
    bg   = "#e4e0d0",           # panel surround is light
    ink  = "#1a1810",
    inkdim = "#6a6050",
))

CURRENCIES = [
    "SGD","MYR","IDR","THB",
    "PHP","VND","MMK","KHR",
    "HKD","CNY","TWD","KRW",
    "JPY","AUD","INR","USD",
]

N = 16

def gen(dark):
    t = DARK_GRID   # grid is ALWAYS dark regardless of theme
    bg_panel = DARK_GRID["bg"] if dark else LIGHT_GRID["bg"]
    ink      = DARK_GRID["ink"] if dark else LIGHT_GRID["ink"]
    inkdim   = DARK_GRID["inkdim"] if dark else LIGHT_GRID["inkdim"]

    o = []
    def E(s): o.append(s)

    # Document size in PX (Rack: 1HP = 15px, 75dpi => px = mm * 75/25.4). 18HP = 270px,
    # 128.5mm = 379.43px (house height, cf. Causeway). The viewBox keeps every coordinate
    # below in mm, and 270/91.44 == Rack's mm2px scale exactly, so the widget's mm2px
    # hit-test lands on the same points the panel draws.
    E(f'<svg width="270.0" height="379.43" '
      f'viewBox="0 0 {SVG_W:.3f} {SVG_H:.3f}" xmlns="http://www.w3.org/2000/svg">')

    # Panel background
    E(f'<rect width="{SVG_W:.3f}" height="{SVG_H:.3f}" fill="{bg_panel}"/>')

    # Top red stripe
    E(f'<rect width="{SVG_W:.3f}" height="2.0" fill="{t["red"]}"/>')

    # ── Matrix geometry ───────────────────────────────────────────────────────
    # We want 16×16 cells with row labels (left) and column labels (top/bottom).
    # Available width after left gutter and right margin:
    GUTTER_L  = 13.0   # mm for row currency labels
    GUTTER_R  =  3.5   # mm right margin / poly indicator
    GUTTER_T  = 10.0   # mm for column labels at top
    GUTTER_B  = 12.0   # mm for brand at bottom

    MX = GUTTER_L                      # matrix left edge (mm)
    MW = SVG_W - GUTTER_L - GUTTER_R  # matrix width
    CELL   = MW / N                    # SQUARE cells — width-constrained
    CELL_W = CELL
    CELL_H = CELL
    MH = CELL * N                      # square matrix
    # centre the square grid in the vertical space between title and legend
    MY = GUTTER_T + 8.0                # top edge: title + column labels above

    # ── Matrix well (dark, always) ────────────────────────────────────────────
    E(f'<rect x="{MX:.3f}" y="{MY:.3f}" width="{MW:.3f}" height="{MH:.3f}" '
      f'fill="{t["well"]}"/>')

    # ── Alternate row tints ───────────────────────────────────────────────────
    for row in range(N):
        if row % 2 == 1:
            E(f'<rect x="{MX:.3f}" y="{MY + row*CELL_H:.3f}" '
              f'width="{MW:.3f}" height="{CELL_H:.3f}" '
              f'fill="{t["booth"]}" opacity="0.8"/>')

    # ── Grid lines ────────────────────────────────────────────────────────────
    for i in range(N + 1):
        # Verticals
        x = MX + i * CELL_W
        E(f'<line x1="{x:.3f}" y1="{MY:.3f}" x2="{x:.3f}" y2="{MY+MH:.3f}" '
          f'stroke="{t["gridln"]}" stroke-width="0.4"/>')
        # Horizontals
        y = MY + i * CELL_H
        E(f'<line x1="{MX:.3f}" y1="{y:.3f}" x2="{MX+MW:.3f}" y2="{y:.3f}" '
          f'stroke="{t["gridln"]}" stroke-width="0.4"/>')


    # (Pins, labels and legend are ALL drawn by the widget — nanosvg ignores <text>,
    #  and baked pins would double under the live ones. The SVG is grid + wells only.)

    E('</svg>')
    return "\n".join(o)

if __name__ == "__main__":
    os.makedirs("res/panels", exist_ok=True)
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/ChangeAlley_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"ChangeAlley {theme}: {out}  ({HP}HP, {SVG_W:.1f}×{SVG_H:.1f}mm)")
