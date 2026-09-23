#!/usr/bin/env python3
"""ca_clearance.py — report the CA header clearances after the 60HP label/mark fixes.
Imports the generator's constants so the numbers are the real ones, not hand-copied."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import gen_change_alley_v2 as g

blockCx = (g.BTN_D + g.REV_C) * 0.5          # button-cluster centre (INTRA frame) — label anchor
lbl_inter = g.PW_MM - blockCx                # COLLAPSE INTER label centre (mirrored)
lbl_intra = blockCx                          # COLLAPSE INTRA label centre
hs = g.GRID_X + g.GRID_W * 0.5               # mark-row centre (matrix centre)
hs_lo = hs - 3.5 * g.HOSTSLOT_P - g.HOSTSLOT_R
hs_hi = hs + 3.5 * g.HOSTSLOT_P + g.HOSTSLOT_R

print("PW_MM=%.1f  HP=%d" % (g.PW_MM, g.HP))
print("EXPR_X=%.2f  J_DOM=%.2f  (expr column centred in the %.2fmm edge->J_DOM gutter)"
      % (g.EXPR_X, g.J_DOM, g.J_DOM))
print("mark row: centre=%.1f  spans %.1f..%.1f  pitch=%.1f  y=%.1f  r=%.1f"
      % (hs, hs_lo, hs_hi, g.HOSTSLOT_P, g.HOSTSLOT_Y, g.HOSTSLOT_R))
print("COLLAPSE INTRA label x=%.1f   COLLAPSE INTER label x=%.1f  (block-derived)"
      % (lbl_intra, lbl_inter))
print("horizontal clearance: mark-row right edge (%.1f) -> INTER label (%.1f) = %.1f mm"
      % (hs_hi, lbl_inter, lbl_inter - hs_hi))
