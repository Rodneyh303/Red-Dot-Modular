#!/usr/bin/env python3
"""Keppel — CV → MPE-out utility (MPE_UTILITY_BUILD_SPEC), Tier 2 panel.

Poly microtonal pitch CV + poly gate IN → MPE MIDI OUT: each voice = nearest-12-TET note + per-note
pitch bend on its own MPE member channel, PLUS the two-layer X/Y/Z expression model (A main-gated base
+ B accent-gated additive) and a within-legato (step) gate. Standalone utility, zero engine coupling.
Named for Keppel Harbour — the outbound port that carries the microtonal performance OUT to MPE gear.

GEOMETRY SOURCE OF TRUTH (see docs/design/MONSOON_PANEL_REVERSE_ENGINEER.md, panel_src/README.md):
this generator is the SINGLE geometry source. It emits a decoration layer (visible wells/knobs, no ids)
and a `<g inkscape:label="components">` layer of near-invisible anchor markers the SvgPanelKit binds by
name. The widget places NOTHING with mm2px — every control is bindInput/bindParam/... by anchor name, and
every label is drawn at centerOf(findNamed(anchor)) (+ a fixed dy for jack labels). The anchor-vs-bind
audit (test/audit_anchor_bind.py) enforces 1:1 anchor↔bind, so "a control/label is misplaced" is a
build-time failure, not a hover-discovery.

Tier 2 widening (8HP → 14HP): the 8 new jacks (X_A/X_B, Y_A/Y_B, Z_A/Z_B, VEL_B, STEP_GATE) plus the
reverse-calc monitor output need room. Jacks are grouped by dimension as [A][B] pairs in two columns,
six rows: PITCH|GATE, ACCENT|STEP, X-A|X-B, Y-A|Y-B, Z-A|Z-B, VEL|VEL-B; the monitor sits centred below.

Anchors (components layer): wordmark, param_bendrange, midi_display, light_active,
input_pitch, input_gate, input_accent, input_vel,
input_x_a, input_x_b, input_y_a, input_y_b, input_z_a, input_z_b, input_vel_b, input_step_gate,
output_monitor, label_bendrange, label_mpeout.
(Jack labels are NOT separate anchors — draw() derives each from its jack anchor + a fixed dy.)
"""
HP = 14
W  = HP * 5.08          # 71.12 mm
H  = 128.5
S  = 75 / 25.4          # px per mm (== Rack mm2px scale: 15/5.08)
PW, PH = round(W*S, 2), round(H*S, 2)
def px(v): return round(v*S, 2)

CX         = W / 2.0
WORDMARK_Y = 7.0
BEND_Y     = 16.0
LABEL_BENDRANGE_Y = 21.0      # "bend range" label anchor, below the knob
MIDI_Y     = 25.0             # top of the MIDI display well
MIDI_H     = 17.0
ACTIVE_Y   = MIDI_Y + MIDI_H + 4.5
# Two columns of jacks, [A] left / [B] right, spread to use the 14HP width.
COL_L      = CX - 17.0
COL_R      = CX + 17.0
JR         = 3.8              # jack radius (mm) — visual wells
# Six jack rows, grouped by dimension. (row_y_mm, [(col_x, marker_id), ...])
JACK_ROWS = [
    (54.0, [(COL_L, "input_pitch"),     (COL_R, "input_gate")]),
    (65.0, [(COL_L, "input_accent"),    (COL_R, "input_step_gate")]),
    (76.0, [(COL_L, "input_x_a"),       (COL_R, "input_x_b")]),
    (87.0, [(COL_L, "input_y_a"),       (COL_R, "input_y_b")]),
    (98.0, [(COL_L, "input_z_a"),       (COL_R, "input_z_b")]),
    (109.0,[(COL_L, "input_vel"),       (COL_R, "input_vel_b")]),
]
MON_Y      = 119.0            # reverse-calc monitor output, centred
LABEL_MPEOUT_Y = 125.0        # "→ MPE OUT" label anchor, below the monitor
MIDI_X, MIDI_W = 8.0, W - 2*8.0

THEMES = {
    "dark":  dict(bg="#16181c", red="#d4001a", ink="#f0f0f0",
                  well="#0f1114", ring="#4a4a4a", knob="#2a2e34", knobring="#5a616a",
                  jackwell="#0c0e11", jackring="#4a4a4a", midiwell="#0a0c0e"),
    "light": dict(bg="#dcdcdc", red="#d4001a", ink="#1a1a1a",
                  well="#e2ddd2", ring="#b0a898", knob="#c8cdd4", knobring="#9aa2ac",
                  jackwell="#e2ddd2", jackring="#b0a898", midiwell="#e8e2d6"),
}

def gen(dark):
    t = THEMES["dark" if dark else "light"]
    d = []  # decoration (visible, no ids)
    A = d.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">')
    A(f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW}" height="{px(1.2)}" fill="{t["red"]}"/>')
    # BEND RANGE knob (top).
    A(f'<circle cx="{px(CX)}" cy="{px(BEND_Y)}" r="{px(5.2)}" fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.5"/>')
    A(f'<circle cx="{px(CX)}" cy="{px(BEND_Y)}" r="{px(4.4)}" fill="{t["knob"]}" stroke="{t["knobring"]}" stroke-width="0.6"/>')
    A(f'<line x1="{px(CX)}" y1="{px(BEND_Y-4.2)}" x2="{px(CX)}" y2="{px(BEND_Y-2.4)}" stroke="{t["ink"]}" stroke-width="0.6"/>')
    # MIDI display well (widget draws the MidiDisplay on the midi_display anchor).
    A(f'<rect x="{px(MIDI_X)}" y="{px(MIDI_Y)}" width="{px(MIDI_W)}" height="{px(MIDI_H)}" rx="{px(1.0)}" fill="{t["midiwell"]}" stroke="{t["ring"]}" stroke-width="0.5"/>')
    # Active indicator well.
    A(f'<circle cx="{px(CX)}" cy="{px(ACTIVE_Y)}" r="{px(1.6)}" fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.3"/>')
    # Poly input jacks — visible wells (no id; the markers are in the components layer below).
    for (yy, cols) in JACK_ROWS:
        for (xx, _mid) in cols:
            A(f'<circle cx="{px(xx)}" cy="{px(yy)}" r="{px(JR)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.5"/>')
    # Monitor output well.
    A(f'<circle cx="{px(CX)}" cy="{px(MON_Y)}" r="{px(JR)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.5"/>')

    # ── SvgPanelKit component (anchor) layer — THE SINGLE GEOMETRY SOURCE. ────────
    # Near-invisible markers (r=0.5, fill none) the widget binds/consumes by name. Every anchor here
    # MUST be bound or findNamed'd in the widget (the audit enforces 1:1).
    c = []  # components/anchors
    C = c.append
    C('<g inkscape:label="components" inkscape:groupmode="layer">')
    def mark(name, x, y):
        C(f'<circle id="{name}" cx="{px(x):.2f}" cy="{px(y):.2f}" r="0.5" fill="none" stroke="none"/>')
    mark("wordmark", CX, WORDMARK_Y)
    mark("param_bendrange", CX, BEND_Y)
    # midi_display marker carries the well's full rect so boundsOf() sizes the MidiDisplay.
    C(f'<rect id="midi_display" x="{px(MIDI_X):.2f}" y="{px(MIDI_Y):.2f}" width="{px(MIDI_W):.2f}" height="{px(MIDI_H):.2f}" fill="none" stroke="none"/>')
    mark("light_active", CX, ACTIVE_Y)
    for (yy, cols) in JACK_ROWS:
        for (xx, mid) in cols:
            mark(mid, xx, yy)
    mark("output_monitor", CX, MON_Y)
    # Standalone label anchors (labels that do NOT sit at a fixed dy above a jack).
    mark("label_bendrange", CX, LABEL_BENDRANGE_Y)
    mark("label_mpeout", CX, LABEL_MPEOUT_Y)
    C('</g>')

    A("\n".join(c))
    A('</svg>')
    return "\n".join(d)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Keppel_panel_dark.svg"), (False, "Keppel_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Keppel {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
