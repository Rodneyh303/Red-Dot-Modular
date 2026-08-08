#!/usr/bin/env python3
"""Monsoon Micro 12 — tuning + scale AUTHORING expander panel (14HP).

12 per-degree vertical strips, left→right = C..B (monotonic pitch order). Each strip:
  - a WEIGHT fader (the scale mask; 0 at bottom = degree disabled),
  - below it a CENTS knob (the tuning; equal-division default) — EXCEPT the root (C), whose cents is
    locked at 0 (no knob; a small locked plate instead),
  - a note-name label above (widget-drawn; nanosvg ignores <text>).

nanosvg-safe: solid fills + strokes only. Text is drawn by the widget (Micro12Labels); the panel
emits geometry + markers only.

Kit id markers:
  param_weight_<i>   weight fader marker, i = 0..11 (all degrees)
  param_cents_<i>    cents knob marker, i = 1..11 (root i=0 has NO cents knob)
  notelabel_<i>      note-name anchor per strip (widget draws the name)
  wordmark           wordmark anchor (widget draws "Micro 12")
  light_connect      ConnectMark position
"""

HP = 14
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

NOTE = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  well="#0f1114", ring="#4a4a4a", knob="#2a2e34", knobring="#5a616a",
                  fadertrack="#20242a", faderfill="#26a69a", faderhandle="#c8cdd4",
                  lockwell="#241f14", sub="#8a94a0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  well="#e2ddd2", ring="#b0a898", knob="#c8cdd4", knobring="#9aa2ac",
                  fadertrack="#cdd2d8", faderfill="#1c7a70", faderhandle="#6a727c",
                  lockwell="#e8e0cc", sub="#5a6470"),
}

MARGIN_X = 5.0
TOP      = 20.0          # below the wordmark
BOT_PAD  = 14.0         # room for connect light
N = 12
STRIP_W  = (W - 2*MARGIN_X) / N

LABEL_Y   = TOP + 2.0            # note name row
FADER_TOP = TOP + 6.0
FADER_H   = 62.0
CENTS_Y   = FADER_TOP + FADER_H + 9.0   # cents knob row (below faders)
KNOB_R    = 3.2

def strip_cx(i): return MARGIN_X + STRIP_W*(i + 0.5)

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle id="wordmark" cx="{px(W/2)}" cy="{px(11.5)}" r="0.5" fill="none" stroke="none"/>')

    fader_w = min(STRIP_W * 0.34, 3.2)
    for i in range(N):
        cx = strip_cx(i)
        # note-name anchor (widget draws the name)
        A(f'<circle id="notelabel_{i}" cx="{px(cx)}" cy="{px(LABEL_Y)}" r="0.5" fill="none" stroke="none"/>')
        # fader track
        A(f'<rect x="{px(cx - fader_w/2)}" y="{px(FADER_TOP)}" width="{px(fader_w)}" height="{px(FADER_H)}" '
          f'rx="{px(fader_w*0.4)}" fill="{t["fadertrack"]}" stroke="{t["ring"]}" stroke-width="0.4"/>')
        # a subtle centre guide line
        A(f'<line x1="{px(cx)}" y1="{px(FADER_TOP+1)}" x2="{px(cx)}" y2="{px(FADER_TOP+FADER_H-1)}" '
          f'stroke="{t["ring"]}" stroke-width="0.3"/>')
        # fader marker (widget binds a VCVSlider here; it sizes to the track)
        A(f'<circle id="param_weight_{i}" cx="{px(cx)}" cy="{px(FADER_TOP+FADER_H/2)}" r="0.5" fill="none" stroke="none"/>')

        cy = CENTS_Y
        if i == 0:
            # root: locked cents plate, no knob
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["lockwell"]}" '
              f'stroke="{t["ring"]}" stroke-width="0.5"/>')
        else:
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R+0.6)}" fill="{t["well"]}" '
              f'stroke="{t["ring"]}" stroke-width="0.4"/>')
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["knob"]}" '
              f'stroke="{t["knobring"]}" stroke-width="0.5"/>')
            A(f'<line x1="{px(cx)}" y1="{px(cy-KNOB_R+0.4)}" x2="{px(cx)}" y2="{px(cy-KNOB_R+1.4)}" '
              f'stroke="{t["gold"]}" stroke-width="0.5"/>')
            A(f'<circle id="param_cents_{i}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    lcx, lcy = W/2, H - BOT_PAD/2
    A(f'<circle cx="{px(lcx)}" cy="{px(lcy)}" r="{px(1.8)}" fill="{t["well"]}" '
      f'stroke="{t["ring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(lcy)}" r="0.5" fill="none" stroke="none"/>')

    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "MonsoonMicro12_panel_dark.svg"), (False, "MonsoonMicro12_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Micro12 {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
