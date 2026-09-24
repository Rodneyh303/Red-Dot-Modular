#!/usr/bin/env bash
# check_anchors.sh — list any bind id in MonsoonWidget.cpp that has NO matching anchor id in the
# generated dark panel (i.e. "anchors the kit can't find"). Empty output = all resolve.
set -e
export PATH=/usr/bin:/c/msys64/mingw64/bin:$PATH
cd "$(dirname "$0")/../.."

grep -oE 'bind(Param|Light|Input|Output|LightParam)<[^(]*>\("[a-zA-Z0-9_]+"' src/MonsoonWidget.cpp \
  | grep -oE '"[a-zA-Z0-9_]+"' | tr -d '"' | sort -u > /tmp/binds.txt

grep -oE 'id="[a-zA-Z0-9_]+"' res/panels/Monsoon_panel_dark_monsoon.svg \
  | grep -oE '"[a-zA-Z0-9_]+"' | tr -d '"' | sort -u > /tmp/anchors.txt

echo "widget bind ids: $(wc -l < /tmp/binds.txt)   panel anchor ids: $(wc -l < /tmp/anchors.txt)"
echo "=== bind ids MISSING a panel anchor (kit can't find) ==="
comm -23 /tmp/binds.txt /tmp/anchors.txt
echo "=== end (empty above = every bind resolves) ==="
