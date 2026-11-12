# SANDS PROBABILITY-BAR LOR EDITING  bug list, status and the test plan

Focus (Rodney): shake all bugs out of the Sands probability-bar LOR editing. It has never been seamless.

## BUG LIST (status verified on feat/phase-duals-gaps)
| # | Bug | Symptom | Cause (code) | Status |
|---|---|---|---|---|
| 1 | Engine clamps lane offset/rotation to 15 | Range snaps at the midpoint (past 16) | MonsoonSandsManager.cpp:392-393 (and the other sites: 450-451, 703-704, 965-966, 1001-1002), literal 15.f | OPEN |
| 2 | Lane drags share the voice-strip drag state | Lane drags can write the voice window / fire onRangeEdit | SandsVisualEditorV4.hpp: rangeStrip is the only drag state (lines 325, 1195, 1220, 1316) | OPEN |
| 3 | Edge handles too small | Presses on an edge fall into the window body | handleWidthF() = min(0.8 cell, 0.5 lane) (line 418) | OPEN |
| 4 | Absolute edge assignment | Edge jumps by the cells crossed before engagement | getStepAtX sets the edge from the cursor step (line 1317) | OPEN |
| 5 | Voice rotation absolute on the strip | Chevron can leave the window | strip ROTATION writes an absolute step, no clamp | OPEN |
| 6 | Literal bounds and stale comments | Future regressions (the "visible 16" comment at line 297) | magic numbers 15/16 in window code | OPEN (named constants per G) |
| 7 | Preset bank (vestigial) | Dead code | presetBank / applyPreset | FIXED (removed) |
| 8 | 32-column display | Data drawn for all 32 columns; interactions buggy | geometry verified consistent | VERIFIED (no display bug) |

## ROOT ISSUE: THE DRAG LOGIC IS NOT TESTABLE
The press, drag and release logic lives inside the GLFW event handlers of the editor widget. It can only be exercised in Rack,
which is why each fix has been checked by eye. Extract it.

## PLAN
1. EXTRACT A PURE DRAG REDUCER. A function dragReduce(state, event) -> state, with no Rack or GLFW types. events are
   press(step, zone), move(step), release. state holds the owner (strip or lane), the press cell, the grab values
   (grabOff, grabLen, grabEdge), and the current window. The widget only translates GLFW events into these calls.
2. FIX 1-5 INSIDE THE REDUCER: named bounds (kLaneOffsetMax, kLaneLengthMax), one owner per drag, delta-from-press
   edges, a minimum edge zone of one cell each side, and rotation clamped to the lane length minus one.
3. A HEADLESS TEST SUITE (test/test_LorDrag.cpp, header-only, in run_all). Scripted sequences assert state after each
   event. Required cases:
   - Edge drag one cell at a time moves the edge one cell at a time, with no jump at engagement.
   - Drag past step 16 and past step 31: no snap at 16, no wrap.
   - A press in a lane body never changes the voice window; a press in the strip band never changes a lane.
   - Press within one cell of an edge grabs the edge, not the body.
   - Rotation stays in [0, length-1] for every length, drag and clamp.
   - Window body drag moves the window with the grab offset held.
4. DELETE THE MAGIC NUMBERS in the window path (G), and fix the stale comments.
5. THEN a short Rack pass to confirm the feel, not the correctness (the tests carry correctness).

## ORDER OF WORK
Extract the reducer first (no behaviour change, with the existing behaviour captured as tests), then fix 1-5 one at a time,
each with its test failing first. This is the only way to be sure the bugs are gone rather than moved.

## DO NOT
- Change the engine arithmetic, the store, or the 32-column geometry.
- Change the audio path.

## NEW BUGS (Rodney's Rack report, after the lane-editing fix)
| # | Bug | Symptom | Likely cause (located) | Status |
|---|---|---|---|---|
| 9 | Range dragged past step 1 or 32 cannot be picked up after release | The window is stuck; it can't be grabbed to move again | A wrapped window (start after end) is not handled by the body hit test (SandsRangeGestureCore inBody / classifyBand with startBoundX > endBoundX). Wrap at the release point is not normalised. | OPEN (located, not reproduced in code) |
| 10 | Body hit area is the interior bars only | A window of length 1 has no body to grab; accent (length 1) cannot be moved | edgeZoneW (START and END zones, one cell each side) takes the whole cell for length 1; inBody returns false. | OPEN (located) |
| 11 | Voice lane has no faint range highlight | The voice strip does not show the window the probability lanes show | drawRangeStrip (SandsVisualEditorV4.hpp:619) draws no body fill for the window; the lanes draw ribbons and a body fill. | OPEN (located) |
| 12 | Rotation chevrons wider than the probability bars at 32 steps | Chevrons do not match the 32-column grid | drawRotationIndicator (line 1027) uses a fixed chevron size, not stepWidthF(). | OPEN (located) |

Which lanes can be selected: the voice lane, rest, variation and legato work; melody, octave and qmix fail after they wrap
(bug 9); accent fails because it is length 1 (bug 10).

## TESTS TO ADD (test/test_LorDrag.cpp, headless, with the reducer)
- Drag the window past step 32 and release; then press in the body and move: the window moves (bug 9).
- Length 1 window: press on the bar, move one step: the window moves by one step (bug 10).
- Length 1 window: press on the edge, move: the edge moves, not the body (bug 10).
- Voice-lane band draws the same window highlight as a lane, for the same offset and length (bug 11, a render-state test).
- Rotation chevron x-extent equals one step width at 32 steps (bug 12, a geometry test).

## NEW BUGS (Rodney's Rack report, after the wrap and length-1 work)
| # | Bug | Symptom | Likely cause (located) | Status |
|---|---|---|---|---|
| 13 | A wrapped range with one step left on one side cannot be dragged back | Grabbing from the step at the edge only drags away from the edge | The edge zone and span maths (SandsVisualEditorV4.hpp ~899-986) use the start and end cells and a wrapped length; the drag does not handle a remainder on the other side of the wrap. | OPEN (located; needs the headless test to confirm) |
| 14 | Length-1 handle markers overlap the panel at the grid edge | When the single step is at step 1 or 32, the outside handle bands are drawn past the grid | drawHandles (~1042-1046) draws the outside bands at cell.pos.x - edgeW and cell.pos.x + cell.size.x with no clamp to the grid rect. | OPEN (located) |
| 15 | Voice-lane range highlight is invisible | The grab range is grey on the grey background | drawRangeStrip (~619-640) uses a low-contrast fill; it needs a lane colour or a contrast check. | OPEN (located) |

## TESTS TO ADD
- Wrapped window with one step on one side: press on the edge step and move toward the edge; the window moves toward the edge (bug 13).
- Length 1 at step 1 and at step 32: the outside handle bands are inside the grid rect (bug 14, a geometry test).
- Voice-lane fill contrast against the strip background meets a minimum ratio (bug 15, a colour test).
