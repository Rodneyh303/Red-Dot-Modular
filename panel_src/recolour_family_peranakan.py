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
BLACK   = "#18181a"   # matt black (was glossy #080808)
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


def clean_esplanade(text):
    """Replace the original messy twin-dome graphic (two A65,65 half-circle domes
    with their spike groups, echo groups, arc outlines AND the spire on top) with
    two clean LOW flat domes + even spikes + a central spire, drawn once. Matches
    the dot.modular mockup. Operates on the panel TEXT (output only) — the real
    source panel is never mutated.
    """
    import math, re as _re
    lines = text.split("\n")
    # span: from the first dome <g fill="url(#silG)"> that precedes an A65,65 arc,
    # through the second dome's arc-outline path AND the spire line+circle after it.
    start = end = None
    for i, ln in enumerate(lines):
        if start is None and 'A65.0,65.0' in ln:
            j = i
            while j > 0 and '<g fill="url(#silG)"' not in lines[j]:
                j -= 1
            start = j
        if 'A65.0,65.0' in ln:
            end = i
    # include the spire (line + circle) immediately after the last arc outline
    k = end + 1
    while k < len(lines) and ('<line' in lines[k] or '<circle' in lines[k]) \
          and ('x1="335"' in lines[k] or 'cx="335"' in lines[k]):
        end = k; k += 1
    if start is None:
        return text  # nothing to do

    BASE_Y = 245.0; DOME_H = 22.0; SPIKE_H = 7.0; N = 17
    def dome(cx, hw):
        o = ['  <g fill="url(#silG)" stroke="none">',
             f'    <path d="M {cx-hw:.1f},{BASE_Y:.1f} A {hw:.1f},{DOME_H:.1f} 0 0,1 {cx+hw:.1f},{BASE_Y:.1f} Z"/>',
             '  </g>',
             '  <g fill="#181e28" stroke="none" opacity="0.85">']
        for i in range(N + 1):
            a = math.pi * (1.0 - i / N)
            bx = cx + hw * math.cos(a); by = BASE_Y - DOME_H * math.sin(a)
            nx = math.cos(a) / hw; ny = -math.sin(a) / DOME_H
            nl = math.hypot(nx, ny) or 1.0; nx, ny = nx/nl, ny/nl
            tx, ty = bx + nx*SPIKE_H, by + ny*SPIKE_H
            wx, wy = -ny*1.7, nx*1.7
            o.append(f'    <polygon points="{bx-wx:.1f},{by-wy:.1f} {tx:.1f},{ty:.1f} {bx+wx:.1f},{by+wy:.1f}"/>')
        o.append('  </g>')
        # central spire + finial
        o.append(f'  <line x1="{cx:.1f}" y1="{BASE_Y-DOME_H:.1f}" x2="{cx:.1f}" y2="{BASE_Y-DOME_H-9:.1f}" stroke="#dc2626" stroke-width="0.7" opacity="0.5"/>')
        o.append(f'  <circle cx="{cx:.1f}" cy="{BASE_Y-DOME_H-10:.1f}" r="1.6" fill="#dc2626" opacity="0.5"/>')
        # rim accent
        o.append(f'  <path d="M {cx-hw:.1f},{BASE_Y:.1f} A {hw:.1f},{DOME_H:.1f} 0 0,1 {cx+hw:.1f},{BASE_Y:.1f}" fill="none" stroke="#dc2626" stroke-width="0.8" opacity="0.45"/>')
        return "\n".join(o)
    block = "\n".join([dome(252.5, 67.5), dome(337.5, 67.5)])
    out = lines[:start] + [block] + lines[end+1:]
    return "\n".join(out)


def process(src, out_dark, out_light, dmap, lmap, dbg, lbg, dims):
    s = open(src).read()
    if "straits" in out_dark:
        s = clean_esplanade(s)   # replace messy domes with clean flat domes
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
            # force grain bg -> matt black (not pure/glossy black)
            body = body.replace('fill="url(#grain)"', 'fill="#18181a"')
            # The bottom-3-row group recesses were nearly opaque (0.86/0.92),
            # totally burying the supertree artwork. Drop them to a light wash so
            # the supertrees read through, and lean on the per-jack wells +
            # recess OUTLINE for control definition.
            body = body.replace('fill-opacity="0.92"', 'fill-opacity="0.28"')
            body = body.replace('fill-opacity="0.86"', 'fill-opacity="0.24"')
            # The supertrees use treeGrad (#18202a) — a dark blue-grey silhouette
            # that vanishes against matt black. Recolour its stops to faint red so
            # the Gardens-by-the-Bay supertrees read as a subtle on-brand silhouette.
            body = body.replace('stop-color="#18202a"', 'stop-color="#d4001a"')
            out = "res/panels/Monsoon_peranakan_dark.svg"
        else:
            # light: grain -> light grey, dark fills -> light, keep red supertrees
            body = body.replace('fill="url(#grain)"', 'fill="#dcdcdc"')
            body = body.replace('fill-opacity="0.92"', 'fill-opacity="0.30"')
            body = body.replace('fill-opacity="0.86"', 'fill-opacity="0.26"')
            for a, b in {
                "#0f1114": "#cfc8bc", "#101216": "#cfc8bc", "#0d0d0d": "#d4cdc0",
                "#111214": "#dcdcdc", "#1a2535": "#b8b0a2", "#1e2530": "#c0b8aa",
                "#222830": "#bcb4a6", "#f0ebe2": "#1a1a1a",
            }.items():
                body = _re.sub(_re.escape(a), b, body, flags=_re.IGNORECASE)
            out = "res/panels/Monsoon_peranakan_light.svg"

        body = body.replace("@@COMPONENTS@@", comp)  # reattach untouched
        # Remove the STALE jack-well circle generation: r=14.74 wells at cy≈396/453
        # are an old layout left in the source; the real jacks are the r=13.0 wells
        # at cy=310/354 (y=105/120mm). The stale ones overlap + sit off-panel.
        import re as _re2
        body = _re2.sub(r'<circle cx="[0-9.]+" cy="(396|453)\.\d+" r="14\.7[0-9]*"[^/]*/>\s*', '', body)
        open(out, "w").write(body)
        print(f"  {src} -> {out.split('/')[-1]} (components layer preserved)")

process_monsoon()
