#!/usr/bin/env python3
"""Colonnades (Micro-12) — tuning + scale AUTHORING expander panel.

TRUE LIFT of Monsoon's note-fader block (COLONNADES_PANEL_LIFT_SPEC.md + ROUND 2). The faders reuse
Monsoon's EXACT geometry so the two panels align pixel-for-pixel when stacked:
  - X = 7.5 + i*9.0 mm  (Monsoon SEMI fader pitch, embed_monsoon.py:6)
  - travel top 45mm → bottom 74.5mm, centre 59.75mm  (MonsoonWidget.hpp SL_TOP=45, SLH=29.5)
  - level-marker TICKS lifted from fader_level_markers.py (Befaco-Octaves style: one column per gap,
    long/solid/uniform, single-sourced from the fader X list so ticks can't drift)
Faders are NUMBERED 1..12 (widget-drawn), BELOW the faders (Monsoon step-strip position). Degrees, not
notes. Below the numbers the per-degree CENTS knobs are staggered on two rows (zigzag); root (0) has
no cents knob — locked plate. The fader is a ColonnadesLightSlider<GreenRedLight>: grey when off,
red when the degree plays (light driven by the module from the host's semiLedBrightness).

nanosvg-safe: solid fills/strokes only. TEXT (wordmark, 1..12) is widget-drawn (ColonnadesLabels).

Kit markers: param_weight_<i> (fader, 0..11), param_cents_<i> (1..11), notelabel_<i> (below faders),
             wordmark, light_connect.
"""

N = 12
PITCH = 9.0
FIRST_X = 7.5                         # EXACT Monsoon SEMI X0 (7.5 + i*9.0) — for stacking alignment
def fader_cx(i): return FIRST_X + i * PITCH

SIDE_MARGIN = 6.5
W = (FIRST_X + (N - 1) * PITCH) + FIRST_X    # symmetric right margin == FIRST_X → 7.5+99+7.5 = 114mm
H = 128.5
S = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
HP = round(W / 5.08, 2)
def px(v): return round(v*S, 2)

# Fader travel (Monsoon SL_TOP=45, bottom=74.5, centre 59.75). The VCVLightSlider widget owns the
# actual handle geometry via its anchor + SVG; these are for the TRACK + TICKS the panel draws.
FADER_TOP = 45.0
FADER_BOT = 74.5
FADER_CY  = 59.75
TRACK_HALF = 1.6                      # visible track half-width (matches fader_level_markers TRACK_HALF_MM)

# Level ticks — lifted from fader_level_markers.py: 9 rows across the travel, outermost 2 dropped → 7.
TICK_LEVELS = 9
TICK_DROP_ENDS = True
TICK_MM = 3.4

WORDMARK_Y = 11.5
NUM_Y      = 80.0                     # degree-number strip BELOW the faders (bottom 74.5 + gap)
CENTS_ROW_A = 92.0                    # even-index cents knobs
CENTS_ROW_B = 101.0                   # odd-index cents knobs (staggered lower)
KNOB_R     = 3.0
CONNECT_Y  = 120.0

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", gold="#c8960c",
                  well="#0f1114", ring="#4a4a4a", knob="#2a2e34", knobring="#5a616a",
                  fadertrack="#20242a", tick="#888888", lockwell="#241f14", sub="#8a94a0"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", gold="#b07d00",
                  well="#e2ddd2", ring="#b0a898", knob="#c8cdd4", knobring="#9aa2ac",
                  fadertrack="#cdd2d8", tick="#999999", lockwell="#e8e0cc", sub="#5a6470"),
}

def gap_centres_mm():
    xs = [fader_cx(i) for i in range(N)]
    return [(xs[i] + xs[i+1]) / 2.0 for i in range(N - 1)]

def tick_ys_mm():
    ys = [FADER_TOP + (FADER_BOT - FADER_TOP) * k / (TICK_LEVELS - 1) for k in range(TICK_LEVELS)]
    return ys[1:-1] if TICK_DROP_ENDS else ys

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle id="wordmark" cx="{px(W/2)}" cy="{px(WORDMARK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # Cents LED display well — a single recessed dark panel in the band above the faders, spanning the
    # fader width. The widget draws all 12 cents values (DSEG 7-seg) inside, at each fader's X, on two
    # staggered rows (paralleling the cents-knob stagger). Emits a `cents_display` bounds marker.
    disp_x0 = fader_cx(0) - PITCH * 0.5
    disp_x1 = fader_cx(N - 1) + PITCH * 0.5
    disp_y0, disp_y1 = 16.0, 42.5
    disp_w, disp_h = disp_x1 - disp_x0, disp_y1 - disp_y0
    A(f'<rect x="{px(disp_x0-0.6)}" y="{px(disp_y0-0.6)}" width="{px(disp_w+1.2)}" height="{px(disp_h+1.2)}" '
      f'rx="{px(1.0)}" fill="{t["ring"]}" opacity="0.5"/>')
    A(f'<rect x="{px(disp_x0)}" y="{px(disp_y0)}" width="{px(disp_w)}" height="{px(disp_h)}" '
      f'rx="{px(0.8)}" fill="#0a0c0e" stroke="{t["ring"]}" stroke-width="0.4"/>')
    A(f'<rect id="cents_display" x="{px(disp_x0)}" y="{px(disp_y0)}" width="{px(disp_w)}" '
      f'height="{px(disp_h)}" fill="none" stroke="none"/>')

    # Fader tracks + anchors.
    track_w = 3.0
    for i in range(N):
        cx = fader_cx(i)
        A(f'<rect x="{px(cx - track_w/2)}" y="{px(FADER_TOP)}" width="{px(track_w)}" '
          f'height="{px(FADER_BOT - FADER_TOP)}" rx="{px(track_w*0.4)}" fill="{t["fadertrack"]}" '
          f'stroke="{t["ring"]}" stroke-width="0.4"/>')
        A(f'<circle id="param_weight_{i}" cx="{px(cx)}" cy="{px(FADER_CY)}" r="0.5" fill="none" stroke="none"/>')

    # Level-marker ticks (lift): one column per GAP between faders, uniform, drawn past the tracks.
    half = TICK_MM / 2.0
    A(f'<g stroke="{t["tick"]}" stroke-width="1.0" opacity="0.75" fill="none" stroke-linecap="round">')
    for cx in gap_centres_mm():
        for y in tick_ys_mm():
            A(f'  <line x1="{px(cx-half)}" y1="{px(y)}" x2="{px(cx+half)}" y2="{px(y)}"/>')
    A('</g>')

    # Degree-number anchors (widget draws 1..12) BELOW the faders.
    for i in range(N):
        A(f'<circle id="notelabel_{i}" cx="{px(fader_cx(i))}" cy="{px(NUM_Y)}" r="0.5" fill="none" stroke="none"/>')

    # Cents knobs, staggered zigzag. Root (0) = locked plate.
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

    # NOTES control — a small DSEG readout well (draggable to bulk-set the active-degree count). It
    # holds no stored value; the widget derives + draws the live active count and, on drag, writes the
    # first-N enable pattern into the weight faders. Sits at the base, left of the connect light.
    nctl_x, nctl_y = W * 0.5 - 22.0, CONNECT_Y
    nw, nh = 14.0, 6.5
    A(f'<rect x="{px(nctl_x-nw/2-0.6)}" y="{px(nctl_y-nh/2-0.6)}" width="{px(nw+1.2)}" height="{px(nh+1.2)}" '
      f'rx="{px(0.8)}" fill="{t["ring"]}" opacity="0.5"/>')
    A(f'<rect x="{px(nctl_x-nw/2)}" y="{px(nctl_y-nh/2)}" width="{px(nw)}" height="{px(nh)}" '
      f'rx="{px(0.6)}" fill="#0a0c0e" stroke="{t["ring"]}" stroke-width="0.4"/>')
    A(f'<rect id="notes_ctrl" x="{px(nctl_x-nw/2)}" y="{px(nctl_y-nh/2)}" width="{px(nw)}" '
      f'height="{px(nh)}" fill="none" stroke="none"/>')

    lcx, lcy = W * 0.5 + 22.0, CONNECT_Y
    A(f'<circle cx="{px(lcx)}" cy="{px(lcy)}" r="{px(1.8)}" fill="{t["well"]}" '
      f'stroke="{t["ring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_connect" cx="{px(lcx)}" cy="{px(lcy)}" r="0.5" fill="none" stroke="none"/>')

    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Colonnades_panel_dark.svg"), (False, "Colonnades_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Colonnades {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
