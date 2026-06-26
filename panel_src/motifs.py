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
    """Marina Bay Sands as a SOLID red silhouette (measured from the concept:
    the three towers + SkyPark slab read as one filled mass ~88px×53px). Three
    pillars joined by a continuous overhanging deck across the top, drawn as a
    single filled path so it reads solid, not as separate stumps."""
    # measured UX block 52..335, top 15, base 126 (mapped). Tower tops meet deck.
    bx0, bx1 = 60.0, 250.0          # block left/right
    deck_top = 14.0
    deck_bot = 30.0
    tw = (bx1 - bx0)
    # three pillars
    pw = 30.0                       # pillar width
    gaps = [bx0, bx0 + (tw-pw)/2, bx1-pw]   # left, centre, right pillar x-starts
    # one filled path: deck slab (overhanging) + three pillars hanging below it
    d = (f"M{MX(bx0-10):.2f} {MY(deck_top):.2f} "
         f"L{MX(bx1+10):.2f} {MY(deck_top):.2f} "
         f"L{MX(bx1+10):.2f} {MY(deck_bot):.2f} ")
    # right pillar down, across to its left edge, up to deck — then notch to next
    # Simpler & robust: deck as a rect, pillars as rects, all same red (fused look)
    A(f'<rect x="{MX(bx0-10):.2f}" y="{MY(deck_top):.2f}" '
      f'width="{MX(bx1+10)-MX(bx0-10):.2f}" height="{MY(deck_bot)-MY(deck_top):.2f}" '
      f'fill="{red}" opacity="{op:.3f}"/>')
    for gx in gaps:
        A(f'<rect x="{MX(gx):.2f}" y="{MY(deck_bot):.2f}" '
          f'width="{MX(gx+pw)-MX(gx):.2f}" height="{MY(base)-MY(deck_bot):.2f}" '
          f'fill="{red}" opacity="{op:.3f}"/>')


def _supertree(MX, MY, tx, tbase, height, rim, ink, red, sw, op, A):
    """A Gardens-by-the-Bay supertree of a GIVEN height/rim (they differ in the
    concept). Trunk rises from the wave (tbase) to a canopy; branches splay out
    AND continue ABOVE the canopy rim (the tips overshoot the bell mouth, as in
    the concept). Concave ribs."""
    top = tbase - height
    stem_top = tbase - height * 0.45
    # trunk
    A(f'<line x1="{MX(tx):.2f}" y1="{MY(tbase):.2f}" x2="{MX(tx):.2f}" y2="{MY(stem_top):.2f}" '
      f'stroke="{red}" stroke-width="{sw*0.9:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')
    ribs = 7
    for r in range(ribs):
        f = r / (ribs - 1)                      # 0..1
        # tips overshoot ABOVE the rim: middle ribs highest, outer ones flare wide
        tipx = tx + (f - 0.5) * 2 * rim
        tipy = top - (1 - abs(f - 0.5) * 2) * height * 0.18   # centre tips highest, above rim
        cx = tx + (f - 0.5) * rim * 0.30        # hug trunk low
        cy = stem_top - (stem_top - top) * 0.35
        A(f'<path d="M{MX(tx):.2f} {MY(stem_top):.2f} Q{MX(cx):.2f} {MY(cy):.2f} '
          f'{MX(tipx):.2f} {MY(tipy):.2f}" fill="none" stroke="{red}" '
          f'stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')
    # canopy rim arc (the bell mouth) sits BELOW the overshooting tips
    A(f'<path d="M{MX(tx-rim):.2f} {MY(top+height*0.10):.2f} '
      f'Q{MX(tx):.2f} {MY(top-height*0.04):.2f} {MX(tx+rim):.2f} {MY(top+height*0.10):.2f}" '
      f'fill="none" stroke="{red}" stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op:.3f}"/>')


def _wave_through_trees(MX, MY, base, trees, x_in, x_out, red, sw, op, A):
    """Red pulse wave: bigger amplitude, dips BELOW the baseline between trees and
    rises to meet each trunk base (the wave and trunks are one continuous line —
    skyline morphing into signal). `trees` = list of (tx, tbase, ...). Returns the
    tree bases as points the wave passes through."""
    import math
    amp = 30.0          # bigger amplitude (measured: troughs well below baseline)
    pts = [(x_in, base)]
    txs = [tr[0] for tr in trees]
    # weave: trough before first tree, up to each trunk base, trough between, etc.
    prev = x_in
    for i, (tx, tbase) in enumerate([(t[0], t[1]) for t in trees]):
        # trough midway between prev structure and this trunk
        mid = (prev + tx) / 2
        pts.append((mid, base + amp))          # dip below baseline
        pts.append((tx, tbase))                # rise to the trunk base
        prev = tx
    pts.append(((prev + x_out) / 2, base + amp * 0.7))
    pts.append((x_out, base))
    # build a smooth-ish polyline (cardinal-style via quadratics between pts)
    d = f"M{MX(pts[0][0]):.2f} {MY(pts[0][1]):.2f} "
    for i in range(1, len(pts)):
        x0, y0 = pts[i-1]; x1, y1 = pts[i]
        mx = (x0 + x1) / 2
        d += f"Q{MX(x0):.2f} {MY(y0):.2f} {MX(mx):.2f} {MY((y0+y1)/2):.2f} "
        d += f"Q{MX(x1):.2f} {MY(y1):.2f} {MX(x1):.2f} {MY(y1):.2f} "
    A(f'<path d="{d}" fill="none" stroke="{red}" stroke-width="{sw*1.0:.2f}" '
      f'stroke-linecap="round" stroke-linejoin="round" opacity="{op:.3f}"/>')


def _lattice_dome(MX, MY, dl, dr, base, dapex, ink, sw, op, A):
    """Esplanade with PERSPECTIVE (measured: shallow wide dome, apex flat across
    the middle, and a CURVED bottom lip — the platform seen at a slight downward
    angle so the front edge bows lower in the centre). Lattice diamond grid
    inside, clipped to the dome shell."""
    import math
    cx = (dl + dr) / 2
    rx = (dr - dl) / 2
    ry = base - dapex
    lip = 14.0   # how much the front platform lip bows below the chord (perspective)

    # top shell: flattened arch (apex broad/flat, measured)
    def top_y(px):
        nx = (px - cx) / rx                  # -1..1
        # flattened cosine: |nx|^1.6 keeps the apex broad
        return dapex + ry * (abs(nx) ** 1.6)
    # bottom lip: a shallow downward arc (perspective ellipse front edge)
    def bot_y(px):
        nx = (px - cx) / rx
        return base + lip * (1 - nx * nx)    # bows DOWN in the middle

    # outline: across the top, down the right, back along the curved front lip
    d = f"M{MX(dl):.2f} {MY(top_y(dl)):.2f} "
    N = 36
    for i in range(1, N + 1):
        px = dl + (dr - dl) * i / N
        d += f"L{MX(px):.2f} {MY(top_y(px)):.2f} "
    for i in range(N + 1):                   # back along the front lip (right→left)
        px = dr - (dr - dl) * i / N
        d += f"L{MX(px):.2f} {MY(bot_y(px)):.2f} "
    d += "Z"
    A(f'<path d="{d}" fill="none" stroke="{ink}" stroke-width="{sw*0.9:.2f}" '
      f'stroke-linejoin="round" stroke-linecap="round" opacity="{op*0.9:.3f}"/>')

    # lattice diamond grid inside the shell (between top_y and bot_y)
    def inside(px, py):
        return top_y(px) <= py <= bot_y(px) and dl <= px <= dr
    step = (dr - dl) / 11.0
    # two diagonal families forming diamonds
    for sign in (+1, -1):
        for c in range(-12, 13):
            x_start = cx + c * step
            pts = []
            for k in range(0, 60):
                px = x_start + sign * k * step * 0.5
                py = base + lip - k * step * 0.5      # rise from the lip upward
                if inside(px, py):
                    pts.append((px, py))
                elif pts:
                    break
            if len(pts) >= 2:
                A(f'<line x1="{MX(pts[0][0]):.2f}" y1="{MY(pts[0][1]):.2f}" '
                  f'x2="{MX(pts[-1][0]):.2f}" y2="{MY(pts[-1][1]):.2f}" stroke="{ink}" '
                  f'stroke-width="{sw*0.32:.2f}" opacity="{op*0.5:.3f}"/>')


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

    # ── supertrees of DIFFERENT heights (measured: middle tallest), with the red
    #    pulse wave weaving below the baseline and rising into each trunk base ──
    # (tx, height, rim) — middle tree tallest & widest, per the concept
    trees = [(300, 70.0, 24.0), (372, 118.0, 34.0), (444, 92.0, 28.0)]
    # the wave passes through each trunk base at the baseline; draw it first
    _wave_through_trees(MX, MY, base, [(tx, base) for tx, _, _ in trees],
                        180.0, 556.0, red, sw, op, A)
    for tx, hgt, rim in trees:
        _supertree(MX, MY, tx, base, hgt, rim, ink, red, sw, op, A)

    # ── esplanade lattice dome with perspective (right, grey) ──
    _lattice_dome(MX, MY, 600.0, 730.0, base, 64.0, ink, sw, op, A)

    # ── connector baseline + dot.modular end nodes ──
    A(f'<line x1="{MX(50):.2f}" y1="{MY(base):.2f}" x2="{MX(160):.2f}" y2="{MY(base):.2f}" '
      f'stroke="{ink}" stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op*0.5:.3f}"/>')
    A(f'<line x1="{MX(560):.2f}" y1="{MY(base):.2f}" x2="{MX(600):.2f}" y2="{MY(base):.2f}" '
      f'stroke="{ink}" stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op*0.5:.3f}"/>')
    A(f'<line x1="{MX(730):.2f}" y1="{MY(base):.2f}" x2="{MX(770):.2f}" y2="{MY(base):.2f}" '
      f'stroke="{ink}" stroke-width="{sw*0.5:.2f}" stroke-linecap="round" opacity="{op*0.5:.3f}"/>')
    for dx in (50, 770):
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
