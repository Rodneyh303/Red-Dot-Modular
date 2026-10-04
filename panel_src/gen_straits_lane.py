#!/usr/bin/env python3
"""Straits Lane Expander panel — ONE bank of 16 per-voice knobs.
Parameterised by lane tint (rest/accent/qmix). Seamless abutment: same GRID_TOP,
ROW_H, and knob grid as the base + other lane expanders.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import math, os, sys

HP = 11
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

TINTS = {
    "rest":   dict(bg="#14171b", tint="#3f7d78", wave="#2a5a56", knob="#1a2e2c", tinttext="#3f7d78"),
    "accent": dict(bg="#14171b", tint="#e08a1a", wave="#8a5410", knob="#3a2a10", tinttext="#e08a1a"),
    "qmix":   dict(bg="#14171b", tint="#8060c0", wave="#4e3a78", knob="#241a3a", tinttext="#8060c0"),
}

MARGIN   = 5.0
BANK_W   = W - 2*MARGIN
TOP      = 16.0
N_ROWS   = 6
COLS     = [6, 6, 4]
ROW_H    = 14.77
KNOB_R   = 4.5
GRID_TOP = TOP + 2.0

def gen(tintkey, dark=True):
    t = TINTS[tintkey]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="#d4001a"/>')
    # Tint band
    A(f'<rect x="{px(MARGIN-1)}" y="{px(TOP-4)}" width="{px(BANK_W+2)}" height="{px(N_ROWS*ROW_H+6)}" '
      f'rx="{px(1.5)}" fill="{t["tint"]}" fill-opacity="0.08" stroke="{t["tint"]}" '
      f'stroke-width="0.3" stroke-opacity="0.45"/>')
    # Knob grid: 3 cols, 6/6/4, col-major (v0 = mono top-left)
    cw = BANK_W / 3
    v = 0
    for c, nrows in enumerate(COLS):
        roff = (N_ROWS - nrows) / 2.0
        for r in range(nrows):
            cx = MARGIN + cw*(c+0.5)
            cy = GRID_TOP + ROW_H*(r+roff+0.5)
            mono = (v == 0)
            face = t["knob"]
            ring = t["tint"]
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{face}" '
              f'stroke="{ring}" stroke-width="{px(0.8 if mono else 0.5)}"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-KNOB_R*0.8)}" '
              f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
            if mono:
                A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R+1.0)}" fill="none" '
                  f'stroke="#d4001a" stroke-width="{px(0.5)}" stroke-opacity="0.8"/>')
            # Anchor marker (invisible — SvgPanelKit finds by id)
            A(f'<circle id="param_{tintkey}_{v}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')
            v += 1
    # Connect mark anchor
    A(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    tintkey = sys.argv[1] if len(sys.argv) > 1 else "qmix"
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"StraitsLane_{tintkey}_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(tintkey, dark))
        print(f"Generated {path}")
