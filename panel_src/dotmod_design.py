"""Shared dot.modular panel design language (native 75 DPI).
All Sands/Straits visual panels import these so the look stays consistent and
nanosvg-safe. RULES (nanosvg = VCV renderer):
  - No masks, gradients, or filters.
  - Every shape carries its OWN fill/stroke/opacity. nanosvg does NOT inherit
    paint from a parent <g>.
  - No <text> for control labels (the widget draws those).
"""
S = 75.0/25.4
def px(mm): return round(mm*S, 2)

def theme(dark):
    if dark: return dict(
        bg="#16181c", ink="#d8d8dc", dim="#8a8f98", teal="#26a69a", gold="#c8960c",
        accent="#dc2626", jackwell="#0b0d10", jackring="#4a5058", well="#0f1114",
        wellring="#3a4048", edrecess="#101216", edborder="#2a2f37", tabband="#202833",
        group="#13151a", groupline="#262b33", motif="#2e333c", motifwave="#333944")
    return dict(
        bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", teal="#1a8276", gold="#a07808",
        accent="#c0202a", jackwell="#dadce0", jackring="#9298a0", well="#d4d6d9",
        wellring="#a8aeb6", edrecess="#d8dade", edborder="#c0c4ca", tabband="#cdd4dc",
        group="#dfe0e3", groupline="#c8ccd2", motif="#cdcfd4", motifwave="#c8cad0")

# ── Real dot.modular wordmark embed (mask-free logo-{dark,light}.svg) ─────────
def logo_embed(dark, x_mm, y_mm, target_w_mm, repo_root="."):
    path = f"{repo_root}/res/logo/dot-modular-logo-dark.svg" if dark else f"{repo_root}/res/logo/dot-modular-logo-light.svg"
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]   # strip the logo's own bg + accent rects
    scale = px(target_w_mm) / 717.0
    return f'<g transform="translate({px(x_mm):.2f},{px(y_mm):.2f}) scale({scale:.5f})">{body}</g>'

# ── Marina Bay Sands silhouette — leaning tuning-fork legs + lifted skypark ───
# nanosvg-safe (polygons + lines, no clip/gradient/mask). Same signature as before
# so both call sites (faint watermark op~0.15 and crisp identity mark op~0.85) get
# the upgraded geometry. Geometry mirrors the user reference: three tower units,
# each a pair of legs that splay apart toward the base (the real building's piers),
# topped by the SkyPark boat deck with a lifted prow at the left.
def mbs(x_mm, y_mm, w_mm, h_mm, t, op):
    # Marina Bay Sands: three leaning tower-pairs (a curved-lean outer leg + a
    # straight inner leg each) carrying the SkyPark "boat" deck, plus a fountain
    # spray fan at lower-left. Geometry mapped from the reference art's content box
    # (x[90..630], y[150..500] in its 800x600 space) into the panel box. NanoSVG-
    # safe (fills/strokes/opacity only). Window-grid + fountain show only when the
    # mark is large (h>55px); the small identity mark stays clean.
    x, y, w, h = px(x_mm), px(y_mm), px(w_mm), px(h_mm)
    fill = t["motif"]; acc = t["accent"]; bright = t["ink"]
    big = (h > 55)
    RX0, RX1, RY0, RY1 = 90.0, 630.0, 150.0, 500.0
    def MX(rx): return x + (rx - RX0) / (RX1 - RX0) * w
    def MY(ry): return y + (ry - RY0) / (RY1 - RY0) * h
    out = []
    # three leaning legs (quadratic flare at the base — the MBS signature)
    lean = [(210,500, 260,400, 280,190, 295,190, 270,400, 250,500),
            (340,500, 390,400, 410,190, 425,190, 400,400, 380,500),
            (470,500, 520,400, 540,190, 555,190, 530,400, 510,500)]
    for (a1,a2,b1,b2,c1,c2,d1,d2,e1,e2,f1,f2) in lean:
        d = (f"M {MX(a1):.1f} {MY(a2):.1f} Q {MX(b1):.1f} {MY(b2):.1f} {MX(c1):.1f} {MY(c2):.1f} "
             f"L {MX(d1):.1f} {MY(d2):.1f} Q {MX(e1):.1f} {MY(e2):.1f} {MX(f1):.1f} {MY(f2):.1f} Z")
        out.append(f'<path d="{d}" fill="{fill}" fill-opacity="{op}"/>')
    # three straight legs
    straight = [(290,500, 305,190, 330,190, 330,500),
                (420,500, 435,190, 460,190, 460,500),
                (550,500, 565,190, 590,190, 590,500)]
    for (a1,a2,b1,b2,c1,c2,d1,d2) in straight:
        out.append(f'<polygon points="{MX(a1):.1f},{MY(a2):.1f} {MX(b1):.1f},{MY(b2):.1f} '
                   f'{MX(c1):.1f},{MY(c2):.1f} {MX(d1):.1f},{MY(d2):.1f}" fill="{fill}" fill-opacity="{op}"/>')
    if big:
        for (sx0, sx1) in [(290,330),(420,460),(550,590)]:
            for f_ in (0.18,0.34,0.50,0.66,0.82):
                ry = 190 + (500-190)*f_
                out.append(f'<line x1="{MX(sx0):.1f}" y1="{MY(ry):.1f}" x2="{MX(sx1):.1f}" y2="{MY(ry):.1f}" '
                           f'stroke="{acc}" stroke-width="0.4" stroke-opacity="{op*0.3:.3f}"/>')
    # SkyPark boat deck across the tops, lifted prow on the left
    deck = (f"M {MX(90):.1f} {MY(150):.1f} Q {MX(365):.1f} {MY(160):.1f} {MX(630):.1f} {MY(150):.1f} "
            f"C {MX(640):.1f} {MY(150):.1f} {MX(640):.1f} {MY(190):.1f} {MX(620):.1f} {MY(190):.1f} "
            f"L {MX(280):.1f} {MY(190):.1f} Q {MX(160):.1f} {MY(190):.1f} {MX(90):.1f} {MY(150):.1f} Z")
    out.append(f'<path d="{deck}" fill="{fill}" fill-opacity="{op}"/>')
    out.append(f'<path d="M {MX(90):.1f} {MY(150):.1f} Q {MX(365):.1f} {MY(160):.1f} {MX(630):.1f} {MY(150):.1f}" '
               f'fill="none" stroke="{bright}" stroke-width="0.8" stroke-opacity="{op*0.7:.3f}" stroke-linecap="round"/>')
    if big:
        fb = MY(500)
        for (tx, ty) in [(70,390),(100,360),(140,340),(180,370),(210,400)]:
            out.append(f'<line x1="{MX(140):.1f}" y1="{fb:.1f}" x2="{MX(tx):.1f}" y2="{MY(ty):.1f}" '
                       f'stroke="{bright}" stroke-width="0.5" stroke-opacity="{op*0.45:.3f}" stroke-linecap="round"/>')
    return "".join(out)

# ── Straits waves (sine bands) — nanosvg-safe per-path stroke ─────────────────
def waves(x_mm, y_mm, t, op, rows=3, span_mm=140.0):
    x0=px(x_mm); col=t["motifwave"]; out=[]
    for i in range(rows):
        yb=px(y_mm+i*3.2); amp=px(1.6); seg=px(span_mm)/8.0
        d=f'M {x0:.1f} {yb:.1f} '
        for k in range(8):
            cx1=x0+seg*k+seg*0.25; cx2=x0+seg*k+seg*0.75; ex=x0+seg*(k+1)
            ud=-amp if k%2==0 else amp
            d+=f'C {cx1:.1f} {yb+ud:.1f} {cx2:.1f} {yb+ud:.1f} {ex:.1f} {yb:.1f} '
        out.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.5" stroke-opacity="{op}"/>')
    return "".join(out)

# ── Common panel furniture ───────────────────────────────────────────────────
def bg_rect(PW, PH, t): return f'<rect width="{PW}" height="{PH}" fill="{t["bg"]}"/>'

def accent_rules(PW, t, top_mm=11.0, bot_mm=120.0):
    return (f'<line x1="0" y1="{px(top_mm):.1f}" x2="{PW}" y2="{px(top_mm):.1f}" stroke="{t["accent"]}" stroke-width="1.5" stroke-opacity="0.55"/>'
            f'<line x1="0" y1="{px(bot_mm):.1f}" x2="{PW}" y2="{px(bot_mm):.1f}" stroke="{t["groupline"]}" stroke-width="1" stroke-opacity="0.7"/>')

def input_group(gx, gy, gw, gh, t, sep_mm=None):
    out=[f'<rect x="{px(gx):.1f}" y="{px(gy):.1f}" width="{px(gw):.1f}" height="{px(gh):.1f}" rx="{px(2):.1f}" fill="{t["group"]}" stroke="{t["groupline"]}" stroke-width="1"/>']
    if sep_mm is not None:
        out.append(f'<line x1="{px(sep_mm):.1f}" y1="{px(gy+2):.1f}" x2="{px(sep_mm):.1f}" y2="{px(gy+gh-2):.1f}" stroke="{t["groupline"]}" stroke-width="0.75" stroke-opacity="0.6"/>')
    return "".join(out)

def editor_recess(ED_X, ED_Y, ED_W, ED_H, t, lanes=3, watermark=False):
    out=[f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y-6):.1f}" width="{px(ED_W):.1f}" height="{px(5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" fill-opacity="0.6"/>',
         f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>']
    # NOTE: the MBS watermark is OFF by default. The live 16-step dynamic visual
    # draws across the full recess, and a building silhouette behind those busy
    # animated lanes reads as visual mud. The crisp MBS identity mark on the water
    # band (drawn separately, outside the recess) carries the Marina Bay identity.
    # Pass watermark=True only for a still/empty editor mockup.
    if watermark:
        out.append(mbs(ED_X+ED_W*0.28, ED_Y+ED_H-5.0, ED_W*0.46, ED_H*0.62, t, op=0.15))
    for k in range(1,lanes):
        ly=ED_Y+k*(ED_H/float(lanes))
        out.append(f'<line x1="{px(ED_X+1):.1f}" y1="{px(ly):.1f}" x2="{px(ED_X+ED_W-1):.1f}" y2="{px(ly):.1f}" stroke="{t["edborder"]}" stroke-width="0.75" stroke-opacity="0.7"/>')
    return "".join(out)

def jack(x_mm, y_mm, t):
    return (f'<circle cx="{px(x_mm):.1f}" cy="{px(y_mm):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>'
            f'<circle cx="{px(x_mm):.1f}" cy="{px(y_mm):.1f}" r="{px(1.7):.1f}" fill="{t["edrecess"]}"/>')

def trim(x_mm, y_mm, t, col):
    return (f'<circle cx="{px(x_mm):.1f}" cy="{px(y_mm):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>'
            f'<line x1="{px(x_mm):.1f}" y1="{px(y_mm):.1f}" x2="{px(x_mm):.1f}" y2="{px(y_mm-2.4):.1f}" stroke="{col}" stroke-width="1"/>')

def svg_open(PW, PH):
    return f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">'
