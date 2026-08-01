# Dice scrub: PatternEngine removal + replacement plan

Traced the current audition/A-B/mode machinery and the existing Philox addressing. Good news: the
counter-addressable structure the scrub model needs ALREADY EXISTS -- the scrub is a re-derivation
over the existing counter, not a new scheme.

## Existing structure we KEEP (it's already the scrub foundation)
- rhythmDrawCtr / melodyDrawCtr: SIGNED int64 counters (PatternEngine.hpp:384, "can go negative on
  reverse"). This IS the scrub position. KEEP.
- DRAW_CHUNK = 1024 (hpp:381): draw N occupies Philox positions [N*CHUNK, N*CHUNK+CHUNK). So the
  pattern at counter N is a PURE FUNCTION of (N, key) -- re-derivable forward AND backward (hpp:373).
  This is exactly at(N) for the scrub window. KEEP.
- advanceRhythmDraw(dir): +/-1 on the counter (hpp:426). KEEP (it's the roll = counter step).
- reverseActive sign: rhythmDrawDir() `(reverseActive && reversible)?-1:+1` (hpp:418) -- KEEP the
  reverseActive sign-flip; REMOVE the `&& reversible` (no more mode flag; reverse always inverts).
- Per-slot Philox draw via base = ctr*DRAW_CHUNK + cursor (hpp:446). KEEP -- this is at(N) slot draw.

## What to REMOVE (audition/A-B-stored/mode machinery)
1. Stored A/B pattern arrays (hpp:239-249): rhythmLockedA/CandB, variation/legato/accent, poly*,
   melody/octave LockedA/CandB. ~18 arrays of 16 (or 15x16) floats. These become UNNECESSARY --
   the scrub re-derives patterns from the counter. REMOVE as source of truth. (May keep a tiny
   at()-result cache if profiling needs it -- fallback only.)
2. Mix latch + applied state (hpp:262-263): rhythmMixLatched/Applied, melodyMixLatched/Applied.
   REPLACE: mix knob becomes the SCRUB position (0..6 across N..N-6 with detents), not an A/B blend
   amount. Keep a "scrub position" float; retire the A/B-specific latch semantics.
3. Trial/audition pending + gating (hpp:319-320 rhythm/melodyTrialPending; 594-601 setPending*Trial/
   LastTrial; the promoteToA=false anchored-A path). REMOVE entirely -- no audition mode.
4. Mode flag (hpp:401-407): rhythmReversible/melodyReversible, set*Reversible, *AuditionsAllowed.
   REMOVE -- one model, no NORMAL/REVERSIBLE split. The *AuditionsAllowed() guards on last-roll/
   last-trial/reseed-roll (590-608) collapse: last-roll stays (it's a counter step back), trial/
   reseed-roll go.
5. liveTrial (hpp:62-63 rhythm/melodyLiveTrial + the rLiveTrial/mLiveTrial paths in .cpp:424/445).
   REMOVE -- trial-as-live-source is an audition concept.
6. reseed-on-roll (in.reseedOnRoll paths .cpp:415/440): DECIDE -- reseed injects fresh entropy which
   breaks counter-reproducibility (a reseed makes at(N) non-reproducible past the reseed point).
   Under a pure scrub model reseed-on-roll conflicts with reversibility. Likely REMOVE, or gate it
   as "reseed = jump to a fresh counter origin" (breaks scrub history at that point -- acceptable if
   explicit). FLAG for Rodney.

## What to REPLACE recomputeEffective* with
Current: slewedRhythm[i] = bl(LockedA[i], CandB[i]) -- blend two STORED arrays by latched mix
(.cpp:272-285). 
New (scrub): the effective pattern is the blend across the 6-draw window by the scrub knob:
- Let s = scrub position (float, 0 at counter N, up to 6 at N-6), with DETENTS at integers.
- floor f = floor(s), frac = s - f. Effective slot i = blend( patternAt(N-f)[i], patternAt(N-f-1)[i],
  frac ). (Adjacent-draw blend, option-1, per Rodney.)
- patternAt(M)[i] = re-derive slot i from Philox at base M*DRAW_CHUNK (the EXISTING addressing).
- IDEAL: compute patternAt on demand (no storage). FALLBACK: cache the <=7 window patterns if the
  per-frame re-derivation profiles badly (Rodney: buffer OK if not efficient).
So recomputeEffective* changes from "blend 2 stored arrays" to "blend 2 re-derived window patterns
at the scrub position." Same blend shape (bl of two patterns); the two patterns come from the
counter window instead of stored A/B.

## Counter direction under reverse (keep, simplify)
Remove `&& reversible` from the dir helpers so reverse mode ALWAYS inverts roll->counter (spec:
forward roll decrements in reverse). reverseActive stays the single source of reverse state.

## Undo (collapses -- see DICE_SCRUB_MODEL)
Dice roll undo = (before_ctr, after_ctr) scalar via StoreEditAction. The A/B array snapshots that a
roll undo would have needed DISAPPEAR with the stored arrays. Scrub-knob = coalesced knob undo.

## Drives (point 4: unify)
Same counter+scrub model for clock, gate, phase. Phase reverse just drives advanceDraw(dir=-1). No
per-drive mode branching. Verify each drive's roll trigger routes to the one advance/scrub path.

## Build sequence (incremental, test after each; build-verify in Rack -- core gen path)
1. Remove mode flag (reversible/*AuditionsAllowed) + simplify dir helpers. Behaviour: reverse always
   inverts. (Smallest, isolatable.)
2. Remove trial/audition (pending+setters+liveTrial+anchored-A promoteToA=false path).
3. Replace recomputeEffective* : stored A/B -> re-derived window blend at scrub position (+detents).
4. Retire stored LockedA/CandB arrays once nothing reads them.
5. Rewire mix knob -> scrub position; wire scrub across clock/gate/phase.
6. Decide reseed-on-roll (flag).
7. Undo -> counter scalar.
Each step keeps tests green; the big behaviour change is step 3 (verify in Rack).

## OPEN for Rodney
- reseed-on-roll under scrub (remove vs "reseed = new counter origin")?
- detent feel: hard snap at integers or soft detent (magnetised but passable)?
- does the scrub knob REPLACE the physical MIX knob, or is MIX repurposed in place?
