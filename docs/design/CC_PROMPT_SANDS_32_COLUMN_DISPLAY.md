# SUPERSEDED  the 32-column data IS drawn for every lane (Rodney, Rack). The geometry is already 32:
# stepWidthF() = gridW() / STEP_COUNT (32), and getStepX / getStepAtX / hit tests use the same width.
# The remaining faults are in the interactions. See CC_FIX_VOICE_RANGE_STRIP_INTERACTION.md.

# CC PROMPT  Sands display: draw 32 columns for ALL lanes (PREREQUISITE for the voice-range strip fix)

Build on feat/phase-duals-gaps. Display only. Engine arithmetic is already 32 (STEP_COUNT = kPatternLengthCap = 32,
SandsVisualEditorV4.hpp:38-39). Lantern stays 16 columns (intended: a half bar at 48 PPQN, a full bar at 24).

## EVIDENCE
- The editor's arithmetic (wrap, offset, length, rotation, chevron) is 32. The header (SandsVisualEditorV4.hpp:34) says a
  32-step lane is fully visible and authorable.
- The drawn grid is 16 columns for every lane: line 297 says "the display cells stay STEP_COUNT = the visible 16", and
  line 550 centres on STEP_COUNT / 2 (= 16).
- Result: positions 16-31 exist in the arithmetic but are not drawn. A window or rotation there is invisible, so the
  range appears to jump (picture 3) and the chevron appears to leave the range (picture 4). Picture 1's transparent
  region over steps ~16-24 is a window drawn past the visible grid.
- The Stage 3 display (32-column Sands grid) has not reached the voice-range strip or the lane probability grid.

## TASK 1  32 columns for every lane (probability bars, mask, range strip, rotation, markers, dim)
1. Draw STEP_COUNT (32) columns in the probability grid of EVERY lane (melody, octave, qmix, rest, accent, variation, legato),
   and in the voice-range strip. Column width = grid width / 32. Replace the 16-cell layout constants and the centre
   (STEP_COUNT / 2) with the 32-column geometry.
2. The 16-column line at SandsVisualEditorV4.hpp:297 is replaced: the display cells are 32, the same as the arithmetic.
3. Bars: each step's probability bar uses its own column (step index 0..31). Dim/lit state, the current-step block and
   the rotation chevron follow the same column mapping.
4. Range window, edge markers and the rotation chevron draw on the 32-column grid, so a window at 16-31 is visible.
5. Keep Lantern at 16 columns (no change).

## TASK 2  verify the mapping, then stop
- A single mapping function: stepToColumn(step) = step (0..31). Every draw and hit test uses it; no other 16 literal.
- Confirm no hit test or draw still uses a 16 literal for a step index (grep for ` 16` used as a step bound in the Sands
  editor and the two strips).

## GATES
- Each lane shows 32 columns; a step at index 20 is drawn at column 20 in every lane.
- A window at offset 20, length 8 is drawn at columns 20-27 and is visible; rotation within it is visible.
- Lantern unchanged (16 columns). Gate mode unchanged.
- Patterns at length <= 16 render unchanged in their first 16 columns (no regression for 16-step patches).
- run_all green on both SDK legs.

## DO NOT
- Change the engine arithmetic, the store, the rotation convention, or the audio.
- Change Lantern's columns.

## THEN
Apply CC_FIX_VOICE_RANGE_STRIP_INTERACTION.md (the strip interaction fix). It is only testable after this lands: picture 3
and picture 4 cannot be verified while positions 16-31 are not drawn.
