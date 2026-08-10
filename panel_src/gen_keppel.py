#!/usr/bin/env python3
"""Keppel — CV → MPE-out utility (MPE_UTILITY_BUILD_SPEC).

Poly microtonal pitch CV + poly gate IN → MPE MIDI OUT: each voice = nearest-12-TET note + per-note
pitch bend on its own MPE member channel. Standalone utility, zero engine coupling. Named for Keppel
Harbour — the outbound port that carries the microtonal performance OUT to MPE gear.

nanosvg-safe: solid fills/strokes only; TEXT (wordmark, labels) is widget-drawn. The MIDI device panel
is a widget-added MidiDisplay positioned on the `midi_display` marker.

Kit markers: wordmark, input_pitch, input_gate, input_accent, input_vel, param_bendrange,
midi_display, light_active.
"""
HP = 8
W  = HP * 5.08
H  = 128.5
S  = 75 / 25.4
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

WORDMARK_Y = 12.0
BEND_Y     = 30.0
MIDI_Y     = 52.0     # top of the MIDI display well
MIDI_H     = 26.0
CX         = W / 2.0
# Inputs laid out as a 2×2 grid: PITCH/GATE top row, ACCENT/VEL bottom row.
COL_L      = CX - 8.5
COL_R      = CX + 8.5
ROW_TOP    = 96.0
ROW_BOT    = 116.0

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0", sub="#8a94a0",
                  well="#0f1114", ring="#4a4a4a", knob="#2a2e34", knobring="#5a616a",
                  jackwell="#0c0e11", jackring="#4a4a4a", midiwell="#0a0c0e"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a", sub="#5a6470",
                  well="#e2ddd2", ring="#b0a898", knob="#c8cdd4", knobring="#9aa2ac",
                  jackwell="#e2ddd2", jackring="#b0a898", midiwell="#e8e2d6"),
}

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    o = []; A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    A(f'<circle id="wordmark" cx="{px(CX)}" cy="{px(WORDMARK_Y)}" r="0.5" fill="none" stroke="none"/>')

    # BEND RANGE knob (top).
    A(f'<circle cx="{px(CX)}" cy="{px(BEND_Y)}" r="{px(5.2)}" fill="{t["well"]}" '
      f'stroke="{t["ring"]}" stroke-width="0.5"/>')
    A(f'<circle cx="{px(CX)}" cy="{px(BEND_Y)}" r="{px(4.4)}" fill="{t["knob"]}" '
      f'stroke="{t["knobring"]}" stroke-width="0.6"/>')
    A(f'<line x1="{px(CX)}" y1="{px(BEND_Y-4.2)}" x2="{px(CX)}" y2="{px(BEND_Y-2.4)}" '
      f'stroke="{t["ink"]}" stroke-width="0.6"/>')
    A(f'<circle id="param_bendrange" cx="{px(CX)}" cy="{px(BEND_Y)}" r="0.5" fill="none" stroke="none"/>')

    # MIDI display well (widget draws the MidiDisplay here).
    mw_x = 3.0
    A(f'<rect x="{px(mw_x)}" y="{px(MIDI_Y)}" width="{px(W-2*mw_x)}" height="{px(MIDI_H)}" '
      f'rx="{px(1.0)}" fill="{t["midiwell"]}" stroke="{t["ring"]}" stroke-width="0.5"/>')
    A(f'<rect id="midi_display" x="{px(mw_x)}" y="{px(MIDI_Y)}" width="{px(W-2*mw_x)}" '
      f'height="{px(MIDI_H)}" fill="none" stroke="none"/>')

    # Active indicator (widget lights when a device is connected + voices are sounding).
    A(f'<circle cx="{px(CX)}" cy="{px(MIDI_Y+MIDI_H+5.0)}" r="{px(1.6)}" fill="{t["well"]}" '
      f'stroke="{t["ring"]}" stroke-width="0.3"/>')
    A(f'<circle id="light_active" cx="{px(CX)}" cy="{px(MIDI_Y+MIDI_H+5.0)}" r="0.5" fill="none" stroke="none"/>')

    # Poly input jacks in a 2×2 grid: PITCH/GATE (top), ACCENT/VEL (bottom).
    for (xx, yy, mid) in [(COL_L, ROW_TOP, "input_pitch"), (COL_R, ROW_TOP, "input_gate"),
                          (COL_L, ROW_BOT, "input_accent"), (COL_R, ROW_BOT, "input_vel")]:
        A(f'<circle cx="{px(xx)}" cy="{px(yy)}" r="{px(4.2)}" fill="{t["jackwell"]}" '
          f'stroke="{t["jackring"]}" stroke-width="0.5"/>')
        A(f'<circle id="{mid}" cx="{px(xx)}" cy="{px(yy)}" r="0.5" fill="none" stroke="none"/>')

    A('</svg>')
    return "\n".join(o)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Keppel_panel_dark.svg"), (False, "Keppel_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Keppel {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
