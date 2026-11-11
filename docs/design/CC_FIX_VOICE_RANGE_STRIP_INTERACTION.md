# CC FIX  voice-range strip interaction (East and Macro share SandsVisualEditorV4)

Build on feat/phase-duals-gaps. UI-only. No engine change.

## SYMPTOMS (Rodney, Rack, pictures 1-4)
- Dragging the left (offset) marker one step past a point makes the range JUMP to the left (picture 3).
- The range markers are hard to grab: the edge target is one cell wide and the zones overlap.
- The voice rotation chevron is hard to move and does not snap to the range (picture 4).

## CAUSES (verified in code)
1. ROTATION CONVENTION SPLIT (SandsVisualEditorV4.hpp rangeStrip ROTATION, ~1437-1441 vs lane ROTATION ~1493-1496).
   - Lane drag: rotation = window-relative (step - offset) mod 16, clamped to [0, L-1].
   - Voice-range-strip drag: lor[2] = the ABSOLUTE step, NOT clamped to the window (comment: "A5 R is NOT clamped").
   - The chevron draws at (offset + rotation) mod 16 (drawRotationIndicator, ~1077), i.e. the window-relative convention.
   Result: a voice R can sit outside [offset, offset+L-1]. This violates the decided rule (voice R is window-relative, 0..L-1,
   anchored at global step 1; see UAS spec lines 22-37 and CC_FIX_VOICE_WINDOW_CYCLE_AND_ROTATION.md).
2. EDGE DRAG NOT GRAB-ANCHORED (rangeStrip START, ~1421-1425). START sets offset = the step under the cursor, absolute. The
   WINDOW drag instead preserves grabOff + deltaSteps. A mismatch between the cursor's step and the handle's hit zone makes
   the first move snap the edge to another step (the picture-3 jump). The strip's dragPos origin at press must be checked.
3. HIT ZONES TOO SMALL AND OVERLAPPING. hitTestRange (~951-983) uses a band of 65% lane height for the body; hitTestHandle
   (~895-942) uses the top 45% for the edge handles and the bottom 35% for rotation; each edge handle is one cell wide.

## FIX
1. ROTATION: the voice-range-strip rotation drag uses the SAME rule as the lanes: rel = ((step - offset) mod 16),
   clamped to [0, L-1]. The chevron then sits at offset + R inside the window and cannot leave it. REMOVE the absolute
   assignment and the "A5 not clamped" comment. Apply the same clamp on EVERY write of voice R (drag, CV, restore).
2. EDGE DRAG: anchor the START and END drags to the grab exactly as WINDOW does. Record the grabbed edge's step at press
   (grabStep) and the offset/length at press (grabOff, grabLen). On move: deltaSteps = step - grabStep, then compute the new
   edge from the press values. Do NOT assign the cursor's absolute step to the offset. Verify the strip's dragPos is seeded
   from the press position in the SAME coordinate space as getStepAtX; fix the seeding if not.
3. HIT ZONES: make them disjoint and wider. Priority at press: edge handle (a fixed hit radius of about 0.5 cell or 8 px on
   either side of the edge column, whichever is larger), then the rotation strip (bottom band), then the window body (the
   rest of the top band). The edge handle zone must not be one cell wide. Draw the handles and grab zones from the same
   constants (the existing rule).

## GATES
- Dragging the start edge one step at a time moves the edge one step at a time, in the direction of the drag, with no jump
  (test with a synthetic press at the edge and a sequence of moves).
- A voice's rotation, set by drag, CV or restore, is always in [0, L-1]; the chevron stays inside the window for every L and R.
- The window drag, the end drag, and the lane drags behave as before.
- The same behaviour in the East and Macro strips (one editor).
- Engine and audio unchanged (no store or maths change). run_all green on both SDK legs.

## DO NOT
- Change the engine, the store, the rotation semantics (voice R stays window-relative), or the lanes' rotation convention.
- Change the clamp or the anchor rule decided in the UAS spec.


## UPDATE (verified after Rodney's report: the data is drawn across all 32 columns)
- Geometry is consistent: stepWidthF() = gridW()/32 and getStepX / getStepAtX / hit tests all use it. Do NOT change it.
- The rangeStrip press seeds dragPos from the press position (SandsVisualEditorV4.hpp:1255). That is correct.
- The remaining faults:
  A. ENGAGEMENT JUMP. The drag engages when the cursor crosses into a new cell (~1375-1384), and grabStep = pressCell is
     the press cell, not the engagement cell. An edge drag then writes the absolute step under the cursor at engagement, so
     the edge jumps by the cells crossed before engagement. Fix: capture the edge's own anchor at press (the edge's step,
     the other edge, the length) and compute every edge move as deltaSteps from the press cell, with no absolute assignment.
  B. EDGE TARGET TOO SMALL. handleWidthF() = min(0.8 cell, laneHeight*0.5) (line 411), so an edge is grabbed within about
     0.8 of one cell; most presses on an edge fall into the WINDOW body and move the whole window instead. Fix: a fixed edge
     hit zone of at least 1 cell on each side of the edge column, with the body zone taking the remainder, and the body
     zone never capturing a press within 1 cell of an edge.
  C. ROTATION ABSOLUTE. The rangeStrip ROTATION sets lor[2] to an absolute step (~1437-1441) with no window clamp. Fix as
     in the original section 1.
- The 32-column prompt above is superseded; these three fixes are the work.
