#!/usr/bin/env python3
"""Apply the dot.modular Peranakan palette across the whole module family —
Junction, Causeway, Straits (East/West), Monsoon, Interchange — by recolouring the
panels already in use. Geometry, layout and (critically) any components layer
with named anchor shapes are left untouched; only artwork colours change.

Peranakan language:
  Dark  = black panel, grey pattern, gold trim, RED detailing, white text.
  Light = light-grey background, near-black pattern, gold trim, red detailing.

Two palette families among the panels:
  A) Junction / Causeway: already dot.modular (#070707 bg, #d4001a red, #26a69a teal
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
# Junction / Causeway (palette family A)
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
    ("res/panels/Junction_panel_dark.svg",  "res/panels/Junction_peranakan_dark.svg",
     "res/panels/Junction_peranakan_light.svg", A_DARK, A_LIGHT, BLACK, LGREY, ("120.0","380.0")),
    ("res/panels/Causeway_panel_dark.svg", "res/panels/Causeway_peranakan_dark.svg",
     "res/panels/Causeway_peranakan_light.svg", A_DARK, A_LIGHT, BLACK, LGREY, ("180.0","380.0")),
    ("res/panels/interchange_wide_straits_dark.svg", "res/panels/straits_peranakan_dark.svg",
     "res/panels/straits_peranakan_light.svg", B_DARK, B_LIGHT, BLACK, LGREY, ("390","380")),
]


def remap(text, mapping):
    for a, b in mapping.items():
        text = re.sub(re.escape(a), b, text, flags=re.IGNORECASE)
    return text


def flatten_gradients(text):
    """Strip NanoSVG-unsafe <linearGradient>/<radialGradient> defs and replace every
    fill/stroke="url(#id)" with the gradient's representative SOLID colour (its last
    stop, i.e. the dominant/base tone). Pre-existing skyG/silG/waveG in the source
    straits panel are near-solid two-stop gradients, so a flat fill is visually
    equivalent and renders reliably in Rack's NanoSVG."""
    import re as _r
    # map gradient id -> representative colour (last stop-color)
    colours = {}
    for m in _r.finditer(r'<(?:linear|radial)Gradient[^>]*id="([^"]+)".*?</(?:linear|radial)Gradient>', text, _r.S):
        stops = _r.findall(r'stop-color="([^"]*)"', m.group(0))
        if stops:
            colours[m.group(1)] = stops[-1]
    # replace url(#id) references
    def sub_url(mm):
        gid = mm.group(1)
        return colours.get(gid, "#181818")
    text = _r.sub(r'url\(#([^)]+)\)', sub_url, text)
    # drop the gradient defs
    text = _r.sub(r'<(?:linear|radial)Gradient[^>]*>.*?</(?:linear|radial)Gradient>', '', text, flags=_r.S)
    # drop a now-empty <defs></defs> if present
    text = _r.sub(r'<defs>\s*</defs>', '', text)
    return text


def force_bg(text, fill, dims):
    w, h = dims
    # match the main full-panel background rect (first big rect)
    pat = re.compile(rf'(<rect width="{re.escape(w)}" height="{re.escape(h)}"[^>]*?fill=")[^"]*(")')
    return pat.sub(rf'\g<1>{fill}\g<2>', text, count=1)


def clean_esplanade(text, shell="#5a5e66", accent="#dc2626", grid_op=0.9,
                    grid_style="fan", _bg="#18181a"):
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

    # ── Reference-style Esplanade: two-family lattice + truss understructure ──
    # Maps the detailed reference (1000×450 space) into the panel esplanade region.
    # Down-scaled for ~165px panel width: arc families thinned (ref had 40+ ellipses
    # → ~12 here) and NO clipPath (NanoSVG-safe; the families are bounded by t-range
    # instead). The truss + hanging spikes are kept — they read best at small scale.
    import math, re as _re2
    # ── Esplanade durian-dome (perspective view) — replaces the prior fan/persp
    #    variants. Maps the 1000x450 reference dome into the panel esplanade region
    #    via MX/MY. NanoSVG-safe: the dome silhouette is cut by SOLID (nonzero) bg
    #    cover shapes (a sampled-curve top polygon + baseline/side rects) instead of
    #    a clipPath; the roof-outline stroke is solid (no gradient); no filter.
    #    `shell` = lattice colour, `accent` = roof-outline + truss colour, `bg` =
    #    the panel background (so the cover shapes are invisible). ──
    TGT_CX, TGT_BASE, TGT_HALFW = 295.0, 250.0, 84.0
    R_LX, R_RX, R_BASE = 35.0, 965.0, 343.0
    rW = R_RX - R_LX
    sc = (2 * TGT_HALFW) / rW
    def MX(x): return TGT_CX - TGT_HALFW + (x - R_LX) * sc
    def MY(y): return TGT_BASE + (y - R_BASE) * sc

    def _cbez(p0, p1, p2, p3, n=24):
        out = []
        for i in range(n + 1):
            t = i / n; mt = 1 - t
            out.append((mt**3*p0[0] + 3*mt*mt*t*p1[0] + 3*mt*t*t*p2[0] + t**3*p3[0],
                        mt**3*p0[1] + 3*mt*mt*t*p1[1] + 3*mt*t*t*p2[1] + t**3*p3[1]))
        return out

    def _roof_pts():
        a = _cbez((35,340),(40,120),(240,45),(500,42))
        b = _cbez((500,42),(720,45),(900,125),(965,250))
        return a + b[1:]

    def dome(bg="#18181a"):
        o = []
        # horizontal bands (8)
        bands = ["M35 340 Q420 240 965 250","M40 300 Q420 205 950 225",
                 "M45 265 Q415 175 925 200","M55 230 Q410 145 890 178",
                 "M70 195 Q400 115 845 155","M90 160 Q380 88 790 138",
                 "M120 130 Q350 68 720 125","M155 102 Q320 52 650 118"]
        def mq(d):
            n = _re2.findall(r'-?\d+\.?\d*', d); x0,y0,cx,cy,x1,y1 = map(float, n)
            return f"M{MX(x0):.1f} {MY(y0):.1f} Q{MX(cx):.1f} {MY(cy):.1f} {MX(x1):.1f} {MY(y1):.1f}"
        o.append(f'  <g fill="none" stroke="{shell}" stroke-width="0.4" opacity="{grid_op*0.55:.2f}" stroke-linecap="round">')
        for b in bands: o.append(f'    <path d="{mq(b)}"/>')
        o.append('  </g>')
        # perspective ribs (ellipses), wider on the front-left, compressed right
        ribs = [(-40,140),(0,145),(40,150),(80,155),(120,160),(160,165),(200,170),
                (240,175),(280,180),(320,185),(360,190),(420,180),(470,170),(520,160),
                (570,150),(620,140),(670,130),(720,120),(770,110),(820,100),(870,90),(920,80),(960,70)]
        o.append(f'  <g fill="none" stroke="{shell}" stroke-width="0.45" opacity="{grid_op*0.65:.2f}">')
        for cx, rx in ribs:
            o.append(f'    <ellipse cx="{MX(cx):.1f}" cy="{MY(380):.1f}" rx="{rx*sc:.1f}" ry="{370*sc:.1f}"/>')
        o.append('  </g>')
        # SOLID bg cover (NanoSVG-safe clip): top polygon bounded by the sampled
        # roof curve + baseline/side rects. Everything outside the shell -> bg.
        cp = _roof_pts()
        top = [f"{MX(-300):.1f},{MY(-300):.1f}"] + \
              [f"{MX(x):.1f},{MY(y):.1f}" for x, y in cp] + \
              [f"{MX(1300):.1f},{MY(-300):.1f}"]
        o.append(f'  <polygon points="{" ".join(top)}" fill="{bg}"/>')
        o.append(f'  <polygon points="{MX(-300):.1f},{MY(343):.1f} {MX(1300):.1f},{MY(343):.1f} {MX(1300):.1f},{MY(900):.1f} {MX(-300):.1f},{MY(900):.1f}" fill="{bg}"/>')
        o.append(f'  <polygon points="{MX(-300):.1f},{MY(-300):.1f} {MX(35):.1f},{MY(-300):.1f} {MX(35):.1f},{MY(343):.1f} {MX(-300):.1f},{MY(343):.1f}" fill="{bg}"/>')
        o.append(f'  <polygon points="{MX(965):.1f},{MY(-300):.1f} {MX(1300):.1f},{MY(-300):.1f} {MX(1300):.1f},{MY(343):.1f} {MX(965):.1f},{MY(343):.1f}" fill="{bg}"/>')
        # roof outline (solid stroke; gradient flattened)
        roof = (f"M{MX(35):.1f} {MY(340):.1f} C{MX(40):.1f} {MY(120):.1f} {MX(240):.1f} {MY(45):.1f} {MX(500):.1f} {MY(42):.1f} "
                f"C{MX(720):.1f} {MY(45):.1f} {MX(900):.1f} {MY(125):.1f} {MX(965):.1f} {MY(250):.1f}")
        o.append(f'  <path d="{roof}" fill="none" stroke="{accent}" stroke-width="1.0" opacity="0.9" stroke-linecap="round"/>')
        # upper beam + front truss (zigzag chord + posts)
        o.append(f'  <path d="M{MX(18):.1f} {MY(342):.1f} L{MX(840):.1f} {MY(285):.1f}" stroke="{accent}" stroke-width="1.3" fill="none" opacity="0.72" stroke-linecap="round"/>')
        tp = [(40,403),(75,404),(130,340),(195,395),(250,392),(325,330),(395,385),
              (455,382),(540,318),(615,374),(675,370),(760,285)]
        pts = " ".join(f"{MX(x):.1f},{MY(y):.1f}" for x, y in tp)
        o.append(f'  <polyline points="{pts}" stroke="{accent}" stroke-width="1.1" fill="none" opacity="0.72" stroke-linejoin="round" stroke-linecap="round"/>')
        for x, y0, y1 in [(130,340,398),(325,330,388),(540,318,380)]:
            o.append(f'  <line x1="{MX(x):.1f}" y1="{MY(y0):.1f}" x2="{MX(x):.1f}" y2="{MY(y1):.1f}" stroke="{accent}" stroke-width="1.1" opacity="0.72"/>')
        return "\n".join(o)

    block = dome(_bg)   # perspective durian-dome; _bg passed by caller for the cover
    out = lines[:start] + [block] + lines[end+1:]
    return "\n".join(out)


def process(src, out_dark, out_light, dmap, lmap, dbg, lbg, dims):
    s = open(src).read()
    if "straits" in out_dark:
        # New Esplanade durian-dome (perspective). Theme accent per the design call:
        #   DARK  = grey lattice + GREY accent (muted, architectural)
        #   LIGHT = grey lattice + RED  accent (Singapore-red hero)
        # _bg is passed so the dome's NanoSVG-safe cover shapes match each panel bg.
        dark_dome  = clean_esplanade(s, shell="#7a7e86", accent="#9a9fa8",
                                     grid_op=0.95, _bg=dbg)
        light_dome = clean_esplanade(s, shell="#6a6e76", accent="#d4001a",
                                     grid_op=0.95, _bg=lbg)
        open(out_dark,  "w").write(flatten_gradients(force_bg(remap(dark_dome,  dmap), dbg, dims)))
        open(out_light, "w").write(flatten_gradients(force_bg(remap(light_dome, lmap), lbg, dims)))
        print(f"  {src} -> {out_dark.split('/')[-1]} (grey accent), {out_light.split('/')[-1]} (red accent)")
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
