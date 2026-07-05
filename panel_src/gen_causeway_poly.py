#!/usr/bin/env python3
"""Causeway — poly CV modulation expander panel (functional; art later).

Two 16ch poly CV inputs (REST, ACCENT). Each voice (2..16) has its own REST and
ACCENT attenuator; a global attenuator per lane is summed on top. "The link
across" = modulation across the voices. ~14HP portrait. nanosvg-safe.
Kit id markers:
  param_restatt_<0..14> / param_accatt_<0..14>   per-voice attenuators
  param_restatt_global / param_accatt_global      global attenuators
  input_restcv / input_accentcv                   16ch poly CV in
  light_connect
"""
HP = 14
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

ROWS = 15
TOP  = 30.0
ROW_H = 6.0
TRIM_R = 2.4
REST_X = W * 0.34
ACC_X  = W * 0.66
GLOBAL_Y = 20.0


def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle cx="{px(W-4)}" cy="{px(6)}" r="{px(1.6)}" fill="{t["red"]}"/>')
    A(f'<line x1="0" y1="{px(15)}" x2="{PW}" y2="{px(15)}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')

    for (cx, col) in ((REST_X, t["restcol"]), (ACC_X, t["acccol"])):
        A(f'<rect x="{px(cx-4)}" y="{px(16.5)}" width="{px(8)}" height="{px(2)}" fill="{col}" opacity="0.6"/>')

    def trim(cx, cy, col, kid, big=False):
        r = TRIM_R * (1.35 if big else 1.0)
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(r)}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>')
        A(f'<line x1="{px(cx)}" y1="{px(cy)}" x2="{px(cx)}" y2="{px(cy-r+0.5)}" stroke="{col}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    trim(REST_X, GLOBAL_Y, t["restcol"], "param_restatt_global", big=True)
    trim(ACC_X,  GLOBAL_Y, t["acccol"],  "param_accatt_global",  big=True)
    A(f'<line x1="{px(4)}" y1="{px(GLOBAL_Y+5)}" x2="{px(W-4)}" y2="{px(GLOBAL_Y+5)}" stroke="{t["groupline"]}" stroke-width="0.75" opacity="0.5"/>')

    for i in range(ROWS):
        cy = TOP + i * ROW_H
        trim(REST_X, cy, t["restcol"], f"param_restatt_{i}")
        trim(ACC_X,  cy, t["acccol"],  f"param_accatt_{i}")

    jy = TOP + ROWS * ROW_H + 6.0
    for (cx, kid) in ((REST_X, "input_restcv"), (ACC_X, "input_accentcv")):
        A(f'<circle cx="{px(cx)}" cy="{px(jy)}" r="{px(3.9)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(jy)}" r="0.5" fill="none" stroke="none"/>')

    A(f'<circle id="light_connect" cx="{px(W*0.5)}" cy="{px(9)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Causeway_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Causeway {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
