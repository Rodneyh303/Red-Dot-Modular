#!/usr/bin/env python3
"""pad_pre.py — widen a pre-change (40HP) Monsoon SVG canvas to the post-change (45HP)
width WITHOUT moving any content, so panel_diff.py can overlay them on a common canvas
(same px/mm, same left origin). Only the <svg width=/viewBox=> attributes change; every
drawn coordinate stays put, so the left ~40HP is a true apples-to-apples overlay and the
extra right strip is empty (any diff there is the intended new geometry).

usage: python pad_pre.py PRE.svg POST.svg OUT.svg
"""
import re, sys

pre_path, post_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]

post = open(post_path).read()
m = re.search(r'viewBox="0 0 ([0-9.]+) ([0-9.]+)"', post)
W = m.group(1)

pre = open(pre_path).read()
pre = re.sub(r'width="[0-9.]+"', 'width="%s"' % W, pre, count=1)
pre = re.sub(r'viewBox="0 0 [0-9.]+ ([0-9.]+)"',
             lambda mm: 'viewBox="0 0 %s %s"' % (W, mm.group(1)), pre, count=1)
open(out_path, "w").write(pre)
print("padded ->", re.search(r'viewBox="[^"]+"', pre).group(0))
