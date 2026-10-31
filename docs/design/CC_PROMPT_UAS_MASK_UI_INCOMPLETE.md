# CC PROMPT — UAS Stage-4 mask UI is incomplete: range-driven default, paint action, click conflation

FINDING (Rodney, Rack): the UAS output-mask ENGINE is built (maskKind_/maskPainted_/maskFollowR_, cycle/
abs1/abs2/abs3, anchor+re-pin, SequencerEngine.cpp:208-335) but the UI to DRIVE it is incomplete:
1. CYCLE vs RANGE look identical — the mask isn't visibly driven from the voice range.
2. NO click-the-strip PAINT action — maskPainted_ exists in the engine but nothing in the UI sets it, so
   abs2/abs3 (painted/fixed-rotatable) are UNREACHABLE; only MASK_CYCLE (all-on default) is ever obtained.
3. "Mask follows rotation (R)" is a buried context-menu checkbox (StraitsEastSandsVisual.cpp:767,
   StraitsSandsMacroVisual.cpp:561) — hard to find, unclear effect.
4. EDITING the voice-range strip CONFLATES with editing the probability lanes — one shared onButton/
   onDragStart path (StraitsEastSandsVisual.cpp:57-63) and shared editor-lane routing (890/1592/1684 branch
   on EDITOR_LANE_VARIATION); the range strip has no distinct hit region, so sometimes lanes can't be
   edited.

So CC built the engine + the Stage-3 range DISPLAY, but left the Stage-4 interactive UI (paint, range-drive,
clean hit-testing) partial.

## DECISIONS (Rodney)
1. RANGE-DRIVEN BY DEFAULT: setting the voice range (voiceLOR O/L) drives the mask by default — the mask =
   the range window. So CYCLE = full (all 16/32 on, no range restriction); RANGE = the O/L window masks the
   output. These are then VISIBLY DIFFERENT (cycle plays everywhere; range plays only in the window). Wire
   the mask to follow the range for the common case — no paint needed to get range behaviour.
2. PAINT WANTED (was in the plan): a click-the-strip action to toggle individual cells play/mute, for
   ARBITRARY masks (abs2/abs3) beyond a contiguous range. Painting overrides the range-driven default →
   arbitrary pattern. (all-on painted = cycle; contiguous = equivalent to range; arbitrary = abs2/3.)
3. FIX THE CLICK CONFLATION: the range strip needs its OWN hit region, separate from the probability lanes,
   so clicking the strip paints/edits the range and clicking a lane edits that lane — never conflated.

## THE FIX (scope)
### A. Range-driven mask (default)
When the voice range (O/L) is set, derive the mask from it: mask cell i = (i within [O, O+L) window). So the
range window IS the mask by default. CYCLE (no range / full) vs RANGE (windowed) now audibly + visually
differ. maskKind_ = MASK_CYCLE when full, the range-derived kind when windowed. (abs1 "range-as-mask" is
this.) Verify the engine's existing abs1 path (range→mask) is what this drives, or wire range→maskPainted_.

### B. Paint action (click the range strip)
- Clicking a cell on the voice-range strip TOGGLES that cell play/mute → writes maskPainted_[v] (per-voice)
  / the macro mask. This moves the voice from range-driven (abs1) to painted (abs2/abs3).
- Drag to paint multiple cells (like painting a rhythm). The strip shows lit = play, dim = mute.
- "Mask follows rotation (R)" (the existing checkbox) selects abs2 (static painted) vs abs3 (painted
  pattern rotates with R) — keep the checkbox but make its effect visible (the painted pattern visibly
  rotates with R when on). Consider surfacing it near the strip, not only in the context menu.

### C. Click conflation fix (the bug)
- Give the voice-range strip its OWN hit region / widget, separate from the probability-lane grid. The
  strip's onButton/onDragStart handles PAINT; the lanes' handles lane editing. No shared routing that makes
  a strip click land on a lane or vice-versa.
- Verify: clicking the range strip never edits a lane; clicking any lane always edits that lane; the range
  strip is reachable and paintable independently.

## DONE =
- Range-driven mask default: cycle (full) vs range (windowed) audibly + visually distinct; setting O/L
  windows the output with no paint needed.
- Paint action: click/drag the range strip toggles cells → maskPainted_ (abs2/abs3); lit=play dim=mute;
  "follows rotation" checkbox selects abs2 vs abs3 with visible effect.
- Click conflation fixed: range strip has its own hit region; lanes always editable; strip independently
  paintable.
- Goldens: the ENGINE mask logic is unchanged (this is UI wiring + range-drive) — pin that audio is
  unchanged for a full/cycle mask (the default), and that a range mask gates output as the engine already
  computes. run_all + strict green.

## NOTE
This is UI-completion of UAS Stage 4 (the engine shipped; the interaction didn't). Not a new feature — the
plan's paint + range-drive, now actually wired, plus the hit-test bug.
