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
