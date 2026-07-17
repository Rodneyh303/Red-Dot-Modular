#!/usr/bin/env python3
"""dot.modular knob assets — fluted, meloDICER-family.

WHAT WAS WRONG. The old knobs modelled the flutes as 18 ELLIPSES painted on the FACE
(rx=1.44 ry=2.88 at r=8.1, gradient-filled). A real fluted knob has no marks on its face:
the flutes are notches cut into the SILHOUETTE. Painted ovals on a flat disc read as
bubbles or dimples, which is exactly how they looked.

WHAT THIS DOES. The body is a single scalloped path -- r(theta) = R - a*(1+cos(N*theta))/2
-- so the flutes are the outline. The cap is a plain disc, the pointer a plain line.

NO GRADIENTS, NO FILTERS. The old assets used radialGradient (18 url() refs in the cream
large) and an feDropShadow filter. nanosvg has no filter support at all, so that shadow has
never rendered; and Rack's svgDraw collapses a gradient to its FIRST and LAST stop, so a
3-stop gradient silently loses its middle. Depth here is solid concentric fills instead:
same result, no dependence on either tolerance. (Rack's SvgKnob also adds its own
CircularShadow, which we disable in MonsoonWidget -- see the note there.)

FOOTPRINTS ARE PRESERVED EXACTLY. SvgKnob sizes its box from the SVG, so changing viewBox
or width would move every knob on every panel. Each generated file keeps the viewBox and
px size of the file it replaces; only the artwork inside changes.
"""
import math

# name -> (viewBox half-extent mm, px width, body radius mm, palette)
# Footprints copied from the existing assets. Small/Straits sizes are new but reuse the
# same 2.9527559 px/mm scale so they sit on the same grid.
CREAM = dict(body='#e9e3d2', bodyLo='#d8d1bd', cap='#f4f0e4', capRim='#c9c2ad',
             edge='#2b2620', flute='#b9b2a0', pointer='#1b1815')
DARK  = dict(body='#2e2e2e', bodyLo='#1f1f1f', cap='#3a3a3a', capRim='#141414',
             edge='#0b0b0b', flute='#141414', pointer='#e8e8e8')

# name -> (viewBox half-extent mm, px width, BODY RADIUS mm, palette)
#
# BODY RADIUS IS LOAD-BEARING, not a style choice. The mod-arc overlay computes its radius
# from the WIDGET BOX -- arc->radius = min(box.size) * radiusFrac + 0.6mm (MonsoonWidget) --
# and the box comes from the SVG's viewBox/width. So the box fixes where the arc sits, and
# the body radius alone decides the gap between knob edge and arc. Shrink a body and the arc
# floats away from it; grow one and the arc cuts into it. Both silent.
#
# These radii are therefore copied verbatim from the pre-existing assets, measured as the
# LARGEST circle in each file. (An earlier pass read the FIRST circle instead -- a 0.42mm
# decoration dot -- and built this table from it, shrinking the cream knobs ~20% and growing
# the dark ones up to 40%. That is the failure this comment exists to prevent.)
# The body/box ratios are inconsistent (0.80 / 0.75 / 0.62 / 0.56 / 0.50) because the old
# margins were sized for an feDropShadow that nanosvg never rendered. Inconsistent or not,
# they must be preserved: they set each knob's arc gap.
SPECS = [
    # existing -- footprint AND body radius must not change
    ('RDM_KnobCream_Large',  13.500, 79.724, 10.80, CREAM),
    ('RDM_KnobCream_Medium', 11.000, 64.961,  8.30, CREAM),
    ('RDM_KnobDark_Large',   13.500, 79.724, 10.80, DARK),
    ('RDM_KnobDark_Medium',  11.000, 64.961,  8.30, DARK),
    ('RDM_KnobLarge',        10.500, 62.010,  6.50, DARK),
    ('RDM_KnobMedium',        9.000, 53.150,  5.00, DARK),
    ('RDM_KnobSmall',         8.000, 47.240,  4.00, DARK),
    # new -- Cream/Dark Small follow RDM_KnobSmall's box+body exactly, so they are
    # interchangeable with it and inherit the same arc gap.
    ('RDM_KnobCream_Small',   8.000, 47.240,  4.00, CREAM),
    ('RDM_KnobDark_Small',    8.000, 47.240,  4.00, DARK),
    # new -- Trimpot scale for the Straits expanders. No existing asset to match; body/box
    # 0.65 sits between the Small (0.50) and Large (0.80) ratios.
    ('RDM_TrimCream',         5.500, 32.480,  3.60, CREAM),
    ('RDM_TrimDark',          5.500, 32.480,  3.60, DARK),
]

# ── Anatomy, from the meloDICER photo ────────────────────────────────────────
# The OUTER CIRCUMFERENCE IS ROUND. The flutes are a raised COG, inset from that rim, whose
# edges are sharp and CONNECT to each other -- no flat between them, meeting at points.
# Earlier passes cut scallops out of the SILHOUETTE, which is a different knob entirely.
#
#   R           round skirt (plain circle) ............ the outline
#   GRIP_OUT    cog points, inset from the rim ........ leaves the round edge visible
#   GRIP_IN     cog valleys ........................... sharp, no flat -> teeth connect
#   FACE        smooth domed top
#
# WHAT RACK ALLOWS. SvgKnob rotates our whole SVG inside a TransformWidget, so anything baked
# here TURNS WITH THE KNOB. Geometry is fine (a cog SHOULD rotate). Radially symmetric shading
# is fine (invisible under rotation). DIRECTIONAL lighting is not: a top-left highlight would
# spin, which is exactly backwards -- on a real knob the light is fixed and the flutes move
# through it. That is why the photo's look cannot be copied literally; "raised" has to be
# carried by a rotation-invariant step (dark valley floor, lighter cog face) plus the
# geometry. A fixed gloss would have to be drawn by the WIDGET, outside the rotation.
# Also unavailable: <filter>/drop shadows (nanosvg ignores them) and >2-stop gradients
# (Rack's svgDraw keeps only the first and last stop).
FLUTES    = 6        # finger grips, per the photo
GRIP_OUT  = 0.93     # cog points, as a fraction of R -- inset, so the round rim shows
GRIP_IN   = 0.76     # cog valleys; sharp, teeth connect (triangle wave, no flat)
FACE_FRAC = 0.70     # smooth domed top, inside the cog
PTR_IN    = 0.16
PTR_OUT   = 0.62

def cog_path(R, n=FLUTES, steps=40):
    """Raised grip: a cog with SHARP points that connect. A triangle wave in r gives sharp
    maxima AND minima with no flat between -- teeth touching, which is what the photo shows.
    (A cosine would give soft equal lobes; a powered cosine gives flats with notches. Both
    were tried and neither is this knob.)"""
    ro, ri = R * GRIP_OUT, R * GRIP_IN
    pts = []
    for k in range(n * steps):
        th = 2.0 * math.pi * k / (n * steps)
        x  = (n * th / math.pi) % 2.0            # 0..2 per tooth
        tri = 1.0 - abs(x - 1.0)                 # 0 ->1 ->0, linear: sharp at both ends
        r  = ri + (ro - ri) * tri
        pts.append((r * math.cos(th), r * math.sin(th)))
    return 'M %.3f %.3f ' % pts[0] + ' '.join('L %.3f %.3f' % p for p in pts[1:]) + ' Z'

def build(half, px, R, c):
    # Six flutes read at every size -- that was the point of choosing a grip over a knurl.
    # (The old 18-20 crest version needed thinning at trim scale or it aliased to mush.)
    L = ['<?xml version="1.0" encoding="UTF-8"?>',
         '<svg xmlns="http://www.w3.org/2000/svg" width="%.3f" height="%.3f" '
         'viewBox="%.3f %.3f %.3f %.3f">' % (px, px, -half, -half, 2*half, 2*half),
         '<!-- generated by panel_src/gen_knobs.py : do not hand-edit -->']
    # 1. Round skirt -- the outline is a CIRCLE, not scalloped.
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="0.30"/>'
             % (R, c['bodyLo'], c['edge']))
    # 2. Valley floor: a dark ring the cog sits proud of. Radially symmetric, so it cannot
    #    betray the rotation -- this is what carries "raised" without directional light.
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R * GRIP_OUT, c['flute']))
    # 3. The raised cog: sharp points, teeth connecting.
    L.append('<path d="%s" fill="%s" stroke="%s" stroke-width="0.22" stroke-linejoin="round"/>'
             % (cog_path(R), c['body'], c['edge']))
    # 4. Domed face.
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R * FACE_FRAC, c['cap']))
    # 5. Pointer: straight up at 0 rotation, as Rack expects.
    L.append('<line x1="0" y1="%.3f" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'stroke-linecap="round"/>' % (-R*PTR_IN, -R*PTR_OUT, c['pointer'], R*0.075))
    L.append('</svg>')
    return '\n'.join(L)

for name, half, px, R, c in SPECS:
    open('res/%s.svg' % name, 'w').write(build(half, px, R, c))
    print('  %-24s viewBox=%.1f  px=%.3f  R=%.2f  flutes=%d' % (name, 2*half, px, R, FLUTES))
