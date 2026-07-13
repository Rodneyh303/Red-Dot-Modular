#!/usr/bin/env python3
"""Causeway — poly CV modulation expander panel (functional; art later).

Two 16ch poly CV inputs (REST, ACCENT). Each voice (2..16) has its own REST and
ACCENT attenuator; a per-lane GLOBAL attenuator is summed on top.

Layout: voices split into TWO groups of 8 side by side → 4 attenuator columns
(GroupA rest, GroupA acc | GroupB rest, GroupB acc), max 8 rows. Globals + the two
poly CV jacks below. nanosvg-safe. Kit id markers:
  param_restatt_<0..14> / param_accatt_<0..14>   per-voice attenuators
  param_restatt_global / param_accatt_global      global attenuators
  input_restcv / input_accentcv                   16ch poly CV in
  light_connect
"""
import os, re
HP = 16
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", jackwell="#0c0e11", jackring="#4a4a4a",
                  well="#0f1114", group="#1c1f24", groupline="#33373d",
                  restcol="#26a69a", acccol="#c8960c", text="#f0f0f0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", jackwell="#e2ddd2", jackring="#b0a898",
                  well="#e8e2d6", group="#d0d0d0", groupline="#bcbcbc",
                  restcol="#1c7a70", acccol="#a07a00", text="#1a1a1a"),
}

NUM_VOICES = 15
GROUP = 8                # 8 rows per group
TOP  = 34.0
ROW_H = 8.0
TRIM_R = 2.6
GLOBAL_Y = 22.0
# 4 columns: groupA rest, groupA acc, groupB rest, groupB acc
COLX = [W*0.18, W*0.38, W*0.62, W*0.82]


# ── dot.modular wordmark embed (nondestructive wordmark crop — strips positioning
#    brackets + blank margins via a translate/scale transform; source logo untouched). ──
_LOGO_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "logo")
def logo_embed(dark, x_mm, y_mm, target_w_mm):
    path = os.path.join(_LOGO_DIR, "dot-modular-logo-dark.svg" if dark else "dot-modular-logo-light.svg")
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]
    body = re.sub(r'<polyline\b[^>]*/>', '', body)   # strip the 2 positioning brackets
    CX, CY, CW = 26.0, 65.0, 657.0                   # wordmark crop (matches *-tight.svg)
    sc = px(target_w_mm) / CW
    tx, ty = px(x_mm), px(y_mm)
    return f'<g transform="translate({tx:.2f},{ty:.2f}) scale({sc:.5f}) translate({-CX:.2f},{-CY:.2f})">{body}</g>'

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle cx="{px(W-4)}" cy="{px(6)}" r="{px(1.6)}" fill="{t["red"]}"/>')
    A(f'<line x1="0" y1="{px(15)}" x2="{PW}" y2="{px(15)}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')

    def trim(cx, cy, col, kid, big=False):
        r = TRIM_R * (1.35 if big else 1.0)
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>')
        A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r+0.5)}" stroke="{col}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    # column header ticks (rest/acc colour per column)
    for cx, col in ((COLX[0], t["restcol"]), (COLX[1], t["acccol"]),
                    (COLX[2], t["restcol"]), (COLX[3], t["acccol"])):
        A(f'<rect x="{px(cx-3)}" y="{px(29)}" width="{px(6)}" height="{px(1.6)}" fill="{col}" opacity="0.6"/>')

    # globals (top): rest global spanning left, acc global spanning right
    trim((COLX[0]+COLX[1])/2, GLOBAL_Y, t["restcol"], "param_restatt_global", big=True)
    trim((COLX[2]+COLX[3])/2, GLOBAL_Y, t["acccol"],  "param_accatt_global",  big=True)
    A(f'<line x1="{px(4)}" y1="{px(GLOBAL_Y+5.5)}" x2="{px(W-4)}" y2="{px(GLOBAL_Y+5.5)}" stroke="{t["groupline"]}" stroke-width="0.75" opacity="0.5"/>')

    # per-voice attenuators: voice v → group g = v//8, row = v%8
    for v in range(NUM_VOICES):
        g = v // GROUP
        row = v % GROUP
        cy = TOP + row * ROW_H
        rest_cx = COLX[0] if g == 0 else COLX[2]
        acc_cx  = COLX[1] if g == 0 else COLX[3]
        trim(rest_cx, cy, t["restcol"], f"param_restatt_{v}")
        trim(acc_cx,  cy, t["acccol"],  f"param_accatt_{v}")

    # poly CV input jacks (bottom, centred)
    jy = TOP + GROUP * ROW_H + 4.0
    for (cx, kid) in ((W*0.34, "input_restcv"), (W*0.66, "input_accentcv")):
        A(f'<circle cx="{px(cx)}" cy="{px(jy)}" r="{px(3.9)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(jy)}" r="0.5" fill="none" stroke="none"/>')

    A(f'<circle id="light_connect" cx="{px(W*0.5)}" cy="{px(9)}" r="0.5" fill="none" stroke="none"/>')
    # dot.modular wordmark — centred horizontally, lower band (a bit below the Sands panels' y≈113)
    A(logo_embed(dark, (W - 30.0) / 2.0, 116.0, 30.0))
    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Causeway_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Causeway {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
