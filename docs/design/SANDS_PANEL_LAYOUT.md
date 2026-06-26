# Sands Panel Layout — vertical space & lane-height notes

Status: **analysis only, not actioned.** Captured while converting Macro to
SvgPanelKit. Revisit once the core (lane order, kit binds, accent wiring) is
solid and built.

## Panel is 128.5mm tall. Current vertical zones (East / Macro 42HP)

| Zone | y (mm) | Notes |
|------|--------|-------|
| Branding strip + red accent rule | 0–11 | logo, tagline |
| View-tab band | ~11–22 | V1..V16 tabs |
| Editor | ED_Y=23 → 71 (ED_H=48) | 4 lanes @ 12mm each |
| Left controls | aligned to editor band (23–71) | 4 jacks + 4 attens + spread per lane |
| Lower band | 71–120 | **East:** mostly empty (BASE latches only). **Macro:** packed with send 2×2 grids (BLEND_TOP=72, BLEND_H=36 → 72–108). |
| Waves artwork | y≈112 | spans ED_X(88)→right edge |
| MBS identity mark | y≈111, lower-right | |
| Footer rule | y=120 | |

The slack is the **bottom-left** quadrant (left of ED_X=88, below ~71mm).
On East it's largely free; the waves/MBS sit on the right portion only.

## Lane-height parity with Mono

Mono lane pitch = (ROW_BOT 108 − ROW_TOP 14) / 6 lanes = **15.7mm/lane**.
East/Macro currently = ED_H 48 / 4 = **12mm/lane** (smaller, more cramped).

Two distinct goals that pull toward different editor heights:

1. **Exact parity with Mono (15.7mm/lane):** ED_H ≈ 63mm, editor bottom ≈ 86mm.
   Fits within existing space on East with only a small downward nudge of the
   waves/MBS. Modest change. This is the literal "lane heights match" goal.

2. **Fill the vertical slack (bigger than Mono):** ED_H ≈ 84mm, bottom ≈ 107mm,
   lanes ≈ 21mm. More legible but *exceeds* Mono's pitch (so not parity). This
   is the one that requires moving the waves/MBS artwork into the corner to
   clear the space.

These are in mild tension: parity does **not** require relocating the artwork;
only the "bigger lanes" option does.

## The Macro constraint

Macro's lower band (72–108mm) is fully occupied by the per-voice MIX-IN send
2×2 grids (4 lanes × 4 sends). Macro's editor **cannot** grow downward without
first moving or shrinking the send section. So:

- **East** can grow its editor freely (only BASE latches below).
- **Macro** is the binding constraint for any three-way lane-height parity.
- **Mono** uses nearly the full height already (14–108).

Full three-way parity therefore is **not free** — it needs a plan for Macro's
sends (relocate to a horizontal strip? collapse to a single row? move to a
context-menu / secondary page?).

## Artwork relocation idea (when option 2 is chosen)

Move the waves + MBS identity mark to the **bottom-left corner** (the currently
empty quadrant), freeing the full editor-width band below the editor so the
editor can extend down. Keep the MBS as a small crisp corner mark; let the
waves become a short corner motif rather than a full-width base band.

Touch points (all in `panel_src/`, then regenerate + the hpp ED_H/ED_LANE_H):
- `gen_east_clean.py`: `waves(ED_X, 112, ...)` and `mbs(W_MM-66, 111, ...)`
  → corner coordinates; bump `ED_H`.
- `gen_macro_mono.py`: same artwork move is cosmetic, but ED_H growth blocked
  until the send section is re-planned.
- `StraitsEastSandsVisual.hpp` / `StraitsSandsMacroVisual.hpp`: `ED_H` (and
  `ED_LANE_H = ED_H/4`) must mirror the gen script exactly (kit binds + prob-out
  rows + left-control rowY all derive from these).

## Related, larger idea (parked)

"Sands Mono as a special page/view of East" — if Mono's 6-lane editor is just
a different view of the same lane model East already implements, the three
panels could converge. This would also dissolve the lane-height parity problem
(one editor, one pitch). Explicitly **not** to be considered until everything
is solid; noted here only so the layout work above is understood as possibly
interim.
