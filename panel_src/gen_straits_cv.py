"""Straits East / West CV expander panels (dot.modular).

Single source of truth for the East (voices 1-8) and West (voices 9-16) poly CV
expander panels. Emits the peranakan-family art treatment PLUS an SVG-kit
component layer (near-invisible id markers at every control centre) so the module
widgets can bind by id via SvgPanelKit instead of hardcoded coordinates.

Native panel coordinates are PX at 75 DPI (the existing CV panels are 390x380 px;
the widgets place controls in px via mm2pxl=2.9528). We therefore author directly
in px to match the widget layout exactly.

nanosvg-safe: no <mask>, no gradients, no url(#...), no <text> for controls,
no fill-rule=evenodd.

Usage:
    python panel_src/gen_straits_cv.py            # builds east+west, plain dark+light
Design notes:
    - `variant` selects the art treatment (plain/red/grey). Markers + control
      geometry are identical across variants, so any variant is kit-compatible.
    - East and West share geometry; they differ only in title and in the absolute
      param/input/output base indices the kit markers carry.
"""
import math

# ── panel + grid geometry (PX @ 75 DPI, matching the widget) ──────────────────
PW, PH = 390.0, 380.0
MM2PX = 2.9528

# control column X (px) — from the East/West widgets (knob/att/modIn + 3 outs)
COL = dict(knob=48.0, att=102.0, modIn=168.0, gateOut=234.0, cvOut=288.0, accOut=342.0)
START_Y = 50.0
SPACING_Y = 35.0
# East adds voices 2-8 (7 rows; voice 1 is Monsoon's own mono voice). West adds
# voices 9-16 (8 rows). The global utility row sits one spacing below the last.
N_ROWS = dict(east=7, west=8)
def global_y(side): return START_Y + N_ROWS[side] * SPACING_Y + 5.0

# ── absolute param/input/output indices (MonsoonIds order) ────────────────────
# These mirror exactly what the East/West widgets bind. The kit markers carry the
# ABSOLUTE index so bindParam("param_<idx>", idx) lines up. We don't need the enum
# values here — the marker id is the index, and the module passes the same enum
# constant as the int id (their numeric values are equal by construction).
# Per voice row i (0..7):
#   param : knob = POLY_REST_PARAM_base + i ; att = POLY_REST_MOD_ATT_base + i
#   input : modIn = POLY_REST_MOD_CV_INPUT_base + i
#   output: gate = POLY_GATE_OUT_1 + i ; cv = POLY_CV_OUT_1 + i ; acc = POLY_ACCENT_OUT_1 + i
# We emit markers keyed by KIND + a stable LABEL, and the widget binds by that
# label. Using labels (not raw ints) keeps the SVG readable and lets the widget
# map label->enum explicitly. Label scheme: "<kind>_<role>_<row>".

def theme(dark):
    if dark:
        return dict(bg="#16181c", ink="#d8d8dc", dim="#8a8f98", accent="#dc2626",
                    jackwell="#0b0d10", jackring="#4a5058",
                    well="#0f1114", wellring="#3a4048",
                    group="#13151a", groupline="#262b33", motif="#2e333c")
    return dict(bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", accent="#c0202a",
                jackwell="#dadce0", jackring="#9298a0",
                well="#d4d6d9", wellring="#a8aeb6",
                group="#dfe0e3", groupline="#c8ccd2", motif="#d8d9dc")

def jackwell(x, y, t, r=9.0):
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{t["jackwell"]}" '
            f'stroke="{t["jackring"]}" stroke-width="1.2"/>')

def trimwell(x, y, t, r=8.0):
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{t["well"]}" '
            f'stroke="{t["wellring"]}" stroke-width="1.2"/>')

def kit_marker(kind, label, x, y):
    # near-invisible bind anchor at the control centre; widget binds by id.
    return (f'<circle id="{kind}_{label}" cx="{x:.2f}" cy="{y:.2f}" r="0.5" '
            f'fill="none" stroke="none"/>')

def mbs_motif(t, op=0.5):
    # Simple Marina Bay Sands dome arc motif, lower-centre. nanosvg-safe arcs via
    # quadratic-bezier polylines. Kept faint as a background watermark.
    cx, base, halfw, h = PW * 0.5, 250.0, 70.0, 46.0
    def qbez(x0, y0, qx, qy, x1, y1, n=22):
        pts = []
        for k in range(n + 1):
            tt = k / n
            mx = (1-tt)**2*x0 + 2*(1-tt)*tt*qx + tt*tt*x1
            my = (1-tt)**2*y0 + 2*(1-tt)*tt*qy + tt*tt*y1
            pts.append(f"{mx:.1f},{my:.1f}")
        return " ".join(pts)
    dome = qbez(cx-halfw, base, cx, base-h, cx+halfw, base)
    L = [f'<g opacity="{op}" stroke="{t["motif"]}" stroke-width="1.2" fill="none">']
    L.append(f'<polyline points="{dome}"/>')
    # a few vertical ribs
    for fx in (0.28, 0.5, 0.72):
        x = cx - halfw + 2*halfw*fx
        ytop = base - h * (1 - (2*fx-1)**2) * 0.92
        L.append(f'<line x1="{x:.1f}" y1="{base:.1f}" x2="{x:.1f}" y2="{ytop:.1f}"/>')
    L.append('</g>')
    return "".join(L)

def gen(side, dark, variant="plain"):
    """side: 'east' or 'west'. Returns SVG text."""
    t = theme(dark)
    title = "STRAITS EAST" if side == "east" else "STRAITS WEST"
    sub   = "VOICES 1-8" if side == "east" else "VOICES 9-16"
    if variant == "red":
        t = dict(t); t["motif"] = t["accent"]
    elif variant == "grey":
        t = dict(t); t["motif"] = "#5a5e66"

    L = []; A = L.append
    A(f'<svg xmlns="http://www.w3.org/2000/svg" width="{PW:.0f}" height="{PH:.0f}" '
      f'viewBox="0 0 {PW:.0f} {PH:.0f}">')
    A(f'<rect x="0" y="0" width="{PW:.0f}" height="{PH:.0f}" fill="{t["bg"]}"/>')
    A(f'<rect x="0" y="0" width="{PW:.0f}" height="4" fill="{t["accent"]}"/>')   # top rule
    A(f'<circle cx="{PW-14:.0f}" cy="14" r="5" fill="{t["accent"]}"/>')          # corner dot

    # background motif
    A(mbs_motif(t))

    # title (display text — NOT a control, so text is fine here)
    A(f'<text x="14" y="30" fill="{t["ink"]}" font-family="sans-serif" '
      f'font-size="13" font-weight="700">{title}</text>')
    A(f'<text x="14" y="44" fill="{t["dim"]}" font-family="sans-serif" '
      f'font-size="9">{sub}</text>')

    # ── visible control art (wells/rings) ─────────────────────────────────────
    A('<g>')
    nrows = N_ROWS[side]
    gy = global_y(side)
    for i in range(nrows):
        y = START_Y + i * SPACING_Y
        A(trimwell(COL["knob"], y, t))
        A(trimwell(COL["att"], y, t))
        A(jackwell(COL["modIn"], y, t))
        A(jackwell(COL["gateOut"], y, t))
        A(jackwell(COL["cvOut"], y, t))
        A(jackwell(COL["accOut"], y, t))
    # global row
    if side == "east":
        A(jackwell(COL["modIn"], gy, t))
        A(jackwell(COL["gateOut"], gy, t))
        A(jackwell(COL["cvOut"], gy, t))
    else:
        A(jackwell(COL["knob"], gy, t))
        A(jackwell(COL["gateOut"], gy, t))
        A(jackwell(COL["cvOut"], gy, t))
    A('</g>')

    # ── SVG-kit component layer (bind anchors; near-invisible) ────────────────
    # label scheme: param_knob_<row>, param_att_<row>, input_modcv_<row>,
    # output_gate_<row>, output_cv_<row>, output_acc_<row>; plus the global row.
    A('<g id="components">')
    for i in range(nrows):
        y = START_Y + i * SPACING_Y
        A(kit_marker("param", f"knob_{i}", COL["knob"], y))
        A(kit_marker("param", f"att_{i}",  COL["att"], y))
        A(kit_marker("input", f"modcv_{i}", COL["modIn"], y))
        A(kit_marker("output", f"gate_{i}", COL["gateOut"], y))
        A(kit_marker("output", f"cv_{i}",   COL["cvOut"], y))
        A(kit_marker("output", f"acc_{i}",  COL["accOut"], y))
    if side == "east":
        A(kit_marker("input",  "global_modcv", COL["modIn"], gy))
        A(kit_marker("output", "global_gate",  COL["gateOut"], gy))
        A(kit_marker("output", "global_cv",    COL["cvOut"], gy))
    else:
        A(kit_marker("input",  "global_cv_in", COL["knob"], gy))
        A(kit_marker("output", "global_gate",  COL["gateOut"], gy))
        A(kit_marker("output", "global_cv",    COL["cvOut"], gy))
    A('</g>')

    A('</svg>')
    return "\n".join(L)


if __name__ == "__main__":
    import os
    out = "res/panels"
    targets = [
        ("east",  True,  "straits_east_peranakan_dark.svg"),
        ("east",  False, "straits_east_peranakan_light.svg"),
        ("west",  True,  "straits_west_peranakan_dark.svg"),
        ("west",  False, "straits_west_peranakan_light.svg"),
    ]
    for side, dark, name in targets:
        svg = gen(side, dark, variant="plain")
        with open(os.path.join(out, name), "w") as f:
            f.write(svg)
        print("wrote", name)
