# CC PROMPT — Position Resolver STEP 4-final: retire the stepIndex + lastStepIndex fields (delete the dead mode-switch cache)

**For the LOCAL CC build (has the Rack SDK).** Touches the module + PatternEngine; build with
`make RACK_DIR=…`, Rodney feel-tests. **Prerequisite: STEP 3b merged** (every external UI reader
already reads `playedColumn()`/`started()`; this pass assumes no widget reads the raw field).

Goal: delete the `stepIndex` and `lastStepIndex` ENGINE fields. `pos_` is the sole position
authority — `playedColumn()` IS the played column. The only thing keeping the fields alive is a
VESTIGIAL mode-switch cache (verified below); this pass deletes it.

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS. BIT-IDENTICAL behaviour. The identity harness proves
`playedColumn()` already equals the retired `stepIndex` at every step. No new behaviour — this is
deletion of redundant/dead state. Headless `run_all.sh` + FULL `make RACK_DIR=…` both green; Rodney
feel-tests the mode switch + playheads.

## BRANCH
Off `lane-expander-refactor` (after step 3b merges). Branch
`fix/position-resolver-step4-final-retire-stepindex`. PR into it.

## WHY THE CACHE IS DEAD (verified — this is the crux)
`switchMelodyMode`/`switchRhythmMode(int& stepIndex, int& lastStepIndex)` cache on enter-realtime
and restore on return-to-dice. That restore feeds NOTHING live:
- `recomputeEffectiveMelody()` (the real pattern restore) reads neither `stepIndex` nor
  `lastStepIndex` — it rebuilds from the draw counter (`melodyDrawCtr`) + the copula/scrub math.
- `cachedMelodyStepIndex`/`cachedMelodyLastStepIndex` (+ Rhythm twins) are read ONLY inside the
  switch. The `cachedQmix*` twins (PatternEngine.hpp:870-871) are declared but NEVER used.
- The engine `lastStepIndex` field (SequencerEngine.hpp:457) is never read outside the cache.
- Post-slice-4 `stepIndex` is a mirror of `pos_.playedColumn()`; the switch does NOT restore
  `pos_`, so the next `advancePlayhead` overwrites the restored value. The playhead flows with the
  transport across a mode switch regardless — the intended behaviour (mode switch changes the
  dice-roll trigger, not the playhead).

So the cache is removed, NOT re-pointed at `pos_`.

## THE SLICE (exactly this)
1. **PatternEngine mode switch** — drop the position cache, keep the PATTERN restore:
   - `switchMelodyMode`/`switchRhythmMode`: remove the `int& stepIndex, int& lastStepIndex`
     parameters and the `cached*StepIndex = stepIndex` / `stepIndex = cached*StepIndex` (and the
     `lastStepIndex` twin) lines. KEEP the seed + `melodyPitchV`/`cachedMelodyPitchV` buffers and
     `recomputeEffectiveMelody()` — the lossless pattern preservation is untouched.
   - Delete the fields `cachedMelodyStepIndex`, `cachedMelodyLastStepIndex`,
     `cachedRhythmStepIndex`, `cachedRhythmLastStepIndex`, `cachedQmixStepIndex`,
     `cachedQmixLastStepIndex` (PatternEngine.hpp:866-871) and the Monsoon.hpp aliases to them
     (:1681-1685).
   - Callers `Monsoon.cpp:528/531` (`switchMelodyMode()`/`switchRhythmMode()`): update to the new
     parameterless signatures.
2. **Delete the engine fields** — `stepIndex` (SequencerEngine.hpp:313), `lastStepIndex` (:457),
   and the Monsoon.hpp aliases `int& stepIndex` (:1649), `int& lastStepIndex` (:1650).
3. **SequencerEngine.cpp — remove the now-dead mirror writes + sentinels:**
   - `advancePlayhead` (~:418, ~:489): replace `int prevStep = stepIndex;` (captured BEFORE
     `pos_.advance`) with `const bool wasStarted = (pos_.playedColumn() != -1);`; delete the mirror
     write `stepIndex = pos_.playedColumn();`; change `wrapped = (prevStep != -1 && pos_.phrasePos()
     == 0)` to `(wasStarted && pos_.phrasePos() == 0)`.
   - `setWindow` (~:288): delete `if (stepIndex != -1) stepIndex = pos_.playedColumn();` (nothing
     reads it now).
   - `snapToPhaseStep` (~:1305): delete `stepIndex = pos_.playedColumn();`.
   - `reset` (~:162-163): delete `stepIndex = -1; lastStepIndex = -1;` — `pos_.reset()` (next line)
     already parks `col_ = -1`.
   - The internal `== -1` sentinels in the gate bodies (~:1937, ~:1979, ~:2098):
     `stepIndex == -1` → `!pos_.started()` (the accessor step 3b added; or `pos_.playedColumn() < 0`).
4. **Monsoon.cpp** — the bare-alias reset `stepIndex = -1;` (~:609): remove it; the engine reset
   (`pos_.reset()`) covers the park. Confirm no remaining bare `stepIndex`/`lastStepIndex` use.

## KEEP (do NOT touch)
- The PATTERN cache (seed, `melodyPitchV`/`cachedMelodyPitchV`, `recomputeEffectiveMelody`) — the
  lossless melody/rhythm preservation is the real feature and is independent of the playhead.
- `pos_`, `playedColumn()`, `started()`, `globalWalk_`/`phaseClock` (slice 5's thread).

## ACCEPTANCE
1. `grep -rn "stepIndex\|lastStepIndex" src/` shows NO engine field, NO alias, NO mode-switch cache
   — only unrelated locals (Intertropical's own tracker, MonsoonModeController's own, the
   `SandsRangeGestureCore::stepIndexAt` geometry helper, comments).
2. `make RACK_DIR=/path/to/Rack-SDK` — full plugin compiles clean.
3. `bash test/run_all.sh` — all green; march goldens (GOLDEN_A/B/BS, UA_NONDIV) BYTE-IDENTICAL;
   `test_position_resolver_identity`, reverse + phase oracles green.
4. `test_16_triage` (GATE 1) green.

## FEEL-TEST (Rodney, in Rack)
- Melody/rhythm mode switch (realtime ↔ dice), WHILE RUNNING: the playhead does NOT jump — it keeps
  flowing with the transport; the restored pattern is the exact pre-switch pattern (lossless).
- The same switch while STOPPED, then resume: the playhead resumes at the transport position (no
  stale column) — this is the one spot the old mirror could glitch; confirm it's clean now.
- Playheads across modes A/B/C/D, gate + phase, forward + reverse, non-default L/O/R — unchanged.

## DELIVERABLE
On `fix/position-resolver-step4-final-retire-stepindex`, PR into `lane-expander-refactor`. Local
`make` green + headless green; Rodney feel-tests the mode switch before merge. This closes the
`stepIndex` thread; `globalWalk_` (slice 5) and C3 remain.
