#!/usr/bin/env python3
"""Monsoon MODE column: button + 3 mode-light anchors, emitted from one source.

MODE_COLLAPSE_6_TO_3: three timing origins -- clock / gate / phase (was A..F). Per
mode a BOXED LETTER, its LED beside it, and a small description underneath. The
cycle button sits on the BPM/LEN/OFFSET line (y=60). Pitch origin (generate vs
quantise) is the q-mix axis, not a mode -- so the column is just the 3 timing
choices. Collapsing 6->3 retires rows 4..6, freeing ~27mm below the column for the
"generative sequencer-quantiser" subtitle.

Only the ANCHORS live here. The widget draws the box, the letter and the
description positioned FROM each light anchor, so the glyph cannot drift off its
LED (the bug this column had: letters drawn at the light coords covered the LED).

Clearances asserted below against the step ring (centre 162,30, outer TICK radius 23mm).
"""
import re

MM = 2.9527559             # Rack px per mm (75dpi) -- anchors are in outer coords

BTN      = (194.0, 60.0)   # cycle button, on the BPM/LEN/OFFSET line
LIGHT_X  = 197.5           # LED column
BOX_CX   = 190.5           # boxed letter centre (widget draws it from the anchor)
BOX_W    = 5.5
ROW_Y    = [13.0, 22.0, 31.0]                                   # 3 timing origins (was 6)
IDS      = ['MODE_A_LIGHT','MODE_B_LIGHT','MODE_C_LIGHT']       # clock / gate / phase

RING_C, RING_R = (162.0, 30.0), 23.0

def ring_reach(y):
    dy = abs(y - RING_C[1])
    return RING_C[0] + ((RING_R**2 - dy**2) ** 0.5 if dy < RING_R else 0.0)

# guards: the boxed letter must clear the ring, and the LED must stay on-panel
for y in ROW_Y:
    clear = (BOX_CX - BOX_W/2) - ring_reach(y)
    assert clear >= 2.0, 'mode row y=%.0f: box clears ring by only %.2fmm' % (y, clear)
assert LIGHT_X + 3.0 < 203.2, 'LED column runs off the panel'
assert BTN[1] == 60.0, 'button should sit on the BPM/LEN/OFFSET line'
print('geometry ok: worst box/ring clearance %.2f mm; LED edge margin %.1f mm' %
      (min((BOX_CX - BOX_W/2) - ring_reach(y) for y in ROW_Y), 203.2 - LIGHT_X))

def anchors():
    out = ['<circle id="param_MODE_PARAM" cx="%.2f" cy="%.2f" r="3" fill="none" stroke="none"/>'
           % (BTN[0]*MM, BTN[1]*MM)]
    for lid, y in zip(IDS, ROW_Y):
        out.append('<circle id="light_%s" cx="%.2f" cy="%.2f" r="3" fill="none" stroke="none"/>'
                   % (lid, LIGHT_X*MM, y*MM))
    return '\n'.join(out)

for theme in ('dark', 'light'):
    p = 'res/panels/Monsoon_panel_%s_monsoon.svg' % theme
    s = open(p).read()
    # strip ALL legacy mode light anchors (A..F) + the button, then re-emit the 3 surviving.
    for c in 'ABCDEF':
        s = re.sub(r'\n?<circle id="light_MODE_%s_LIGHT"[^>]*/>' % c, '', s)
    s = re.sub(r'\n?\s*<circle id="param_MODE_PARAM"[^>]*/>', '', s)
    k = s.index('id="components">') + len('id="components">')
    s = s[:k] + '\n' + anchors() + s[k:]
    open(p, 'w').write(s)
    print('%-6s  button y=%.0f (on the knob line), %d LEDs at x=%.1f, y=%s'
          % (theme, BTN[1], len(IDS), LIGHT_X, [int(y) for y in ROW_Y]))
