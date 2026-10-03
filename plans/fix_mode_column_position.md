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


## UPDATE 2 — THREE coordinated fixes (why nudging keeps failing)
Screenshot after the last attempt shows: (a) the MODE cycle BUTTON stranded in the lower-right
corner (BTN=(194,60), y=60 is on the BPM/LEN/OFFSET knob line) far from the C/G/P stack it controls;
(b) the C/G/P column NOT vertically centred on the ring — ROW_Y=[13,22,31] centres on y=22, but the
ring centre is y=30 (FLYER_C.y), so it rides high. These must be fixed TOGETHER with the stale-RING_C
fix, because all three interact:

1. **RING_C is STILL stale.** mode_column.py line 28: `RING_C=(162.0,30.0)`. The real ring is at
   `(162+DX_RIGHT, 30) = (187.4, 30)`. Fix first (import/share DX_RIGHT=25.4 from one place).
2. **Vertically centre the column on the ring:** ROW_Y -> `[21.0, 30.0, 39.0]` (same ~9mm pitch,
   centred on y=30). This is INSIDE the ring's vertical span AND near its widest horizontal reach.
3. **So the column CANNOT sit beside the ring** (it reaches x~210 there, past the 203.2 panel edge).
   The column + its cycle BUTTON must move to a band that clears the real ring:
   EITHER move the whole group (BTN + ROW_Y + LIGHT_X/BOX_CX) BELOW the ring (rows y>~55, into the
   ~27mm the 6->3 collapse freed) and keep them vertically grouped as a unit;
   OR keep rows centred on y=30 but place the column to the LEFT of the ring (x well below
   187.4-23=164.4) if that region is free.
   Pick one; the BUTTON must end up ADJACENT to the C/G/P stack (grouped), not on the knob row.

**Decide the target region first (below-ring vs left-of-ring), then place button+column+rows as ONE
group in it, with RING_C corrected so the clearance assert actually guards.** Do not nudge
individual values — that is why it keeps breaking one thing while fixing another. Regenerate,
Rack-check: button grouped with C/G/P, column clears the ring, letters track their LEDs.
