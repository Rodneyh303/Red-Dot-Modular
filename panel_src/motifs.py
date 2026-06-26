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
def skyline_pulse(x, y, w, h, t, accent=None, op=1.0):
    """The 'Singapore Skyline-Pulse' from the concept sheet, studied closely:

      • MBS: three TAPERED towers under one overhanging sky-deck bar.
      • Supertrees: each a CLUSTER of thin curved lines radiating from a single
        base point, splaying up-and-out into a fountain/canopy (concave ribs,
        wider at top) — NOT a trunk+blob.
      • The towers/trees sit on a red SINE WAVEFORM (the 'pulse') that undulates
        2–3 cycles along the baseline and flows into the dome — skyline morphing
        into signal.
      • Esplanade dome: a faceted grey half-dome terminus on the right (NOT red).
      • A thin connector baseline with dot.modular circle-nodes at both ends.

    Left/centre in accent red; dome in ink grey. UX 0..1000, UY 0..200."""
    import math
    UX, UY = 1000.0, 200.0
    MX, MY = _scaler(x, y, w, h, UX, UY)
    red = accent or t["accent"]
    ink = t["ink"]; bg = t["bg"]
    base = 140.0
    L = []; A = L.append
    sw = max(1.6, w / 190.0)

    def line(x1, y1, x2, y2, c=red, width=None, o=op):
        ww = width if width else sw
        A(f'<line x1="{MX(x1):.2f}" y1="{MY(y1):.2f}" x2="{MX(x2):.2f}" y2="{MY(y2):.2f}" '
          f'stroke="{c}" stroke-width="{ww:.2f}" stroke-linecap="round" opacity="{o:.3f}"/>')

    def path(d, c=red, width=None, o=op, fill="none"):
        ww = width if width else sw
        A(f'<path d="{d}" fill="{fill}" stroke="{c}" stroke-width="{ww:.2f}" '
          f'stroke-linecap="round" stroke-linejoin="round" opacity="{o:.3f}"/>')

    # ── MBS: three tapered towers + overhanging sky-deck (left) ──
    deck_y = 48.0
    for bx in (90, 122, 154):
        topw, botw = 4.0, 7.0
        path(f"M{MX(bx-botw/2):.2f} {MY(base):.2f} L{MX(bx-topw/2):.2f} {MY(deck_y+3):.2f} "
             f"L{MX(bx+topw/2):.2f} {MY(deck_y+3):.2f} L{MX(bx+botw/2):.2f} {MY(base):.2f}",
             fill=red, width=sw*0.4)
    path(f"M{MX(74):.2f} {MY(deck_y):.2f} L{MX(170):.2f} {MY(deck_y):.2f}", width=sw*1.8)

    # ── the pulse waveform: a flat baseline that, in the MIDDLE, dips into sharp
    #    V-troughs; the supertrees rise as tall narrow fountains FROM those troughs
    #    (the trunks ARE the upstroke of the wave — skyline morphing into signal). ──
    trough_xs = [298, 364, 430]      # three supertrees
    twidth = 24.0
    tree_top = 18.0                  # rise higher (taller fountains)
    dwave = f"M{MX(60):.2f} {MY(base):.2f} L{MX(trough_xs[0]-twidth-12):.2f} {MY(base):.2f} "
    for i, tx in enumerate(trough_xs):
        dwave += (f"L{MX(tx-10):.2f} {MY(base+22):.2f} "    # sharper, deeper V trough
                  f"L{MX(tx):.2f} {MY(base):.2f} ")
        if i < len(trough_xs) - 1:
            nx = trough_xs[i+1]
            dwave += f"L{MX((tx+nx)/2):.2f} {MY(base-12):.2f} "
    dwave += f"L{MX(trough_xs[-1]+twidth+12):.2f} {MY(base):.2f} L{MX(556):.2f} {MY(base):.2f} "
    path(dwave, width=sw*1.05)

    # ── supertrees: tall delicate fountains rising from each trough ──
    for tx in trough_xs:
        ribs = 9
        spread = 20.0
        for r in range(ribs):
            f = r / (ribs - 1)
            ex = tx + (f - 0.5) * 2 * spread
            ey = tree_top + abs(f - 0.5) * 22     # outer ribs splay down/out
            cx = tx + (f - 0.5) * spread * 0.3    # ribs hug the trunk low, splay high
            cy = base - (base - tree_top) * 0.42
            path(f"M{MX(tx):.2f} {MY(base):.2f} Q{MX(cx):.2f} {MY(cy):.2f} "
                 f"{MX(ex):.2f} {MY(ey):.2f}", width=sw*0.38)

    # ── esplanade dome terminus (grey, FLAT wide half-dome on baseline) ──
    dl, dr, dapex = 596.0, 722.0, 78.0
    cx_dome = (dl + dr) / 2
    path(f"M{MX(dl):.2f} {MY(base):.2f} "
         f"C{MX(dl+6):.2f} {MY(dapex+18):.2f} {MX(cx_dome-38):.2f} {MY(dapex):.2f} {MX(cx_dome):.2f} {MY(dapex):.2f} "
         f"C{MX(cx_dome+38):.2f} {MY(dapex):.2f} {MX(dr-6):.2f} {MY(dapex+18):.2f} {MX(dr):.2f} {MY(base):.2f}",
         c=ink, width=sw*1.0, o=op*0.9)
    for u in (0.3, 0.5, 0.7):
        mx = dl + (dr - dl) * u
        bow = dapex + 8 + abs(u - 0.5) * 26
        path(f"M{MX(mx):.2f} {MY(base):.2f} Q{MX(mx):.2f} {MY(bow):.2f} {MX(cx_dome):.2f} {MY(dapex+3):.2f}",
             c=ink, width=sw*0.38, o=op*0.6)

    # ── connector baseline + dot.modular end nodes ──
    line(50, base, 730, base, c=ink, width=sw*0.55, o=op*0.5)
    for dx in (50, 730):
        A(f'<circle cx="{MX(dx):.2f}" cy="{MY(base):.2f}" r="{6.0/UX*w:.2f}" '
          f'fill="none" stroke="{ink}" stroke-width="{sw*0.55:.2f}" opacity="{op*0.75:.3f}"/>')
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
