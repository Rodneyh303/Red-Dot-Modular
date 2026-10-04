#!/usr/bin/env python3
"""Straits Base panel — frame + IO (5 poly-cable outs + quantiser CV in) + voice count.
No lane knobs. Lane expanders dock right.

nanosvg-safe (solid fills/strokes, no gradient/mask/text/url).
"""
import math, os

HP = 12
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

MARGIN = 5.0
JACK_Y = 111.1
JACK_R = 4.0
KNOB_R = 4.5

def gen(dark=True):
    bg = "#14171b" if dark else "#dcdcdc"
    jackface = "#0c0e11" if dark else "#e2ddd2"
    jackring = "#4a4a4a" if dark else "#b0a898"
    knobface = "#2a2e33" if dark else "#e8e2d6"
    knobring = "#4a5058" if dark else "#b0a898"
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{bg}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="#d4001a"/>')
    # Voice count knob (centred, upper area)
    vcx, vcy = W/2, 30.0
    A(f'<circle cx="{px(vcx)}" cy="{px(vcy)}" r="{px(KNOB_R)}" fill="{knobface}" '
      f'stroke="{knobring}" stroke-width="{px(0.5)}"/>')
    A(f'<line x1="{px(vcx)}" y1="{px(vcy)}" x2="{px(vcx)}" y2="{px(vcy-KNOB_R*0.8)}" '
      f'stroke="#c0c8d0" stroke-width="{px(0.5)}"/>')
    A(f'<circle id="param_voicecount" cx="{px(vcx)}" cy="{px(vcy)}" r="0.5" fill="none" stroke="none"/>')
    # 5 output jacks + 1 input jack, evenly spaced at JACK_Y
    jack_ids = ["output_polygate", "output_polystepgate", "output_polyslegato",
                "output_polycv", "output_polyaccent", "input_quantcv"]
    n = len(jack_ids)
    spacing = (W - 2*MARGIN) / n
    for i, jid in enumerate(jack_ids):
        jx = MARGIN + spacing*(i+0.5)
        A(f'<circle cx="{px(jx)}" cy="{px(JACK_Y)}" r="{px(JACK_R)}" fill="{jackface}" '
          f'stroke="{jackring}" stroke-width="{px(0.5)}"/>')
        A(f'<circle id="{jid}" cx="{px(jx)}" cy="{px(JACK_Y)}" r="0.5" fill="none" stroke="none"/>')
    # Connect mark anchor
    A(f'<circle id="light_connect" cx="{px(W/2)}" cy="{px(H-8)}" r="0.5" fill="none" stroke="none"/>')
    A('</svg>')
    return '\n'.join(o)

if __name__ == "__main__":
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "res", "panels")
    for dark in [True, False]:
        suffix = "dark" if dark else "light"
        path = os.path.join(outdir, f"StraitsBase_panel_{suffix}.svg")
        with open(path, 'w') as f:
            f.write(gen(dark))
        print(f"Generated {path}")
