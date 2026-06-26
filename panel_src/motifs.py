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
    """Marina Bay Sands: three SOLID filled rectangular towers + a solid
    overhanging SkyPark slab across the top (studied from concept)."""
    deck_top, deck_bot = 44.0, 52.0
    tower_top = deck_bot
    tw = 9.0                      # tower width (solid)
    xs = [88, 116, 144]
    for tx in xs:                 # solid filled pillars
        A(f'<rect x="{MX(tx-tw/2):.2f}" y="{MY(tower_top):.2f}" '
          f'width="{(tw/1000.0)* (MX(1000)-MX(0)):.2f}" height="{MY(base)-MY(tower_top):.2f}" '
          f'fill="{red}" opacity="{op:.3f}"/>')
    # SkyPark slab: solid bar overhanging the outer towers
    sx0, sx1 = xs[0]-tw/2-7, xs[-1]+tw/2+7
    A(f'<rect x="{MX(sx0):.2f}" y="{MY(deck_top):.2f}" '
      f'width="{MX(sx1)-MX(sx0):.2f}" height="{MY(deck_bot)-MY(deck_top):.2f}" '
      f'fill="{red}" opacity="{op:.3f}"/>')


def _supertree(MX, MY, tx, tbase, red, sw, op, A):
    """One Gardens-by-the-Bay supertree: a thin stem flaring into a CONCAVE
    trumpet/wineglass canopy (curved ribs bowing outward then up)."""
    top = 22.0
    stem_top = (tbase + top) * 0.52
    rim = 30.0                    # canopy half-width at the top
    # stem
    A(f'<line x1="{MX(tx):.2f}" y1="{MY(tbase):.2f}" x2="{MX(tx):.2f}" y2="{MY(stem_top):.2f}" '
      f'stroke="{red}" stroke-width="{sw*0.8:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')
    # canopy ribs: concave flare from stem-top out to the rim
    ribs = 7
    for r in range(ribs):
        f = r / (ribs - 1)                 # 0..1
        ex = tx + (f - 0.5) * 2 * rim
        ey = top + abs(f - 0.5) * 6        # slight droop at the outer tips
        # concave: control pt low & near axis → rib hugs stem then flares up/out
        cx = tx + (f - 0.5) * rim * 0.35
        cy = stem_top - (stem_top - top) * 0.25
        A(f'<path d="M{MX(tx):.2f} {MY(stem_top):.2f} Q{MX(cx):.2f} {MY(cy):.2f} '
          f'{MX(ex):.2f} {MY(ey):.2f}" fill="none" stroke="{red}" '
          f'stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')
    # canopy rim arc connecting tips (the flared bell mouth)
    A(f'<path d="M{MX(tx-rim):.2f} {MY(top+6):.2f} Q{MX(tx):.2f} {MY(top-6):.2f} '
      f'{MX(tx+rim):.2f} {MY(top+6):.2f}" fill="none" stroke="{red}" '
      f'stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')


def _lattice_dome(MX, MY, dl, dr, base, dapex, ink, sw, op, A):
    """Esplanade: a smooth half-dome outline filled with a diagonal LATTICE
    (diamond geodesic grid), on a flat base — studied from concept."""
    import math
    cx = (dl + dr) / 2
    rx = (dr - dl) / 2
    ry = base - dapex
    # outline (half-ellipse)
    d = f"M{MX(dl):.2f} {MY(base):.2f} "
    N = 40
    for i in range(N + 1):
        ang = math.pi * (i / N)            # 0..pi → left to right over the top
        ex = cx - rx * math.cos(ang)
        ey = base - ry * math.sin(ang)
        d += f"L{MX(ex):.2f} {MY(ey):.2f} "
    A(f'<path d="{d}" fill="none" stroke="{ink}" stroke-width="{sw*0.9:.2f}" '
      f'stroke-linejoin="round" stroke-linecap="round" opacity="{op*0.9:.3f}"/>')
    # base line
    A(f'<line x1="{MX(dl):.2f}" y1="{MY(base):.2f}" x2="{MX(dr):.2f}" y2="{MY(base):.2f}" '
      f'stroke="{ink}" stroke-width="{sw*0.7:.2f}" stroke-linecap="round" opacity="{op*0.9:.3f}"/>')
    # lattice: two families of diagonal lines clipped to the dome interior.
    # sample points inside; draw short diagonals on a diamond grid.
    def inside(px, py):
        nx = (px - cx) / rx
        ny = (base - py) / ry
        return nx*nx + ny*ny <= 1.0 and py <= base
    step = (dr - dl) / 9.0
    diag = step
    # NE-SW diagonals
    for c in range(-9, 10):
        x0 = dl + c * step
        pts = []
        for k in range(0, 12):
            px = x0 + k * diag * 0.7
            py = base - k * diag * 0.7
            if inside(px, py): pts.append((px, py))
        if len(pts) >= 2:
            A(f'<line x1="{MX(pts[0][0]):.2f}" y1="{MY(pts[0][1]):.2f}" '
              f'x2="{MX(pts[-1][0]):.2f}" y2="{MY(pts[-1][1]):.2f}" stroke="{ink}" '
              f'stroke-width="{sw*0.34:.2f}" opacity="{op*0.55:.3f}"/>')
    # NW-SE diagonals
    for c in range(-9, 10):
        x0 = dl + c * step
        pts = []
        for k in range(0, 12):
            px = x0 + k * diag * 0.7
            py = base - k * diag * 0.7
            # mirror by flipping x direction
            px = x0 - k * diag * 0.7 + (dr - dl)
            if inside(px, py): pts.append((px, py))
        # simpler: redo with explicit NW-SE
    # NW-SE family (explicit)
    for c in range(-9, 10):
        x0 = dr - c * step
        pts = []
        for k in range(0, 12):
            px = x0 - k * diag * 0.7
            py = base - k * diag * 0.7
            if inside(px, py): pts.append((px, py))
        if len(pts) >= 2:
            A(f'<line x1="{MX(pts[0][0]):.2f}" y1="{MY(pts[0][1]):.2f}" '
              f'x2="{MX(pts[-1][0]):.2f}" y2="{MY(pts[-1][1]):.2f}" stroke="{ink}" '
              f'stroke-width="{sw*0.34:.2f}" opacity="{op*0.55:.3f}"/>')


def skyline_pulse(x, y, w, h, t, accent=None, op=1.0):
    """The 'Singapore Skyline-Pulse' — built from a close per-element study:
       MBS solid towers+slab, trumpet-canopy supertrees rising from a smooth red
       pulse wave, and a lattice esplanade dome terminus. Left/centre red; dome
       grey. UX 0..1000, UY 0..200."""
    import math
    UX, UY = 1000.0, 200.0
    MX, MY = _scaler(x, y, w, h, UX, UY)
    red = accent or t["accent"]
    ink = t["ink"]; bg = t["bg"]
    base = 140.0
    L = []; A = L.append
    sw = max(1.6, w / 190.0)

    # ── MBS (left) ──
    _mbs(MX, MY, base, red, sw, op, A)

    # ── pulse wave through the middle; trees rise from its crests ──
    trough_xs = [300, 366, 432]
    # smooth sine baseline that the trees sit on (gentle, not sharp V)
    dwave = f"M{MX(170):.2f} {MY(base):.2f} "
    wx0, wx1 = 250.0, 500.0
    for i in range(0, 49):
        u = i / 48
        wx = wx0 + (wx1 - wx0) * u
        wy = base - 14 * math.sin(2 * math.pi * 2.0 * u)
        dwave += f"L{MX(wx):.2f} {MY(wy):.2f} "
    dwave += f"L{MX(560):.2f} {MY(base):.2f} "
    A(f'<path d="{dwave}" fill="none" stroke="{red}" stroke-width="{sw*1.0:.2f}" '
      f'stroke-linecap="round" stroke-linejoin="round" opacity="{op:.3f}"/>')

    # ── supertrees rising from the wave crests ──
    for tx in trough_xs:
        # crest y at this x
        u = (tx - wx0) / (wx1 - wx0)
        cy = base - 14 * math.sin(2 * math.pi * 2.0 * u)
        _supertree(MX, MY, tx, cy, red, sw, op, A)

    # ── esplanade lattice dome (right, grey) ──
    _lattice_dome(MX, MY, 600.0, 724.0, base, 70.0, ink, sw, op, A)

    # ── connector baseline + dot.modular end nodes ──
    A(f'<line x1="{MX(50):.2f}" y1="{MY(base):.2f}" x2="{MX(732):.2f}" y2="{MY(base):.2f}" '
      f'stroke="{ink}" stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op*0.5:.3f}"/>')
    for dx in (50, 732):
        A(f'<circle cx="{MX(dx):.2f}" cy="{MY(base):.2f}" r="{6.0/UX*w:.2f}" '
          f'fill="none" stroke="{ink}" stroke-width="{sw*0.5:.2f}" opacity="{op*0.7:.3f}"/>')
    return "".join(L)


# ─────────────────────────────────────────────────────────────────────────────
# self-test render
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
