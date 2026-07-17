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
# OFF-WHITE. Sampled from the meloDICER photo: the four big faces mean #d8d1c2, i.e. warmth
# (R-B) of 22 -- essentially IDENTICAL to our CREAM's 23. So ours was never warmer than the
# reference; it is brighter and, decisively, FLAT. The photo reads white because each knob
# carries a near-white specular highlight with shading falling away, and the eye reads "white
# object, lit" and discounts the warm midtone.
#
# We cannot bake that highlight: SvgKnob rotates our SVG, so a fixed light source would spin
# with the knob. Faking it is off the table. The honest move is therefore to cut the WARMTH
# rather than add brightness -- an actually off-white knob instead of a beige one pretending
# to be lit. R-B drops 23 -> 8.
OFFWHITE = dict(body='#eae8e2', bodyLo='#d9d7d0', cap='#f6f5f2', capRim='#c4c2bc',
                edge='#2b2926', flute='#b6b4ad', pointer='#1b1815', dot='#d4001a')

CREAM = dict(body='#e9e3d2', bodyLo='#d8d1bd', cap='#f4f0e4', capRim='#c9c2ad',
             edge='#2b2620', flute='#b9b2a0', pointer='#1b1815', dot='#d4001a')
# OFF-WHITE. Sampling the meloDICER photo, the LIT face of the big knobs is #e7e2cf
# (warmth R-B +26) -- i.e. our CREAM #e9e3d2 (+23) already matches the PHOTOGRAPH almost
# exactly. But the photo is a warm-lit studio shot, so it is an unreliable witness to the
# object: the real knob is very likely cooler than it renders, and on a black panel a warm
# cream reads as white anyway (simultaneous contrast). This palette keeps the same lightness
# and drops the warmth to ~+8 -- a departure from the photo, toward the thing the photo is of.
OFFWHITE = dict(body='#eeece6', bodyLo='#dcdad3', cap='#f8f7f4', capRim='#cdccc6',
                edge='#2a2a27', flute='#bfbeb8', pointer='#1b1815', dot='#d4001a')
DARK  = dict(body='#2e2e2e', bodyLo='#1f1f1f', cap='#3a3a3a', capRim='#141414',
             edge='#0b0b0b', flute='#141414', pointer='#e8e8e8', dot='#d4001a')
# GREY: a third tier for the DARK theme. That panel is overwhelmingly dark -- cream heroes on
# dark everything-else -- so the secondary knobs (BPM/LEN/OFFSET, MIX R/M, SLEW) can lift it
# without going all-cream, which would flatten the hierarchy the big 5 currently have.
# Warmed slightly toward the cream so it reads as the same family rather than a grey part in
# a beige instrument. Pointer stays dark: on grey a dark mark out-contrasts a light one.
GREY  = dict(body='#8b8a84', bodyLo='#74736e', cap='#9d9c96', capRim='#5f5e5a',
             edge='#2a2a27', flute='#5a5955', pointer='#1b1815', dot='#d4001a')

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
    # The ONLY assets the code loads today: MonsoonWidget's RDM_Knob*::setSvg() paths.
    # Footprint AND body radius are load-bearing -- viewBox/width set the widget box, the box
    # sets where the mod arc lands, and the body radius alone sets the gap to it. Do not touch
    # without re-checking the arc gaps (3.30mm cream family / 4.60mm dark family).
    # Everything else now lives in res/knobs/ as a library; migrating these paths there is a
    # separate step.
    ('RDM_KnobCream_Large',  13.500, 79.724, 10.80, CREAM),
    ('RDM_KnobCream_Medium', 11.000, 64.961,  8.30, CREAM),
    ('RDM_KnobDark_Large',   13.500, 79.724, 10.80, DARK),
    ('RDM_KnobDark_Medium',  11.000, 64.961,  8.30, DARK),
    ('RDM_KnobLarge',        10.500, 62.010,  6.50, DARK),
    ('RDM_KnobMedium',        9.000, 53.150,  5.00, DARK),
    ('RDM_KnobSmall',         8.000, 47.240,  4.00, DARK),
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
FLUTES    = 6        # 6 sharp points
GRIP_OUT  = 0.93     # the points, inset from the round rim
GRIP_IN   = 0.70     # depth of the concave scoop at the flute centre
FACE_FRAC = 0.66     # smooth domed top
PTR_IN    = 0.16
PTR_OUT   = 0.60

# ── Legibility floors ────────────────────────────────────────────────────────
# A knob is drawn at 2.95 px/mm at zoom 1, so R=3.6mm trim is TEN PIXELS across. Two things
# broke at that size and neither is cosmetic:
#   * the pointer scaled linearly with R -> 0.075*3.6 = 0.27mm = 0.8px. SUB-PIXEL: the one
#     mark a knob must show, invisible until zoomed. RDM_KnobSmall (R=4.0) was 0.9px, so
#     BPM/LEN/OFFSET and MIX R/M were affected too, not just the trims.
#   * strokes were FIXED at 0.30mm regardless of size -> 8.3% of the trim's radius versus
#     2.8% of the Large's: proportionally 3x heavier, so small knobs read as outlined blobs.
# Hence: floor the pointer, and scale the stroke.
PTR_W_MIN = 0.45     # mm -- roughly 1.3px at zoom 1, the thinnest that survives
STROKE_FR = 0.028    # outline as a fraction of R (was a fixed 0.30mm)

def ptr_w(R):    return max(R * 0.075, PTR_W_MIN)
def stroke(R):   return max(R * STROKE_FR, 0.10)

def cog_path(R, n=FLUTES, gout=None, gin=None, steps=26):
    """Raised grip: 6 sharp points with ONE CONCAVE ARC between each pair.

    Not two convex lobes meeting in a valley, and not a triangle wave -- between adjacent
    points the edge is a single circular arc that scoops INWARD. That is the classic fluted
    knob and it is what the photo shows.

    Each scoop is a circle centred OUTSIDE the knob, on the flute's mid-angle, passing
    through both neighbouring points and dipping to GRIP_IN at the middle. Three points fix
    it; by symmetry the centre lies on the mid-angle ray at:
        cx = (Rp^2 - Rv^2) / (2*(Rp*cos(pi/n) - Rv))      rc = cx - Rv
    A ray at angle d off the flute centre meets that circle (near side) at:
        t = cx*cos d - sqrt(cx^2*cos^2 d - cx^2 + rc^2)
    """
    Rp = R * (GRIP_OUT if gout is None else gout)
    Rv = R * (GRIP_IN if gin is None else gin)
    half = math.pi / n                      # half a flute, in radians
    cx = (Rp*Rp - Rv*Rv) / (2.0 * (Rp*math.cos(half) - Rv))
    rc = cx - Rv                            # centre outside, arc bulges inward => concave
    pts = []
    for k in range(n):
        phi = 2.0*math.pi*k/n + half        # this flute's mid-angle
        for j in range(steps + 1):
            d  = -half + 2.0*half*j/steps   # -half .. +half across the scoop
            cd = math.cos(d)
            t  = cx*cd - math.sqrt(max(0.0, cx*cx*cd*cd - cx*cx + rc*rc))
            th = phi + d
            pts.append((t*math.cos(th), t*math.sin(th)))
    return 'M %.3f %.3f ' % pts[0] + ' '.join('L %.3f %.3f' % p for p in pts[1:]) + ' Z'

def body_cog(R, c):
    """Hero style: round rim, raised 6-point cog inset from it, domed face."""
    L=[]
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R, c['bodyLo'], c['edge'], stroke(R)))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*GRIP_OUT, c['flute']))
    L.append('<path d="%s" fill="%s" stroke="%s" stroke-width="%.3f" stroke-linejoin="round"/>'
             % (cog_path(R, FLUTES, GRIP_OUT, GRIP_IN), c['body'], c['edge'], stroke(R)*0.75))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*FACE_FRAC, c['cap']))
    L.append('<line x1="0" y1="%.3f" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'stroke-linecap="round"/>' % (-R*PTR_IN, -R*PTR_OUT, c['pointer'], ptr_w(R)))
    return ''.join(L)

def body_slot(R, c):
    """Slotted: plain disc + a screwdriver slot ACROSS the face. Says set-and-forget by kind,
    not just by size. One high-contrast mark spanning the whole face, so it survives the
    downsample where a thin pointer cannot -- and it is already the bar-through-centre an
    attenuverter wants to read as bipolar."""
    w = max(R*0.30, 0.75)
    L=[]
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R, c['body'], c['edge'], stroke(R)))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*0.80, c['cap']))
    L.append('<line x1="0" y1="%.3f" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'stroke-linecap="round"/>' % (-R*0.74, R*0.74, c['flute'], w))
    L.append('<line x1="0" y1="%.3f" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'stroke-linecap="round"/>' % (-R*0.74, -R*0.20, c['pointer'], w))
    return ''.join(L)

def body_quad(R, c):
    """Four flutes: same concave-arc language as the hero knob, but ~2.7px per feature at
    actual size instead of ~1.8. Distinct silhouette, still obviously family."""
    L=[]
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R, c['bodyLo'], c['edge'], stroke(R)))
    L.append('<path d="%s" fill="%s" stroke="%s" stroke-width="%.3f" stroke-linejoin="round"/>'
             % (cog_path(R, 4, 0.95, 0.62), c['body'], c['edge'], stroke(R)*0.75))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*0.52, c['cap']))
    L.append('<line x1="0" y1="%.3f" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'stroke-linecap="round"/>' % (-R*0.10, -R*0.58, c['pointer'], ptr_w(R)))
    return ''.join(L)

def body_dot(R, c):
    """Red dot: position shown by a filled dot, not a line. A disc survives downsampling far
    better than a stroke, and it is the brand mark. Caveat: red across ~38 trims dilutes the
    accent -- may suit only ACTIVE/modulated trims (pairs with DimmableTrimpot::dimWhen)."""
    L=[]
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R, c['body'], c['edge'], stroke(R)))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*0.78, c['cap']))
    L.append('<circle cx="0" cy="%.3f" r="%.3f" fill="%s"/>'
             % (-R*0.55, max(R*0.16, 0.34), c['dot']))
    return ''.join(L)

def body_bar(R, c):
    """Attenuverter: hero cog body, but the pointer is a BAR THROUGH CENTRE rather than a
    line from it. That single change says "bipolar" pre-attentively, at any size, before the
    angle is read -- which is the one thing an attenuverter must communicate and a value knob
    must not. Family silhouette, different job."""
    L=[]
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s" stroke="%s" stroke-width="%.3f"/>'
             % (R, c['bodyLo'], c['edge'], stroke(R)))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*GRIP_OUT, c['flute']))
    L.append('<path d="%s" fill="%s" stroke="%s" stroke-width="%.3f" stroke-linejoin="round"/>'
             % (cog_path(R, FLUTES, GRIP_OUT, GRIP_IN), c['body'], c['edge'], stroke(R)*0.75))
    L.append('<circle cx="0" cy="0" r="%.3f" fill="%s"/>' % (R*FACE_FRAC, c['cap']))
    # tail is dimmed so the pointing end still leads, but the bar reads as bipolar
    L.append('<line x1="0" y1="0" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'opacity="0.42" stroke-linecap="round"/>' % (R*PTR_OUT, c['pointer'], ptr_w(R)))
    L.append('<line x1="0" y1="0" x2="0" y2="%.3f" stroke="%s" stroke-width="%.3f" '
             'stroke-linecap="round"/>' % (-R*PTR_OUT, c['pointer'], ptr_w(R)))
    return ''.join(L)

STYLES = {'cog': body_cog, 'slot': body_slot, 'quad': body_quad, 'dot': body_dot,
          'bar': body_bar}

def build(half, px, R, c, style='cog'):
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" width="%.3f" height="%.3f" '
            'viewBox="%.3f %.3f %.3f %.3f">\n'
            '<!-- generated by panel_src/gen_knobs.py : do not hand-edit -->\n'
            % (px, px, -half, -half, 2*half, 2*half)
            + STYLES[style](R, c) + '\n</svg>\n')

# ── res/knobs/ : the full library ────────────────────────────────────────────
# Naming: RDM_<Palette>_<Size>_<Style>.svg
#
# PALETTES.  Chosen by measurement, not taste (see the palette notes above):
#   OffWhite  hero on the DARK panel   12.8:1
#   Dark      hero on the LIGHT panel  10.9:1
#   Grey      SECONDARY on BOTH        4.5:1 dark / 2.8:1 light -- one tier, either theme
#   Cream     the previous hero, kept until the bindings move off it
#
# SIZES are the existing footprints, unchanged. viewBox/width fix the widget box, and the box
# fixes where the mod arc lands (arc->radius = min(box)*radiusFrac + 0.6mm), so the body
# radius alone sets the arc gap. These are not free parameters -- see SPECS.
#
# STYLES.  cog = hero/value. bar = attenuverter (bipolar). slot/quad/dot = trim concepts,
# undecided. Every size is emitted in every style: they are ~3KB each and it costs nothing to
# be able to try one on a panel.
SIZES = [('Large',   13.500, 79.724, 10.80),
         ('Medium',  11.000, 64.961,  8.30),
         ('Mid',     10.500, 62.010,  6.50),
         ('Compact',  9.000, 53.150,  5.00),
         ('Small',    8.000, 47.240,  4.00),
         ('Trim',     5.500, 32.480,  3.60)]
PALETTES = [('OffWhite', OFFWHITE), ('Cream', CREAM), ('Grey', GREY), ('Dark', DARK)]

import os
os.makedirs('res/knobs', exist_ok=True)
n = 0
for pname, pal in PALETTES:
    for sname, half, px, R in SIZES:
        for style in ('cog', 'bar', 'slot', 'quad', 'dot'):
            fn = 'res/knobs/RDM_%s_%s_%s.svg' % (pname, sname, style.capitalize())
            open(fn, 'w').write(build(half, px, R, pal, style))
            n += 1
print('  res/knobs/: %d files  (%d palettes x %d sizes x %d styles)'
      % (n, len(PALETTES), len(SIZES), 5))

# ── the currently BOUND assets, kept at their existing paths so the build is unaffected.
# MonsoonWidget's setSvg() calls point here; migrating those to res/knobs/ is a separate step.
print()
for name, half, px, R, c in SPECS:
    open('res/%s.svg' % name, 'w').write(build(half, px, R, c, 'cog'))
print('  res/RDM_*.svg: %d bound assets regenerated (paths unchanged)' % len(SPECS))
