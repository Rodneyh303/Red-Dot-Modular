#!/usr/bin/env python3
"""Apply the dot.modular Peranakan palette across the whole module family —
Surge, Causeway, Straits (East/West), Monsoon, Interchange — by recolouring the
panels already in use. Geometry, layout and (critically) any components layer
with named anchor shapes are left untouched; only artwork colours change.

Peranakan language:
  Dark  = black panel, grey pattern, gold trim, RED detailing, white text.
  Light = light-grey background, near-black pattern, gold trim, red detailing.

Two palette families among the panels:
  A) Surge / Causeway: already dot.modular (#070707 bg, #d4001a red, #26a69a teal
     accent, #f0ede8 cream, #6a6e76 grey). Just teal->red + light inversion.
  B) Straits / Interchange: greys + #dc2626/teal + blue tints. Greys are the
     pattern; map accents to red, invert for light.

Monsoon: its bright #ff0000/#00ffff/#00ff00 are NAMED COMPONENT ANCHORS in the
components layer (widgets render over them) — we must NOT recolour those, and we
must preserve the rich supertree artwork. So Monsoon gets a conservative map that
only touches the dark background + teal/cyan *artwork* accents, and we skip the
components layer entirely.
"""
import re, sys

# Shared Peranakan targets
RED     = "#d4001a"
GOLD_D  = "#c8960c"   # gold on dark
GOLD_L  = "#b07d00"   # gold on light
BLACK   = "#080808"
LGREY   = "#dcdcdc"   # light background

# ── Per-family colour maps ────────────────────────────────────────────────
# Surge / Causeway (palette family A)
A_DARK = {
    "#26a69a": RED,        # teal accent -> red
    "#070707": BLACK,      # bg -> blacker
}
A_LIGHT = {
    "#26a69a": RED,
    "#070707": LGREY,      # bg -> light grey
    "#0f1114": "#cfc8bc",  # recess/dark fills -> light tan-grey
    "#f0ede8": "#1a1a1a",  # cream text/marks -> near-black
    "#6a6e76": "#3a3a3a",  # grey -> dark grey (pattern reads on light)
    "#d4af37": GOLD_L,
}

# Straits / Interchange (palette family B)
B_DARK = {
    "#dc2626": RED,
    "#26a69a": RED,
    "#1a4060": "#3a1414",  # blue tint -> dark red tint
    "#2a5878": "#5a1e1e",
    "#0a1828": "#100808",
    "#0a1520": "#0c0606",
    "#1e2228": "#161616",
    "#141416": BLACK,
}
B_LIGHT = {
    "#dc2626": RED,
    "#26a69a": RED,
    "#666666": "#2a2a2a",  # primary pattern grey -> near-black
    "#888888": "#333333",
    "#555555": "#1c1c1c",
    "#1a4060": "#d8b0b0",  # blue tint -> light red tint
    "#2a5878": "#e0bcbc",
    "#181e28": "#cccccc",
    "#1e2228": "#cccccc",
    "#0a1828": "#dcdcdc",
    "#0a1520": "#dcdcdc",
    "#141416": LGREY,
    "#d4af37": GOLD_L,
}

# Monsoon: conservative — preserve supertree art + DO NOT touch components layer.
M_DARK = {
    "#00ffff": "#00ffff",  # (left as-is; in components layer, see skip below)
}
# We won't actually remap Monsoon component colours; handled by layer-skip.

TASKS = [
    # (src, out_dark, out_light, dark_map, light_map, dark_bg, light_bg, bg_dims)
    ("res/panels/Surge_panel_dark.svg",  "res/panels/Surge_peranakan_dark.svg",
     "res/panels/Surge_peranakan_light.svg", A_DARK, A_LIGHT, BLACK, LGREY, ("120.0","380.0")),
    ("res/panels/Causeway_panel_dark.svg", "res/panels/Causeway_peranakan_dark.svg",
     "res/panels/Causeway_peranakan_light.svg", A_DARK, A_LIGHT, BLACK, LGREY, ("180.0","380.0")),
    ("res/panels/interchange_wide_straits_dark.svg", "res/panels/straits_peranakan_dark.svg",
     "res/panels/straits_peranakan_light.svg", B_DARK, B_LIGHT, BLACK, LGREY, ("390","380")),
]


def remap(text, mapping):
    for a, b in mapping.items():
        text = re.sub(re.escape(a), b, text, flags=re.IGNORECASE)
    return text


def force_bg(text, fill, dims):
    w, h = dims
    # match the main full-panel background rect (first big rect)
    pat = re.compile(rf'(<rect width="{re.escape(w)}" height="{re.escape(h)}"[^>]*?fill=")[^"]*(")')
    return pat.sub(rf'\g<1>{fill}\g<2>', text, count=1)


def process(src, out_dark, out_light, dmap, lmap, dbg, lbg, dims):
    s = open(src).read()
    dark = force_bg(remap(s, dmap), dbg, dims)
    light = force_bg(remap(s, lmap), lbg, dims)
    open(out_dark, "w").write(dark)
    open(out_light, "w").write(light)
    print(f"  {src} -> {out_dark.split('/')[-1]}, {out_light.split('/')[-1]}")


if __name__ == "__main__":
    print("Recolouring family to Peranakan:")
    for t in TASKS:
        process(*t)


# ── Monsoon: special handling ──────────────────────────────────────────────
# Artwork is already on-palette (red supertrees, cream text). We only: force the
# grain background to solid, and for LIGHT invert the dark fills/greys. We must
# NOT recolour the components layer (named anchor shapes with bright fills) — so
# we split it off, transform only the artwork, and reattach the components layer
# verbatim.
def process_monsoon():
    import re as _re
    for src, outsuf in [("res/panels/Monsoon_panel_dark_monsoon.svg", "dark"),
                        ("res/panels/Monsoon_panel_light_monsoon.svg", "light")]:
        s = open(src).read()
        # protect components layer
        m = _re.search(r'(<g inkscape:label="components".*?</g>)', s, _re.DOTALL)
        comp = m.group(1) if m else ""
        body = s.replace(comp, "@@COMPONENTS@@") if comp else s

        if outsuf == "dark":
            # force grain bg -> solid black
            body = body.replace('fill="url(#grain)"', 'fill="#080808"')
            out = "res/panels/Monsoon_peranakan_dark.svg"
        else:
            # light: grain -> light grey, dark fills -> light, keep red supertrees
            body = body.replace('fill="url(#grain)"', 'fill="#dcdcdc"')
            for a, b in {
                "#0f1114": "#cfc8bc", "#101216": "#cfc8bc", "#0d0d0d": "#d4cdc0",
                "#111214": "#dcdcdc", "#1a2535": "#b8b0a2", "#1e2530": "#c0b8aa",
                "#222830": "#bcb4a6", "#f0ebe2": "#1a1a1a",
            }.items():
                body = _re.sub(_re.escape(a), b, body, flags=_re.IGNORECASE)
            out = "res/panels/Monsoon_peranakan_light.svg"

        body = body.replace("@@COMPONENTS@@", comp)  # reattach untouched
        open(out, "w").write(body)
        print(f"  {src} -> {out.split('/')[-1]} (components layer preserved)")

process_monsoon()
