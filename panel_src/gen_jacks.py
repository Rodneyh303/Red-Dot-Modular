#!/usr/bin/env python3
"""dot.modular JACK assets — sibling of gen_controls.py.

WHY. The stock Rack PJ301M jack is visually large on a dense panel; shrinking the
jacks (knobs unchanged) frees real horizontal room next to each lane — enough to add
the voice-LOR controls (R knob + mod jacks + attenuverters + direction/delegation)
on the base panel without a subpanel. See UNIFIED_ADDRESSING_SYSTEM.md (voice-LOR gaps).

WHAT. Generates jack-face SVGs at several sizes, in the SAME palette families as the
knobs (gen_controls.py: OFFWHITE / CREAM / DARK / GREY), so the jacks read as members
of the same instrument rather than Rack defaults. The widget (ui/GeneratedJack.hpp)
is an rack::app::SvgPort subclass — it INHERITS all cable/drag/shadow/hover plumbing
from Rack (the whole reason GoldPolyPort inherits PJ301MPort) and only swaps the
GRAPHIC via setSvg(). So: custom artwork + custom size, WITHOUT reimplementing the
port's interaction. "Optionally replace, else keep stock" — a use site picks a
generated jack or the stock PJ301M per instance.

SAME DISCIPLINE AS THE KNOBS:
- px/mm = 2.9527559 (zoom-1 scale). Sizes reuse that grid.
- NO gradients, NO filters (nanosvg ignores filters; Rack's svgDraw keeps only a
  gradient's first+last stop). Depth is solid concentric fills, like the knobs.
- Strokes scale with radius; a legibility floor so small jacks don't vanish.
- Palette dicts are COPIED from gen_controls.py so the families match exactly. If the
  knob palettes change, re-sync these (they are intentionally duplicated here, small,
  rather than importing, to keep the two generators independent files like the repo's
  existing split).

CENTERED HOLE IS LOAD-BEARING. SvgPort sizes its box from the SVG and connects cables
at the box CENTER. The jack hole MUST be at the viewBox center (0,0 here, since the
viewBox is symmetric [-half,+half]) or cables attach off-center. Enforced by
construction: every element is drawn around (0,0).
"""
import math

PXMM = 2.9527559  # px per mm at zoom 1 (same as gen_controls.py)

# ── Palettes (copied from gen_controls.py — keep in sync) ────────────────────
# A jack uses: rim (metal nut), bore (the dark hole), insert (the sleeve ring),
# edge (outline), and an optional BRAND tint (Singapore red) for signature jacks.
OFFWHITE = dict(rim='#dcdad3', rimLo='#c4c2bc', bore='#141311', insert='#9a978f',
                edge='#2a2a27')
CREAM    = dict(rim='#d8d1bd', rimLo='#c3bca8', bore='#141210', insert='#9c957f',
                edge='#2b2620')
DARK     = dict(rim='#3a3a3a', rimLo='#262626', bore='#0a0a0a', insert='#565656',
                edge='#0b0b0b')
GREY     = dict(rim='#74736e', rimLo='#5f5e5a', bore='#101010', insert='#8b8a84',
                edge='#2a2a27')

PALETTES = {'Dark': DARK, 'OffWhite': OFFWHITE, 'Cream': CREAM, 'Grey': GREY}

# Singapore-red brand ring (the GoldPolyPort signature), drawn optionally on top.
BRAND_RED  = '#d4001a'
BRAND_GOLD = '#c9a227'

# ── Sizes ────────────────────────────────────────────────────────────────────
# name -> (viewBox half-extent mm, body radius mm). px width = 2*half*PXMM.
# Stock PJ301M is ~8.0mm across (half ~4.2mm incl. nut). "Small" / "Tiny" are the
# space savers for dense lane rows; "Std" matches the stock footprint for drop-in.
# Body radius = the metal nut radius; the bore + insert are fractions of it.
SIZES = [
    ('Std',   4.20, 3.65),   # ~ stock PJ301M footprint, dot.modular styling
    ('Small', 3.40, 2.95),   # ~19% narrower — the lane-row space saver
    ('Tiny',  2.90, 2.50),   # ~31% narrower — tightest rows / per-voice banks
]

# ── Anatomy (fractions of the nut radius R) ──────────────────────────────────
INSERT_FR = 0.82   # the sleeve ring (lighter metal) inside the nut
BORE_FR   = 0.50   # the dark hole the plug enters
HILITE_FR = 0.62   # a faint inner ring for a touch of depth (rotation-invariant)

STROKE_FR = 0.055  # outline as a fraction of R
def stroke(R):  return max(R * STROKE_FR, 0.10)


def jack_face(R, c):
    """A clean panel jack: metal nut (rim) + sleeve insert + dark bore, all centered.
    Solid concentric fills (no gradients) — reads as a recessed metal jack and
    survives downsampling. Everything around (0,0) so the bore is the cable point."""
    L = []
    # outer metal nut
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R, c['rimLo'], c['edge'], stroke(R)))
    # nut face (slightly lighter ring)
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R * 0.90, c['rim']))
    # sleeve insert
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R * INSERT_FR, c['insert']))
    # faint inner depth ring (rotation-invariant shading, like the knobs' concentric fills)
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" opacity="0.55"/>'
             % (R * HILITE_FR, c['rimLo']))
    # the bore (dark hole)
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R * BORE_FR, c['bore'], c['edge'], stroke(R) * 0.7))
    return ''.join(L)


def build(half, R, c):
    # Convention copied from gen_controls.py EXACTLY: width/height in PX, viewBox in MM
    # (-half..+half), contents drawn in MM coordinates, NO scale transform. The px-vs-mm
    # ratio IS the px/mm scale; SvgPort sizes its box from width/height.
    px = 2 * half * PXMM
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" width="%.3f" height="%.3f" '
            'viewBox="%.3f %.3f %.3f %.3f">\n'
            '<!-- generated by panel_src/gen_jacks.py : do not hand-edit -->\n'
            '%s\n</svg>\n'
            % (px, px, -half, -half, 2 * half, 2 * half, jack_face(R, c)))


def main():
    import os
    outdir = os.path.join(os.path.dirname(__file__), '..', 'res', 'controls')
    os.makedirs(outdir, exist_ok=True)
    names = []
    for sname, half, R in SIZES:
        for pname, pal in PALETTES.items():
            fn = 'RDM_Jack%s_%s' % (pname, sname)
            path = os.path.join(outdir, fn + '.svg')
            with open(path, 'w') as f:
                f.write(build(half, R, pal))
            names.append(fn)
    print('gen_jacks.py: wrote %d jack SVGs to res/controls/ (%d sizes x %d palettes)'
          % (len(names), len(SIZES), len(PALETTES)))
    return names


if __name__ == '__main__':
    main()
