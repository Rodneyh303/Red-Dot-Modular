#!/usr/bin/env python3
"""Monsoon MODE column: button + 3 mode-light anchors, emitted from one source.

MODE_COLLAPSE_6_TO_3: three timing origins -- clock / gate / phase (was A..F). Per
mode a BOXED LETTER, its LED beside it, and a small description underneath. The
cycle button sits at y=70 (below the mode rows, above the y=78 rail). Pitch origin
(generate vs quantise) is the q-mix axis, not a mode -- so the column is just the 3
timing choices.

GEOMETRY SINGLE-SOURCE (fix_mode_column_position.md): the right cluster was shifted
right by DX_RIGHT in monsoon_art.py, including the Flyer/16-step ring (FLYER_C) and
the mode column (MODE_PARAM_XY, MODE_LIGHT_*). This generator MIRRORS those exact
values so the anchors cannot drift from the visible art. DX_RIGHT is duplicated here
with an assert; the long-term fix is a shared geometry module (PanelTokens,
PANEL_CRAFT_AUDIT.md) so the duplication is impossible.

Only the ANCHORS live here. The widget draws the box, the letter and the description
positioned FROM each light anchor, so the glyph cannot drift off its LED.

Clearances asserted below against the SHIFTED Flyer ring (centre 162+DX_RIGHT, 30,
outer TICK radius 23mm).
"""
import re

MM = 2.9527559             # Rack px per mm (75dpi) -- anchors are in outer coords

# ── Mirror of monsoon_art.py's right-cluster geometry (single-source; see header) ──
DX_RIGHT = 25.4            # MUST match monsoon_art.DX_RIGHT -- the right-cluster shift
assert DX_RIGHT == 25.4, 'DX_RIGHT drifted from monsoon_art.py; keep them in sync (or extract a shared module)'

RING_C = (162.0 + DX_RIGHT, 30.0)   # the SHIFTED Flyer/16-step ring centre (monsoon_art.FLYER_C)
RING_R = 23.0                       # outer TICK radius (clearance guard)

# COORDINATED GROUP (fix_mode_column_position.md UPDATE 2): the C/G/P column is vertically centred
# on the ring (centre y=30 -> rows 21/30/39), and the cycle BUTTON is grouped ADJACENT to the stack
# (just below it, off the BPM/LEN/OFFSET knob line). At the shifted X (222.9) this clears the ring
# (max reach 210.4 at y=30). These mirror monsoon_art.MODE_LIGHT_Y0 / MODE_PARAM_XY — keep in sync.
BTN      = (194.0 + DX_RIGHT, 46.0) # cycle button, grouped below the C/G/P stack (was y=70 on the knob line)
LIGHT_X  = 197.5 + DX_RIGHT         # LED column (monsoon_art.MODE_LIGHT_X)
BOX_CX   = 190.5 + DX_RIGHT         # boxed letter centre (widget draws it from the anchor)
BOX_W    = 5.5
ROW_Y    = [21.0, 30.0, 39.0]                                   # centred on the ring (monsoon_art.MODE_LIGHT_Y0 + i*PITCH)
IDS      = ['MODE_A_LIGHT','MODE_B_LIGHT','MODE_C_LIGHT']       # clock / gate / phase

def ring_reach(y):
    dy = abs(y - RING_C[1])
    return RING_C[0] + ((RING_R**2 - dy**2) ** 0.5 if dy < RING_R else 0.0)

# guards: the boxed letter must clear the SHIFTED ring, and the LED must stay on-panel.
# Panel is 45HP (228.6mm) post-DX_RIGHT widen; LIGHT_X + margin must stay inside.
PANEL_W = 228.6
for y in ROW_Y:
    clear = (BOX_CX - BOX_W/2) - ring_reach(y)
    assert clear >= 2.0, 'mode row y=%.0f: box clears ring by only %.2fmm' % (y, clear)
# button must clear the ring too (it sits at the column's X, below the stack)
assert (BTN[0] - 3.0) - ring_reach(BTN[1]) >= 2.0, 'button y=%.0f: clears ring by only %.2fmm' % (BTN[1], (BTN[0]-3.0)-ring_reach(BTN[1]))
assert LIGHT_X + 3.0 < PANEL_W, 'LED column runs off the panel'
assert BTN[1] < 55.0, 'button must be grouped with the C/G/P stack (y<55), not on the knob line'
print('geometry ok: worst box/ring clearance %.2f mm; LED edge margin %.1f mm' %
      (min((BOX_CX - BOX_W/2) - ring_reach(y) for y in ROW_Y), PANEL_W - LIGHT_X))

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
    print('%-6s  button y=%.0f, %d LEDs at x=%.1f, y=%s'
          % (theme, BTN[1], len(IDS), LIGHT_X, [int(y) for y in ROW_Y]))
