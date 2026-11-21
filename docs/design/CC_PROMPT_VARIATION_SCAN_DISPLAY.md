# CC PROMPT — variation playhead shows the scan: display READS the engine's scanned position (NOT a recompute)

CONTEXT (Rodney): freeze fix works (no freeze, bipolar), but the Sands variation playhead doesn't move
independently during a hold/stutter — it's not showing the true scanned step. Rodney's RIGHT challenge: we
did a LOT of work to make display READ the engine, never RECOMPUTE. So this must NOT be a "display adds
scanOfs_ itself" parallel computation — that would re-create the divergence disease. It must be the display
READING the engine's single source of truth for the scanned variation position.

## THE SINGLE SOURCE OF TRUTH (already in the engine)
The engine reads the variation draw via stepAdvanceDrawAt(drawTickDelta) (SequencerEngine.cpp:1830) — "THE
ONE advance-draw expression (decideStepAdvance_ delegates here; the test mirror reads the same): the
variation draw at the strand tick + delta, wrapped by the variation lane's own length." The RISE passes
scanOfs_ (line 412, wrapped by varLen at 415). So the engine's scanned variation POSITION is computed
inside stepAdvanceDrawAt. That POSITION is the single source of truth.

## THE FIX — expose the scanned POSITION; the display READS it (one function, no divergence)
1. Factor the POSITION computation out of stepAdvanceDrawAt into a const accessor, e.g.
   int stepAdvanceStrandIdx(int drawTickDelta) const  // the variation strand tick + delta, wrapped by varLen
   stepAdvanceDrawAt then reads the value AT stepAdvanceStrandIdx(delta) — SAME function, refactored so the
   position is nameable. (One expression, two readers: the value-read and the display.)
2. The DISPLAY (getStepLightBrightness / the Sands per-lane chevron for the VARIATION strand) computes the
   variation chevron position by CALLING stepAdvanceStrandIdx(scanOfs_) — the SAME function the engine's
   decision uses — instead of deriving its own tick from totalStepsElapsed. So the chevron is drawn at
   EXACTLY the index the engine read. Display and engine CANNOT diverge: one function, both call it.
3. This is NOT "display recomputes the scan" — it is "display reads the engine's scanned position," the
   display-reads-engine principle. The scan math lives in ONE place (stepAdvanceStrandIdx); if it ever
   changes, engine and display update together.

## SCOPE
- VARIATION strand only; GATE MODE only (scanOfs_ is 0 elsewhere → the function returns the normal position,
  so clock/phase and other lanes are unaffected automatically — no mode branch needed in the display, it
  just calls the function which is a no-op when scanOfs_=0).
- Display/chevron only — NO audio/decision change (the decision already uses it). Goldens UNCHANGED.
- Direction follows naturally (the position function already accounts for the variation lane's direction/
  wrap).

## WHY THIS IS CONSISTENT WITH THE ENGINE (Rodney's challenge answered)
The variation read position is computed ONCE (stepAdvanceStrandIdx, used by stepAdvanceDrawAt). The engine
reads the VALUE there; the display draws the CHEVRON there. Same function → provably in sync → the exact
display-reads-engine discipline we built. The earlier "display adds scanOfs_" phrasing was WRONG (a parallel
recompute); this refactor makes it a single-source read.

## DONE =
- stepAdvanceStrandIdx(delta) const factored out (stepAdvanceDrawAt reads the value at it); the Sands
  variation chevron calls stepAdvanceStrandIdx(scanOfs_) for its position — no independent tick math.
- In gate mode the variation chevron scans (knob-sign) during holds, independent of other lanes, re-aligns
  on advance = the visual confirmation of the freeze fix. Other lanes / clock / phase unchanged.
- Goldens UNCHANGED (display-read only; the one scan expression is unchanged, just factored). run_all green.
