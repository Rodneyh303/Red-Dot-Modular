#!/usr/bin/env python3
"""
gen_change_alley.py — Change Alley pin matrix panel
16 voices (rows) × 2 stream pools (R/M columns), EMS-style.
Rows labelled with SGD-adjacent FX currency pairs vs USD — the actual Change Alley booths.
Panel: dark alley corridor feel, matrix as the exchange counter grid.
"""

import math, os

PW, PH = 270.0, 380.0   # 18HP
HP = 18

def px(mm): return mm * (PW / (HP * 5.08))   # 1HP = 5.08mm, panel is HP*5.08mm wide

DARK = dict(
    bg       = "#0a0b08",
    red      = "#d4001a",
    ink      = "#f0ead8",
    inkdim   = "#a09880",
    steel    = "#3c4650",
    steeldim = "#2a333c",
    steelfaint="#1c242c",
    steelhi  = "#586470",
    amber    = "#c8900c",
    amberdim = "#7a5808",
    amberfaint="#3a2c10",
    teal     = "#2a7870",
    tealdim  = "#1a4840",
    tealfaint= "#0e2820",
    gold     = "#b8901c",
    golddim  = "#5a4818",
    accent   = "#d4001a",
    node     = "#4a5560",
    well     = "#060706",
    alley    = "#0e1008",   # slightly warm dark for the alley corridor
    booth    = "#181c10",   # booth separator bands
    boothhi  = "#222818",
)

LIGHT = dict(
    bg       = "#e4e0d0",
    red      = "#d4001a",
    ink      = "#1a1810",
    inkdim   = "#6a6050",
    steel    = "#8090a0",
    steeldim = "#a0adb8",
    steelfaint="#c0c8d0",
    steelhi  = "#607080",
    amber    = "#906008",
    amberdim = "#c0a060",
    amberfaint="#ddd0a8",
    teal     = "#287068",
    tealdim  = "#6aada8",
    tealfaint="#c0dcd8",
    gold     = "#806010",
    golddim  = "#b8a060",
    accent   = "#d4001a",
    node     = "#7a8895",
    well     = "#c8c4b0",
    alley    = "#d8d4c0",
    booth    = "#ccc8b4",
    boothhi  = "#e0dcd0",
)

# 16 currency codes — the Change Alley FX booths, SGD neighbourhood first
CURRENCIES = [
    "SGD","MYR","IDR","THB",   # ASEAN 4
    "PHP","VND","MMK","KHR",   # ASEAN 4 more
    "HKD","CNY","TWD","KRW",   # North Asia
    "JPY","AUD","INR","USD",   # Further + base
]

# Column pool labels
POOLS = ["R", "M"]   # rhythm, melody — two dice pools

def logo_embed(dark, cx, cy, size):
    c = "#d4001a"
    r = size * 0.18
    o = size * 0.09
    return (
        f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r*1.8)}" fill="{c}" opacity="0.15"/>'
        f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r*1.2)}" fill="{c}" opacity="0.25"/>'
        f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{c}"/>'
        f'<text x="{px(cx+o*2.2)}" y="{px(cy+o*0.38)}" '
        f'font-family="monospace" font-size="{px(o*1.1)}" fill="{c}" font-weight="bold">modular</text>'
    )

def gen(dark):
    t = DARK if dark else LIGHT
    A = []
    def E(s): A.append(s)

    E(f'<svg width="{px(PW)}" height="{px(PH)}" viewBox="0 0 {px(PW)} {px(PH)}" '
      f'xmlns="http://www.w3.org/2000/svg">')

    # background
    E(f'<rect width="{px(PW)}" height="{px(PH)}" fill="{t["bg"]}"/>')

    # top red stripe
    E(f'<rect width="{px(PW)}" height="{px(2.5)}" fill="{t["red"]}"/>')

    # alley corridor — vertical warm-dark band behind the matrix
    MX = 28.0; MY = 18.0   # matrix top-left in mm
    CELL = 9.5             # cell size in mm
    COLS = 2; ROWS = 16
    MW = COLS * CELL        # matrix width
    MH = ROWS * CELL        # matrix height
    GUTTER_L = 22.0         # left label gutter
    GUTTER_T = 12.0         # top label gutter

    E(f'<rect x="{px(MX - GUTTER_L - 2)}" y="{px(MY - GUTTER_T - 1)}" '
      f'width="{px(MW + GUTTER_L + 4)}" height="{px(MH + GUTTER_T + 3)}" '
      f'rx="{px(1.5)}" fill="{t["alley"]}" opacity="0.7"/>')

    # booth bands (alternating row tint — the booths)
    for row in range(ROWS):
        if row % 2 == 1:
            ry = MY + row * CELL
            E(f'<rect x="{px(MX - GUTTER_L)}" y="{px(ry)}" '
              f'width="{px(MW + GUTTER_L + 1)}" height="{px(CELL)}" '
              f'fill="{t["boothhi"]}" opacity="0.25"/>')

    # matrix cell backgrounds (wells)
    for row in range(ROWS):
        for col in range(COLS):
            cx = MX + col * CELL + CELL * 0.5
            cy = MY + row * CELL + CELL * 0.5
            E(f'<rect x="{px(MX + col*CELL + 1)}" y="{px(MY + row*CELL + 1)}" '
              f'width="{px(CELL - 2)}" height="{px(CELL - 2)}" '
              f'rx="{px(1.2)}" fill="{t["well"]}" opacity="0.85"/>')

    # column header grid lines and pool labels
    for col in range(COLS):
        cx = MX + col * CELL + CELL * 0.5
        pool_col = t["amber"] if col == 0 else t["teal"]
        pool_dim = t["amberdim"] if col == 0 else t["tealdim"]
        # header label
        E(f'<text x="{px(cx)}" y="{px(MY - 3.5)}" text-anchor="middle" '
          f'font-family="monospace" font-size="{px(4.5)}" font-weight="bold" '
          f'fill="{pool_col}">{POOLS[col]}</text>')
        # vertical column separator line
        E(f'<line x1="{px(MX + col*CELL)}" y1="{px(MY - 1)}" '
          f'x2="{px(MX + col*CELL)}" y2="{px(MY + MH + 1)}" '
          f'stroke="{t["steel"]}" stroke-width="{px(0.5)}" opacity="0.5"/>')
    # right edge
    E(f'<line x1="{px(MX + MW)}" y1="{px(MY - 1)}" '
      f'x2="{px(MX + MW)}" y2="{px(MY + MH + 1)}" '
      f'stroke="{t["steel"]}" stroke-width="{px(0.5)}" opacity="0.5"/>')

    # row separator lines
    for row in range(ROWS + 1):
        E(f'<line x1="{px(MX - 1)}" y1="{px(MY + row*CELL)}" '
          f'x2="{px(MX + MW + 1)}" y2="{px(MY + row*CELL)}" '
          f'stroke="{t["steel"]}" stroke-width="{px(0.4)}" opacity="0.4"/>')

    # currency row labels
    for row, cur in enumerate(CURRENCIES):
        cy = MY + row * CELL + CELL * 0.5
        is_sgd = (cur == "SGD")
        is_usd = (cur == "USD")
        col = t["red"] if is_sgd else (t["inkdim"] if is_usd else t["ink"])
        # FX pair label: CUR/USD
        label = f"{cur}" if is_usd else f"{cur}/USD"
        weight = 'font-weight="bold"' if is_sgd else ''
        if is_usd:
            E(f'<text x="{px(MX - 2.5)}" y="{px(cy + 1.5)}" text-anchor="end" '
              f'font-family="monospace" font-size="{px(3.5)}" fill="{col}" {weight}>{cur} (base)</text>')
        else:
            E(f'<text x="{px(MX - 2.5)}" y="{px(cy + 1.5)}" text-anchor="end" '
              f'font-family="monospace" font-size="{px(3.5)}" fill="{col}" {weight}>'
              f'<tspan>{cur}</tspan>'
              f'<tspan font-size="{px(2.5)}" fill="{t["inkdim"]}" opacity="0.6">/USD</tspan>'
              f'</text>')
        # /USD suffix dim


    # identity diagonal pins (default state, shown as dim outlines)
    for i in range(ROWS):
        col = i % COLS   # identity diagonal maps voice to pool by modulo
        cx = MX + col * CELL + CELL * 0.5
        cy = MY + i * CELL + CELL * 0.5
        pin_col = t["amberdim"] if col == 0 else t["tealdim"]
        E(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(2.2)}" '
          f'fill="none" stroke="{pin_col}" stroke-width="{px(0.8)}" opacity="0.4"/>')

    # example active pin (SGD row, R column) — to show the live pin aesthetic
    E(f'<circle cx="{px(MX + 0*CELL + CELL*0.5)}" cy="{px(MY + 0*CELL + CELL*0.5)}" '
      f'r="{px(2.8)}" fill="{t["amber"]}"/>')
    E(f'<circle cx="{px(MX + 0*CELL + CELL*0.5)}" cy="{px(MY + 0*CELL + CELL*0.5)}" '
      f'r="{px(1.2)}" fill="{t["bg"]}" opacity="0.4"/>')

    # "vs USD" axis label (rotated, left of rows)
    E(f'<text transform="rotate(-90,{px(MX-GUTTER_L+3)},{px(MY+MH/2)})" '
      f'x="{px(MX-GUTTER_L+3)}" y="{px(MY+MH/2)}" text-anchor="middle" '
      f'font-family="monospace" font-size="{px(2.8)}" fill="{t["inkdim"]}" opacity="0.5">'
      f'consuming voice</text>')

    # column axis label (pool / source)
    E(f'<text x="{px(MX + MW/2)}" y="{px(MY - GUTTER_T + 1.5)}" text-anchor="middle" '
      f'font-family="monospace" font-size="{px(2.8)}" fill="{t["inkdim"]}" opacity="0.5">'
      f'source pool</text>')

    # module title — top
    E(f'<text x="{px(PW * 0.5)}" y="{px(8.5)}" text-anchor="middle" '
      f'font-family="monospace" font-size="{px(5.5)}" font-weight="bold" '
      f'letter-spacing="{px(0.8)}" fill="{t["ink"]}">CHANGE ALLEY</text>')

    # bottom brand + dot
    bot = PH - 8.0
    E(f'<circle cx="{px(PW*0.5 - 11)}" cy="{px(bot)}" r="{px(1.8)}" fill="{t["red"]}"/>')
    E(f'<text x="{px(PW*0.5 - 7.5)}" y="{px(bot + 1.8)}" '
      f'font-family="monospace" font-size="{px(3.8)}" fill="{t["ink"]}">modular</text>')

    # bottom note: FX exchange framing
    E(f'<text x="{px(PW*0.5)}" y="{px(PH - 15)}" text-anchor="middle" '
      f'font-family="monospace" font-size="{px(2.5)}" fill="{t["inkdim"]}" opacity="0.55">'
      f'rhythm · melody  ×  voice allocation</text>')

    E('</svg>')
    return "\n".join(A)

if __name__ == "__main__":
    os.makedirs("res/panels", exist_ok=True)
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/ChangeAlley_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"ChangeAlley {theme}: {out}  ({HP}HP, {PW}×{PH}px)")
