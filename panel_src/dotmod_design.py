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
        group="#13151a", groupline="#262b33", motif="#2e333c", motifwave="#333944",
        steel="#8fa3b0", ledblue="#5ab0e0")
    return dict(
        bg="#e8e8ea", ink="#2a2a2e", dim="#888d96", teal="#1a8276", gold="#a07808",
        accent="#c0202a", jackwell="#dadce0", jackring="#9298a0", well="#d4d6d9",
        wellring="#a8aeb6", edrecess="#d8dade", edborder="#c0c4ca", tabband="#cdd4dc",
        group="#dfe0e3", groupline="#c8ccd2", motif="#cdcfd4", motifwave="#c8cad0",
        steel="#6b7a86", ledblue="#2a80b0")

# ── Real dot.modular wordmark embed (mask-free logo-{dark,light}.svg) ─────────
def logo_embed(dark, x_mm, y_mm, target_w_mm, repo_root=None):
    import re, os
    if repo_root is None:
        repo_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    path = os.path.join(repo_root, "res", "logo", "dot-modular-logo-dark.svg" if dark else "dot-modular-logo-light.svg")
    s = open(path).read()
    body = s[s.find('<g '):s.rfind('</svg>')]   # strip the logo's own bg + accent rects
    # Nondestructive wordmark crop (viewBox-style zoom): the source is 717x190 with big
    # margins + two <polyline> positioning brackets (top-left & bottom-right registration
    # marks for physical module placement). Crop to the wordmark bbox (matches the
    # *-tight.svg variant: x26 y65 w657 h95), strip the bracket polylines, and zoom so the
    # wordmark fills target_w_mm with no blank margins. Implemented as a translate/scale
    # transform (nanosvg-safe — no clip / nested <svg>); the source file is untouched.
    body = re.sub(r'<polyline\b[^>]*/>', '', body)   # remove the 2 positioning brackets
    CX, CY, CW = 26.0, 65.0, 657.0
    sc = px(target_w_mm) / CW
    return f'<g transform="translate({px(x_mm):.2f},{px(y_mm):.2f}) scale({sc:.5f}) translate({-CX:.2f},{-CY:.2f})">{body}</g>'

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

def owner_block(owner_x, lane_ys, ed_right_x, t, cell_w_mm=6.0, label="SRC", draw_cells=True, cell_h_frac=0.9):
    """Per-lane owner-source block, drawn right of the editor before the prob-outs.
    v2-subtle: a thin separator from the editor grid, a faint backing strip, a
    small label, and (when draw_cells) one outline cell per lane row. When the
    LIVE OwnerCell widget draws the cells itself, pass draw_cells=False so only the
    container (backing + separator + label) is baked. cell_h_frac sets the cell
    height as a fraction of the lane spacing (match the live widget). lane_ys =
    lane centre Y."""
    if not lane_ys:
        return ""
    top = min(lane_ys); bot = max(lane_ys)
    cell_h = ((bot - top) / max(1, len(lane_ys)-1)) * cell_h_frac if len(lane_ys) > 1 else 8.0
    pad = 1.6
    bx = owner_x - cell_w_mm*0.5 - pad
    by = top - cell_h*0.5 - pad
    bw = cell_w_mm + 2*pad
    bh = (bot - top) + cell_h + 2*pad
    out = [
        f'<rect x="{px(bx):.1f}" y="{px(by):.1f}" width="{px(bw):.1f}" height="{px(bh):.1f}" rx="{px(1.0):.1f}" fill="#ffffff" fill-opacity="0.05"/>',
        f'<line x1="{px(ed_right_x+1.4):.1f}" y1="{px(by):.1f}" x2="{px(ed_right_x+1.4):.1f}" y2="{px(by+bh):.1f}" stroke="{t["edborder"]}" stroke-width="1.2" stroke-opacity="0.8"/>',
        f'<text x="{px(owner_x):.1f}" y="{px(by-1.4):.1f}" font-family="sans-serif" font-size="{px(2.0):.1f}" fill="{t["edborder"]}" text-anchor="middle">{label}</text>',
    ]
    if draw_cells:
        for cy in lane_ys:
            out.append(f'<rect x="{px(owner_x-cell_w_mm*0.5):.1f}" y="{px(cy-cell_h*0.5):.1f}" width="{px(cell_w_mm):.1f}" height="{px(cell_h):.1f}" rx="{px(0.8):.1f}" fill="none" stroke="{t["edborder"]}" stroke-width="1.0" stroke-opacity="0.55"/>')
    return "".join(out)

def kit_shape(kind, idx, x_mm, y_mm):
    """Emit a named, near-invisible marker circle the SvgPanelKit can bind to.
    `kind` is one of 'param'|'input'|'output'|'light'; `idx` is the control's
    absolute param/input/output/light INDEX (the source of truth the widget binds
    by). The kit reads the shape's bounds centre, so a tiny transparent circle at
    the control's centre is all that's needed. id form: '{kind}_{idx}', e.g.
    'input_0', 'param_3'. A future widget binds e.g. bindParam("param_3", id).
    These go in a dedicated 'components' layer and never affect the visible art."""
    return (f'<circle id="{kind}_{idx}" cx="{px(x_mm):.2f}" cy="{px(y_mm):.2f}" '
            f'r="0.5" fill="none" stroke="none"/>')


def jack(x_mm, y_mm, t):
    return (f'<circle cx="{px(x_mm):.1f}" cy="{px(y_mm):.1f}" r="{px(3.9):.1f}" fill="{t["jackwell"]}" stroke="{t["jackring"]}" stroke-width="1"/>'
            f'<circle cx="{px(x_mm):.1f}" cy="{px(y_mm):.1f}" r="{px(1.7):.1f}" fill="{t["edrecess"]}"/>')

def trim(x_mm, y_mm, t, col):
    return (f'<circle cx="{px(x_mm):.1f}" cy="{px(y_mm):.1f}" r="{px(3.2):.1f}" fill="{t["well"]}" stroke="{col}" stroke-width="1.25"/>'
            f'<line x1="{px(x_mm):.1f}" y1="{px(y_mm):.1f}" x2="{px(x_mm):.1f}" y2="{px(y_mm-2.4):.1f}" stroke="{col}" stroke-width="1"/>')

def svg_open(PW, PH):
    return f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="{PW}" height="{PH}" viewBox="0 0 {PW} {PH}">'

# ── Sands Helix — the Helix Bridge as a double-helix hero mark ────────────────
# nanosvg-safe (lines/circles/polys + opacity; no gradient/mask/filter). Same
# (x,y,w,h,t,op) signature as mbs()/waves() so any panel can place it. Two teal
# helical rails 180° opposed (the DNA double helix the bridge is modelled on),
# occluded (front hides back via per-segment painter-sort), gold rungs + light
# steel near-end diagonals for truss texture, blue LED nodes on the front rails,
# and the real mbs() towers staged upper-right (helix sweeps up toward MBS —
# the Helix Bridge → Marina Bay Sands geography). Designed as a foreground HERO
# mark (high op); the Sands Helix (macro) panel places it in the bottom-left.
def helix_sands(x_mm, y_mm, w_mm, h_mm, t, op, repo_root="."):
    import math
    x, y, w, h = px(x_mm), px(y_mm), px(w_mm), px(h_mm)
    teal=t["teal"]; gold=t["gold"]; dim=t["dim"]
    steel=t.get("steel","#8fa3b0"); blue=t.get("ledblue","#5ab0e0")
    out=[]
    # MBS towers upper-right, its own space (helix sweeps UP TO it, not into it)
    out.append(mbs(x_mm+w_mm*0.62, y_mm+h_mm*0.02, w_mm*0.38, h_mm*0.46, t, op=op*0.60))
    N=220; turns=2.5; R0=h*0.30
    def axis(u):
        ax=x+w*0.05+w*0.60*(u**0.88)
        ay=y+h*0.62 - h*0.30*(u**1.15)
        sc=1.05-0.74*u
        return ax, ay, max(0.06, sc)
    def strand(phase):
        p=[]
        for i in range(N+1):
            u=i/N; ax,ay,sc=axis(u); ang=phase+turns*2*math.pi*u
            oy=R0*sc*math.sin(ang); ox=R0*sc*0.36*math.cos(ang)
            p.append((ax+ox, ay+oy, sc, math.cos(ang)))
        return p
    A=strand(0.0); B=strand(math.pi)
    out.append(f'<path d="M '+" L ".join(f"{axis(i/N)[0]:.1f} {axis(i/N)[1]:.1f}" for i in range(N+1))+
               f'" fill="none" stroke="{dim}" stroke-width="{px(1.3):.1f}" stroke-opacity="{op*0.40:.2f}"/>')
    segs=[]
    for i in range(0,N+1,5):   # gold rungs across the tube
        x1,y1,s1,d1=A[i]; x2,y2,_,d2=B[i]; depth=(d1+d2)*0.5
        segs.append((depth-0.03, f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{gold}" stroke-width="{max(0.4,1.0*s1):.1f}" stroke-opacity="{op*(0.28+0.28*s1):.2f}"/>'))
    for i in range(0,int(N*0.55),4):   # near-end steel truss texture
        j=min(N,i+5); x1,y1,s1,d1=A[i]; x2,y2,_,d2=B[j]; depth=(d1+d2)*0.5
        segs.append((depth-0.05, f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{steel}" stroke-width="{max(0.3,0.8*s1):.1f}" stroke-opacity="{op*(0.22+0.28*s1):.2f}"/>'))
    for pts in (A,B):   # bold occluded rails
        for i in range(N):
            x1,y1,s1,d1=pts[i]; x2,y2,s2,d2=pts[i+1]; depth=(d1+d2)*0.5; front=depth>0
            lw=max(1.0,(4.4 if front else 2.2)*s1); oo=op*((1.0 if front else 0.55)*(0.55+0.45*s1))
            segs.append((depth, f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{teal}" stroke-width="{lw:.1f}" stroke-opacity="{oo:.2f}" stroke-linecap="round"/>'))
    for pts in (A,B):   # LED nodes on front rails
        for i in range(0,N+1,6):
            x1,y1,s1,d1=pts[i]
            if d1>0.15 and s1>0.3:
                segs.append((1.49, f'<circle cx="{x1:.1f}" cy="{y1:.1f}" r="{max(1.3,3.4*s1):.1f}" fill="{blue}" fill-opacity="{op*0.20:.2f}"/>'))
                segs.append((1.50, f'<circle cx="{x1:.1f}" cy="{y1:.1f}" r="{max(0.8,1.9*s1):.1f}" fill="{blue}" fill-opacity="{op:.2f}"/>'))
    for d,s_ in sorted(segs, key=lambda z:z[0]): out.append(s_)
    return "".join(out)
