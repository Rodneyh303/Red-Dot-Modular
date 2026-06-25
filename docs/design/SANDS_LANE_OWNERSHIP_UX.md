# Sands — Lane Ownership UX options

Status: **options for discussion, not actioned.** Supersedes/expands
SANDS_COMBINATIONS.md question B and the UX note in
SANDS_ARCHITECTURE_CONSOLIDATION.md.

## The problem

Ownership = per lane, per poly voice, does the lane's base come from **East**
(this voice's own LOR edit) or **Macro** (the global base)? Today it's toggled
via a right-click context menu on the editor lane row.

**Why the context menu is fiddly (root cause):** the editor lane row *is the
data-editing surface*. `SandsVisualEditorV4::onButton` hit-tests LOR handles
and the window on that exact row (left-click drag = edit length/offset/
rotation). Right-click has a clean callback path (`onLaneRightClick`), but it
shares the same spatial zone as the draggable data, so:
- it's easy to be a few px off and grab a handle instead,
- in VCV a stray right-click can surface the module menu,
- there's no resting visual indication of current ownership — you have to open
  the menu to find out, then close it without nudging anything.

**Design rule for a good fix:** put the ownership control in a zone that is
**not** a data-editing surface, and make current ownership **visible at rest**.

## Editor geometry (the zones available)

From `SandsVisualEditorV4::Layout`:
- `topPadding = 18px` — header/label row above the lanes (NOT a data surface).
- `padding = 6px` — left inset before the step grid; lane name is drawn
  right-aligned at `padding - 8` (i.e. in a thin left gutter, NOT a data
  surface).
- Lane rows themselves (`getLaneY..+laneHeightF`) — THE data surface; avoid.
- Step grid starts at `padding`; handles/window live here.

So the **left gutter** (where the lane label already sits) and the **header
row** are the collision-free zones. The label gutter is the natural home —
it's already per-lane, already non-interactive, and reads as "the lane's
identity strip."

## Options

### A. Ownership chip in the left label gutter  ⭐ recommended
Widen the left gutter a little and put a small clickable chip per lane, beside
(or behind) the lane name. Click toggles East↔Macro for the current voice.

- **Collision-free:** the gutter is left of `padding`; the editor's onButton
  hit-test is for `x >= padding`. A chip here never overlaps handles/window.
- **Visible at rest:** the chip *is* the indicator — its colour/glyph shows
  ownership without opening anything (see "Visual indication" below).
- **One click**, on the thing it affects, on the correct voice (the editor is
  already showing one voice's lanes).
- Implementation: extend `Layout` with a `gutterW` (e.g. 14px), draw the chip
  in `drawLaneLabel`, add a hit-test in `onButton` for `e.pos.x < padding`
  BEFORE the lane-data hit-test, calling a new `onLaneOwnershipToggle(lane)`
  host callback. Right-click menu can stay as a power-user fallback.

### B. Click the lane LABEL text itself
No new chip — clicking the lane *name* (MELODY/OCTAVE/…) toggles ownership.
The label already lives in the gutter, so same collision-free zone.

- Pro: zero extra panel space; the label does double duty.
- Con: less discoverable (a name doesn't *look* clickable); and the label
  colour is already overloaded (selected-lane highlight). Workable but the
  chip (A) is clearer.

### C. Dedicated owner column of buttons (kit-bound params)
A vertical strip of 4 latch buttons (one per lane) just left of the editor,
bound via SvgPanelKit like every other control. This is essentially today's
"lower blend section latches" relocated to sit beside the lanes and aligned to
lane rows.

- Pro: real params (automatable, save/restore for free, no custom hit-testing).
  Aligned to lane rows so the mapping is obvious.
- Pro: fits the kit model we just adopted — markers in the gen script, bound in
  C++, no editor-internal mouse code.
- Con: costs panel width beside the editor; ownership is per-voice so the button
  must reflect/drive the *display proxy* (ownerDispId) that syncs per tab — same
  proxy pattern already used for attens/sends, so not new machinery.
- This is the most "in-grain" option for the current kit architecture and the
  least custom-code. Strong contender alongside A.

### D. Double-click lane row to toggle
Double-click anywhere on the lane toggles ownership.

- Pro: no new UI at all.
- Con: back on the data surface — exactly the collision we're trying to escape.
  A double-click still starts with a single click that the drag logic sees.
  Rejected for the same reason the context menu is fiddly.

### E. Modifier + click (e.g. Ctrl/Alt-click the lane)
Hold a modifier and click the lane to toggle ownership.

- Pro: no new UI; unambiguous vs plain drag.
- Con: invisible affordance (nothing shows it's possible); still on the data
  surface so a mis-timed release could register a tiny drag. Better than D, but
  worse than the gutter approaches. Possible as a *secondary* accelerator.

## Visual indication of ownership (independent of the toggle method)

The strongest improvement may be the *indicator*, not the toggle. Make
ownership a legible lane state so the user reads it at a glance:

1. **Gutter chip colour/glyph (pairs with A):** e.g. filled dot = Macro
   (global), hollow/outline = East (per-voice). Or a tiny "G"/"V" glyph.
2. **Lane background tint:** Macro-owned lanes get a faint global-source tint
   (a cool/desaturated wash) behind the step grid; East-owned lanes stay the
   normal per-voice lane colour. Subtle, doesn't fight the data, and you see
   the whole pattern of ownership across lanes instantly.
3. **Left edge bar:** a 2px colour bar at the lane's left edge (in the gutter),
   colour-coded by owner. Minimal, crisp, kit-friendly.

Tint (2) + chip (1) together = you both *see* and *set* ownership without ever
touching the data surface. This is the user's "change lane visuals to indicate
ownership" instinct, made concrete.

## Recommendation

- **Toggle:** Option **A** (gutter chip) if we want it inside the editor with
  live per-voice context and minimal panel cost; Option **C** (kit-bound owner
  column) if we prefer real automatable params and zero editor-internal mouse
  code. Both are collision-free. A is slicker; C is more in-grain with the kit
  refactor and lower-risk to implement.
- **Indicator:** do the **lane background tint + gutter mark** regardless of
  which toggle wins — the visible resting state is most of the felt
  improvement.
- Keep the existing right-click menu as a harmless power-user fallback.

## Note on the bigger picture

If the PRE/POST send tap and any East/Macro merge land later
(SANDS_ARCHITECTURE_CONSOLIDATION.md), the gutter chip / owner column is exactly
the surface that would also carry the per-lane source mode — so building it now
as a clean per-lane control is forward-compatible, not throwaway.
