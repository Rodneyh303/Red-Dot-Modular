#!/usr/bin/env python3
"""panel_diff.py — render two panel SVGs and measure how different they are.

Makes "accurate" measurable for the Monsoon reverse-engineer (MONSOON_PANEL_REVERSE_ENGINEER.md step 2).

  python3 panel_src/panel_diff.py LIVE.svg CANDIDATE.svg [--scale 3] [--out /tmp/diff.png]
         [--no-components]

- Renders both with cairosvg at the same pixel size (scale x viewBox).
- --no-components strips the kit layers (`components`, invisible anchors) from BOTH before rendering,
  since anchors are display:none / unfilled and would only add noise.
- Reports: % pixels differing (> tolerance), mean abs diff, max diff, and the bounding box of the
  differing region — so you can see WHERE it's wrong, not just that it is.
- Writes a triptych PNG: live | candidate | amplified diff (red = mismatch).

Caveat: cairosvg is a proxy for Rack's nanosvg. The live panel is nanosvg-safe (no gradients/patterns/
url()/text), so for this panel the proxy is fair for the SVG layer. Runtime-drawn framing/labels
(MonsoonWidget::draw) are NOT in either SVG and cannot be checked here.
"""
import argparse, io, re, sys
import cairosvg
import numpy as np
from PIL import Image

TOL = 24  # per-channel difference below this counts as a match (anti-aliasing slack)


def strip_kit(svg: str) -> str:
    return re.sub(r'<g[^>]*id="components"[^>]*>.*?</g>', '', svg, flags=re.S)


def viewbox(svg: str):
    m = re.search(r'viewBox="([\d.\s-]+)"', svg)
    x, y, w, h = (float(v) for v in m.group(1).split())
    return w, h


def render(svg: str, W: int, H: int) -> np.ndarray:
    png = cairosvg.svg2png(bytestring=svg.encode(), output_width=W, output_height=H,
                           background_color="#000000")
    return np.asarray(Image.open(io.BytesIO(png)).convert("RGB"), dtype=np.int16)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("live"); ap.add_argument("cand")
    ap.add_argument("--scale", type=float, default=3.0)
    ap.add_argument("--out", default="/tmp/panel_diff.png")
    ap.add_argument("--no-components", action="store_true")
    a = ap.parse_args()

    s1 = open(a.live).read(); s2 = open(a.cand).read()
    if a.no_components:
        s1, s2 = strip_kit(s1), strip_kit(s2)
    w, h = viewbox(s1)
    W, H = int(round(w * a.scale)), int(round(h * a.scale))
    A, B = render(s1, W, H), render(s2, W, H)

    d = np.abs(A - B).max(axis=2)
    bad = d > TOL
    pct = 100.0 * bad.mean()
    print(f"render {W}x{H}  differing pixels: {pct:.3f}%  mean|d|: {np.abs(A-B).mean():.2f}  max|d|: {int(d.max())}")
    if bad.any():
        ys, xs = np.where(bad)
        print(f"diff bbox (viewBox units): x {xs.min()/a.scale:.1f}-{xs.max()/a.scale:.1f}  "
              f"y {ys.min()/a.scale:.1f}-{ys.max()/a.scale:.1f}")
    else:
        print("IDENTICAL within tolerance")

    diff = np.zeros_like(A); diff[..., 0] = np.clip(d * 4, 0, 255)
    trip = np.concatenate([A, B, diff], axis=1).astype(np.uint8)
    Image.fromarray(trip).save(a.out)
    print("wrote", a.out)
    return 0 if pct < 0.05 else 1


if __name__ == "__main__":
    sys.exit(main())
