"""dot.modular Straits motifs — built from scratch to match the concept sheet.

Three generators, all nanosvg-safe (no <mask>, no gradients, no url(#...),
every shape carries its OWN stroke/fill — nanosvg does not inherit):

  esplanade_dome(x, y, w, h, t, op)   — faint faceted durian-shell watermark
  marina_waves(x, y, w, h, t, op)     — faint stacked water rows
  skyline_pulse(x, y, w, h, t)        — the red "Singapore Skyline-Pulse" line:
                                        MBS triple-towers → supertree fans →
                                        esplanade dome, joined by a pulse/ECG
                                        baseline with dot.modular connector dots.

Coordinates: each function maps an internal unit box into the given (x,y,w,h).
"""

def _scaler(x, y, w, h, ux, uy):
    """Return MX, MY mapping internal (0..ux, 0..uy) into the (x,y,w,h) box."""
    def MX(rx): return x + rx / ux * w
    def MY(ry): return y + ry / uy * h
    return MX, MY


# ─────────────────────────────────────────────────────────────────────────────
# 1. ESPLANADE DOME  (faint watermark — the durian shell, cleanly bounded)
# ─────────────────────────────────────────────────────────────────────────────
def esplanade_dome(x, y, w, h, t, op=0.5):
    """A faceted half-dome on a slim platform. Drawn from scratch so nothing
    extends beyond the dome outline: longitudinal ribs and latitudinal bands are
    both generated as curves that START and END on the dome outline, so there is
    no skirt/spill to mask. UX 0..200 wide, 0..120 tall (dome sits 0..100, the
    platform 100..120)."""
    UX, UY = 200.0, 120.0
    MX, MY = _scaler(x, y, w, h, UX, UY)
    motif = t["motif"]; ink = t["ink"]
    L = []; A = L.append

    # dome outline: a flattened arch. base from (10,100) to (190,100), apex (100,12).
    # Parametrise the outline so ribs/bands can land exactly on it.
    import math
    def outline_pt(u):           # u in 0..1 left→right along the dome rim
        # elliptical arch: x spans 10..190, y = apex + (1-sin) profile
        ax = 10 + 180 * u
        # height profile: half-sine, flatter top (durian shell)
        ay = 100 - 88 * math.sin(math.pi * u) ** 0.85
        return ax, ay

    # --- latitudinal bands (horizontal-ish arcs at decreasing height) ---
    A(f'<g fill="none" stroke="{motif}" stroke-width="0.9" stroke-linecap="round">')
    for frac in [0.16, 0.34, 0.52, 0.70, 0.86]:
        # a band at height fraction `frac` up the dome: sample left & right x
        # where the dome is at that height, draw a shallow arc between them.
        # invert the height profile: sin(pi u)^.85 = frac  → u
        uu = math.asin(frac ** (1/0.85)) / math.pi
        lx, ly = outline_pt(uu)
        rx, ry = outline_pt(1 - uu)
        midy = ly - (ly - (100 - 88)) * 0.10   # slight upward bow
        A(f'<path d="M{MX(lx):.2f} {MY(ly):.2f} Q{MX(100):.2f} {MY(midy):.2f} '
          f'{MX(rx):.2f} {MY(ry):.2f}"/>')
    A('</g>')

    # --- longitudinal ribs (meridians from base to apex, landing on outline) ---
    A(f'<g fill="none" stroke="{motif}" stroke-width="0.9" stroke-linecap="round">')
    for u in [0.10, 0.225, 0.35, 0.50, 0.65, 0.775, 0.90]:
        bx, by = outline_pt(u)
        # rib curves up to just below the apex, bowing toward centre
        A(f'<path d="M{MX(bx):.2f} {MY(by):.2f} Q{MX(100):.2f} {MY(by - (by-12)*0.55):.2f} '
          f'{MX(100):.2f} {MY(16):.2f}"/>')
    A('</g>')

    # --- dome outline (brighter rim) ---
    pts = " ".join(f"{MX(px):.2f} {MY(py):.2f}" for px, py in
                   ([outline_pt(i/40) for i in range(41)]))
    # build as a polyline-ish path
    d = "M" + " L".join(pts.split("  ")) if False else None
    op_path = "M" + " L".join(f"{MX(px):.2f} {MY(py):.2f}" for px, py in
                              [outline_pt(i/40) for i in range(41)])
    A(f'<path d="{op_path}" fill="none" stroke="{ink}" stroke-width="1.6" '
      f'opacity="0.9" stroke-linecap="round" stroke-linejoin="round"/>')

    # --- platform / truss under the dome (slim, contained to the base width) ---
    A(f'<g fill="none" stroke="{ink}" stroke-width="1.3" stroke-linejoin="round" stroke-linecap="round" opacity="0.85">')
    # top deck line just under the rim base
    A(f'<line x1="{MX(8):.2f}" y1="{MY(102):.2f}" x2="{MX(192):.2f}" y2="{MY(102):.2f}"/>')
    # truss zigzag between deck and lower chord
    zig = ["M"]
    pts2 = []
    n = 9
    for i in range(n + 1):
        zx = 12 + (176) * i / n
        zy = 102 if i % 2 == 0 else 112
        pts2.append(f"{MX(zx):.2f} {MY(zy):.2f}")
    A(f'<polyline points="{" ".join(p.replace(" ", ",") for p in pts2)}" fill="none" '
      f'stroke="{motif}" stroke-width="1.0"/>')
    # lower chord
    A(f'<line x1="{MX(12):.2f}" y1="{MY(112):.2f}" x2="{MX(188):.2f}" y2="{MY(112):.2f}"/>')
    A('</g>')

    return f'<g opacity="{op:.3f}">' + "".join(L) + '</g>'


# ─────────────────────────────────────────────────────────────────────────────
# 2. MARINA WAVES  (faint stacked water rows)
# ─────────────────────────────────────────────────────────────────────────────
def marina_waves(x, y, w, h, t, op=0.34, rows=None):
    """Stacked sine rows filling the box. Each row is its own self-styled path."""
    col = t["motifwave"]
    row_gap = 9.0
    if rows is None:
        rows = max(1, int(h / row_gap))
    L = []; A = L.append
    seg = w / 8.0
    amp = 4.2
    for i in range(rows):
        yb = y + i * row_gap
        d = f"M {x:.1f} {yb:.1f} "
        for k in range(8):
            cx1 = x + seg * k + seg * 0.25
            cx2 = x + seg * k + seg * 0.75
            ex = x + seg * (k + 1)
            ud = -amp if k % 2 == 0 else amp
            d += f"C {cx1:.1f} {yb+ud:.1f} {cx2:.1f} {yb+ud:.1f} {ex:.1f} {yb:.1f} "
        A(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.0" '
          f'opacity="{op:.3f}" stroke-linecap="round"/>')
    return "".join(L)


# ─────────────────────────────────────────────────────────────────────────────
# 3. SKYLINE-PULSE  (the red identity line from the concept)
# ─────────────────────────────────────────────────────────────────────────────
def _mbs(MX, MY, base, red, sw, op, A):
    """Marina Bay Sands traced from the concept: three SLIGHTLY-TAPERED pillars
    (a touch wider at the base), a BOAT-shaped SkyPark deck overhanging across
    their tops, and a small mast. Pillars drawn as 4-point tapered polygons; the
    deck as a shallow boat (lens) polygon — not blocky rects."""
    # measured pillar top/base x-edges (UX) and y (deck bottom→base)
    ptop, pbase = 27.0, base
    pillars = [(94, 152, 81, 155), (187, 245, 181, 248), (281, 336, 274, 342)]
    for (tl, tr, bl, br) in pillars:
        A(f'<polygon points="{MX(tl):.2f},{MY(ptop):.2f} {MX(tr):.2f},{MY(ptop):.2f} '
          f'{MX(br):.2f},{MY(pbase):.2f} {MX(bl):.2f},{MY(pbase):.2f}" '
          f'fill="{red}" opacity="{op:.3f}"/>')
    # SkyPark boat deck: a shallow lens overhanging both ends, sitting on the tops
    dl, dr = 45.0, 342.0
    dtop, dmid, dbot = 17.0, 13.0, 27.0
    A(f'<path d="M{MX(dl):.2f} {MY(dbot):.2f} '
      f'Q{MX((dl+dr)/2):.2f} {MY(dmid):.2f} {MX(dr):.2f} {MY(dbot):.2f} '
      f'Q{MX((dl+dr)/2):.2f} {MY(dtop):.2f} {MX(dl):.2f} {MY(dbot):.2f} Z" '
      f'fill="{red}" opacity="{op:.3f}"/>')
    # mast on the deck
    A(f'<line x1="{MX(116):.2f}" y1="{MY(dtop):.2f}" x2="{MX(116):.2f}" y2="{MY(8):.2f}" '
      f'stroke="{red}" stroke-width="{sw*0.7:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')


def _supertree(MX, MY, tx, tbase, top, halfw, red, sw, op, A):
    """Gardens-by-the-Bay supertree (measured): trunk rises from the wave at
    tbase; branches splay into a concave canopy AND the central branches
    OVERSHOOT above the canopy rim (measured: tips reach well above the bell).
    `top` = highest branch-tip y; `halfw` = canopy half-width."""
    stem_top = tbase - (tbase - top) * 0.42
    # trunk
    A(f'<line x1="{MX(tx):.2f}" y1="{MY(tbase):.2f}" x2="{MX(tx):.2f}" y2="{MY(stem_top):.2f}" '
      f'stroke="{red}" stroke-width="{sw*0.95:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')
    # rim of the bell (canopy mouth) — sits BELOW the overshooting centre tips
    rim_y = top + (tbase - top) * 0.26
    A(f'<path d="M{MX(tx-halfw):.2f} {MY(rim_y):.2f} Q{MX(tx):.2f} {MY(rim_y-(tbase-top)*0.10):.2f} '
      f'{MX(tx+halfw):.2f} {MY(rim_y):.2f}" fill="none" stroke="{red}" '
      f'stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')
    # branches: concave ribs from stem_top; centre ribs overshoot ABOVE rim to `top`
    ribs = 7
    for r in range(ribs):
        f = r / (ribs - 1)                       # 0..1
        centre = 1 - abs(f - 0.5) * 2            # 1 at middle, 0 at edges
        tipx = tx + (f - 0.5) * 2 * halfw
        tipy = top + (1 - centre) * (rim_y - top) * 0.9   # centre tips highest (above rim)
        cx = tx + (f - 0.5) * halfw * 0.28       # hug the trunk low → concave
        cy = stem_top - (stem_top - rim_y) * 0.4
        A(f'<path d="M{MX(tx):.2f} {MY(stem_top):.2f} Q{MX(cx):.2f} {MY(cy):.2f} '
          f'{MX(tipx):.2f} {MY(tipy):.2f}" fill="none" stroke="{red}" '
          f'stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')


def _wave_to_trunks(MX, MY, base, trees, x_in, x_out, trough_y, red, sw, op, A):
    """Red pulse: weaves with BIG amplitude, dropping into a deep trough BETWEEN
    each tree and rising to meet each trunk base (wave and trunks are one line —
    skyline morphing into signal). trough_y measured well below the baseline."""
    txs = [t[0] for t in trees]
    pts = [(x_in, base)]
    prev = x_in
    for tx in txs:
        pts.append(((prev + tx) / 2, trough_y))   # deep trough before the trunk
        pts.append((tx, base))                     # rise to trunk base
        prev = tx
    pts.append(((prev + x_out) / 2, trough_y))
    pts.append((x_out, base))
    # smooth through points with quadratic midpoint smoothing
    d = f"M{MX(pts[0][0]):.2f} {MY(pts[0][1]):.2f} "
    for i in range(1, len(pts)):
        x0p, y0p = pts[i-1]; x1p, y1p = pts[i]
        mx = (x0p + x1p) / 2; my = (y0p + y1p) / 2
        d += f"Q{MX(x0p):.2f} {MY(y0p):.2f} {MX(mx):.2f} {MY(my):.2f} "
    d += f"L{MX(pts[-1][0]):.2f} {MY(pts[-1][1]):.2f} "
    A(f'<path d="{d}" fill="none" stroke="{red}" stroke-width="{sw*1.05:.2f}" '
      f'stroke-linecap="round" stroke-linejoin="round" opacity="{op:.3f}"/>')


def _lattice_dome(MX, MY, base, ink, sw, op):
    """Esplanade traced from the concept (true 3/4 PERSPECTIVE): the top shell is
    an asymmetric arch with a broad flat apex; the LEFT springs from a higher
    platform point, the RIGHT/front lip bows LOWER and extends further — the
    durian seen at an angle. Outline is the traced point sequence; a lattice
    diamond grid fills the shell. Returns the SVG string."""
    L = []; A = L.append
    # traced TOP outline (UX,UY) and BOTTOM/front-lip (UX,UY) from pixel measurement
    top = [(710,145),(748,95),(774,74),(800,65),(826,63),(852,65),
           (877,70),(903,78),(929,101)]
    bot = [(929,175),(855,175),(806,170),(768,147),(710,145)]   # right→left along lip
    pathd = "M" + " L".join(f"{MX(x):.2f} {MY(y):.2f}" for x, y in top)
    pathd += " L" + " L".join(f"{MX(x):.2f} {MY(y):.2f}" for x, y in bot) + " Z"
    A(f'<path d="{pathd}" fill="none" stroke="{ink}" stroke-width="{sw*0.9:.2f}" '
      f'stroke-linejoin="round" stroke-linecap="round" opacity="{op*0.9:.3f}"/>')
    # build interpolators for the shell interior to clip the lattice
    def interp(seq, px):
        for i in range(len(seq)-1):
            x0,y0 = seq[i]; x1,y1 = seq[i+1]
            lo,hi = min(x0,x1),max(x0,x1)
            if lo <= px <= hi and x1 != x0:
                u=(px-x0)/(x1-x0); return y0+(y1-y0)*u
        return None
    botL = sorted(bot)                      # left→right
    def top_y(px): return interp(top, px)
    def bot_y(px): return interp(botL, px)
    xmin, xmax = 712, 928
    def inside(px, py):
        ty, by = top_y(px), bot_y(px)
        return ty is not None and by is not None and ty <= py <= by
    step = (xmax - xmin) / 11.0
    cx = (xmin + xmax) / 2
    for sign in (+1, -1):
        for c in range(-12, 13):
            xs = cx + c * step
            seg = []
            for k in range(0, 60):
                px = xs + sign * k * step * 0.5
                py = 178 - k * step * 0.5
                if inside(px, py): seg.append((px, py))
                elif seg: break
            if len(seg) >= 2:
                A(f'<line x1="{MX(seg[0][0]):.2f}" y1="{MY(seg[0][1]):.2f}" '
                  f'x2="{MX(seg[-1][0]):.2f}" y2="{MY(seg[-1][1]):.2f}" stroke="{ink}" '
                  f'stroke-width="{sw*0.3:.2f}" opacity="{op*0.48:.3f}"/>')
    return "".join(L)


def skyline_pulse(x, y, w, h, t, accent=None, op=1.0):
    """'Singapore Skyline-Pulse', rebuilt from per-element pixel MEASUREMENT of
    the concept: 3 MBS pillars + SkyPark deck; 3 supertrees of DIFFERENT sizes
    (centre tallest/widest) whose centre branches overshoot above the canopy rim
    and whose trunks rise out of a big-amplitude red pulse wave; an esplanade
    lattice dome with perspective. Red left/centre, grey dome. UX 0..1000."""
    UX, UY = 1000.0, 200.0
    MX, MY = _scaler(x, y, w, h, UX, UY)
    red = accent or t["accent"]; ink = t["ink"]
    base = 126.0          # measured baseline
    L = []; A = L.append
    sw = max(1.6, w / 190.0)

    _mbs(MX, MY, base, red, sw, op, A)

    # trees: (tx, canopy_top, halfwidth) measured — B centre tallest & widest
    trees = [(455, 65, 30), (542, 6, 46), (639, 65, 26)]
    _wave_to_trunks(MX, MY, base, trees, 175.0, 700.0, base + 56.0, red, sw, op, A)
    for tx, top, hw in trees:
        _supertree(MX, MY, tx, base, top, hw, red, sw, op, A)

    A(_lattice_dome(MX, MY, base, ink, sw, op))

    # connector nodes at the ends
    for dx in (150, 960):
        A(f'<circle cx="{MX(dx):.2f}" cy="{MY(base):.2f}" r="{6.0/UX*w:.2f}" '
          f'fill="none" stroke="{ink}" stroke-width="{sw*0.5:.2f}" opacity="{op*0.6:.3f}"/>')
    return "".join(L)


# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    import cairosvg
    T = dict(bg="#16181c", ink="#d8d8dc", dim="#8a8f98", accent="#dc2626",
             motif="#3a4250", motifwave="#333944")
    W, H = 1000, 640
    parts = [f'<rect width="{W}" height="{H}" fill="{T["bg"]}"/>']
    # row 1: skyline-pulse big
    parts.append(skyline_pulse(40, 20, 920, 180, T))
    # row 2: esplanade dome
    parts.append(esplanade_dome(120, 230, 760, 200, T, op=0.85))
    # row 3: waves
    parts.append(marina_waves(60, 470, 880, 140, T, op=0.5))
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}">' + "".join(parts) + '</svg>'
    open("/tmp/motifs_test.svg", "w").write(svg)
    cairosvg.svg2png(url="/tmp/motifs_test.svg", write_to="/mnt/user-data/outputs/motifs_test.png", scale=1.3)
    print("rendered motifs_test.png")
