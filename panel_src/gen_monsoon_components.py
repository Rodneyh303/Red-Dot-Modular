"""
Generate Monsoon panel with a VCV-helper.py-compatible 'components' layer.
helper.py colour convention (createmodule --panel):
  params  : #ff0000 (red)
  inputs  : #00ff00 (green)
  outputs : #0000ff (blue)
  lights  : #00ffff (cyan) — actually helper uses specific; we use a 'lights' sublayer
  custom widgets: yellow #ffff00
Circle CENTER = component position. Radius arbitrary (helper reads cx,cy).
Coordinates are in mm; SVG authored at scale S so 1 user-unit = 1px, viewBox in px.
helper.py reads positions in mm via the 'mm' unit OR via px→mm using the document.
We'll author the components layer in MM units (Inkscape document is mm).
"""
S = 3.7795
W_MM, H_MM = 203.2, 128.5

# ── Component inventory (id_name, x_mm, y_mm) ──
# Order MUST match enum order in Monsoon.hpp for helper.py to map ids correctly.
params = [
    # semitone sliders SEMI0..SEMI11
    *[(f"SEMI{i}_PARAM", 7.5 + i*9.0, 59.75) for i in range(12)],
    ("OCT_LO_PARAM", 119.0, 59.75),
    ("OCT_HI_PARAM", 128.0, 59.75),
    ("MODE_PARAM", 197.0, 12.0),
    ("DICE_R_PARAM", 118.0, 87.0),
    ("DICE_M_PARAM", 133.0, 87.0),
    ("LOCK_PARAM", 148.0, 87.0),
    ("MUTE_PARAM", 163.0, 87.0),
    ("RESET_BUTTON_PARAM", 178.0, 87.0),
    ("RUN_GATE_PARAM", 193.0, 87.0),
    # DNA knobs
    ("NOTE_VALUE_PARAM", 16.0, 22.0),
    ("VARIATION_PARAM", 42.0, 22.0),
    ("LEGATO_PARAM", 68.0, 22.0),
    ("REST_PARAM", 94.0, 22.0),
    ("ACCENT_KNOB", 120.0, 22.0),
    ("BPM_PARAM", 148.0, 58.0),
    ("PATTERN_LENGTH_PARAM", 163.0, 58.0),
    ("PATTERN_OFFSET_PARAM", 178.0, 58.0),
]
inputs = [
    ("RUN_GATE_INPUT", 16.0, 105.0), ("RESET_TRIGGER_INPUT", 30.0, 105.0),
    ("SEED_INPUT", 48.0, 105.0), ("LENGTH_INPUT", 66.0, 105.0), ("OFFSET_INPUT", 84.0, 105.0),
    ("CLK_INPUT", 16.0, 120.0), ("GATE1_INPUT", 30.0, 120.0), ("GATE2_INPUT", 48.0, 120.0),
    ("CV1_INPUT", 66.0, 120.0), ("CV2_INPUT", 84.0, 120.0),
]
outputs = [
    ("GATE_OUTPUT", 104.0, 105.0), ("TIE_OUTPUT", 122.0, 105.0), ("LEGATO_OUTPUT", 140.0, 105.0),
    ("TIE_OR_LEGATO_OUTPUT", 158.0, 105.0), ("ACCENT_OUTPUT", 176.0, 105.0),
    ("CV_OUTPUT", 104.0, 120.0), ("SEED_OUTPUT", 122.0, 120.0), ("RUN_GATE_OUTPUT", 140.0, 120.0),
    ("RESET_TRIGGER_OUTPUT", 158.0, 120.0),
]
# lights: step ring (16), mode (4), action lights (6), expander (3), slider LEDs handled in-param
import math
RCX,RCY,RLED = 162.0, 30.0, 14.0
ring = []
for i in range(16):
    ang = i/16*2*math.pi - math.pi/2
    ring.append((f"STEP_LIGHT_{i}", RCX+RLED*math.cos(ang), RCY+RLED*math.sin(ang)))
lights = [
    *ring,
    *[(f"MODE_A_LIGHT_{i}", 192.0, 34.0+i*8.0) for i in range(4)],
    ("RHYTHM_DICE_LIGHT", 118.0, 93.0), ("MELODY_DICE_LIGHT", 133.0, 93.0),
    ("LOCK_LIGHT", 148.0, 93.0), ("MUTE_LIGHT", 163.0, 93.0),
    ("RESET_LIGHT", 178.0, 93.0), ("RUN_GATE_LIGHT", 193.0, 93.0),
    ("SCALE_EXPANDER_LIGHT", 4.0, 4.0), ("DNA_EXPANDER_LIGHT", 9.0, 4.0),
    ("POLY_EXPANDER_LIGHT", 14.0, 4.0),
]

def circles(items, color, r=1.0):
    out = []
    for name,x,y in items:
        out.append(f'    <circle id="{name}" cx="{x:.3f}" cy="{y:.3f}" r="{r}" '
                   f'style="fill:{color};stroke:none"/>')
    return "\n".join(out)

# Build components layer in MM (Inkscape mm document)
comp_layer = f'''  <g inkscape:groupmode="layer" inkscape:label="components" id="components"
     style="display:inline">
    <!-- params (red) -->
{circles(params, "#ff0000")}
    <!-- inputs (green) -->
{circles(inputs, "#00ff00")}
    <!-- outputs (blue) -->
{circles(outputs, "#0000ff")}
    <!-- lights (cyan) -->
{circles(lights, "#00ffff", r=0.6)}
  </g>'''

# Emit a components-only overlay SVG in MM units (for helper.py).
svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
     width="{W_MM}mm" height="{H_MM}mm" viewBox="0 0 {W_MM} {H_MM}">
  <g inkscape:groupmode="layer" inkscape:label="panel" id="panel"
     style="display:inline">
    <rect width="{W_MM}" height="{H_MM}" fill="#232326"/>
  </g>
{comp_layer}
</svg>'''

with open("panel_src/Monsoon_components.svg","w") as f: f.write(svg)
print(f"Monsoon components: {len(params)} params, {len(inputs)} inputs, "
      f"{len(outputs)} outputs, {len(lights)} lights")
print("Written panel_src/Monsoon_components.svg (MM units, for helper.py)")
