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
# Accent promoted to a poly lane: 3 new columns (accent probability knob, accent CV in,
# accent CV attenuverter) added beside the existing per-voice accent OUT. Column spacing
# tightened from the old generous ~54px to 43px (wells are ~18-20px, so still a clean gap),
# so all 9 columns fit in 28HP — only +2HP vs the old 26HP 6-column panel.
PW, PH = 420.0, 380.0   # 28HP (was 26HP/390px); +2HP absorbs the 3 accent columns
MM2PX = 2.9528

# 9 control columns @ 43px. Rest lane: knob/att/modIn. Voice outs: gate/cv. Accent lane:
# accKnob/accAtt/accModIn grouped just left of the existing accent OUT.
COL = dict(knob=38.0, att=81.0, modIn=124.0, gateOut=167.0, cvOut=210.0,
           accKnob=253.0, accAtt=296.0, accModIn=339.0, accOut=382.0)
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

import os as _os, re as _re2
_ESPL_REF_PATH = _os.path.join(_os.path.dirname(__file__), "art_refs", "esplanade_dome_nanosvg.svg")
try:
    with open(_ESPL_REF_PATH) as _f:
        _ESPL_REF = _f.read()
except Exception:
    _ESPL_REF = ""

def esplanade(x, y, w, h, t, op=0.16):
    # Faithful embed of the real asset (panel_src/art_refs/esplanade_dome_nanosvg.svg
    # — the geodesic durian-shell, truss platform, shoulder + shadow). Earlier this
    # function hand-reconstructed a PARTIAL version (missing the facet grid, truss
    # structure, platform) which rendered crudely. Instead we now port the asset
    # itself: recolour its greys→motif / bright→ink and its black sky-frame→bg, and
    # flatten every group's stroke/fill onto its child shapes (nanosvg does NOT
    # inherit). Scaled into (x,y,w,h) via a transform group (nanosvg-safe, same as
    # logo_embed). `op` scales the overall structure opacity.
    motif = t["motif"]; ink = t["ink"]; bg = t["bg"]
    RX, RY = 1000.0, 450.0
    sx, sy = w / RX, h / RY
    s = _ESPL_REF
    if not s:
        return ""
    s = _re2.sub(r'<svg[^>]*>', '', s); s = s.replace('</svg>', '')
    s = _re2.sub(r'<rect width="1000" height="450" fill="#000"/>', '', s)
    s = _re2.sub(r'<!--.*?-->', '', s, flags=_re2.DOTALL)
    # recolour: dome structure greys → motif, brightest highlight → ink,
    # blacks/darks (sky-frame + platform fills + shadow) → panel bg
    for g in ['#666', '#6c6c6c', '#727272', '#767676', '#777', '#787878']:
        s = s.replace(f'"{g}"', f'"{motif}"')
    s = s.replace('#8a8a8a', ink)
    for b in ['#000', '#111', '#222', '#474747']:
        s = s.replace(f'"{b}"', f'"{bg}"')
    # flatten group attrs onto children (nanosvg has no inheritance)
    def _flatten(m):
        gattr = dict(_re2.findall(r'([\w-]+)="([^"]*)"', m.group(1)))
        inner = m.group(2)
        def _add(tag):
            existing = dict(_re2.findall(r'([\w-]+)="([^"]*)"', tag))
            extra = "".join(f' {k}="{v}"' for k, v in gattr.items() if k not in existing)
            return _re2.sub(r'/?>', extra + '/>', tag, count=1) if tag.endswith('/>') else tag
        return _re2.sub(r'<(?:path|ellipse|polyline|polygon|line|rect)[^>]*/>',
                        lambda mm: _add(mm.group(0)), inner)
    prev = None
    while prev != s:
        prev = s
        s = _re2.sub(r'<g([^>]*)>(((?!<g).)*?)</g>', _flatten, s, flags=_re2.DOTALL)
    # apply overall structure opacity by wrapping (multiplies with per-shape opacity
    # in the renderer via group alpha — nanosvg supports group opacity)
    return (f'<g transform="translate({x:.2f},{y:.2f}) scale({sx:.5f},{sy:.5f})" '
            f'opacity="{min(1.0, op*3.1):.3f}">{s}</g>')


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

def peranakan_edge(x, ytop, h, t, op=0.42, every=30.0):
    # Subtle peranakan jade sideline: a thin double rule with small diamond motifs
    # at intervals. nanosvg-safe (explicit lines/paths, every shape self-styled).
    green = "#1f7a5a" if t["bg"][1] in "0123456" else "#2f8a6a"  # jade; lift on light bg
    L = []; A = L.append
    A(f'<line x1="{x:.1f}" y1="{ytop:.1f}" x2="{x:.1f}" y2="{ytop+h:.1f}" stroke="{green}" stroke-width="1.3" opacity="{op:.2f}"/>')
    A(f'<line x1="{x+2.6:.1f}" y1="{ytop:.1f}" x2="{x+2.6:.1f}" y2="{ytop+h:.1f}" stroke="{green}" stroke-width="0.7" opacity="{op*0.5:.2f}"/>')
    yy = ytop + 10.0
    while yy < ytop + h - 10.0:
        A(f'<path d="M{x-2.6:.1f} {yy:.1f} L{x+1.3:.1f} {yy-3.6:.1f} L{x+5.2:.1f} {yy:.1f} '
          f'L{x+1.3:.1f} {yy+3.6:.1f} Z" fill="none" stroke="{green}" stroke-width="0.7" opacity="{op*0.8:.2f}"/>')
        yy += every
    return "".join(L)

def row_label(side, i):
    # Reference labelling: West module = voices 9..16, East = voices 2..8 (v1 is
    # Monsoon's mono voice). Show the voice number; first row also tags the side.
    if side == "east":
        v = i + 2          # east rows are voices 2..8
    else:
        v = i + 9          # west rows are voices 9..16
    return f"V{v}"

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

    # background motif, composed like the reference sheet: the Esplanade dome sits
    # HIGH-RIGHT, crisp and contained; the Marina Bay waves run LOWER-LEFT (the
    # straits). Distinct zones — the dome's truss no longer crosses the water.
    A(esplanade(PW*0.60, 66.0, PW*0.36, 66.0, t, op=0.24))    # upper-right, contained
    A(waves(15.0, 312.0, t, op=0.40, rows=4, span=PW*0.58))    # lower-left water band

    # title (display text — NOT a control, so text is fine here)
    A(f'<text x="14" y="30" fill="{t["ink"]}" font-family="sans-serif" '
      f'font-size="13" font-weight="700">{title}</text>')
    A(f'<text x="14" y="44" fill="{t["dim"]}" font-family="sans-serif" '
      f'font-size="9">{sub}</text>')

    # column header labels (display text above the first control row)
    _hdr_y = START_Y - 20.0
    for _cx, _lbl in [(COL["knob"],"LVL"), (COL["att"],"ATT"), (COL["modIn"],"MOD"),
                      (COL["gateOut"],"GATE"), (COL["cvOut"],"CV"),
                      (COL["accKnob"],"ACC"), (COL["accAtt"],"A·ATT"),
                      (COL["accModIn"],"A·MOD"), (COL["accOut"],"A·CV")]:
        A(f'<text x="{_cx:.1f}" y="{_hdr_y:.1f}" fill="{t["dim"]}" font-family="sans-serif" '
          f'font-size="8" text-anchor="middle">{_lbl}</text>')

    # ── visible control art (wells/rings) ─────────────────────────────────────
    A('<g>')
    nrows = N_ROWS[side]
    gy = global_y(side)
    # peranakan jade sideline down both edges (subtle family signature, nanosvg-safe)
    A(peranakan_edge(7.0, START_Y-18.0, (gy+10.0)-(START_Y-18.0), t))
    A(peranakan_edge(PW-7.0, START_Y-18.0, (gy+10.0)-(START_Y-18.0), t))
    for i in range(nrows):
        y = START_Y + i * SPACING_Y
        # row label (voice number / side) — display text, left of the knob column
        rlab = row_label(side, i)
        A(f'<text x="{COL["knob"]-22:.1f}" y="{y+3:.1f}" fill="{t["dim"]}" '
          f'font-family="sans-serif" font-size="9" text-anchor="end">{rlab}</text>')
        A(trimwell(COL["knob"], y, t))
        A(trimwell(COL["att"], y, t))
        A(jackwell(COL["modIn"], y, t))
        A(jackwell(COL["gateOut"], y, t))
        A(jackwell(COL["cvOut"], y, t))
        A(trimwell(COL["accKnob"], y, t))
        A(trimwell(COL["accAtt"], y, t))
        A(jackwell(COL["accModIn"], y, t))
        A(jackwell(COL["accOut"], y, t))
    # global row
    if side == "east":
        A(jackwell(COL["modIn"], gy, t))
        A(jackwell(COL["gateOut"], gy, t))
        A(jackwell(COL["cvOut"], gy, t))
        A(jackwell(COL["accModIn"], gy, t))   # shared accent CV
    else:
        A(jackwell(COL["knob"], gy, t))
        A(jackwell(COL["gateOut"], gy, t))
        A(jackwell(COL["cvOut"], gy, t))
        A(jackwell(COL["accModIn"], gy, t))   # shared accent CV
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
        A(kit_marker("param",  f"accknob_{i}", COL["accKnob"], y))
        A(kit_marker("param",  f"accatt_{i}",  COL["accAtt"], y))
        A(kit_marker("input",  f"accmodcv_{i}", COL["accModIn"], y))
        A(kit_marker("output", f"acc_{i}",  COL["accOut"], y))
    if side == "east":
        A(kit_marker("input",  "global_modcv", COL["modIn"], gy))
        A(kit_marker("output", "global_gate",  COL["gateOut"], gy))
        A(kit_marker("output", "global_cv",    COL["cvOut"], gy))
        A(kit_marker("input",  "global_acc_cv", COL["accModIn"], gy))
    else:
        A(kit_marker("input",  "global_cv_in", COL["knob"], gy))
        A(kit_marker("output", "global_gate",  COL["gateOut"], gy))
        A(kit_marker("output", "global_cv",    COL["cvOut"], gy))
        A(kit_marker("input",  "global_acc_cv", COL["accModIn"], gy))
        A(kit_marker("input",  "global_acc_cv", COL["accModIn"], gy))
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
