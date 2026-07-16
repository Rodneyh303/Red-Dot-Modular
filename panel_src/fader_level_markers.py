#!/usr/bin/env python3
"""Regenerate Monsoon's fader level markers as meloDICER-style ticks FLANKING each
fader, so they land in the gaps instead of hiding under the (opaque) slider track.

Two bugs are fixed at once:

 1. HIDDEN. The old markers were single lines centred ON each fader: major +/-2.8mm,
    minor +/-1.6mm, against a slider track ~2mm half-width. So the minors were fully
    covered and the majors poked out ~0.8mm a side -- which is why they read as faint
    dashes rather than level lines.

 2. LO/HI DRIFT (cleanup doc A3). The old markers assumed 14 evenly spaced faders at
    7.5 + i*9mm, but the widget puts OCT_LO/OCT_HI at 119/128mm -- there is a gap after
    B. So markers 12 and 13 sat 3.5mm left of their sliders. Here every tick is derived
    from the SAME fader centre list the widget uses, so a tick cannot miss its fader.

NOTE this is still two sources (widget mm vs this script) until the from-scratch
generator (cleanup step 5) emits the fader ANCHOR and its ticks from one loop, and the
widget binds by anchor id. Then drift is impossible rather than merely corrected.
"""
import re

MM = 3.779527559          # inner SVG units per mm (inner units are 96dpi px)
# Fader centres, mm -- MUST match MonsoonWidget.cpp: 7.5f + i*9.f, then 119.f, 128.f
FADERS_MM = [7.5 + i * 9.0 for i in range(12)] + [119.0, 128.0]
# Level rows, inner units (unchanged -- these already span the sliders' travel exactly)
MAJOR_Y = [170.08, 225.83, 281.57]
MINOR_Y = [184.01, 197.95, 211.89, 239.76, 253.70, 267.64]

INNER_MM = 2.6            # tick starts here: clear of the ~2mm track half-width
MAJOR_MM = 4.2            # major tick outer edge (stops short of the gap midpoint 4.5)
MINOR_MM = 3.5            # minor tick outer edge

def ticks(y, outer_mm, indent):
    out = []
    for cx_mm in FADERS_MM:
        cx = cx_mm * MM
        a, b = INNER_MM * MM, outer_mm * MM
        out.append('%s<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f"/>' % (indent, cx - b, y, cx - a, y))
        out.append('%s<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f"/>' % (indent, cx + a, y, cx + b, y))
    return out

def build(stroke):
    L = ['  <g stroke="%s" fill="none">' % stroke]
    for y in MAJOR_Y:
        L.append('    <g stroke-width="1.0" opacity="0.80">')
        L += ticks(y, MAJOR_MM, '      '); L.append('    </g>')
    for y in MINOR_Y:
        L.append('    <g stroke-width="0.65" opacity="0.45">')
        L += ticks(y, MINOR_MM, '      '); L.append('    </g>')
    L.append('  </g>')
    return '\n'.join(L)

for theme, stroke in (('dark', '#888888'), ('light', '#999999')):
    p = 'res/panels/Monsoon_panel_%s_monsoon.svg' % theme
    s = open(p).read()
    m = re.search(r'  <g stroke="%s" fill="none">' % re.escape(stroke), s)
    i, depth = m.end(), 0
    while True:
        o, c = s.find('<g', i), s.find('</g>', i)
        if o != -1 and o < c: depth += 1; i = o + 2
        else:
            if depth == 0: break
            depth -= 1; i = c + 4
    old = s[m.start():c + 4]
    new = build(stroke)
    open(p, 'w').write(s.replace(old, new, 1))
    print('%-6s  %3d -> %3d ticks  (14 faders x 9 levels x 2 sides)' %
          (theme, old.count('<line'), new.count('<line')))
