#!/usr/bin/env bash
# verify_qmix_geometry.sh — the step-5 verification for feat/sands-qmix-geometry.
# Pads the pre-change (40HP) panels onto the post-change (45HP) canvas (same px/mm,
# same left origin) then runs panel_diff.py per theme. Differences should be confined
# to the right-hand cluster, the new 6th knob and the spread trees.
set -e
export PATH=/usr/bin:/c/msys64/mingw64/bin:$PATH
cd "$(dirname "$0")/../.."   # repo root (Red-Dot-Modular)

PRE=/tmp/panel_pre
POST=/tmp/panel_post

for th in dark light; do
  python panel_src/tools/pad_pre.py \
    "$PRE/Monsoon_panel_${th}_monsoon.svg" \
    "$POST/Monsoon_panel_${th}_monsoon.svg" \
    "$PRE/padded_${th}.svg"
done

for th in dark light; do
  echo "==================== THEME: ${th} ===================="
  python panel_src/panel_diff.py \
    "$PRE/padded_${th}.svg" \
    "$POST/Monsoon_panel_${th}_monsoon.svg" \
    --no-components --out "/tmp/panel_diff_${th}.png" || true
done
