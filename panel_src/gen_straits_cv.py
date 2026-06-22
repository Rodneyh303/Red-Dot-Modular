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
                    group="#13151a", groupline="#262b33", motif="#2e333c", motifwave="#333944")
    return dict(bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", accent="#c0202a",
                jackwell="#dadce0", jackring="#9298a0",
                well="#d4d6d9", wellring="#a8aeb6",
                group="#dfe0e3", groupline="#c8ccd2", motif="#d8d9dc", motifwave="#d2d4d8")

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

def esplanade(x, y, w, h, t, op=0.16):
    # The improved Esplanade durian-dome concept (ported from
    # panel_src/art_refs/esplanade_dome_nanosvg.svg): a perspective faceted dome
    # shell (longitudinal ribs as ellipse arcs + latitudinal bands as Q-curves +
    # a faint diamond facet grid), the dome outline, and the truss understructure
    # + platform. Reference viewBox 0..1000 x 0..450 mapped into (x,y,w,h). The
    # original's black "sky-frame" mask paths are recoloured to the panel bg so
    # the rib spill above the dome curve is hidden on any background. nanosvg-safe
    # (no gradients/masks/clipPath — the curve-following frame does the clipping).
    RX1, RY1 = 1000.0, 450.0
    def MX(rx): return x + rx / RX1 * w
    def MY(ry): return y + ry / RY1 * h
    def MP(d):
        # scale a path's numbers — only used for the simple L/Q/C/M dome paths
        # below where we emit coordinates directly via MX/MY, so not needed.
        return d
    fill = t["motif"]; ink = t["ink"]; bg = t["bg"]
    L = []; A = L.append

    # longitudinal latitude arcs (the dome's horizontal banding)
    A(f'<g fill="none" stroke="{fill}" stroke-width="1.0" opacity="{op*3:.3f}" stroke-linecap="round">')
    for (mx0,my0,qx,qy,ex,ey) in [
        (35,340,420,240,965,250),(40,300,420,205,950,225),(45,265,415,175,925,200),
        (55,230,410,145,890,178),(70,195,400,115,845,155),(90,160,380,88,790,138),
        (120,130,350,68,720,125),(155,102,320,52,650,118)]:
        A(f'<path d="M{MX(mx0):.1f} {MY(my0):.1f} Q{MX(qx):.1f} {MY(qy):.1f} {MX(ex):.1f} {MY(ey):.1f}"/>')
    A('</g>')
    # longitudinal ribs (vertical ellipse slices)
    A(f'<g fill="none" stroke="{fill}" stroke-width="1.4" opacity="{op*3.5:.3f}">')
    ribs = [(-40,140),(0,145),(40,150),(80,155),(120,160),(160,165),(200,170),(240,175),
            (280,180),(320,185),(360,190),(420,180),(470,170),(520,160),(570,150),(620,140),
            (670,130),(720,120),(770,110),(820,100),(870,90),(920,80),(960,70)]
    for (cx,rx) in ribs:
        A(f'<ellipse cx="{MX(cx):.1f}" cy="{MY(380):.1f}" rx="{rx/RX1*w:.1f}" ry="{370/RY1*h:.1f}"/>')
    A('</g>')

    # sky-frame: fill above/outside the dome curve with the panel bg so rib/band
    # spill is hidden, following the dome curve exactly (replaces #000 in the ref).
    A(f'<path d="M{MX(0):.1f} {MY(0):.1f} L{MX(1000):.1f} {MY(0):.1f} L{MX(1000):.1f} {MY(250):.1f} '
      f'C{MX(900):.1f} {MY(125):.1f} {MX(720):.1f} {MY(45):.1f} {MX(500):.1f} {MY(42):.1f} '
      f'C{MX(240):.1f} {MY(45):.1f} {MX(40):.1f} {MY(120):.1f} {MX(35):.1f} {MY(340):.1f} '
      f'L{MX(0):.1f} {MY(340):.1f} Z" fill="{bg}"/>')
    A(f'<path d="M{MX(0):.1f} {MY(343):.1f} L{MX(1000):.1f} {MY(343):.1f} L{MX(1000):.1f} {MY(450):.1f} L{MX(0):.1f} {MY(450):.1f} Z" fill="{bg}"/>')
    A(f'<path d="M{MX(965):.1f} {MY(250):.1f} L{MX(1000):.1f} {MY(250):.1f} L{MX(1000):.1f} {MY(343):.1f} L{MX(965):.1f} {MY(343):.1f} Z" fill="{bg}"/>')

    # dome outline + truss understructure (the bright metal edges)
    A(f'<path d="M{MX(35):.1f} {MY(340):.1f} C{MX(40):.1f} {MY(120):.1f} {MX(240):.1f} {MY(45):.1f} {MX(500):.1f} {MY(42):.1f} '
      f'C{MX(720):.1f} {MY(45):.1f} {MX(900):.1f} {MY(125):.1f} {MX(965):.1f} {MY(250):.1f}" '
      f'fill="none" stroke="{ink}" stroke-width="2.0" opacity="{op*4:.3f}" stroke-linecap="round"/>')
    A(f'<polyline points="{MX(40):.1f},{MY(403):.1f} {MX(130):.1f},{MY(340):.1f} {MX(250):.1f},{MY(392):.1f} '
      f'{MX(395):.1f},{MY(385):.1f} {MX(540):.1f},{MY(318):.1f} {MX(675):.1f},{MY(370):.1f} {MX(760):.1f},{MY(285):.1f}" '
      f'fill="none" stroke="{ink}" stroke-width="1.5" opacity="{op*3:.3f}" stroke-linejoin="round" stroke-linecap="round"/>')
    return "".join(L)


def mbs(x, y, w, h, t, op=0.16):
    # Rich Esplanade / Marina Bay motif (ported from gen_east_clean so the CV
    # expander panels match the visual panels). Self-contained reference box
    # 90..630 x 150..500 mapped into (x,y,w,h). All raw px, nanosvg-safe.
    fill=t["motif"]; acc=t["accent"]; bright=t["ink"]
    big=(h>55)
    RX0,RX1,RY0,RY1 = 90.0,630.0,150.0,500.0
    def MX(rx): return x+(rx-RX0)/(RX1-RX0)*w
    def MY(ry): return y+(ry-RY0)/(RY1-RY0)*h
    L=[]; A=L.append
    lean=[(210,500,260,400,280,190,295,190,270,400,250,500),
          (340,500,390,400,410,190,425,190,400,400,380,500),
          (470,500,520,400,540,190,555,190,530,400,510,500)]
    for (a1,a2,b1,b2,c1,c2,d1,d2,e1,e2,f1,f2) in lean:
        A(f'<path d="M {MX(a1):.1f} {MY(a2):.1f} Q {MX(b1):.1f} {MY(b2):.1f} {MX(c1):.1f} {MY(c2):.1f} '
          f'L {MX(d1):.1f} {MY(d2):.1f} Q {MX(e1):.1f} {MY(e2):.1f} {MX(f1):.1f} {MY(f2):.1f} Z" '
          f'fill="{fill}" fill-opacity="{op}"/>')
    for (a1,a2,b1,b2,c1,c2,d1,d2) in [(290,500,305,190,330,190,330,500),(420,500,435,190,460,190,460,500),(550,500,565,190,590,190,590,500)]:
        A(f'<polygon points="{MX(a1):.1f},{MY(a2):.1f} {MX(b1):.1f},{MY(b2):.1f} {MX(c1):.1f},{MY(c2):.1f} {MX(d1):.1f},{MY(d2):.1f}" fill="{fill}" fill-opacity="{op}"/>')
    if big:
        for (sx0,sx1) in [(290,330),(420,460),(550,590)]:
            for f_ in (0.18,0.34,0.50,0.66,0.82):
                ry=190+(500-190)*f_
                A(f'<line x1="{MX(sx0):.1f}" y1="{MY(ry):.1f}" x2="{MX(sx1):.1f}" y2="{MY(ry):.1f}" stroke="{acc}" stroke-width="0.4" stroke-opacity="{op*0.3:.3f}"/>')
    A(f'<path d="M {MX(90):.1f} {MY(150):.1f} Q {MX(365):.1f} {MY(160):.1f} {MX(630):.1f} {MY(150):.1f} '
      f'C {MX(640):.1f} {MY(150):.1f} {MX(640):.1f} {MY(190):.1f} {MX(620):.1f} {MY(190):.1f} '
      f'L {MX(280):.1f} {MY(190):.1f} Q {MX(160):.1f} {MY(190):.1f} {MX(90):.1f} {MY(150):.1f} Z" fill="{fill}" fill-opacity="{op}"/>')
    A(f'<path d="M {MX(90):.1f} {MY(150):.1f} Q {MX(365):.1f} {MY(160):.1f} {MX(630):.1f} {MY(150):.1f}" fill="none" stroke="{bright}" stroke-width="0.8" stroke-opacity="{op*0.7:.3f}" stroke-linecap="round"/>')
    return "".join(L)

def waves(x, y, t, op=0.6, rows=3, span=360.0):
    # Marina Bay water — ported from gen_east_clean, raw px. Faint wave rows.
    col=t["motifwave"]; L=[]; A=L.append
    for i in range(rows):
        yb=y+i*9.4; amp=4.7; seg=span/8.0
        d=f'M {x:.1f} {yb:.1f} '
        for k in range(8):
            cx1=x+seg*k+seg*0.25; cx2=x+seg*k+seg*0.75; ex=x+seg*(k+1)
            ud=-amp if k%2==0 else amp
            d+=f'C {cx1:.1f} {yb+ud:.1f} {cx2:.1f} {yb+ud:.1f} {ex:.1f} {yb:.1f} '
        A(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.5" stroke-opacity="{op}"/>')
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

    # background motif: the improved Esplanade durian-dome watermark (lower-centre)
    # + Marina Bay water (waves) near the footer, matching the visual aesthetic.
    A(esplanade(PW*0.10, 232.0, PW*0.80, 92.0, t, op=0.13))
    A(waves(15.0, 338.0, t, op=0.5, rows=3, span=PW-30.0))

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
    # dot.modular connect mark anchor (footer-centre; reposition here).
    A(kit_marker("light", "connect", PW*0.5, PH-20.0))
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
