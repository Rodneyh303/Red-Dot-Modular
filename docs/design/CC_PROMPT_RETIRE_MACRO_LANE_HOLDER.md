# CC PROMPT — retire the macro-tier lane holder (a display-only parallel "where are we?", the globalWalk_ class)

A SWEEP for parallel position trackers (the `globalWalk_`/dual-position disease) found exactly ONE
survivor: the **macro-tier lane holder**. It is a second position machine — `macroLOR_` (length-only)
→ `macroLanePhase_` → `macroLaneTick_` / `macroLaneSign_` (+ `macroLaneKey_`) — seeded at
`anchorTransport`, advanced every `tickHolders`, re-derived every `deriveHolders`, all in the AUDIO
THREAD, and read by NOTHING but the Macro strip's display (`StraitsSandsMacroVisual.cpp:1030/1032`
direction cue, `:1073` playhead). No audio path reads it. This is the same anti-pattern just retired
for `globalWalk_` → `pos_.phaseClock`.

Worse than `globalWalk_`: it is **lossy**. `macroLOR_` is written from `macroBase[el][0]` ONLY (the
length); offset and rotation are dropped (`effectiveMacroLaneLor` returns `O=0, R=0`). So the Macro
playhead is computed length-only — a likely LYING DISPLAY when the macro lanes carry nonzero
offset/rotation (which they do: the full L/O/R lives in `macroBase[el][0..2]`, flows to the per-voice
lane stores via `MonsoonExpanderManager::splitWriteLane` → `pushLaneLorCvPoly`, and is read by the
AUDIO through `effectiveLaneLor` — which C1b already LATCHES). So the macro lanes' real, latched LOR
is the per-voice path; `macroLaneTick_` is a redundant, O/R-blind side-tracker.

SWEEP RESULT (for the record): every other derived position — `laneTick_`/`laneTickV_`/`laneSign_`/
`laneSignV_`/`voiceRead_`/`voiceDriver_`/`voiceSign_` — IS an audio authority (read by the mask-read/
output path). The display reading those is correct (one source). ONLY the macro-tier lane holder is
display-only-parallel. The problem is bounded to this.

## CARDINAL RULE / DISCIPLINE
AUDIO IS BYTE-IDENTICAL by construction: `macroLaneTick_`/`macroLaneSign_` feed no audio reader, so
deleting the holder CANNOT move a golden — the full battery must stay byte-identical (that is the
proof the deletion is audio-safe). The DISPLAY changes (it becomes O/R-correct). Prove the display
divergence first (see below), then fix. No Rack here — the macro-strip playhead is Rodney's feel-test.

## BRANCH
Off `lane-expander-refactor`. `refactor/retire-macro-lane-holder`. PR into it.

## THE CHANGE
1. **Delete the macro-tier lane holder**: `macroLOR_`, `macroLanePhase_`, `macroLaneKey_`,
   `macroLaneTick_`, `macroLaneSign_` (SequencerEngine.hpp) and their seed/advance/derive sites
   (`anchorTransport` ~:847, `tickHolders` ~:941, `laneSeed` LaneWin::Macro ~:1056, `deriveHolders`
   ~:1363, the reset writes). Remove `effectiveMacroLaneLor` once its last reader is gone.
2. **Re-source the Macro strip's lane playhead + direction from the ONE authority**, derived (not a
   maintained holder), using the FULL latched macro-lane LOR (length + offset + rotation):
   - PREFERRED: a VIEW over the macro-row frame authority — `macroFramePhase_` (the audio-maintained
     macro row frame) mapped through the latched macro-lane LOR (the `effectiveLaneLor`/per-voice
     path's L/O/R) — the exact `globalWalk_ → phaseClock` move (replace a parallel holder with a view
     over the authority). This is O/R-correct for free.
   - This resolves the one design question below.

## THE ONE DESIGN DECISION (Rodney)
The Macro strip shows the SHARED macro lane, but position now lives per-voice (`laneTickV_`) and
macro-following voices can diverge (per-voice `advanceOffset_`). So "the macro playhead" needs a
defined representative:
- (a) **Macro-frame view** (recommended): the macro row's own frame position (`macroFramePhase_`) is
  the canonical macro position; the lane playhead is that mapped through the macro lane's full LOR. No
  per-voice pick needed — the macro tier has its own frame authority.
- (b) **Representative voice**: show a fixed macro-following voice's `laneTickV_`/`laneSignV_` (e.g.
  the lowest-indexed follower). Simpler to wire but arbitrary when voices diverge.
Default (a). Confirm, or pick (b).

## VERIFY THE DIVERGENCE FIRST (reproduce-first, the lying-display claim)
Before deleting, CAPTURE the bug: with a macro lane at nonzero offset/rotation, assert the current
`macroLaneTick_`-derived display cell ≠ the cell the AUDIO actually plays (the per-voice
`effectiveLaneLor` path). If it diverges, the fix corrects it (pin the corrected display == audio cell).
If it does NOT diverge (the draw path happens to re-apply O/R elsewhere), say so — the retirement is
then pure ODR cleanup with no visible change, still worth doing.

## ACCEPTANCE
1. `bash test/run_all.sh` — full battery BYTE-IDENTICAL (audio untouched; the holder fed no audio).
2. `make RACK_DIR=…` — compiles; the Macro strip playhead + direction cue now read the derived view.
3. Grep: `macroLaneTick_`/`macroLaneSign_`/`macroLanePhase_`/`macroLOR_`/`effectiveMacroLaneLor` gone
   from the engine; the Macro visual reads the unified authority.

## FEEL-TEST (Rodney)
- Macro strip playhead tracks the actual played cell at nonzero macro-lane offset/rotation (the fix),
  and still tracks forward/reverse and the direction cue, at non-default L/O/R.

## NON-SCOPE / NOTES
- Separate from the cadence arc (C0–C4) and C3. Pure consolidation — the second "where are we?" the
  earlier DRY sweep missed (it hunted LOR reads/twins/literals, not position trackers; this is the
  class to always check after a position-unification).
- The macro lanes' AUDIO LOR is already latched (per-voice path) — do NOT latch `macroLOR_`; delete it.

## DELIVERABLE
On `refactor/retire-macro-lane-holder`, PR into `lane-expander-refactor`. Audio byte-identical;
Rodney feel-tests the Macro playhead. Closes the last parallel position tracker.
