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

    E(f'<svg width="{SVG_W:.3f}" height="{SVG_H:.3f}" '
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
    MY = GUTTER_T + 4.0                # matrix top edge (mm)
    MW = SVG_W - GUTTER_L - GUTTER_R  # matrix width
    MH = SVG_H - MY - GUTTER_B - 2.0  # matrix height
    CELL_W = MW / N
    CELL_H = MH / N

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

    # ── Column labels (top: source voice = currency code) ─────────────────────
    label_y = MY - 1.8
    for col, cur in enumerate(CURRENCIES):
        cx = MX + col * CELL_W + CELL_W * 0.5
        is_sgd = (cur == "SGD")
        col_ink = t["red"] if is_sgd else inkdim
        E(f'<text x="{cx:.3f}" y="{label_y:.3f}" text-anchor="middle" '
          f'font-family="monospace" font-size="2.3" fill="{col_ink}">'
          f'{cur}</text>')

    # "source pool →" axis label at top
    E(f'<text x="{MX + MW*0.5:.3f}" y="{MY - GUTTER_T*0.6:.3f}" '
      f'text-anchor="middle" font-family="monospace" font-size="2.6" '
      f'fill="{inkdim}" opacity="0.6">source voice →</text>')

    # ── Row labels (left: consuming voice = currency code) ────────────────────
    for row, cur in enumerate(CURRENCIES):
        cy = MY + row * CELL_H + CELL_H * 0.5 + 0.9
        is_sgd  = (cur == "SGD")
        is_mono = (row == 0)
        col_ink = t["mono"] if is_mono else (t["red"] if is_sgd else ink)
        weight = 'font-weight="bold"' if (is_mono or is_sgd) else ''
        E(f'<text x="{MX - 1.2:.3f}" y="{cy:.3f}" text-anchor="end" '
          f'font-family="monospace" font-size="2.5" fill="{col_ink}" {weight}>'
          f'{cur}</text>')

    # ── Identity diagonal pins (default state — dim concentric) ───────────────
    # White outer ring (rhythm), red inner dot (melody), both on diagonal.
    r_outer = min(CELL_W, CELL_H) * 0.30
    r_inner = min(CELL_W, CELL_H) * 0.14
    for i in range(N):
        cx = MX + i * CELL_W + CELL_W * 0.5
        cy = MY + i * CELL_H + CELL_H * 0.5
        # Outer ring (rhythm) — dim white
        E(f'<circle cx="{cx:.3f}" cy="{cy:.3f}" r="{r_outer:.3f}" '
          f'fill="{t["rhythmD"]}" opacity="0.55"/>')
        # Inner dot (melody) — dim red
        E(f'<circle cx="{cx:.3f}" cy="{cy:.3f}" r="{r_inner:.3f}" '
          f'fill="{t["melodyD"]}" opacity="0.55"/>')

    # ── Example active pins (show the live aesthetic) ─────────────────────────
    # Row 1 (MYR) rhythm → SGD column (col 0): white filled pin
    ex_row, ex_col = 1, 0
    ecx = MX + ex_col * CELL_W + CELL_W * 0.5
    ecy = MY + ex_row * CELL_H + CELL_H * 0.5
    E(f'<circle cx="{ecx:.3f}" cy="{ecy:.3f}" r="{r_outer:.3f}" fill="{t["rhythm"]}"/>')
    # Row 2 (IDR) both → col 2 (IDR's own = identity, but shown bright to illustrate)
    ex2r, ex2c = 2, 0
    e2cx = MX + ex2c * CELL_W + CELL_W * 0.5
    e2cy = MY + ex2r * CELL_H + CELL_H * 0.5
    E(f'<circle cx="{e2cx:.3f}" cy="{e2cy:.3f}" r="{r_outer:.3f}" fill="{t["rhythm"]}"/>')
    E(f'<circle cx="{e2cx:.3f}" cy="{e2cy:.3f}" r="{r_inner:.3f}" fill="{t["melody"]}"/>')

    # ── Module title ──────────────────────────────────────────────────────────
    E(f'<text x="{SVG_W*0.5:.3f}" y="{MY - GUTTER_T + 1.5:.3f}" '
      f'text-anchor="middle" font-family="monospace" font-size="4.0" '
      f'font-weight="bold" letter-spacing="0.5" fill="{ink}">CHANGE ALLEY</text>')

    # ── Legend: white = rhythm, red = melody ─────────────────────────────────
    leg_y = SVG_H - GUTTER_B + 2.5
    leg_x = MX
    r_leg = 1.2
    E(f'<circle cx="{leg_x + r_leg:.3f}" cy="{leg_y:.3f}" r="{r_leg:.3f}" fill="{t["rhythm"]}"/>')
    E(f'<text x="{leg_x + r_leg*2 + 1:.3f}" y="{leg_y + 0.9:.3f}" '
      f'font-family="monospace" font-size="2.4" fill="{inkdim}">rhythm</text>')
    leg_x2 = leg_x + 22.0
    E(f'<circle cx="{leg_x2 + r_leg:.3f}" cy="{leg_y:.3f}" r="{r_leg:.3f}" fill="{t["melody"]}"/>')
    E(f'<text x="{leg_x2 + r_leg*2 + 1:.3f}" y="{leg_y + 0.9:.3f}" '
      f'font-family="monospace" font-size="2.4" fill="{inkdim}">melody</text>')

    # ── Brand ─────────────────────────────────────────────────────────────────
    bot = SVG_H - 3.5
    E(f'<circle cx="{SVG_W*0.5 - 8:.3f}" cy="{bot:.3f}" r="1.5" fill="{t["red"]}"/>')
    E(f'<text x="{SVG_W*0.5 - 4.5:.3f}" y="{bot + 1.0:.3f}" '
      f'font-family="monospace" font-size="3.2" fill="{ink}">modular</text>')

    E('</svg>')
    return "\n".join(o)

if __name__ == "__main__":
    os.makedirs("res/panels", exist_ok=True)
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/ChangeAlley_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"ChangeAlley {theme}: {out}  ({HP}HP, {SVG_W:.1f}×{SVG_H:.1f}mm)")
