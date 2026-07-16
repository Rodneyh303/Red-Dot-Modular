#!/usr/bin/env python3
"""Monsoon faders: emit the ANCHORS and their level-marker ticks from ONE loop.

This is the single-source fix (cleanup doc A3 / step 5), applied to the faders only.

Before: the widget placed the 14 faders from hardcoded mm (MonsoonWidget.cpp:
        7.5f + i*9.f, then 119.f/128.f) while the ticks were absolute coords in the
        SVG. Two grids -> they drifted, and the LO/HI markers ended up 3.5mm from
        their sliders because the tick grid never knew about the gap after B.

After:  FADERS_MM below is the only place a fader position exists. This script emits
          * one <circle id="param_SEMIn_PARAM"> anchor per fader (components layer)
          * the tick columns, derived from the same list
        and MonsoonWidget binds by anchor id via SvgPanelKit::bindLightParam, so it
        no longer carries coordinates at all. A tick cannot drift from its fader
        because both come from one loop.

Tick design follows Befaco Octaves (res/panels/Octaves.svg): one column per gap,
long, solid, uniform weight -- no major/minor, which at this size only made the
minors vanish.
"""
import re

MM_INNER = 3.779527559     # inner SVG units per mm (inner = 96dpi px)
MM_OUTER = 2.9527559       # outer px per mm (Rack mm2px, 75dpi) -- anchors live here

# ── THE SINGLE SOURCE ────────────────────────────────────────────────────────
# 12 semitones on a 9mm pitch, then the octave pair past a wider gap after B.
FADERS_MM = [7.5 + i * 9.0 for i in range(12)] + [119.0, 128.0]
FADER_Y_MM = 59.75         # slider centre line (matches the sliders' travel below)
ANCHOR_IDS = ['param_SEMI%d_PARAM' % i for i in range(12)] + \
             ['param_OCT_LO_PARAM', 'param_OCT_HI_PARAM']

# The travel, inner units. SL_TOP=45mm .. SL_TOP+SLH=74.5mm in MonsoonWidget.hpp.
TOP_Y, BOT_Y = 170.08, 281.57
# Rows across that span. The OUTERMOST two are dropped: a tick exactly at the travel limit
# sits level with the cap at its end stop, so it read as overshooting the slider rather than
# marking it. 9 divisions, 7 drawn.
LEVELS  = 9
DROP_ENDS = True
TICK_MM = 3.4              # ~38% of the 9mm pitch
TRACK_HALF_MM = 1.6        # visible slider track half-width; ticks must clear it

def gap_centres_mm():
    return [(FADERS_MM[i] + FADERS_MM[i + 1]) / 2.0 for i in range(len(FADERS_MM) - 1)]

def build_ticks(stroke):
    ys = [TOP_Y + (BOT_Y - TOP_Y) * k / (LEVELS - 1) for k in range(LEVELS)]
    if DROP_ENDS: ys = ys[1:-1]
    half = TICK_MM * MM_INNER / 2.0
    L = ['  <g stroke="%s" stroke-width="1.5" opacity="0.75" fill="none" stroke-linecap="round">' % stroke]
    for cx_mm in gap_centres_mm():
        cx = cx_mm * MM_INNER
        for y in ys:
            L.append('    <line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f"/>' % (cx - half, y, cx + half, y))
    L.append('  </g>')
    return '\n'.join(L)

def build_anchors():
    return '\n'.join(
        '<circle id="%s" cx="%.2f" cy="%.2f" r="3" fill="none" stroke="none"/>'
        % (aid, mm * MM_OUTER, FADER_Y_MM * MM_OUTER)
        for aid, mm in zip(ANCHOR_IDS, FADERS_MM))

def cut_group(s, open_re):
    m = re.search(open_re, s); i, depth = m.end(), 0
    while True:
        o, c = s.find('<g', i), s.find('</g>', i)
        if o != -1 and o < c: depth += 1; i = o + 2
        else:
            if depth == 0: return m.start(), c + 4
            depth -= 1; i = c + 4

# sanity: no tick may touch a track, nor reach its neighbour's
half = TICK_MM / 2.0
for cx in gap_centres_mm():
    for f in FADERS_MM:
        assert abs(cx - f) - half > TRACK_HALF_MM + 0.2 or abs(cx - f) > 6, \
            'tick at %.2f would touch the fader at %.2f' % (cx, f)
print('geometry ok: tick %.1fmm, clears %.1fmm track by %.2fmm' %
      (TICK_MM, TRACK_HALF_MM * 2, (4.5 - half) - TRACK_HALF_MM))

for theme, stroke in (('dark', '#888888'), ('light', '#999999')):
    p = 'res/panels/Monsoon_panel_%s_monsoon.svg' % theme
    s = open(p).read()
    a, b = cut_group(s, r'  <g stroke="%s"[^>]*fill="none"[^>]*>' % re.escape(stroke))
    old = s[a:b]
    s = s[:a] + build_ticks(stroke) + s[b:]
    # anchors: drop any we emitted before, then insert at the top of the components layer
    for aid in ANCHOR_IDS:
        s = re.sub(r'\n?<circle id="%s"[^>]*/>' % aid, '', s)
    k = s.index('id="components">') + len('id="components">')
    s = s[:k] + '\n' + build_anchors() + s[k:]
    open(p, 'w').write(s)
    print('%-6s  ticks %d -> %d   anchors emitted: %d' %
          (theme, old.count('<line'), build_ticks(stroke).count('<line'), len(ANCHOR_IDS)))
