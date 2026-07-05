#!/usr/bin/env python3
"""Recolour the IN-USE Interchange panel (interchange_gemini_new2.svg) to the
dot.modular Peranakan palette — dark + light — keeping the existing hex design,
control geometry, grain texture and glows. We only remap colours.

Dark:  black panel, grey pattern kept, gold trim kept, teal detailing -> RED,
       white lettering (widget-drawn).
Light: light-grey background with BLACK peranakan pattern (greys darkened to
       near-black, background lightened), gold trim, teal -> red, dark text.
"""
import re

SRC = "res/panels/interchange_gemini_new2.svg"

# Colour remaps (case-insensitive on the hex). Order doesn't matter; exact hex.
DARK = {
    "#26a69a": "#d4001a",  # teal accent  -> Singapore red
    "#141416": "#080808",  # grain base   -> blacker
    "#2a2a2c": "#1e1e20",  # grain line   -> subtle
    # golds + greys kept as-is (gold trim, grey pattern)
}

# Light: invert tone. Background becomes light grey; the grey "pattern" strokes
# and grain become near-black so the peranakan/hex design reads as black on grey.
LIGHT = {
    "#26a69a": "#d4001a",  # teal -> red
    "#141416": "#dcdcdc",  # grain base -> light grey background
    "#2a2a2c": "#c4c4c4",  # grain line -> mid grey
    "#777777": "#2a2a2a",  # primary pattern grey -> near-black
    "#888888": "#333333",
    "#999999": "#3a3a3a",
    "#6a6a6a": "#242424",
    "#666666": "#222222",
    "#555555": "#1c1c1c",
    "#d4af37": "#b07d00",  # gold -> deeper gold for contrast on light
}


def recolour(src_text, mapping, bg_fill):
    out = src_text
    for a, b in mapping.items():
        out = re.sub(re.escape(a), b, out, flags=re.IGNORECASE)
    # Replace the grain-pattern background with a SOLID fill, so the base reads
    # reliably (the grain pattern renders inconsistently and washes grey). The
    # grain texture is decorative; a solid base is cleaner and nanosvg-safe.
    out = out.replace('<rect width="270" height="380" fill="url(#grain)" />',
                      f'<rect width="270" height="380" fill="{bg_fill}" />')
    return out


def main():
    s = open(SRC).read()
    dark = recolour(s, DARK, "#080808")     # near-black panel
    light = recolour(s, LIGHT, "#dcdcdc")   # light-grey panel
    open("res/panels/interchange_peranakan_dark.svg", "w").write(dark)
    open("res/panels/interchange_peranakan_light.svg", "w").write(light)
    print("wrote res/panels/interchange_peranakan_{dark,light}.svg (recoloured from in-use panel)")


if __name__ == "__main__":
    main()
