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

# ── Marina Bay Sands silhouette (trapezoidal towers + skypark) — nanosvg-safe ─
def mbs(x_mm, y_mm, w_mm, h_mm, t, op):
    x,y,w,h = px(x_mm),px(y_mm),px(w_mm),px(h_mm)
    fill=t["motif"]; acc=t["accent"]
    tw=w*0.165; gap=(w-3*tw)/2.0; th=h; by=y+h; lean=tw*0.06
    out=[]
    xs=[x, x+tw+gap, x+2*(tw+gap)]
    for tx in xs:
        x1,x2 = tx+lean, tx+tw-lean
        x3,x4 = tx+tw, tx
        out.append(f'<polygon points="{x1:.1f},{y:.1f} {x2:.1f},{y:.1f} {x3:.1f},{by:.1f} {x4:.1f},{by:.1f}" fill="{fill}" fill-opacity="{op}"/>')
        for f_ in (0.36, 0.64):
            wy=y+th*f_
            out.append(f'<line x1="{tx+lean*0.6:.1f}" y1="{wy:.1f}" x2="{tx+tw-lean*0.6:.1f}" y2="{wy:.1f}" stroke="{acc}" stroke-width="0.5" stroke-opacity="{op*0.4:.3f}"/>')
        cxk=tx+tw*0.5
        out.append(f'<line x1="{cxk:.1f}" y1="{y:.1f}" x2="{cxk:.1f}" y2="{y-h*0.10:.1f}" stroke="{acc}" stroke-width="0.6" stroke-opacity="{op*0.6:.3f}"/>')
        out.append(f'<circle cx="{cxk:.1f}" cy="{y-h*0.12:.1f}" r="1.1" fill="{acc}" fill-opacity="{op*0.6:.3f}"/>')
    dy=y-h*0.06
    out.append(f'<polygon points="{x-w*0.02:.1f},{dy:.1f} {x+w+w*0.06:.1f},{dy-h*0.05:.1f} {x+w+w*0.06:.1f},{dy-h*0.05+h*0.06:.1f} {x-w*0.02:.1f},{dy+h*0.06:.1f}" fill="{fill}" fill-opacity="{op}"/>')
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

def editor_recess(ED_X, ED_Y, ED_W, ED_H, t, lanes=3):
    out=[f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y-6):.1f}" width="{px(ED_W):.1f}" height="{px(5):.1f}" rx="{px(1):.1f}" fill="{t["tabband"]}" fill-opacity="0.6"/>',
         f'<rect x="{px(ED_X):.1f}" y="{px(ED_Y):.1f}" width="{px(ED_W):.1f}" height="{px(ED_H):.1f}" rx="{px(1.5):.1f}" fill="{t["edrecess"]}" stroke="{t["edborder"]}" stroke-width="1"/>']
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
