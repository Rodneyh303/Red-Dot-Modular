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


def clean_esplanade(text, shell="#5a5e66", accent="#dc2626", grid_op=0.9):
    """Replace the ENTIRE original Esplanade structure (building body + base steps
    + windows + both messy domes + spikes + spire) with two clean Esplanade
    'durian' shells in the style of the reference render:
      - a criss-cross (diagrid) grid over each dome shell,
      - Y-shaped support struts beneath,
      - the lower roof edge sweeping UP at the outer sides (upturned eaves),
      - NO building structure underneath.
    `shell` colours the grid; pass a grey or red to get the two variants.
    Operates on panel TEXT (output only) — source panel never mutated.
    """
    import math, re as _re
    lines = text.split("\n")
    # Span: from the FIRST <g fill="url(#silG)"> (the building body group) through
    # the spire after the last dome arc-outline. That covers body+steps+windows
    # +both domes+spikes+outlines+spire.
    start = end = None
    for i, ln in enumerate(lines):
        if start is None and '<g fill="url(#silG)"' in ln:
            start = i
        if 'A65.0,65.0' in ln:
            end = i
    k = (end or 0) + 1
    while k < len(lines) and ('<line' in lines[k] or '<circle' in lines[k]) \
          and ('x1="335"' in lines[k] or 'cx="335"' in lines[k]):
        end = k; k += 1
    if start is None or end is None:
        return text

    BASE_Y = 245.0          # eaves baseline (centre); outer edges sweep up above it
    HW     = 78.0           # dome half-width (solo dome, wider to fill the region)
    DOME_H = 26.0           # shell height at centre
    EAVE_UP = 7.0           # how far the outer lower edge rises above BASE_Y

    # Geometry per the reference: a domed top over a STRAIGHT bottom edge that
    # slopes UP from left (low) to right (high). Taller overall (bottom-to-top).
    # Each dome's bottom-left sits low, bottom-right sits high.
    BOT_L = 6.0     # bottom edge height above BASE_Y at the LOW (left) end
    BOT_R = 20.0    # ... at the HIGH (right) end  (=> straight slope up)
    TOP_RISE = 64.0 # tall dome

    def bot_y(cx, u):  # straight bottom edge, u in -1..1 (left..right)
        h = BOT_L + (BOT_R - BOT_L) * (u + 1) / 2.0
        return BASE_Y - h
    def top_y(cx, u):  # domed top: bottom-edge level minus a parabolic rise
        be = bot_y(cx, u)
        return be - TOP_RISE * (1 - u*u)

    def shell_path(cx):
        lx, rx = cx-HW, cx+HW
        bl, br = bot_y(cx, -1), bot_y(cx, 1)     # straight bottom edge endpoints
        # top: smooth dome from bottom-left up over apex down to bottom-right
        apex_y = top_y(cx, 0)
        return (f'M {lx:.1f},{bl:.1f} '
                f'Q {cx:.1f},{apex_y-4:.1f} {rx:.1f},{br:.1f} '   # domed top (curve)
                f'L {lx:.1f},{bl:.1f} Z')                          # STRAIGHT bottom edge

    def dome(cx):
        lx, rx = cx-HW, cx+HW
        o = []
        # filled shell base
        o.append(f'  <path d="{shell_path(cx)}" fill="#101216" fill-opacity="0.5" stroke="none"/>')
        # conforming diagrid: M ribs from straight bottom edge up to the domed top
        M = 12
        cols = []
        for i in range(M+1):
            u = -1 + 2*i/M
            x = cx + HW*u
            cols.append((x, bot_y(cx, u), top_y(cx, u)))
        o.append(f'  <g stroke="{shell}" stroke-width="0.5" fill="none" opacity="{grid_op}">')
        for (x, by, ty) in cols:                              # vertical ribs
            o.append(f'    <line x1="{x:.1f}" y1="{by:.1f}" x2="{x:.1f}" y2="{ty:.1f}"/>')
        K = 4
        for kf in range(K+1):                                  # horizontal courses
            f = kf/float(K)
            pts = [f"{x:.1f},{(ty + (by-ty)*f):.1f}" for (x, by, ty) in cols]
            o.append(f'    <polyline points="{" ".join(pts)}"/>')
        o.append('  </g>')
        o.append(f'  <g stroke="{shell}" stroke-width="0.45" fill="none" opacity="{grid_op*0.7}">')
        for i in range(M):                                     # diagonal cross-bracing
            x0, by0, ty0 = cols[i]; x1, by1, ty1 = cols[i+1]
            o.append(f'    <line x1="{x0:.1f}" y1="{by0:.1f}" x2="{x1:.1f}" y2="{ty1:.1f}"/>')
            o.append(f'    <line x1="{x0:.1f}" y1="{ty0:.1f}" x2="{x1:.1f}" y2="{by1:.1f}"/>')
        o.append('  </g>')
        # ── support struts: stand on a flat base (BASE_Y), PROGRESSIVELY TALLER
        #    toward the high (right) side, meeting the sloped bottom edge. ──
        o.append(f'  <g stroke="{accent}" stroke-width="1.1" fill="none" opacity="0.8">')
        n_str = 5
        for i in range(n_str):
            u = -0.8 + 1.6*i/(n_str-1)
            sx = cx + HW*u
            top = bot_y(cx, u)          # meets the sloped bottom edge (higher on right)
            base = BASE_Y + 2           # common flat footing
            joint = top + 5             # Y joint just below the edge
            o.append(f'    <line x1="{sx:.1f}" y1="{base:.1f}" x2="{sx:.1f}" y2="{joint:.1f}"/>')      # stem (taller on right)
            o.append(f'    <line x1="{sx:.1f}" y1="{joint:.1f}" x2="{sx-5:.1f}" y2="{top:.1f}"/>')     # Y left arm
            o.append(f'    <line x1="{sx:.1f}" y1="{joint:.1f}" x2="{sx+5:.1f}" y2="{top:.1f}"/>')     # Y right arm
        o.append('  </g>')
        # crisp outline
        o.append(f'  <path d="{shell_path(cx)}" fill="none" stroke="{accent}" stroke-width="0.9" opacity="0.6"/>')
        return "\n".join(o)

    block = dome(295.0)   # single Esplanade shell, centred in the motif region
    out = lines[:start] + [block] + lines[end+1:]
    return "\n".join(out)


def process(src, out_dark, out_light, dmap, lmap, dbg, lbg, dims):
    s = open(src).read()
    if "straits" in out_dark:
        # Two Esplanade treatments: grey (architectural) + red (hero). Emit both
        # dark variants so they can be compared; light uses the grey shell.
        grey = clean_esplanade(s, shell="#7a7e86", accent="#9a9fa8", grid_op=0.95)
        red  = clean_esplanade(s, shell="#d4001a", accent="#dc2626", grid_op=0.95)
        open(out_dark.replace(".svg", "_grey.svg"), "w").write(force_bg(remap(grey, dmap), dbg, dims))
        open(out_dark.replace(".svg", "_red.svg"),  "w").write(force_bg(remap(red,  dmap), dbg, dims))
        # default dark = red (hero); light = grey shell
        open(out_dark, "w").write(force_bg(remap(red, dmap), dbg, dims))
        open(out_light, "w").write(force_bg(remap(grey, lmap), lbg, dims))
        print(f"  {src} -> {out_dark.split('/')[-1]} (+_grey/_red), {out_light.split('/')[-1]}")
        return
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
