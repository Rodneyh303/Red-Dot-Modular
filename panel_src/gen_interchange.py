#!/usr/bin/env python3
"""Interchange (Tonal CV expander) panel generator — dot.modular.

Produces the LIVE Interchange panels (res/panels/interchange_gemini_new2.svg +
_light.svg) so Interchange joins "generation for everything" — leaving only Monsoon
hand-authored.

APPROACH — faithful, in-Rack-correct. The hand-authored "gemini" panels are the
visual ground truth and are already almost entirely nanosvg-safe. The ONLY parts VCV's
nanosvg renderer cannot draw are three <defs>-based elements:
  • a `grain` <pattern> used for the background rect, and
  • two radialGradient glow fills (`hexGlow`/`hexGlowCool`) on the per-cell hex polygons.
In Rack those simply don't render — so reproducing the *in-Rack* look means emitting the
panel WITHOUT them. This generator reads the pristine source panels (kept in
panel_src/sources/) and applies exactly that nanosvg-safe transform:
  1. drop the <defs> block,
  2. replace the grain-pattern background rect with a flat fill (the pattern's base
     colour: #141416 dark / #f7f7f7 light),
  3. replace the radial-gradient hex glow fills with a faint FLAT tint (gold #d4af37 /
     teal #26a69a at low alpha) — a hair of warmth where Rack would otherwise show flat
     background, true to the design intent.
Everything else (hexes, spokes, gold vertex dots, connectors, diamonds, teal edge chain,
header accent, control wells, and the #components marker layer) is passed through verbatim
— so the geometry and the SvgPanelKit bind markers are byte-faithful to the original.

Run from repo root:  python3 panel_src/gen_interchange.py
"""
import re, os

SRC = "panel_src/sources"
OUT = "res/panels"

VARIANTS = [
    # (source file,                          output file,                     flat bg)
    ("interchange_gemini_dark.src.svg",  "interchange_gemini_new2.svg",  "#141416"),
    ("interchange_gemini_light.src.svg", "interchange_gemini_light.svg", "#f7f7f7"),
]

def rackify(src_text, bg_flat):
    """Strip the 3 nanosvg-unsupported def-based elements, flat equivalents in their place."""
    # 1. remove the entire <defs>...</defs> (pattern + radial gradients live only there)
    out = re.sub(r"<defs>.*?</defs>\s*", "", src_text, flags=re.S)
    # 2. grain-pattern background → flat base colour
    out = out.replace('<rect width="270" height="380" fill="url(#grain)" />',
                      f'<rect width="270" height="380" fill="{bg_flat}" />')
    # 3. radial-gradient hex glows → faint flat tints (cool=teal, warm=gold)
    out = out.replace('fill="url(#hexGlowCool)"', 'fill="#26a69a" fill-opacity="0.05"')
    out = out.replace('fill="url(#hexGlow)"',     'fill="#d4af37" fill-opacity="0.06"')
    return out

def main():
    if not os.path.isdir(SRC):
        raise SystemExit(f"missing {SRC}/ — run from repo root; need the pristine source panels")
    for src_name, out_name, bg in VARIANTS:
        src_path = os.path.join(SRC, src_name)
        with open(src_path) as f:
            src_text = f.read()
        out_text = rackify(src_text, bg)
        leftover = out_text.count("url(")
        if leftover:
            raise SystemExit(f"{out_name}: {leftover} url() refs remain — nanosvg-unsafe")
        out_path = os.path.join(OUT, out_name)
        with open(out_path, "w") as f:
            f.write(out_text)
        # marker sanity
        markers = out_text.count('id="param_') + out_text.count('id="input_') + out_text.count('id="output_')
        print(f"wrote {out_path}  ({out_text.count(chr(10))+1} lines, {markers} component markers, nanosvg-safe)")

if __name__ == "__main__":
    main()
