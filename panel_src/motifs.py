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
    """The 'Singapore Skyline-Pulse': from left to right —
       MBS three towers + sky-deck, then three supertree fans, then a flat
       baseline that spikes into an ECG pulse, then the esplanade dome silhouette;
       all joined by one baseline with dot.modular connector dots at the ends.
       UX 0..1000, UY 0..200. Drawn in the accent (red)."""
    UX, UY = 1000.0, 200.0
    MX, MY = _scaler(x, y, w, h, UX, UY)
    red = accent or t["accent"]
    ink = t["ink"]
    base = 150.0
    L = []; A = L.append
    sw = max(2.0, w / 150.0)   # stroke scales with size

    def line(x1, y1, x2, y2, c=red, width=None, o=op):
        ww = width if width else sw
        A(f'<line x1="{MX(x1):.2f}" y1="{MY(y1):.2f}" x2="{MX(x2):.2f}" y2="{MY(y2):.2f}" '
          f'stroke="{c}" stroke-width="{ww:.2f}" stroke-linecap="round" opacity="{o:.3f}"/>')

    def path(d, c=red, width=None, o=op, fill="none"):
        ww = width if width else sw
        A(f'<path d="{d}" fill="{fill}" stroke="{c}" stroke-width="{ww:.2f}" '
          f'stroke-linecap="round" stroke-linejoin="round" opacity="{o:.3f}"/>')

    # ── MBS three towers + sky deck (left) ──
    for bx in [70, 120, 170]:
        line(bx, base, bx, 55)
    # the boat sky-deck across the tops (slightly angled)
    path(f"M{MX(55):.2f} {MY(52):.2f} L{MX(150):.2f} {MY(40):.2f} L{MX(185):.2f} {MY(46):.2f}")

    # ── supertree fans (middle-left): three fan/umbrella shapes ──
    for fx in [250, 320, 390]:
        # trunk
        line(fx, base, fx, 76)
        # fuller fan: more ribs, spreading wider into a canopy
        for dx in (-42, -28, -14, 0, 14, 28, 42):
            path(f"M{MX(fx):.2f} {MY(78):.2f} Q{MX(fx+dx*0.45):.2f} {MY(44):.2f} "
                 f"{MX(fx+dx):.2f} {MY(34):.2f}", width=sw*0.6)
        # canopy arc connecting the rib tips
        path(f"M{MX(fx-42):.2f} {MY(34):.2f} Q{MX(fx):.2f} {MY(26):.2f} {MX(fx+42):.2f} {MY(34):.2f}",
             width=sw*0.55)

    # ── the pulse: baseline that spikes (ECG) then settles ──
    # flat lead-in, sharp spike up+down, flat tail into the dome
    path(f"M{MX(440):.2f} {MY(base):.2f} "
         f"L{MX(560):.2f} {MY(base):.2f} "
         f"L{MX(600):.2f} {MY(52):.2f} "
         f"L{MX(640):.2f} {MY(base):.2f} "
         f"L{MX(700):.2f} {MY(base):.2f}")

    # ── esplanade dome silhouette (right) ──
    # half-dome outline (smooth, symmetric-ish durian shell)
    path(f"M{MX(735):.2f} {MY(base):.2f} "
         f"C{MX(740):.2f} {MY(74):.2f} {MX(820):.2f} {MY(56):.2f} {MX(875):.2f} {MY(58):.2f} "
         f"C{MX(925):.2f} {MY(60):.2f} {MX(950):.2f} {MY(105):.2f} {MX(950):.2f} {MY(base):.2f}")
    # evenly-spaced internal meridian facets
    for u in (0.25, 0.5, 0.75):
        mx = 735 + (950 - 735) * u
        path(f"M{MX(mx):.2f} {MY(base):.2f} Q{MX(mx):.2f} {MY(70):.2f} {MX(842):.2f} {MY(58):.2f}",
             width=sw*0.55)
    # one latitudinal band
    path(f"M{MX(755):.2f} {MY(108):.2f} Q{MX(842):.2f} {MY(86):.2f} {MX(935):.2f} {MY(110):.2f}",
         width=sw*0.55)

    # ── unifying baseline + dot.modular connector dots ──
    line(40, base, 960, base, width=sw*0.7, o=op*0.65)
    for dx, dr in [(40, 7), (960, 7)]:
        A(f'<circle cx="{MX(dx):.2f}" cy="{MY(base):.2f}" r="{dr/UX*w:.2f}" '
          f'fill="{red}" opacity="{op:.3f}"/>')
        A(f'<circle cx="{MX(dx):.2f}" cy="{MY(base):.2f}" r="{dr*0.45/UX*w:.2f}" '
          f'fill="{t["bg"]}"/>')

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
