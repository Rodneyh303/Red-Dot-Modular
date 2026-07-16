#!/usr/bin/env python3
"""Monsoon MODE column: emit the button + 5 mode-light anchors from one source.

The bug this fixes: the letters A/B/C/D were drawn at EXACTLY the light coordinates
(x=192, y=34+i*8). Text draws after widgets, so each letter covered its own light --
which is why the letter appeared to light up rather than the LED, and why on the light
theme a near-black glyph sat on an unlit light's dark circle and vanished entirely.

Layout (right strip, x 185..203). Verified against the step ring at centre 162,30 with
an outer tick radius of 23mm: at the light rows the ring reaches x=186 at worst, so a
light column at x=189 clears it by >=3mm and the letters sit outboard at x=197.

Mode E is included: the engine has always handled modeSelect==4 (the cycle is
(modeSelect+1)%5) but there was no fifth light, so selecting E just went dark.
"""
import re

MM_OUTER = 2.9527559       # Rack px per mm (75dpi) -- anchors live in outer coords
BTN   = (194.0, 14.0)      # cycle button
LIGHT_X = 189.0            # LED column
FIRST_Y, DY = 30.0, 8.0    # 5 rows: 30,38,46,54,62 -- all clear of the ring and of the
                           # control-row labels at y=81
IDS = ['MODE_A_LIGHT','MODE_B_LIGHT','MODE_C_LIGHT','MODE_D_LIGHT','MODE_E_LIGHT']

def anchors():
    out = ['<circle id="param_MODE_PARAM" cx="%.2f" cy="%.2f" r="3" fill="none" stroke="none"/>'
           % (BTN[0]*MM_OUTER, BTN[1]*MM_OUTER)]
    for i, lid in enumerate(IDS):
        out.append('<circle id="light_%s" cx="%.2f" cy="%.2f" r="3" fill="none" stroke="none"/>'
                   % (lid, LIGHT_X*MM_OUTER, (FIRST_Y + i*DY)*MM_OUTER))
    return '\n'.join(out)

# guard: every light row must clear the step ring (centre 162,30, tick radius 23)
for i in range(len(IDS)):
    y = FIRST_Y + i*DY
    dy = abs(y - 30.0)
    dx = (23.0**2 - dy**2) ** 0.5 if dy < 23.0 else 0.0
    clear = LIGHT_X - (162.0 + dx)
    assert clear >= 2.5, 'mode light row y=%.0f only clears the ring by %.1fmm' % (y, clear)
print('geometry ok: 5 light rows, worst ring clearance %.1f mm' %
      min(LIGHT_X - (162.0 + ((23.0**2 - abs(FIRST_Y+i*DY - 30.0)**2) ** 0.5
          if abs(FIRST_Y+i*DY-30.0) < 23.0 else 0.0)) for i in range(len(IDS))))

for theme in ('dark', 'light'):
    p = 'res/panels/Monsoon_panel_%s_monsoon.svg' % theme
    s = open(p).read()
    for lid in IDS:
        s = re.sub(r'\n?<circle id="light_%s"[^>]*/>' % lid, '', s)
    s = re.sub(r'\n?\s*<circle id="param_MODE_PARAM"[^>]*/>', '', s)
    k = s.index('id="components">') + len('id="components">')
    s = s[:k] + '\n' + anchors() + s[k:]
    open(p, 'w').write(s)
    print('%-6s  mode anchors emitted: button + %d lights' % (theme, len(IDS)))
