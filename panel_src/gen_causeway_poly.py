#!/usr/bin/env python3
"""Causeway — poly CV modulation expander panel (functional, low-HP; art later).

Two 16ch poly CV inputs (REST, ACCENT) each with an attenuverter. "The link
across" = modulation across the poly voices. Compact ~6HP placeholder.
nanosvg-safe. Kit id markers:
  input_restcv / param_restatt      REST modulation poly CV in + attenuverter
  input_accentcv / param_accentatt  ACCENT modulation poly CV in + attenuverter
  light_connect                     connect mark anchor
"""
HP = 6
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", jackwell="#0c0e11", jackring="#4a4a4a",
                  well="#0f1114", group="#1c1f24", groupline="#33373d",
                  restcol="#26a69a", acccol="#c8960c"),
    "light": dict(bg="#dcdcdc", red="#d4001a", jackwell="#e2ddd2", jackring="#b0a898",
                  well="#e8e2d6", group="#d0d0d0", groupline="#bcbcbc",
                  restcol="#1c7a70", acccol="#a07a00"),
}


def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle cx="{px(W-3)}" cy="{px(6)}" r="{px(1.4)}" fill="{t["red"]}"/>')
    A(f'<line x1="0" y1="{px(15)}" x2="{PW}" y2="{px(15)}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')

    cx = W * 0.5
    def jack(y, kid):
        A(f'<circle cx="{px(cx)}" cy="{px(y)}" r="{px(3.9)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(y)}" r="0.5" fill="none" stroke="none"/>')
    def trim(y, col, kid):
        A(f'<circle cx="{px(cx)}" cy="{px(y)}" r="{px(3.0)}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>')
        A(f'<line x1="{px(cx)}" y1="{px(y)}" x2="{px(cx)}" y2="{px(y-2.4)}" stroke="{col}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(cx)}" cy="{px(y)}" r="0.5" fill="none" stroke="none"/>')

    # REST group (upper), ACCENT group (lower)
    for (y0, col, cvkid, attkid) in ((34, t["restcol"], "input_restcv", "param_restatt"),
                                     (78, t["acccol"], "input_accentcv", "param_accentatt")):
        A(f'<rect x="{px(cx-8)}" y="{px(y0-8)}" width="{px(16)}" height="{px(30)}" rx="{px(1.5)}" '
          f'fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
        A(f'<rect x="{px(cx-8)}" y="{px(y0-8)}" width="{px(16)}" height="{px(2.2)}" fill="{col}" opacity="0.5"/>')
        jack(y0, cvkid)
        trim(y0 + 14, col, attkid)

    A(f'<circle id="light_connect" cx="{px(cx)}" cy="{px(9)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Causeway_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Causeway {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
