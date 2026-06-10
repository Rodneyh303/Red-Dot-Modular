#!/usr/bin/env python3
"""Peranakan-themed Interchange panel — dark + light.

Visual language (from the dot.modular concept sheet):
  - Black panel with a grey Peranakan tile lattice (diamonds + micro-dots)
  - Gold trim rings on trimpots
  - Red detailing + red knob indicators
  - White lettering

Matches the real Interchange geometry (interchange_gemini_new2.svg): 270x380 px,
raw-px coords. 6 semitone rows (y=80..280 step 40) + octave row (y=320).
4 columns: jack 48, trimpot 102, trimpot 168, jack 222.

Screws are drawn in C++ (ui/RedScrew.hpp) — NOT painted here.
"""
import math

W, H = 270, 380
ROWS_Y = [80 + i*40 for i in range(6)] + [320]   # 6 semis + octave
COL_JACK_L, COL_TRIM_L, COL_TRIM_R, COL_JACK_R = 48, 102, 168, 222

THEMES = {
    "dark": dict(
        bg="#0a0a0a", lattice="#262626", latticedot="#333333",
        well="#050505", wellring="#3a3a3a",
        gold="#c8960c", red="#d4001a", redsoft="#dc2626",
        text="#f0f0f0", textdim="#9a9a9a", recess="#141414", recessln="#2a2a2a",
    ),
    "light": dict(
        bg="#f0ebe2", lattice="#d8cfc0", latticedot="#c8bca8",
        well="#e8e2d6", wellring="#c0b8a8",
        gold="#b07d00", red="#d4001a", redsoft="#dc2626",
        text="#1a1a1a", textdim="#6a6a6a", recess="#e2dccf", recessln="#cabfaf",
    ),
}


def lattice(t):
    """Peranakan diamond lattice + micro-dots, clipped to the panel."""
    o = ['<g opacity="0.9">']
    step = 18
    # diagonal diamond grid (forward + backward leaning lines)
    for d in range(-H, W + H, step):
        o.append(f'<line x1="{d}" y1="0" x2="{d+H}" y2="{H}" stroke="{t["lattice"]}" stroke-width="0.6"/>')
        o.append(f'<line x1="{d}" y1="{H}" x2="{d+H}" y2="0" stroke="{t["lattice"]}" stroke-width="0.6"/>')
    # micro-dots at lattice intersections (every other node)
    for yy in range(0, H + step, step):
        for xx in range(0, W + step, step):
            o.append(f'<circle cx="{xx}" cy="{yy}" r="0.9" fill="{t["latticedot"]}"/>')
    o.append('</g>')
    return o


def jack_well(x, y, t):
    return (f'<circle cx="{x}" cy="{y}" r="9.5" fill="{t["well"]}" stroke="{t["wellring"]}" stroke-width="1.4"/>'
            f'<circle cx="{x}" cy="{y}" r="5.2" fill="none" stroke="{t["red"]}" stroke-width="0.8" opacity="0.55"/>')


def trim_well(x, y, t):
    # gold trim ring + red indicator notch (pointing up)
    return (f'<circle cx="{x}" cy="{y}" r="8.5" fill="{t["well"]}" stroke="{t["gold"]}" stroke-width="1.6"/>'
            f'<circle cx="{x}" cy="{y}" r="8.5" fill="none" stroke="{t["gold"]}" stroke-width="0.5" opacity="0.4"/>'
            f'<line x1="{x}" y1="{y-2}" x2="{x}" y2="{y-7}" stroke="{t["red"]}" stroke-width="1.6" stroke-linecap="round"/>')


def panel(theme):
    t = THEMES[theme]
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" '
         f'xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
         f'width="{W}" height="{H}" viewBox="0 0 {W} {H}">']
    o.append('<g inkscape:label="artwork" inkscape:groupmode="layer">')
    # base
    o.append(f'<rect width="{W}" height="{H}" fill="{t["bg"]}"/>')
    # peranakan lattice
    o += lattice(t)
    # top + bottom red detail bands
    o.append(f'<rect x="0" y="0" width="{W}" height="3" fill="{t["red"]}"/>')
    o.append(f'<rect x="0" y="{H-3}" width="{W}" height="3" fill="{t["red"]}"/>')
    # header recess
    o.append(f'<rect x="10" y="30" width="{W-20}" height="34" rx="4" '
             f'fill="{t["recess"]}" stroke="{t["recessln"]}" stroke-width="1"/>')
    # central column divider (subtle)
    o.append(f'<line x1="{W/2}" y1="70" x2="{W/2}" y2="{H-20}" stroke="{t["recessln"]}" stroke-width="0.8" opacity="0.6"/>')
    o.append('</g>')

    # components layer (SvgHelper-named wells, for future binding/review)
    o.append('<g inkscape:label="components" inkscape:groupmode="layer">')
    for i, y in enumerate(ROWS_Y):
        kind = "OCT" if i == 6 else f"SEMI{i}"
        o.append(f'<g>{jack_well(COL_JACK_L, y, t)}</g>')
        o.append(f'<g>{trim_well(COL_TRIM_L, y, t)}</g>')
        o.append(f'<g>{trim_well(COL_TRIM_R, y, t)}</g>')
        o.append(f'<g>{jack_well(COL_JACK_R, y, t)}</g>')
    o.append('</g>')

    o.append('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for theme in ("dark", "light"):
        out = f"res/panels/interchange_peranakan_{theme}.svg"
        open(out, "w").write(panel(theme))
        print(f"wrote {out}")
