# CC PROMPT — Straits lane-expander resnap hardening (two improvements)

CONTEXT: StraitsBase already has resnapLanes() (sort by canonical order + setModulePosForce to reposition
lanes adjacent to the base, shoving other modules aside) + StraitsSeamFiller (visual continuity between
adjacent panels) + onDragEnd on the base triggers resnap. The GAP: resnap only fires on the BASE's
drag-end. If a lane expander is dragged away, or another module is inserted between base and lanes, the
chain breaks and nothing resnaps. Two improvements to make separation self-healing:

## 1. Trigger resnap on the LANE EXPANDER's drag-end
When a StraitsLaneExpander's onDragEnd fires, check if it's still adjacent to its chain (is its
leftExpander the base or another lane expander?). If NOT adjacent → have the BASE resnap all lanes
(resnapLanes()), pulling the wandered lane back into position. Implementation: the lane expander needs a
reference to its base (it already finds it via the leftExpander walk); on its own onDragEnd, if
disconnected, set baseMod->needsResnap = true (the same flag the base's onDragEnd sets). The base's
step() already checks this flag and calls resnapLanes(). So the lane just sets the flag — the base does
the actual resnap on the next step. Minimal code: add onDragEnd to StraitsLaneExpander that sets the
base's resnap flag when the lane detects it's no longer adjacent.

## 2. Periodic adjacency check (catches module INSERTION between base and lanes)
The insertion case (another module dragged between base and a lane) doesn't trigger any drag-end on the
Straits chain — the INSERTED module's drag is what causes the separation. So: poll adjacency periodically
in the base's step(). Every N frames (e.g. every 1024 frames, ~once per second at 60fps — cheap), walk the
rightExpander chain and check that the expected lanes are still adjacent and in order. If a gap or wrong
order is detected → set needsResnap. This is lightweight (one pointer walk per second, not per-frame) and
catches the insertion case that no drag-end event covers.

## DONE =
- Lane expander onDragEnd sets base's needsResnap when no longer adjacent to chain. Test: drag a lane
  expander away → it snaps back on release.
- Base step() polls adjacency every ~1024 frames; sets needsResnap on gap/misordering. Test: insert
  another module between base and a lane → the lane resnaps within ~1 second.
- Existing resnapLanes() + StraitsSeamFiller + canonical ordering unchanged (they do the work; these
  improvements just trigger them more often).

## NOT THIS PROMPT
- Panel redesign, 2x8 compaction, new Straits knobs. This is just resnap trigger hardening.
