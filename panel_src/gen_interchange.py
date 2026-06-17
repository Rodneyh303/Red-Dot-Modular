#!/usr/bin/env python3
"""Interchange (Tonal CV expander) panel generator — dot.modular Peranakan lattice.

Reverse-engineered from the hand-authored res/panels/interchange_gemini_new2.svg
so the panel can be regenerated and, crucially, carries a #components marker layer
for SvgPanelKit binding (the hand SVG had none, which blocked kit migration).

Panel: 270 x 380 px (this module is laid out in PX, unlike the mm-based brand
panels — kept as-is to match the existing widget's hard-coded coordinates so a
kit migration is a drop-in).

Layout (ground truth = MonsoonInterchangeExpander widget):
  6 semitone rows at y = 80,120,160,200,240,280; each row has, L→R:
    CV-in (x=48)  atten (x=102)  atten (x=168)  CV-in (x=222)
    left pair  = semitone i (0..5), right pair = semitone i+6 (6..11)
  octave row at y=320: oct-lo CV(48) oct-lo att(102) oct-hi att(168) oct-hi CV(222)

Visual: the Peranakan hex-lattice motif from the original — outer/mid/inner
hexagon rings, spokes, gold vertex dots, teal edge chain, header accent. Drawn as
a faint background watermark; control wells + the #components markers sit on top.

Decorative layer uses solid strokes/fills only (no gradients/masks) so it stays
nanosvg-friendly; the original's grain pattern + radial glows are dropped (they
were defs the panel renderer tolerated but aren't needed for the look).
"""
import math

W, H = 270, 380                       # px (this module is px-native)

THEMES = {
    "dark": dict(
        bg="#141416", panel="#18181a", top="#d4001a",
        hexOuter="#6a6a6a", hexMid="#555555", hexInner="#888888",
        spoke="#999999", gold="#d4af37", connector="#666666",
        teal="#26a69a", well="#0f1114", wellring="#3a3a3a", text="#f0f0f0",
    ),
    "light": dict(
        bg="#dcdcdc", panel="#e4e4e4", top="#d4001a",
        hexOuter="#9a9a9a", hexMid="#aeaeae", hexInner="#888888",
        spoke="#888888", gold="#b07d00", connector="#a0a0a0",
        teal="#1f8a80", well="#e8e2d6", wellring="#c0b8a8", text="#1a1a1a",
    ),
}

# ── Control layout (px) ──────────────────────────────────────────────────────
COL_X = [48.0, 102.0, 168.0, 222.0]   # CV-in L, atten L, atten R, CV-in R
SEMI_ROWS_Y = [80.0 + i * 40.0 for i in range(6)]
OCT_Y = 320.0


def hexagon(cx, cy, r, rot=0.0):
    pts = []
    for k in range(6):
        a = rot + k * math.pi / 3.0
        pts.append(f"{cx + r*math.cos(a):.1f},{cy + r*math.sin(a):.1f}")
    return " ".join(pts)


def hex_cell(cx, cy, t, r_out=15.0):
    """One Peranakan hex motif centred on a control: nested hexes + spokes +
    gold vertex dots + a small centre diamond. Matches the original's per-control
    tiling (each control sits inside its own cell)."""
    L = []
    A = L.append
    r_mid, r_in = r_out * 0.66, r_out * 0.36
    rot = math.pi / 6
    A(f'<polygon points="{hexagon(cx, cy, r_out, rot)}" fill="none" stroke="{t["hexOuter"]}" stroke-width="0.7" opacity="0.5"/>')
    A(f'<polygon points="{hexagon(cx, cy, r_mid, rot)}" fill="none" stroke="{t["hexMid"]}" stroke-width="0.5" opacity="0.4"/>')
    A(f'<polygon points="{hexagon(cx, cy, r_in, rot)}" fill="none" stroke="{t["hexInner"]}" stroke-width="0.5" opacity="0.45"/>')
    # spokes inner→outer + gold vertex dots
    for k in range(6):
        a = rot + k * math.pi / 3.0
        A(f'<line x1="{cx + r_in*math.cos(a):.1f}" y1="{cy + r_in*math.sin(a):.1f}" '
          f'x2="{cx + r_out*math.cos(a):.1f}" y2="{cy + r_out*math.sin(a):.1f}" '
          f'stroke="{t["spoke"]}" stroke-width="0.4" opacity="0.35"/>')
        A(f'<circle cx="{cx + r_in*math.cos(a):.1f}" cy="{cy + r_in*math.sin(a):.1f}" r="0.9" fill="{t["gold"]}" opacity="0.55"/>')
    return "".join(L)


def diamond(cx, cy, t, r=7.0):
    """Gold nested-diamond connector motif between the two centre columns."""
    return (f'<polygon points="{cx:.1f},{cy-r:.1f} {cx+r:.1f},{cy:.1f} {cx:.1f},{cy+r:.1f} {cx-r:.1f},{cy:.1f}" '
            f'fill="none" stroke="{t["gold"]}" stroke-width="0.6" opacity="0.5"/>'
            f'<polygon points="{cx:.1f},{cy-r*0.5:.1f} {cx+r*0.5:.1f},{cy:.1f} {cx:.1f},{cy+r*0.5:.1f} {cx-r*0.5:.1f},{cy:.1f}" '
            f'fill="none" stroke="{t["gold"]}" stroke-width="0.5" opacity="0.45"/>')


def lattice(t):
    """Per-control Peranakan hex cells (one centred on every control) + a central
    diamond chain between the two attenuverter columns + teal edge ticks."""
    L = []
    A = L.append
    all_rows = SEMI_ROWS_Y + [OCT_Y]
    # a hex cell on every control position
    for y in all_rows:
        for x in COL_X:
            A(hex_cell(x, y, t))
        # centre diamond between the two atten columns
        A(diamond((COL_X[1] + COL_X[2]) * 0.5, y, t))
    # teal edge chain — small ticks down each side, subtler than a big zigzag
    A(f'<g fill="none" stroke="{t["teal"]}" stroke-width="0.9" opacity="0.4">')
    for ex in (16.0, W - 16.0):
        for yy in range(56, 340, 20):
            A(f'<path d="M {ex-3:.1f} {yy:.1f} L {ex:.1f} {yy+5:.1f} L {ex-3:.1f} {yy+10:.1f}"/>')
    A('</g>')
    return "".join(L)


def well(cx, cy, t, ring=None):
    ring = ring or t["wellring"]
    return (f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="7.0" fill="{t["well"]}" '
            f'stroke="{ring}" stroke-width="0.8" opacity="0.7"/>')


def marker(kind, label, cx, cy):
    return (f'<circle id="{kind}_{label}" cx="{cx:.2f}" cy="{cy:.2f}" r="0.5" '
            f'fill="none" stroke="none"/>')


def panel(dark):
    t = THEMES["dark" if dark else "light"]
    o = []
    A = o.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    # background + top accent rule + brand corner dot
    A(f'<rect width="{W}" height="{H}" fill="{t["bg"]}"/>')
    A(f'<rect width="{W}" height="4" fill="{t["top"]}"/>')
    A(f'<circle cx="{W-14}" cy="14" r="5" fill="{t["top"]}"/>')

    # decorative lattice
    A('<g id="artwork">')
    A(lattice(t))
    A('</g>')

    # control wells (graphics) — drawn under the bind markers
    A('<g id="control-graphics">')
    for y in SEMI_ROWS_Y:
        A(well(COL_X[0], y, t)); A(well(COL_X[3], y, t))            # CV jacks
        A(well(COL_X[1], y, t, t["gold"])); A(well(COL_X[2], y, t, t["gold"]))  # attens
    A(well(COL_X[0], OCT_Y, t)); A(well(COL_X[3], OCT_Y, t))
    A(well(COL_X[1], OCT_Y, t, t["gold"])); A(well(COL_X[2], OCT_Y, t, t["gold"]))
    A('</g>')

    # ── SvgPanelKit component layer: named markers at every control centre. ──
    A('<g id="components">')
    for i in range(6):
        y = SEMI_ROWS_Y[i]
        A(marker("input", f"SEMI_CV_{i}",      COL_X[0], y))   # semitone i
        A(marker("param", f"SEMI_ATT_{i}",     COL_X[1], y))
        A(marker("param", f"SEMI_ATT_{i+6}",   COL_X[2], y))   # semitone i+6
        A(marker("input", f"SEMI_CV_{i+6}",    COL_X[3], y))
    A(marker("input", "OCT_LO_CV",  COL_X[0], OCT_Y))
    A(marker("param", "OCT_LO_ATT", COL_X[1], OCT_Y))
    A(marker("param", "OCT_HI_ATT", COL_X[2], OCT_Y))
    A(marker("input", "OCT_HI_CV",  COL_X[3], OCT_Y))
    # dot.modular connect mark anchor (footer-centre).
    A(marker("light", "connect", W * 0.5, H - 20.0))
    A('</g>')

    A('</svg>')
    return "\n".join(o)


if __name__ == "__main__":
    for dark, name in [(True, "interchange_gemini_new2.svg"),
                       (False, "interchange_gemini_light.svg")]:
        with open(f"res/panels/{name}", "w") as f:
            f.write(panel(dark))
        print(f"wrote res/panels/{name}")
