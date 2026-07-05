#!/usr/bin/env python3
"""Changi — per-voice output expander panel (functional, low-HP; art later).

"Departures" — poly voices leave as individual mono jacks. 15 poly voices
(voices 2..16), each with GATE / CV / ACCENT jacks laid out as a grid (3 columns
x 15 rows, but that's tall — use 3 col x 8 row blocks). ~16HP placeholder.
nanosvg-safe. Kit id markers:
  output_gate_<0..14> / output_cv_<0..14> / output_accent_<0..14>
  light_connect
"""
HP = 16
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", jackwell="#0c0e11", jackring="#4a4a4a",
                  group="#1c1f24", groupline="#33373d",
                  gatecol="#6c8cd4", cvcol="#26a69a", acccol="#c8960c", text="#f0f0f0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", jackwell="#e2ddd2", jackring="#b0a898",
                  group="#d0d0d0", groupline="#bcbcbc",
                  gatecol="#4c6ab0", cvcol="#1c7a70", acccol="#a07a00", text="#1a1a1a"),
}

# 15 voices, 3 jacks each. Split into TWO groups of 8 side by side → 6 columns:
# (groupA gate/cv/acc | groupB gate/cv/acc), max 8 rows. Voice v → group v//8, row v%8.
NUM_VOICES = 15
GROUP = 8
TOP  = 22.0
ROW_H = 9.0
JACK_R = 3.4
# 6 columns
COL_X = [W*0.12, W*0.26, W*0.40,  W*0.60, W*0.74, W*0.88]  # A:gate,cv,acc  B:gate,cv,acc


def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle cx="{px(W-4)}" cy="{px(6)}" r="{px(1.6)}" fill="{t["red"]}"/>')
    A(f'<line x1="0" y1="{px(15)}" x2="{PW}" y2="{px(15)}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')

    # column header ticks (both groups)
    for (cx, col) in zip(COL_X, (t["gatecol"], t["cvcol"], t["acccol"],
                                 t["gatecol"], t["cvcol"], t["acccol"])):
        A(f'<rect x="{px(cx-3)}" y="{px(18)}" width="{px(6)}" height="{px(1.8)}" fill="{col}" opacity="0.6"/>')

    def jack(cx, y, kid, col):
        A(f'<circle cx="{px(cx)}" cy="{px(y)}" r="{px(JACK_R)}" fill="{t["jackwell"]}" stroke="{col}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(y)}" r="0.5" fill="none" stroke="none"/>')

    for v in range(NUM_VOICES):
        g = v // GROUP
        row = v % GROUP
        y = TOP + row * ROW_H
        base = 0 if g == 0 else 3
        jack(COL_X[base + 0], y, f"output_gate_{v}",   t["gatecol"])
        jack(COL_X[base + 1], y, f"output_cv_{v}",     t["cvcol"])
        jack(COL_X[base + 2], y, f"output_accent_{v}", t["acccol"])

    A(f'<circle id="light_connect" cx="{px(W*0.5)}" cy="{px(9)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Changi_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Changi {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
