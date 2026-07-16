#!/usr/bin/env python3
"""Monsoon fader level markers — Befaco Octaves style.

Reference: Befaco Octaves (res/panels/Octaves.svg). Its sliders are scaled by ONE column
of ticks sitting in the gap beside each fader: long (roughly a third of the fader pitch),
solid, uniform weight, evenly spanning the travel. Legible at a glance and quiet.

History of this panel's markers:
  v1  one line drawn ACROSS each fader (major +/-2.8mm, minor +/-1.6mm) against a
      ~2mm track half-width -> minors fully hidden, majors poking out ~0.8mm. Read as dashes.
  v2  short ticks flanking BOTH sides of every fader -> 252 marks, visible but speckled.
  v3  (this) one column per GAP, long and uniform. 13 columns x 9 levels = 117 ticks.

Geometry is derived from the fader centres, so a tick cannot drift from its fader --
including OCT_LO/OCT_HI, which sit past a gap after B and which v1 missed by 3.5mm
(cleanup doc A3).

STILL TWO SOURCES until cleanup step 5: the fader mm live in MonsoonWidget.cpp and are
copied here. The fix is for the generator to emit each fader's ANCHOR and its ticks from
ONE loop, with the widget binding by anchor id -- then drift is impossible, not merely
corrected.
"""
import re

MM = 3.779527559          # inner SVG units per mm (inner units are 96dpi px)
# Fader centres, mm -- MUST match MonsoonWidget.cpp: 7.5f + i*9.f, then 119.f, 128.f
FADERS_MM = [7.5 + i * 9.0 for i in range(12)] + [119.0, 128.0]

TOP_Y, BOT_Y = 170.08, 281.57     # inner units: the sliders' travel, unchanged
LEVELS = 9                         # tick rows, evenly spaced across the travel
TICK_MM = 3.0                      # tick length ~1/3 of the 9mm pitch, as Befaco

def gap_centres_mm():
    """One column per gap between adjacent faders (13 for 14 faders), as Befaco."""
    return [(FADERS_MM[i] + FADERS_MM[i + 1]) / 2.0 for i in range(len(FADERS_MM) - 1)]

def build(stroke):
    ys = [TOP_Y + (BOT_Y - TOP_Y) * k / (LEVELS - 1) for k in range(LEVELS)]
    L = ['  <g stroke="%s" stroke-width="0.9" opacity="0.55" fill="none" stroke-linecap="round">' % stroke]
    half = TICK_MM * MM / 2.0
    for cx_mm in gap_centres_mm():
        cx = cx_mm * MM
        for y in ys:
            L.append('    <line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f"/>' % (cx - half, y, cx + half, y))
    L.append('  </g>')
    return '\n'.join(L)

for theme, stroke in (('dark', '#888888'), ('light', '#999999')):
    p = 'res/panels/Monsoon_panel_%s_monsoon.svg' % theme
    s = open(p).read()
    m = re.search(r'  <g stroke="%s"[^>]*fill="none"[^>]*>' % re.escape(stroke), s)
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
    print('%-6s  %3d -> %3d ticks  (13 gaps x %d levels)' % (theme, old.count('<line'), new.count('<line'), LEVELS))
