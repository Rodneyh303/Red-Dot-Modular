#!/usr/bin/env python3
"""ca_clearance.py — report the CA header clearances after the 60HP label/mark fixes.
Imports the generator's constants so the numbers are the real ones, not hand-copied."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import gen_change_alley_v2 as g

blockCx  = (g.BTN_D + g.REV_C) * 0.5         # button-cluster centre (INTRA frame) — label anchor
lbl_inter = g.PW_MM - blockCx                # COLLAPSE INTER label centre (mirrored, block-derived)
lbl_intra = blockCx                          # COLLAPSE INTRA label centre
title_cx  = g.PW_MM * 0.5                     # "CHANGE ALLEY" centred by the widget
title_right = title_cx + g.TITLE_HALF_W       # estimated title right edge
hs_x0 = title_cx + g.TITLE_HALF_W + g.TITLE_GAP + g.HOSTSLOT_R    # first mark centre
hs_lo = hs_x0 - g.HOSTSLOT_R
hs_hi = hs_x0 + 7 * g.HOSTSLOT_P + g.HOSTSLOT_R

print("PW_MM=%.1f  HP=%d" % (g.PW_MM, g.HP))
print("EXPR_X=%.2f  J_DOM=%.2f  (edge gap %.2f  >  inner gap to J_DOM %.2f)"
      % (g.EXPR_X, g.J_DOM, g.EXPR_X, g.J_DOM - g.EXPR_X))
print("title centre=%.1f  est. right edge=%.1f" % (title_cx, title_right))
print("mark row: first=%.1f  last=%.1f  spans %.1f..%.1f  pitch=%.1f  y=%.1f"
      % (hs_x0, hs_x0 + 7*g.HOSTSLOT_P, hs_lo, hs_hi, g.HOSTSLOT_P, g.HOSTSLOT_Y))
print("COLLAPSE INTRA label x=%.1f   COLLAPSE INTER label x=%.1f  (block-derived)"
      % (lbl_intra, lbl_inter))
print("clearance: title right (%.1f) -> first mark (%.1f) = %.1f mm"
      % (title_right, hs_x0, hs_x0 - title_right))
print("clearance: mark-row right edge (%.1f) -> INTER label (%.1f) = %.1f mm"
      % (hs_hi, lbl_inter, lbl_inter - hs_hi))
