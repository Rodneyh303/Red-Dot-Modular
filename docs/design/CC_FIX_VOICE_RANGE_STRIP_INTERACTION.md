# CC FIX  voice-range strip interaction (East and Macro share SandsVisualEditorV4). UI only, no engine change.

Build on feat/phase-duals-gaps.

## VERIFIED FACTS (code and Rack)
- The pattern is 32 steps. STEP_COUNT = kPatternLengthCap = 32 (SandsVisualEditorV4.hpp:38-39, static_assert).
- The data is drawn across all 32 columns for every lane (Rack confirmed).
- Geometry is consistent: stepWidthF() = gridW() / STEP_COUNT (line 410). getStepX, getStepCenterX, getStepRect, the
  draw cells, and getStepAtX (line 1536) all use the same width. DO NOT change the geometry.
- The range-strip press seeds dragPos from the press position (line 1255). That is correct.
- Stale comment: line 297 says "the display cells stay STEP_COUNT = the visible 16". It is wrong: the display is 32.
  Fix the comment (see task 0). Line 550 (the Peak preset centred at STEP_COUNT / 2) is CORRECT (the centre of 32 is 16);
  it is a shape parameter, not a display bound. Do not change it.

## THE THREE FAULTS (what Rodney sees)
A. ENGAGEMENT JUMP. A range drag engages only when the cursor crosses into a new cell (~lines 1375-1384). grabStep is the
   PRESS cell, but the first applied move uses the cell at engagement. For the START and END edges the code assigns the
   ABSOLUTE step under the cursor (START: offset = step, ~1421-1425 in the lane path; the strip path ~1398-1411). So an edge
   jumps by the cells crossed before engagement. Picture 3 is this.
B. EDGE TARGET TOO SMALL. handleWidthF() = min(0.8 * cell, 0.5 * laneHeight) (line 411), and the START/END zones use it
   (~973-976). An edge is grabbed only within ~0.8 cell, so most presses on an edge fall into the WINDOW body and move the
   whole window. This is why the markers are hard to grab.
C. ROTATION NOT IN THE WINDOW. The strip's ROTATION drag (~1437-1441) writes lor[2] = the ABSOLUTE step with no clamp.
   The chevron (drawRotationIndicator) is placed at offset + rotation. So the chevron can leave the window (picture 4).
   The lanes' ROTATION drag (~1493-1496) is already window-relative and clamped; match it.

## FIX
0. Comment: rewrite line 297 to say the display cells are STEP_COUNT (32) wide and the mask plane holds the full pattern.
1. ENGAGEMENT / EDGE DRAGS (A). Capture at press: the edge being dragged (START or END), the other edge's step, offset and
   length (grabOff, grabLen, grabEdge). Every edge move is computed as deltaSteps = (step under cursor) - pressCell, applied
   to the press values. Remove the absolute assignment (offset = step). The first move after engagement must produce the
   same edge as a move with deltaSteps = 0 at the press cell (no jump at engagement).
2. EDGE TARGET (B). Make the START and END zones at least one cell wide on each side of the edge column (a fixed minimum,
   e.g. max(1 cell, 8 px) each side). The WINDOW body zone takes the rest of the strip and never captures a press within one
   cell of an edge. Draw the handles and grab zones from the same constants.
3. ROTATION (C). The strip's ROTATION drag uses the lanes' rule: rel = (step - offset) mod 32, clamped to [0, length-1].
   Apply the same clamp on every write of voice R (drag, CV, restore). The chevron is then always inside the window.

## GATES
- Edge drag: pressing the start edge and moving one cell at a time moves the edge one cell at a time, in the drag direction,
  with no jump at engagement (synthetic press and a sequence of moves, asserting the edge after each move).
- Target: a press within one cell of an edge grabs that edge, not the window body; a press in the middle of the window moves
  the window.
- Rotation: for every offset, length and drag position, the voice's rotation is in [0, length-1] and the chevron is drawn
  inside [offset, offset+length-1].
- Window body drag and the lane drags behave as before.
- East and Macro strips behave identically (one editor).
- No change to geometry, the engine, the store, the audio or the rotation convention. run_all green on both SDK legs.

## DO NOT
- Change stepWidthF, getStepX, getStepAtX or the 32-column draw.
- Change the Peak preset (line 550).
- Change the engine, the store, the window arithmetic, or the rotation convention.

## ADDITIONS (Rodney: yes)

### D. REMOVE THE PRESET BANK (vestigial)
- Delete presetBank, PRESET_BANK_SIZE, PresetSeed and the applyPreset path from src/ui/SandsVisualEditorV4.hpp
  (bank at ~230-231; the Flat/Peak/Ramp/Random seeding at ~550-575; the index apply at ~577-579; the Custom fill at ~570).
- Nothing in the live editor, the East or Macro visuals, or the plugin registration calls it. The copy in
  src/deprecated/ is already out of the build; leave it.
- Gate: the editor builds with no reference to presetBank or applyPreset in src/ui or src/*Sands*; run_all green.

### E. SEPARATE THE VOICE-STRIP DRAG FROM THE LANE DRAGS
Verified: the editor keeps one drag state. The voice-range strip writes its own lor[3] through that state and fires
onRangeEdit (~1428-1448). The probability lanes have their own window fields (lanes[l].offset/length/rotation). Nothing
gives the lane drags a separate owner, so a press in a lane's window can fall into the strip branch.
Fix:
1. Two owners. Keep the voice-strip drag state (rangeStrip.drag, lor[3], onRangeEdit) and a separate lane drag state
   (laneDrag: the lane index, its press values, and its own grab). A drag has exactly one owner, set at press.
2. Hit-test order at press: (a) the voice-strip band (rangeLaneH at the top of the grid) is tested first and, if hit,
   starts a STRIP drag; (b) otherwise the press is in a probability lane and starts a LANE drag for that lane.
   A LANE drag never calls onRangeEdit and never writes lor[]. A STRIP drag never writes lanes[l].
3. The lane drags keep their existing semantics (window, rotation, per lane, lane-relative and clamped) and get the same
   press-anchored deltas as in fix A.
Gates:
- Press and drag in a probability lane: that lane's window (offset/length/rotation) changes; the voice window (lor[]) and
  onRangeEdit do not change; no other lane changes.
- Press and drag in the voice-range band: the voice window changes; no lane changes.
- A press on the boundary between the band and the first lane goes to the band (documented, tested).
- East and Macro behave identically (one editor).

### F. LANE OFFSET AND ROTATION CLAMPED TO 15 IN THE ENGINE PATH (root cause of the midpoint snap)
Verified (Rodney's picture: the lane range snaps when dragged past 16). MonsoonSandsManager clamps the lane OFFSET and
ROTATION to 0..15, while the editor works in 0..31:
- lines 392-393 (applyMonoCV, offset and rotation to 0.f..15.f)
- lines 450-451 (math::clamp of baseOff and baseRot to 0..15)
- lines 703-704 (round(addCV(off/rot)) to 0..15)
- lines 965-966 (applyMacroCV off and rot to 0..15)
- lines 1001-1002 (endpointDeltas for offset and rotation to 0..15)
Result: an offset past 15 is pinned to 15, so the range jumps back when the drag crosses the midpoint.
Fix:
1. OFFSET: replace 15.f with (kPatternLengthCap - 1) = 31 at every site above (base, CV, Macro, endpoint).
2. ROTATION: the lane rotation rule (window-relative): clamp to [0, L-1] where L is the lane's length, not a fixed 15.
   Replace the fixed bounds with the lane's length-1 at each site (apply the clamp after the length is known).
3. Check PatternEngine.hpp:1088 (`newCount = std::min(newCount, 15)`). If it bounds a lane length or offset, it must
   also follow kPatternLengthCap; if it bounds something else (a count of voices or lanes), leave it and say so.
4. The CV paths (addCV, applyMacroCV, applyMonoCV) keep their behaviour; only the range changes.
Gates:
- Dragging a lane's window past 16 keeps the range continuous (no snap at 16 or at any other step).
- Offsets 16..31 apply and display correctly for every lane; the lane's transparent range matches the engine window.
- Rotation in each lane is in [0, L-1] for every L, from drag, CV and restore.
- ≤16 patches are unchanged where their values are already below 16.
- run_all green on both SDK legs.
