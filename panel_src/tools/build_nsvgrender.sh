#!/bin/sh
# Build nsvgrender: rasterizes SVGs with nanosvg — the SVG PARSER Rack uses — so paint/opacity
# semantics match Rack (e.g. opacity is NOT compounded through <g>; innermost wins).
# Rack draws the parsed shapes with NanoVG rather than nanosvgrast, so anti-aliasing differs
# slightly; paint semantics are identical. Headers fetched from upstream memononen/nanosvg.
set -e
cd "$(dirname "$0")"
[ -f nanosvg.h ]     || curl -sSfL -o nanosvg.h     https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h
[ -f nanosvgrast.h ] || curl -sSfL -o nanosvgrast.h https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h
cc -O2 -o nsvgrender nsvgrender.c -lm
echo "built $(pwd)/nsvgrender"
