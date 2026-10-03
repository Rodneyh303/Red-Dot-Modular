# Step 3+4 — Macro panel redesign: sends to RHS + helix room

## Current layout (48HP = 243.84mm × 128.5mm)

```
X:  0     40    80    120   160   200   240
|-----|-----|-----|-----|-----|-----|
| JACKS  |SPRD |     EDITOR      |OWN|DIR|MOD|PROB|
| 6-33   | 80  | 88-199          |205|212|220|236 |
| ATTEN  |     |                 |   |   |   |    |
| 43-70  |     |                 |   |   |   |    |
|-----|-----|-----|-----|-----|-----|-----|
|                                     |
|  HELIX ART (4,82, 74×33)           |
|                                     |
|  SEND GROUPS (Y=85-120)            |
|  5 groups × (4 sends + 2 taps)    |
|  below editor, horizontal          |
|-----|-----|-----|-----|-----|-----|
```

**Left section (X=1.5-86):** 4 CV jacks + 4 attens + 1 spread per lane (5 lanes)
**Center (X=88-199):** Editor grid (111mm wide, 5 lanes × 16 steps)
**Right (X=205-236):** Owner(205) + Dir(212) + DirMod(220) + ProbOut(236) — 4 columns
**Below editor (Y=85-120):** 5 send groups, each 22.2mm wide × 35mm tall
**Bottom-left (4,82):** Helix artwork (74×33mm)

## Problem

1. Sends are BELOW the editor (horizontal groups), not row-aligned to lanes
2. Only 5 send groups — needs 7 (POLY_LANES=7 now)
3. Helix is bottom-left, plan wants it on the LEFT side proper
4. Editor should move RIGHT to make room for helix on LEFT

## Proposed new layout

The sends become a 6-column × 7-row grid on the FAR RIGHT, each row aligned to its lane.
The helix moves to the LEFT, editor moves RIGHT.

```
X:  0     40    80    120   160   200   240
|-----|-----|-----|-----|-----|-----|-----|
|HELIX|JACKS|SPRD |     EDITOR      |SENDS|PROB|
|ART  |6-33 | 80  | 88-199          |200+|236+|
|     |ATTEN|     |                 |6col|    |
|     |43-70|     |                 |    |    |
|-----|-----|-----|-----|-----|-----|-----|
```

Wait — this doesn't work. The panel is only 243.84mm wide. Current right edge is 236.
Adding 6 send columns (~36mm) would push to 272mm — too wide.

## Alternative: sends REPLACE the below-editor space, row-aligned

Keep panel width. Move sends from below to the RIGHT of the editor, between editor
and the owner/dir/probout columns. This means the owner/dir/probout columns need to
shift right, or the editor needs to narrow.

**Option A: Narrow editor, sends on right**
- Editor: 88-170 (82mm, was 111mm — narrower but still usable)
- Sends: 172-200 (28mm = 6 columns × ~4.7mm each — tight but workable for trimpots)
- Owner/Dir/Mod/Prob: 205-236 (unchanged)
- Below editor: free for helix artwork (now wider)

**Option B: Widen panel to 52HP (264mm)**
- Adds 20mm for the send columns
- Less disruptive to existing geometry
- But changes panel width (needs new SVG, rack spacing)

**Option C: Sends in the below-editor space, but row-aligned (not grouped)**
- Keep sends below editor but arrange as a grid: 7 rows × 6 columns
- Each row aligns to a lane (Y = laneY(lane) + offset)
- Frees the right side for other uses
- Doesn't require panel widening

## Key question

The plan says "6 columns per-lane" and "row-aligned to the lane" — this means
each lane gets 6 send knobs in a horizontal strip at the lane's Y position.
With 7 lanes, that's 7 horizontal strips. But where do they go?

If below the editor: the below-editor space is 85-120mm (35mm tall). 7 strips
in 35mm = 5mm per strip — too tight for trimpots (need ~9mm).

If on the right side: need ~36mm of width for 6 columns. The current right
section (205-236) is only 31mm. Would need to narrow the editor or widen panel.

## Recommendation

Start with **Option A** (narrow editor, sends on right) — it keeps the panel
width unchanged and puts sends where the plan wants them (RHS, row-aligned).
The helix moves to the freed below-editor space (Step 4).
