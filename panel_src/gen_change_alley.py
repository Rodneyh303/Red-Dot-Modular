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

PW_MM = 23 * 5.08   # 116.84 mm (23HP)
PH_MM = 128.5       # standard Rack height

HP = 23

# All coordinates in mm; SVG uses the same unit via viewBox.
SVG_W, SVG_H = PW_MM, PH_MM

DARK_GRID = dict(
    bg      = "#18181a",        # panel body — Raffles house charcoal (was greenish)
    bgdeep  = "#0f1012",        # recessed tone (matrix well surround, header trough)
    red     = "#d4001a",
    ink     = "#e8e2d0",
    inkdim  = "#7a7670",
    steel   = "#46464c",        # house structure grey (frame lines, Raffles)
    steeldim= "#3a3a3e",
    well    = "#0b0c0d",        # cell background (deepest, so pins pop)
    gridln  = "#26262b",        # grid lines — neutral, lifted off pure black
    booth   = "#141416",        # alternate row tint (neutral now)
    # Pin colours — red & white on black (confirmed on the Change Alley board)
    rhythm  = "#f2f2f0",        # white — rhythm source
    melody  = "#d4001a",        # red — melody source
    rhythmD = "#4a4a4e",        # ghost outline — rhythm
    melodyD = "#5a1018",        # ghost outline — melody
    active  = "#d4001a",
    inactive= "#2a2a2e",
    mono    = "#c8960c",        # amber (Raffles amber) for row/col 1 (mono)
)

# Light mode: keep the matrix grid dark (forced, same as Lantern LCD); surround follows
# the house LIGHT panel (warm greys, Raffles).
LIGHT_GRID = dict(**DARK_GRID)
LIGHT_GRID.update(dict(
    bg     = "#e8e2d6",         # panel body — Raffles house cream
    bgdeep = "#dcdcdc",         # recessed tone
    ink    = "#1a1810",
    inkdim = "#6a6458",
    steel  = "#c0b8a8",         # house light structure grey
    steeldim="#bcbcbc",
    mono   = "#b07d00",         # Raffles light amber
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
    bgdeep   = DARK_GRID["bgdeep"] if dark else LIGHT_GRID["bgdeep"]
    steel    = DARK_GRID["steel"] if dark else LIGHT_GRID["steel"]

    o = []
    def E(s): o.append(s)

    # Document size in PX (Rack: 1HP = 15px, 75dpi => px = mm * 75/25.4). 18HP = 270px,
    # 128.5mm = 379.43px (house height, cf. Causeway). The viewBox keeps every coordinate
    # below in mm, and 345/116.84 == Rack's mm2px scale exactly, so the widget's mm2px
    # hit-test lands on the same points the panel draws.
    E(f'<svg width="345.0" height="379.43" '
      f'viewBox="0 0 {SVG_W:.3f} {SVG_H:.3f}" xmlns="http://www.w3.org/2000/svg">')

    # Panel background
    E(f'<rect width="{SVG_W:.3f}" height="{SVG_H:.3f}" fill="{bg_panel}"/>')

    # Top red stripe
    E(f'<rect width="{SVG_W:.3f}" height="2.0" fill="{t["red"]}"/>')

    # ── Matrix geometry ───────────────────────────────────────────────────────
    GUTTER_L  = 13.0   # mm for row currency labels
    GUTTER_R  =  3.5   # mm right margin / poly indicator
    GUTTER_T  =  9.0   # mm for column labels at top
    GUTTER_B  = 10.0   # mm for brand at bottom

    MY = GUTTER_T + 8.0
    avail_h = SVG_H - MY - GUTTER_B - 2.0
    CELL   = avail_h / N
    CELL_W = CELL; CELL_H = CELL
    MW = CELL * N; MH = CELL * N
    MX = GUTTER_L + (SVG_W - GUTTER_L - GUTTER_R - MW) * 0.5

    # ── Header trough (recessed strip behind the title) ───────────────────────
    E(f'<rect x="0" y="2.0" width="{SVG_W:.3f}" height="{MY - CELL*1.2:.3f}" '
      f'fill="{bgdeep}"/>')
    E(f'<line x1="0" y1="{MY - CELL*1.2:.3f}" x2="{SVG_W:.3f}" y2="{MY - CELL*1.2:.3f}" '
      f'stroke="{steel}" stroke-width="0.3" opacity="0.5"/>')

    # ── Recessed frame around the matrix (bgdeep bezel + steel hairline) ───────
    pad = 2.2
    E(f'<rect x="{MX-pad:.3f}" y="{MY-pad:.3f}" '
      f'width="{MW+2*pad:.3f}" height="{MH+2*pad:.3f}" rx="1.2" '
      f'fill="{bgdeep}" stroke="{steel}" stroke-width="0.35"/>')

    # ── Matrix well (deepest dark, so red/white pins pop) ─────────────────────
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
        x = MX + i * CELL_W
        E(f'<line x1="{x:.3f}" y1="{MY:.3f}" x2="{x:.3f}" y2="{MY+MH:.3f}" '
          f'stroke="{t["gridln"]}" stroke-width="0.4"/>')
        y = MY + i * CELL_H
        E(f'<line x1="{MX:.3f}" y1="{y:.3f}" x2="{MX+MW:.3f}" y2="{y:.3f}" '
          f'stroke="{t["gridln"]}" stroke-width="0.4"/>')

    # ── Bottom dot.modular logo — embed the REAL asset (res/logo/dot-modular-logo-*.svg),
    #    same wordmark crop the Sands/Straits panels use. My coordinate space is mm (viewBox),
    #    so the crop transform is expressed directly in mm. nanosvg renders the logo's paths
    #    (it's outlined vector, unlike a <text> element). ──
    import os as _os, re as _re
    _logo_dir = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "res", "logo")
    _logo_path = _os.path.join(_logo_dir, "dot-modular-logo-dark.svg" if dark else "dot-modular-logo-light.svg")
    _ls = open(_logo_path).read()
    _body = _ls[_ls.find('<g '):_ls.rfind('</svg>')]
    _body = _re.sub(r'<polyline\b[^>]*/>', '', _body)   # strip positioning brackets
    CX, CY, CW = 26.0, 65.0, 657.0                      # wordmark crop (matches *-tight.svg)
    logo_w_mm = 30.0
    sc = logo_w_mm / CW
    tx = (SVG_W - logo_w_mm) * 0.5
    ty = SVG_H - GUTTER_B * 0.5 - 2.0
    E(f'<g transform="translate({tx:.3f},{ty:.3f}) scale({sc:.6f}) translate({-CX:.3f},{-CY:.3f})">{_body}</g>')

    # (Pins, labels, legend, crosshairs are drawn by the widget — nanosvg ignores
    #  <text>; the logo above is outlined vector so it renders in the panel.)

    E('</svg>')
    return "\n".join(o)

if __name__ == "__main__":
    os.makedirs("res/panels", exist_ok=True)
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/ChangeAlley_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"ChangeAlley {theme}: {out}  ({HP}HP, {SVG_W:.1f}×{SVG_H:.1f}mm)")
