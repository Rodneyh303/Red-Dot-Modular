# CC FIX  voice-range strip: place the window at the GLOBAL OFFSET — DISPLAY-ONLY (the OFFSET axis)

Build on fix/macro-voicelane-shared-frame @ 1de4795a (the 16-triage + the cycle-mode PHRASE ANCHOR already
landed there). Spec authority: UNIFIED_ADDRESSING_SYSTEM.md lines 22-37 (the nesting), 64 and 186-187
(offset = placement relative to the parent frame). Rodney confirmed semantic A: the voice window is PLACED
within the global frame, so its absolute position carries the global offset.

## SCOPE — this is the OFFSET-PLACEMENT axis, which is SEPARATE from the phrase anchor (already fixed)
There were TWO distinct bugs in the voice-lane cycle behaviour under a global offset. Keep them apart:
- (1) THE PHRASE ANCHOR — ENGINE — ALREADY FIXED in 1de4795a. In CYCLE the voice must re-anchor to R at
  EVERY global step 1 (not free-run across the phrase boundary). The CYCLE branches now fold the
  phrase-relative position (wG/wGv, reusing globalWalk_). Verified: read returns to R at T=1, 17, 33.
  DO NOT re-open or re-touch this.
- (2) THE OFFSET PLACEMENT — DISPLAY — THIS DOC, STILL OPEN. The strip draws the window without the global
  offset while the read carries it. This is a display-coordinate fix only; the engine read is correct FOR
  THIS AXIS and must not be changed to fix the display.
(An earlier draft of this doc claimed "the engine is correct, don't touch it" as a blanket statement — that
was wrong: the phrase anchor (1) WAS a real engine fix. The claim holds ONLY for the offset axis (2).)

## THE FINDING (verified by a headless probe against 1de4795a, not reasoned)
global len=16, OFFSET=3, cycling voice L=7 O=2 R=4, CYCLE. Expect read = offG+O+R = 9, window
[offG+O, offG+O+L) = [5,12).
```
 T  voiceRead_[V2] / macroFrameRead_
  1     9    <-- phrase start: offG+O+R = 9 ✓   (re-anchors here — the phrase-anchor fix)
  4     5    (wraps within the window)
 17     9    <-- phrase start: 9 again (re-anchor; pre-1de4795a this drifted to 11)
 33     9
```
The read lands on offG+O+R at every global step 1 AND never leaves [offG+O, offG+O+L). Both East
(voiceRead_) and Macro (macroFrameRead_) reads are correct. So the remaining defect is purely the DISPLAY
placement.

## THE BUG — the strip draws the window WITHOUT the global offset, the read HAS it
Both voice-range strips draw the window ribbon from the voice's (L, O) = [O, O+L) but feed a read cell that
includes offG, so the playhead renders offG steps OUTSIDE the drawn window (and, relative to the mis-placed
window, never appears to land on R). Identical bug, both tiers:
- EAST  StraitsEastSandsVisual.cpp:1943  setRangeState(max(1,L), O, R, rp, ...) with rp = eng.voiceReadPosition(vslot)
  (= voiceRead_, includes offG). Window drawn at O; read at offG+O+phase.
- MACRO StraitsSandsMacroVisual.cpp:1103  setRangeState(mL, mO, mR, rp, ...) with rp = eng.macroFrameRead()
  (= macroFrameRead_, includes offG). Window drawn at mO; read at offG+mO+phase.
This is exactly the item CC_FIX_VOICE_WINDOW_CYCLE_AND_ROTATION.md §OPEN left unresolved ("whether the global
offset shifts the window placement for cycling voices ... report which"). Answer: it does (semantic A); the
read already implements it; only the DISPLAY placement is missing.

## THE FIX (display-only, ONE shared law, A-consistent)
The displayed window placement must be (globalOffset + O), matching the read. The global offset is
SequencerEngine::cachedOffset (accessor getOffsetStep()).
1. ONE shared helper (the windowContains / lanePlayheadBar precedent — rack-free, in SandsRangeGestureCore or
   its neighbour): given (O, globalOffset, ring) return the displayed window START = wrapMod(O + globalOffset,
   ring). Both strips call it; no second copy. Length L and rotation R are unchanged (R is window-relative; the
   read already folds it).
2. EAST (:1943) and MACRO (:1103): pass the placed start (O + globalOffset) to setRangeState as the window
   offset, so the ribbon, the edge handles and the read cell share ONE coordinate. The strip read (rp) is
   UNCHANGED — it is already correct.
3. EDITING IS PRESERVED: dragging the window still authors the voice O (and Macro mO), not O+offG. The +offG is
   a DISPLAY placement only. Where the gesture reads the window start back from the displayed value, subtract
   globalOffset so the authored store stays voice-local. Verify the gesture round-trips O unchanged when
   globalOffset != 0 (a drag of 0 cells must not shift O by offG).

## SECONDARY — the Macro probability-LANE playheads (verify, do not blanket-change)
StraitsSandsMacroVisual.cpp:1347 and :1372 compute the per-lane playhead as
`calcPlayhead(eng.stepIndex, ownLen, ownOff, ownRot) & 0x0F`. Two smells, both to CONFIRM in Rack before
changing (triage, not blanket-replace):
- The `& 0x0F` is a 16-step fold (flagged on diag/macro-voice-playhead): wrong past step 16 at 32 steps. It
  should wrap on the active ring, not 0x0F.
- It ignores the global offset AND the voice-frame placement, so a Macro lane's playhead is mis-placed by the
  same offG when offG != 0 (the lane sits inside the voice frame, which carries offG). The DRY fix is for the
  lane playhead to hang off the engine's frame read the way the strip does, not a stepIndex recompute. Confirm
  the intended lane-display law with Rodney first; the voice strip (above) is the reported bug and ships first.

## THE TEST (the headless pin that closes the rack-only gap)
Add test/test_voice_strip_offset.cpp (header-only, in run_all), driving the pure placement law + the engine:
1. PLACEMENT LAW: displayedWindowStart(O, globalOffset, ring) == wrapMod(O + globalOffset, ring), exhaustive
   over O in 0..ring-1, globalOffset in 0..ring-1.
2. READ-IN-WINDOW (the bug, reproduced): for global len=16, a sweep of offG in {0,3,5,7}, a cycling voice
   (L,O,R) over a roster (incl. 7@2 r4, 6@1 r3), drive advancePlayhead across >=2 phrases and assert the engine
   read (voiceRead_ for an East-local voice; macroFrameRead_ for the Macro row) lies within the DISPLAYED window
   [displayedStart, displayedStart+L) on EVERY step. FAILS today for offG != 0 (read in [offG+O,...], window
   drawn at [O,...]); passes after the placement fix.
3. FIRST-STEP ANCHOR (regression guard on the landed phrase anchor): at every global phrase start the read ==
   wrapMod(offG+O+R, ring) — pin T=1, 17, 33 for East and Macro so a later display change can't mask an engine
   regression.
4. EDIT ROUND-TRIP: a display-placed window whose drag delta is 0 leaves the authored O unchanged for every
   globalOffset (the +offG never leaks into the store).

## GATES
- offG == 0: bit-identical to today (the placement adds 0). No golden re-pin; goldens are engine-only anyway.
- offG != 0: the strip playhead is INSIDE the drawn window for every (O, R, tick), East and Macro (test 2).
- The engine is UNTOUCHED by this commit (grep: no engine-file changes) — the phrase anchor in 1de4795a stays
  as-is; this commit only moves display placement.
- V1..V16 identical on Macro (one global row — unchanged).
- run_all green on both SDK legs; anchor/bind audit + array-guard clean.

## DO NOT
- Change the engine: voiceFrameTickAt, the derive twins, the CYCLE phrase anchor (landed in 1de4795a),
  driverTickAt, the mask, or laneRecomp. The read is correct for the offset axis (the probe proves it). This
  is display placement only.
- Re-pin any golden (no engine/digest change).
- Blanket-change the lane playheads; confirm the secondary with Rodney and ship it separately.
- Special-case any voice index; let the +offG placement be the one law both strips call.
