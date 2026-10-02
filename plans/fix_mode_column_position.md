# FIX — mode column (C/G/P) overlaps the Flyer LED ring

## Root cause (confirmed by geometry)
The whole RIGHT CLUSTER was shifted right by `DX_RIGHT = 25.4mm` in monsoon_art.py — including the
Flyer / 16-step LED ring: `FLYER_C = (162.0 + DX_RIGHT, 30.0)` = actual centre x ≈ 187.4, radius 23.
But `panel_src/mode_column.py` still uses the STALE pre-shift ring centre `RING_C = (162.0, 30.0)` in
its clearance guard. So the guard passes against a ring position that no longer exists, while the real
shifted ring now reaches x ≈ 210–222 at the mode rows (y=13/22/31) — the C/G/P boxes at x≈188 sit
26–34mm INSIDE the ring. That is why CC couldn't fix it by nudging X: there is NO valid X in the
current row-Y band — the shifted ring reaches PAST the panel edge (203.2) there, so the column cannot
go to its right.

## Fix
The mode column did not follow the right-cluster `DX_RIGHT` shift into space that exists. Two routes:
1. **Move the rows OUT of the ring's vertical span.** The ring spans y ≈ 7..53 (centre 30, r 23). Put
   the 3 rows ABOVE (y < ~5) or BELOW (y > ~55) the ring, where its horizontal reach no longer collides.
   BELOW uses the ~27mm the 6→3 collapse FREED (rows 4..6 retired). This is the most likely clean fix.
2. Or re-place the column to the right of the SHIFTED ring — but geometry says there is no room
   (ring reaches past the 203.2 panel edge at the mode rows), so route 1 is preferred.

## Required edits
- In `mode_column.py`: update `RING_C` to the ACTUAL shifted centre `(162.0 + DX_RIGHT, 30.0)` (import
  or mirror DX_RIGHT = 25.4 so the two files cannot disagree again — ideally source it from one place),
  and move `ROW_Y` (and `LIGHT_X`/`BOX_CX` as needed) to a band that CLEARS the real ring. Keep the
  clearance assert — with the corrected RING_C it will now actually protect the layout.
- Regenerate the panel; verify in Rack the C/G/P boxes + LEDs clear the ring and the "MODE" title and
  descriptions still track.
- panel_diff to confirm nothing else moved.

## Lesson
DX_RIGHT is applied in monsoon_art.py to a list of elements (FLYER_C, PHASE_*, STATUS_DOT, logo...).
The mode column lives in a SEPARATE generator (mode_column.py) that hard-codes the OLD ring centre, so
it was not carried by the shift and its guard silently compared against a stale position. One shared
source for DX_RIGHT / RING_C across both files would prevent this class of drift (same lesson as the
PanelTokens single-source work in PANEL_CRAFT_AUDIT.md).
