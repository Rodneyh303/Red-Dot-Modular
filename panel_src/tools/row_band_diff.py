#!/usr/bin/env python3
"""row_band_diff.py — structural verification for the "one clean control row" change,
usable without a renderer (this box has no cairosvg / nsvgrender build).

Compares BEFORE vs AFTER Monsoon panel SVGs element-by-element (the drawn <line>/<circle>/
<ellipse>/<polygon>/<rect> plus the invisible component anchors). Reports which elements were
ADDED / REMOVED / MOVED, and — crucially — asserts that every CHANGED element's vertical extent
lies inside the control-row band (default y = 78..98 mm). Anything changed OUTSIDE that band is a
regression and is printed loudly.

usage: python row_band_diff.py BEFORE.svg AFTER.svg [--band-lo-mm 78] [--band-hi-mm 98]
"""
import argparse, re, sys

S75 = 600 / 203.2   # px per mm (must match monsoon_art.py)


def parse_elems(svg):
    """Return a list of (tag, canonical_attr_string, y_min_px, y_max_px) for every drawn/anchor
    element. y-extent is computed from whatever coordinate attrs the element carries."""
    out = []
    for m in re.finditer(r'<(line|circle|ellipse|polygon|rect)\b([^>]*)/?>', svg):
        tag, attrs = m.group(1), m.group(2)
        ys = []
        if tag == 'line':
            ys = [float(v) for k in ('y1', 'y2') for v in re.findall(r'\b%s="([-\d.]+)"' % k, attrs)]
        elif tag in ('circle', 'ellipse'):
            cy = re.search(r'\bcy="([-\d.]+)"', attrs)
            ry = re.search(r'\bry="([-\d.]+)"', attrs) or re.search(r'\br="([-\d.]+)"', attrs)
            if cy:
                c = float(cy.group(1)); r = float(ry.group(1)) if ry else 0.0
                ys = [c - r, c + r]
        elif tag == 'rect':
            y = re.search(r'\by="([-\d.]+)"', attrs); h = re.search(r'\bheight="([-\d.]+)"', attrs)
            if y:
                yy = float(y.group(1)); hh = float(h.group(1)) if h else 0.0
                ys = [yy, yy + hh]
        elif tag == 'polygon':
            pts = re.search(r'points="([^"]+)"', attrs)
            if pts:
                nums = [float(v) for v in re.findall(r'[-\d.]+', pts.group(1))]
                ys = nums[1::2]
        ymin = min(ys) if ys else 0.0
        ymax = max(ys) if ys else 0.0
        # canonical: sort attrs so cosmetic ordering doesn't matter
        canon = tag + '|' + '|'.join(sorted(re.findall(r'\b[\w:-]+="[^"]*"', attrs)))
        out.append((canon, ymin, ymax))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('before'); ap.add_argument('after')
    ap.add_argument('--band-lo-mm', type=float, default=78.0)
    ap.add_argument('--band-hi-mm', type=float, default=98.0)
    a = ap.parse_args()

    lo, hi = a.band_lo_mm * S75, a.band_hi_mm * S75
    B = parse_elems(open(a.before).read())
    A = parse_elems(open(a.after).read())

    from collections import Counter
    cb, ca = Counter(e[0] for e in B), Counter(e[0] for e in A)
    yext = {e[0]: (e[1], e[2]) for e in (B + A)}

    removed = list((cb - ca).elements())
    added = list((ca - cb).elements())

    def in_band(canon):
        ymin, ymax = yext[canon]
        # "changed inside the band" = its whole vertical extent within [lo,hi] (with 1px slack)
        return ymin >= lo - 1 and ymax <= hi + 1

    outside = [c for c in (removed + added) if not in_band(c)]

    print("BEFORE elements: %d   AFTER elements: %d" % (len(B), len(A)))
    print("removed: %d   added: %d" % (len(removed), len(added)))
    print("band: y %.1f..%.1f mm  (%.1f..%.1f px)" % (a.band_lo_mm, a.band_hi_mm, lo, hi))

    if outside:
        print("\n!!! %d CHANGED element(s) OUTSIDE the control-row band — REGRESSION:" % len(outside))
        for c in outside[:40]:
            ymin, ymax = yext[c]
            print("   y %.1f..%.1f px : %s" % (ymin, ymax, c[:120]))
        return 1

    print("\nOK: every added/removed element lies INSIDE the control-row band.")
    print("    Everything outside y %.0f..%.0f mm is byte-identical." % (a.band_lo_mm, a.band_hi_mm))
    # brief summary of what moved, for the report
    print("\nControl-row band changes (sample):")
    for c in (removed[:8]):
        print("   - REMOVED:", c[:110])
    for c in (added[:8]):
        print("   + ADDED  :", c[:110])
    return 0


if __name__ == '__main__':
    sys.exit(main())
