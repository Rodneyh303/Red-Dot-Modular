#!/usr/bin/env python3
"""Straits — base poly expander panel (functional, low-HP; art refined later).

The redesign/simplification of the old Straits East/West pair into ONE module.
Carries the per-poly-voice REST + ACCENT probability knobs (15 poly voices =
voices 2..16) as a compact grid, plus three 16ch poly-cable output jacks
(gate / CV / accent). CV modulation moved to Causeway; per-voice mono outs to
Changi — neither appears here.

This is a FUNCTIONAL placeholder panel: solid fills, kit id markers, no art.
~22HP. nanosvg-safe (no gradients/masks/text-as-font/url).

Kit id markers emitted (bound by MonsoonStraitsExpander widget):
  param_rest_<0..14>     REST probability knob, poly voice v+2
  param_accent_<0..14>   ACCENT probability knob, poly voice v+2
  output_polygate        16ch poly gate cable
  output_polycv          16ch poly CV cable
  output_polyaccent      16ch poly accent cable
  light_connect          dot.modular connect mark anchor
"""

HP = 22
W  = HP * 5.08                 # mm (1HP = 5.08mm)
H  = 128.5                     # mm (3U)
S  = 75 / 25.4                 # px per mm
PW, PH = round(W * S, 2), round(H * S, 2)
def px(v): return round(v * S, 2)

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", redsoft="#dc2626", gold="#c8960c",
                  well="#0f1114", wellring="#3a3a3a", jackwell="#0c0e11",
                  jackring="#4a4a4a", group="#1c1f24", groupline="#33373d",
                  text="#f0f0f0", restcol="#26a69a", acccol="#c8960c"),
    "light": dict(bg="#dcdcdc", red="#d4001a", redsoft="#c0001a", gold="#b07d00",
                  well="#e8e2d6", wellring="#c0b8a8", jackwell="#e2ddd2",
                  jackring="#b0a898", group="#d0d0d0", groupline="#bcbcbc",
                  text="#1a1a1a", restcol="#1c7a70", acccol="#a07a00"),
}

# ── Layout ───────────────────────────────────────────────────────────────────
# 15 poly voices, REST + ACCENT each. Lay out as 8 rows: 4 knob columns per row
# (2 REST + 2 ACCENT) would give 16 slots/8 rows — but we have 15 voices, so we
# use a simpler readable scheme: two side-by-side blocks (REST | ACCENT), each a
# column of 15 small knobs is too tall; instead 15 = 8 rows x 2 cols (last cell
# spare) PER block. So: left block REST (8x2), right block ACCENT (8x2).
ROWS = 8
COLS = 2                       # per block
KNOB_R = 3.0
TOP    = 20.0                  # first knob row y
ROW_H  = 11.0                  # vertical pitch
COL_W  = 9.0                   # horizontal pitch within a block

REST_X0 = 10.0                 # left block first column x
ACC_X0  = W - 10.0 - COL_W     # right block first column x (mirrored)

def voice_pos(block_x0, idx):
    """idx 0..14 → (x,y) within an 8row x 2col block, column-major."""
    col = idx // ROWS          # 0 or 1
    row = idx %  ROWS
    return block_x0 + col * COL_W, TOP + row * ROW_H

JACK_Y = H - 16.0              # poly output jacks along the bottom
JACK_R = 3.9


def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []
    A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    # top red hairline + brand dot
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle cx="{px(W-4)}" cy="{px(6)}" r="{px(1.6)}" fill="{t["red"]}"/>')
    # header separator
    A(f'<line x1="0" y1="{px(15)}" x2="{PW}" y2="{px(15)}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.7"/>')
    # two block backgrounds (REST | ACCENT)
    for (bx, col) in ((REST_X0, t["restcol"]), (ACC_X0, t["acccol"])):
        gx = bx - 4.0
        gw = COL_W + 8.0
        A(f'<rect x="{px(gx)}" y="{px(TOP-6)}" width="{px(gw)}" height="{px(ROWS*ROW_H+2)}" '
          f'rx="{px(1.5)}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>')
        A(f'<rect x="{px(gx)}" y="{px(TOP-6)}" width="{px(gw)}" height="{px(2.2)}" fill="{col}" opacity="0.5"/>')

    # knob wells + kit id markers
    def knob(x, y, col, kid):
        A(f'<circle cx="{px(x)}" cy="{px(y)}" r="{px(KNOB_R)}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>')
        A(f'<line x1="{px(x)}" y1="{px(y)}" x2="{px(x)}" y2="{px(y-KNOB_R+0.6)}" stroke="{col}" stroke-width="1"/>')
        # invisible kit marker (bind anchor)
        A(f'<circle id="{kid}" cx="{px(x)}" cy="{px(y)}" r="0.5" fill="none" stroke="none"/>')

    for i in range(15):
        rx, ry = voice_pos(REST_X0, i)
        knob(rx, ry, t["restcol"], f"param_rest_{i}")
        ax, ay = voice_pos(ACC_X0, i)
        knob(ax, ay, t["acccol"], f"param_accent_{i}")

    # poly output jacks (3, along the bottom)
    def jack(x, y, kid):
        A(f'<circle cx="{px(x)}" cy="{px(y)}" r="{px(JACK_R)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>')
        A(f'<circle id="{kid}" cx="{px(x)}" cy="{px(y)}" r="0.5" fill="none" stroke="none"/>')

    jx = [W*0.28, W*0.5, W*0.72]
    jack(jx[0], JACK_Y, "output_polygate")
    jack(jx[1], JACK_Y, "output_polycv")
    jack(jx[2], JACK_Y, "output_polyaccent")
    A(f'<line x1="0" y1="{px(JACK_Y-7)}" x2="{PW}" y2="{px(JACK_Y-7)}" stroke="{t["groupline"]}" stroke-width="1" opacity="0.6"/>')

    # connect mark anchor
    A(f'<circle id="light_connect" cx="{px(W*0.5)}" cy="{px(9)}" r="0.5" fill="none" stroke="none"/>')

    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark in (True, False):
        theme = "dark" if dark else "light"
        out = f"res/panels/Straits_panel_{theme}.svg"
        open(out, "w").write(gen(dark))
        print(f"Straits {theme}: {out}  ({HP}HP, {PW}x{PH}px)")
