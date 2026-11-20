#!/usr/bin/env python3
"""Woodlands — MPE MIDI IN -> poly CV utility (the mirror of Keppel), Tier 2 panel.

[WORKING NAME] Woodlands is the inbound land checkpoint — the arrival gateway into Singapore, mirroring
Keppel Harbour (the outbound port Keppel ships the microtonal performance OUT through). Rename before
release if a better inbound name fits; the slug is set in src/Woodlands.cpp + plugin.json.

Receives MPE (lower zone) and emits, per voice: true (microtonal) pitch V/OCT with the member bend folded
in at the set range, GATE, VEL, R-VEL (release velocity), Y (CC74 timbre), Z (channel pressure), and a
standalone X (raw bend, semitones). Closes the gaps in stock VCV MIDI-CV MPE-in (no native CC74, split
V/OCT+bend, no release velocity). Shares the MpeMath value<->CV conventions with Keppel, so a Keppel->
Woodlands loop is identity.

GEOMETRY SOURCE OF TRUTH (see panel_src/README.md): this generator is the SINGLE geometry source. It
emits a decoration layer (visible wells/knobs, no ids) and a `<g inkscape:label="components">` layer of
near-invisible anchor markers the SvgPanelKit binds by name. The widget places NOTHING with mm2px. The
anchor-vs-bind audit (test/audit_anchor_bind.py) enforces 1:1 anchor<->bind.

Anchors (components layer): wordmark, param_bendrange, midi_display, light_active,
output_pitch, output_gate, output_vel, output_rvel, output_y, output_z, output_x,
label_bendrange, label_mpein.
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
MIDI_Y     = 25.0             # top of the MIDI display well (INPUT device)
MIDI_H     = 17.0
ACTIVE_Y   = MIDI_Y + MIDI_H + 4.5
# Two columns of output jacks, spread to use the 14HP width.
COL_L      = CX - 17.0
COL_R      = CX + 17.0
JR         = 3.8              # jack radius (mm) — visual wells
# Three output rows of pairs, grouped by dimension; X sits centred below.
JACK_ROWS = [
    (60.0, [(COL_L, "output_pitch"), (COL_R, "output_gate")]),
    (74.0, [(COL_L, "output_vel"),   (COL_R, "output_rvel")]),
    (88.0, [(COL_L, "output_y"),     (COL_R, "output_z")]),
]
X_Y        = 102.0           # standalone X (raw bend) output, centred
LABEL_MPEIN_Y = 125.0        # "MPE IN →" label anchor, below the X output
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
    # MIDI input device well (widget draws the MidiDisplay on the midi_display anchor).
    A(f'<rect x="{px(MIDI_X)}" y="{px(MIDI_Y)}" width="{px(MIDI_W)}" height="{px(MIDI_H)}" rx="{px(1.0)}" fill="{t["midiwell"]}" stroke="{t["ring"]}" stroke-width="0.5"/>')
    # Active indicator well.
    A(f'<circle cx="{px(CX)}" cy="{px(ACTIVE_Y)}" r="{px(1.6)}" fill="{t["well"]}" stroke="{t["ring"]}" stroke-width="0.3"/>')
    # Output jacks — visible wells (no id; the markers are in the components layer below).
    for (yy, cols) in JACK_ROWS:
        for (xx, _mid) in cols:
            A(f'<circle cx="{px(xx)}" cy="{px(yy)}" r="{px(JR)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.5"/>')
    # X (raw bend) output well, centred.
    A(f'<circle cx="{px(CX)}" cy="{px(X_Y)}" r="{px(JR)}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="0.5"/>')

    # ── SvgPanelKit component (anchor) layer — THE SINGLE GEOMETRY SOURCE. ────────
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
    mark("output_x", CX, X_Y)
    # Standalone label anchors (labels that do NOT sit at a fixed dy above a jack).
    mark("label_bendrange", CX, LABEL_BENDRANGE_Y)
    mark("label_mpein", CX, LABEL_MPEIN_Y)
    C('</g>')

    A("\n".join(c))
    A('</svg>')
    return "\n".join(d)

def main():
    import os
    out = os.path.join(os.path.dirname(__file__), "..", "res", "panels")
    for dark, name in [(True, "Woodlands_panel_dark.svg"), (False, "Woodlands_panel_light.svg")]:
        with open(os.path.join(out, name), "w") as fh:
            fh.write(gen(dark))
        print(f"Woodlands {'dark' if dark else 'light'}: res/panels/{name}  ({HP}HP, {PW}x{PH}px)")

if __name__ == "__main__":
    main()
