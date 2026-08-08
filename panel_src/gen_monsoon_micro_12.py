#!/usr/bin/env python3
"""Monsoon Micro 12 — tuning + scale AUTHORING expander panel.

LIFT-AND-SHIFT of Monsoon's note-fader block (COLONNADES_PANEL_LIFT_SPEC.md): the 12 weight faders
use Monsoon's SAME 9.0mm horizontal pitch (embed_monsoon.py:6, SEMI{i} at 7.5+i*9.0) so the two
panels visually rhyme. Faders are NUMBERED 1..12 (widget-drawn) — degrees, not note names, because an
arbitrary tuning's degrees aren't notes. Below the faders the per-degree CENTS knobs are STAGGERED on
two rows (even indices upper, odd lower — a zigzag) so each has horizontal room; each cents knob is
X-aligned to its fader. Root (degree 0) has NO cents knob — locked at 0 — its slot is a locked plate.

Width is set by the fader math (12 * 9.0mm + margins), not a guessed HP.

nanosvg-safe: solid fills/strokes only. All TEXT (wordmark, the 1..12 numbers) is widget-drawn
(Micro12Labels); the panel emits geometry + markers only.

Kit id markers:
  param_weight_<i>   weight LIGHT-slider marker (fader centre), i = 0..11
  param_cents_<i>    cents knob marker, i = 1..11 (root i=0 = locked plate, no marker)
  notelabel_<i>      degree-number anchor per strip (widget draws "1".."12")
  wordmark           wordmark anchor
  light_connect      ConnectMark position
"""

N = 12
PITCH = 9.0                 # Monsoon's fader pitch — the consistency anchor
FADER_CENTER_SPAN = (N - 1) * PITCH        # 99.0mm centre-to-centre
SIDE_MARGIN = 6.5
W = FADER_CENTER_SPAN + 2 * SIDE_MARGIN     # ~112mm
H = 128.5
S = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
HP = round(W / 5.08, 2)
def px(v): return round(v*S, 2)

FIRST_X = SIDE_MARGIN
def fader_cx(i): return FIRST_X + i * PITCH

# Vertical budget
WORDMARK_Y = 11.5
NUM_Y      = 22.0           # degree-number strip (above faders)
FADER_CY   = 52.0           # fader centre (VCVLightSlider sizes itself; ~28mm tall → ~38..66)
CENTS_ROW_A = 90.0          # even-index cents knobs
CENTS_ROW_B = 99.5          # odd-index cents knobs (staggered lower)
KNOB_R     = 3.0
CONNECT_Y  = 120.0

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  well="#0f1114", ring="#4a4a4a", knob="#2a2e34", knobring="#5a616a",
                  fadertrack="#20242a", lockwell="#241f14", sub="#8a94a0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  well="#e2ddd2", ring="#b0a898", knob="#c8cdd4", knobring="#9aa2ac",
                  fadertrack="#cdd2d8", lockwell="#e8e0cc", sub="#5a6470"),
}

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle id="wordmark" cx="{px(W/2)}" cy="{px(WORDMARK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # Per-degree number anchors (widget draws 1..12). Placed at the fader X, above the fader.
    for i in range(N):
        A(f'<circle id="notelabel_{i}" cx="{px(fader_cx(i))}" cy="{px(NUM_Y)}" r="0.5" fill="none" stroke="none"/>')

    # Faders: a subtle track behind each; the VCVLightSlider widget is centred on param_weight_<i>.
    # Track height ≈ the slider travel (~28mm), purely decorative so the panel doesn't look empty
    # behind the (SVG-sized) slider.
    track_h = 30.0
    track_w = 3.0
    for i in range(N):
        cx = fader_cx(i)
        A(f'<rect x="{px(cx - track_w/2)}" y="{px(FADER_CY - track_h/2)}" width="{px(track_w)}" '
          f'height="{px(track_h)}" rx="{px(track_w*0.4)}" fill="{t["fadertrack"]}" '
          f'stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<circle id="param_weight_{i}" cx="{px(cx)}" cy="{px(FADER_CY)}" r="0.5" fill="none" stroke="none"/>')

    # Cents knobs, staggered zigzag. Root (0) = locked plate on row A; others alternate A/B by parity.
    for i in range(N):
        cx = fader_cx(i)
        cy = CENTS_ROW_A if (i % 2 == 0) else CENTS_ROW_B
        if i == 0:
            A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["lockwell"]}" '
              f'stroke="{t["ring"]}" stroke-width="0.5"/>')
            continue
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R+0.6)}" fill="{t["well"]}" '
          f'stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<circle cx="{px(cx)}" cy="{px(cy)}" r="{px(KNOB_R)}" fill="{t["knob"]}" '
          f'stroke="{t["knobring"]}" stroke-width="0.5"/>')
        A(f'<line x1="{px(cx)}" y1="{px(cy-KNOB_R+0.4)}" x2="{px(cx)}" y2="{px(cy-KNOB_R+1.4)}" '
          f'stroke="{t["gold"]}" stroke-width="0.5"/>')
        A(f'<circle id="param_cents_{i}" cx="{px(cx)}" cy="{px(cy)}" r="0.5" fill="none" stroke="none"/>')

    lcx, lcy = W/2, CONNECT_Y
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
