#!/usr/bin/env python3
"""Shophouse Micro — tuning+scale scene modulation for the Colonnades family (SHOPHOUSE_MICRO_SPEC).

The microtonal generalisation of Shophouse: FOUR (12-mode) / TWO (24-mode) tuning-slot FRONTS, each a
loaded .dmtune; a CV input sampled at the phrase boundary selects the active front. Panel = a neutral
scene-slot strip of ALTERNATING-COLOUR CELLS (NOT piano keys — microtonal surfaces never imply 12-TET
structure; consistent with the Lantern N-row + Colonnades panels). Each front has a NAME BAND (drawn
by the widget, showing the loaded .dmtune name, truncated) that doubles as the per-front load click
target, plus an active-front lantern. Bottom: CONSERVATION switch + INDEX_CV jack + attenuverter.

nanosvg-safe: solid fills/strokes; TEXT (wordmark, names) is widget-drawn.

Kit markers: cell_<f> (front cell bounds, f=0..3), name_band_<f> (name readout / load target),
             lantern_<f> (active-front indicator), wordmark, param_conservation, input_indexcv,
             param_indexcvatt.
"""

N_FRONTS = 4                         # panel draws the 12-mode max; 24-mode uses the first 2 (widget hides 3-4)
W = 60.0                             # mm (~11.8HP) — compact scene switcher
H = 128.5
S = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
HP = round(W / 5.08, 2)
def px(v): return round(v*S, 2)

WORDMARK_Y = 11.0
FRONT_TOP  = 20.0                    # first cell top
FRONT_H    = 20.0                    # cell height
FRONT_GAP  = 3.0                     # gap between cells
NAME_H     = 6.0                     # name band height within the cell (bottom strip)
CONSERV_Y  = 112.0
CV_Y       = 120.0

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", sub="#8a94a0",
                  ring="#4a4a4a", cellA="#22252a", cellB="#2b323a", band="#0a0c0e",
                  well="#0f1114", lantern_off="#3a1f22", knob="#2a2e34", knobring="#5a616a"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", sub="#5a6470",
                  ring="#b0a898", cellA="#d2d6da", cellB="#c4ccd2", band="#0a0c0e",
                  well="#e2ddd2", lantern_off="#e8d0cc", knob="#c8cdd4", knobring="#9aa2ac"),
}

def front_y(f): return FRONT_TOP + f * (FRONT_H + FRONT_GAP)

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle id="wordmark" cx="{px(W/2)}" cy="{px(WORDMARK_Y)}" r="0.5" fill="none" stroke="none"/>')

    cell_x0, cell_w = 5.0, W - 10.0
    for f in range(N_FRONTS):
        y = front_y(f)
        fill = t["cellA"] if (f % 2 == 0) else t["cellB"]   # ALTERNATING colour cells
        # Cell body.
        A(f'<rect x="{px(cell_x0)}" y="{px(y)}" width="{px(cell_w)}" height="{px(FRONT_H)}" '
          f'rx="{px(1.2)}" fill="{fill}" stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<rect id="cell_{f}" x="{px(cell_x0)}" y="{px(y)}" width="{px(cell_w)}" '
          f'height="{px(FRONT_H)}" fill="none" stroke="none"/>')
        # Active-front lantern (small dot top-left of the cell; widget lights only the active one).
        lx, ly = cell_x0 + 3.0, y + 3.5
        A(f'<circle cx="{px(lx)}" cy="{px(ly)}" r="{px(1.6)}" fill="{t["lantern_off"]}" '
          f'stroke="{t["ring"]}" stroke-width="0.3"/>')
        A(f'<circle id="lantern_{f}" cx="{px(lx)}" cy="{px(ly)}" r="0.5" fill="none" stroke="none"/>')
        # Name band (bottom strip of the cell) — recessed dark; widget draws the .dmtune name + is the
        # load click target.
        by = y + FRONT_H - NAME_H - 1.0
        A(f'<rect x="{px(cell_x0+1.5)}" y="{px(by)}" width="{px(cell_w-3.0)}" height="{px(NAME_H)}" '
          f'rx="{px(0.6)}" fill="{t["band"]}" stroke="{t["ring"]}" stroke-width="0.3"/>')
        A(f'<rect id="name_band_{f}" x="{px(cell_x0+1.5)}" y="{px(by)}" width="{px(cell_w-3.0)}" '
          f'height="{px(NAME_H)}" fill="none" stroke="none"/>')

    # CONSERVATION switch (left) + label drawn by widget.
    A(f'<rect x="{px(8.0-2.5)}" y="{px(CONSERV_Y-3.0)}" width="{px(5.0)}" height="{px(6.0)}" '
      f'rx="{px(0.8)}" fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.4"/>')
    A(f'<circle id="param_conservation" cx="{px(8.0)}" cy="{px(CONSERV_Y)}" r="0.5" fill="none" stroke="none"/>')

    # INDEX CV jack + attenuverter (bottom row).
    A(f'<circle cx="{px(18.0)}" cy="{px(CV_Y)}" r="{px(4.2)}" fill="{t["well"]}" '
      f'stroke="{t["ring"]}" stroke-width="0.4"/>')
    A(f'<circle id="input_indexcv" cx="{px(18.0)}" cy="{px(CV_Y)}" r="0.5" fill="none" stroke="none"/>')
    A(f'<circle cx="{px(34.0)}" cy="{px(CV_Y)}" r="{px(3.4)}" fill="{t["knob"]}" '
      f'stroke="{t["knobring"]}" stroke-width="0.5"/>')
    A(f'<line x1="{px(34.0)}" y1="{px(CV_Y-3.0)}" x2="{px(34.0)}" y2="{px(CV_Y-1.8)}" '
      f'stroke="{t["ink"]}" stroke-width="0.5"/>')
    A(f'<circle id="param_indexcvatt" cx="{px(34.0)}" cy="{px(CV_Y)}" r="0.5" fill="none" stroke="none"/>')

    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "ShophouseMicro_panel_dark.svg"), (False, "ShophouseMicro_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"ShophouseMicro {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
