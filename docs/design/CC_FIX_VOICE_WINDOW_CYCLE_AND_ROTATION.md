# CC FIX  voice window: cycle within the window (period L), rotation within the window like the other lanes

Build on feat/phase-duals-gaps. Spec authority: docs/design/UNIFIED_ADDRESSING_SYSTEM.md, lines 13-20 (R = phase into
the window), 22-37 (nesting: voice reads its window from global 1 and loops within it), 64 and 186-187 (offset is
placement, relative to the parent), 100-101 (length = polymeter, rotation = phase within the window).

## DECIDED (Rodney)
- OFFSET O = where the window STARTS. LENGTH L = the window's length. Both are the voice's window placement and size.
- A CYCLING voice (cycle mode) repeats every L steps, staying inside its window. It does NOT follow the global pattern length.
- ONLY THE ABSOLUTE MODE (abs1, the full-phrase read: voiceFrameReadsFullPhrase) cycles the GLOBAL pattern length.
- ROTATION is restricted WITHIN THE WINDOW, like every other lane (melody, octave, variation, ...): 0 .. L-1, window-relative.

## BUG 1  the cycle period follows the global length (SequencerEngine.cpp, voiceFrameTickAt ~line 665-688)
Today: tNowV = driverTickAt(effTick, cachedLength, cachedOffset) wraps the transport at the GLOBAL length first, then the
voice window is folded through L. A 12-step voice under a 16-step global therefore resets at every global wrap and its
period is the global length, not L.
Fix: for a cycling voice fold the UNWRAPPED transport tick (effTick1based) through the voice's own L:
  wnow = laneTickFor(vd, effTick1based, L) (+ rotation, see Bug 2), so the period is exactly L.
The window PLACEMENT (O) comes from the global offset/frame as the nesting requires (see OPEN below). The global
length does not set the cycle period for cycling voices.
Keep the abs1 branch (voiceFrameReadsFullPhrase -> L = kMaskRing, the global ring) EXACTLY as it is: it is the one mode
that cycles the global pattern length.

## THE ANCHOR RULE (Rodney, per UAS spec lines 22-37)
- VOICE R = where the voice reads when GLOBAL step 1 arrives, as a WINDOW-STEP index 0..L-1 (spec example: voice L=8,
  R=7 at global 1 reads window-step 7). So rho = R, anchored to GLOBAL step 1, independent of O.
- LANE R = the lane's position when the VOICE is at its first (lowest-numbered) step, i.e. the window start. Lane R is
  anchored to VOICE step 1, NOT global step 1 (the spec calls the global anchor the OLD behaviour).
- O only places the window (where it starts in the phrase). R only sets the phase at the anchor.

## BUG 2  rotation is not kept within the window
Today: the voice rotation clamp is 0 .. kMaskRing-1 (= 15) (SequencerEngine.cpp:307-308), NOT 0 .. L-1. And the fold is
rho = ((R - O) mod L) (lines 679 and 865), which treats R as an absolute position depending on O, so the same R gives a
different starting phase when O changes, and an out-of-window R is folded silently.
Fix:
1. Clamp R to 0 .. L-1 for the voice, the same as every other lane's rotation (check the lane rotation clamp and copy
   its convention exactly; do not invent a new one).
2. Use R as the WINDOW-RELATIVE phase directly: rho = R (phase into the window at phrase start, line 13), NOT (R - O).
   Confirm the lane convention matches before changing.
3. A voice's R outside 0..L-1 can no longer exist (the clamp follows L). If L shrinks below the current R, the R is
   clamped to L-1 at the authoring path, not silently folded.

## OPEN (the one thing to confirm in code, not a design question)
Whether the GLOBAL OFFSET shifts the window's placement for cycling voices. Per the nesting (lines 64, 186-187) it should:
the voice O is relative to the parent frame, which carries the global offset. Implement that: the window starts at the
global-frame position plus O. If the existing code already routes the global offset into the window, keep it; if not,
add it. Report which.

## BEHAVIOUR CHANGE (declare it, do not hide it)
- Voices with R != 0 or O != 0 will change behaviour (the rotation now means window-relative, not (R-O)). Document the
  change; re-pin ONLY the cycle goldens that carry a non-zero R or O, with a comment naming this change. Identity voices
  (L=16, O=0, R=0) are unchanged.
- Cycling voices with L < the global length change period (now L). Document and re-pin those goldens deliberately.

## GATES
- IDENTITY (L=16, O=0, R=0, forward): bit-identical to today on every existing golden. No re-pin.
- PERIOD: a cycling voice with L=12 under a 16-step global repeats every 12 steps across a global wrap (no jump at 16).
- ABS1: the full-phrase read still cycles the global length (unchanged; its goldens unchanged).
- ROTATION: R is clamped to 0..L-1; rho = R; a voice with L=8, R=7 starts at window position 7, and R cannot reach 8..15.
- OFFSET: the window starts at the global frame position plus O (or the existing routing, reported).
- LANE ANCHOR: a lane with R=k is at window-step k when the VOICE is at step 1 (voice window start), not when GLOBAL is
  at step 1. Check the code: if lanes still anchor to global step 1, that is a third bug and is fixed here too.
- Other lanes (melody, octave, variation ...) unchanged except for the anchor above.
- run_all green on both SDK legs.

## DO NOT
- Change the abs1 full-phrase branch, the mask, or the composition of the mask with cycle/abs1.
- Change the lane rotation convention to suit the voice; the voice matches the lanes.

## PAINTED PATTERN FOLLOWS ROTATION (Rodney: keep it, rotation stays restricted to the range)
Current behaviour (verified, SequencerEngine.hpp:743-750 and SequencerEngine.cpp:366): the painted pattern shifts by the
DELTA of the voice rotation from its anchor, shift = rowR - anchorR (rotMaskRing), and re-pins on a base change by
baseR - anchorR. So the figure follows any change in voice R. Restricting R to 0..L-1 does not break this; it bounds how
far the figure can travel to +/-(L-1) from its anchor.

Requirements for the fix:
1. The follow input is the RESTRICTED voice R (the clamped baseR and rowR), never the unclamped stored value.
2. A window-length change that CLAMPS R (L shrinks below the current R) changes baseR, so the anchor re-pins and the
   figure shifts by the clamp delta. This is the intended consequence of "follow rotation"; declare it, do not hide it.
3. rotMaskRing rotates modulo the ACTIVE pattern length (16 or 32 with the 32-step work), not a fixed 16. Check the
   modulus used there and make it the active length.
Gates (added):
- With L=8, sweeping R over 0..7 moves the painted figure by R minus its anchor; R cannot reach 8..15.
- Shrinking L below the current R re-pins, and the figure moves by the clamp delta; the anchor then tracks the clamped R.
- Abs2 (followR off) rows never re-pin (unchanged).
