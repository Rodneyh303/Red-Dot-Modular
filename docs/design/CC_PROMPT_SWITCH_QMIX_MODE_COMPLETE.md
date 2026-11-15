# CC PROMPT — complete switchQmixMode (lossless Q-MIX dice↔realtime, the melody/rhythm twin)

**For the LOCAL CC build (Rack SDK).** Feature, not refactor — it ADDS the lossless pattern
preservation that melody/rhythm already have, to Q-MIX, plus the missing Monsoon context-menu item.
Best done AFTER step 4-final (so it mirrors the CLEAN, pattern-only mode switch — see ORDERING).

## WHAT EXISTS (verified — the Q-MIX plumbing is mostly there)
- `qmixMode` (PatternEngine.hpp:829, `0=dice 1=realtime`), reset with the twins (PatternEngine.cpp:82).
- `recomputeEffectiveQmix()` / `recomputeEffectiveQmixForVoice()` (PatternEngine.cpp:687/:866) — the
  pattern restore math (draw-counter based, like melody).
- The pattern cache fields: `cachedQmixSeedFloat` (PatternEngine.hpp:859), `cachedQmix[kStepStorage]`
  (:865). (These are the PATTERN cache — the twin of `cachedMelodySeedFloat`/`cachedMelodyPitchV`.)
- Die-actions `DA_REDICE_Q` / `DA_LIVESTATIC_Q` / `DA_LASTDICE_Q` wired in `Monsoon::fireDieAction`
  (Monsoon.cpp:710-716), fed by the Gate 3 assignable AND Raffles's dedicated gates. `diceQmix()`,
  `setPendingQmixRoll`/`setPendingQmixLastRoll` exist. `qmixMode` alias (Monsoon.hpp:1646).

## WHAT'S MISSING
1. **`PatternEngine::switchQmixMode()`** — the lossless cache/restore, the twin of
   `switchMelodyMode` AS CORRECTED BY STEP 4-FINAL: PATTERN-ONLY. On enter-realtime cache
   `cachedQmixSeedFloat` + `cachedQmix[]` (+ a `qmixSeedCached` guard, the twin of
   `melodySeedCached`); on return-to-dice restore them + call `recomputeEffectiveQmix()`. **NO
   position cache** — do NOT read/write `stepIndex`/`lastStepIndex`, do NOT reinstate
   `cachedQmixStepIndex`/`cachedQmixLastStepIndex` (step 4-final deletes those as dead). Parameterless
   signature (post-4-final `switchMelodyMode`/`switchRhythmMode` are parameterless too).
2. **`Monsoon::switchQmixMode()`** wrapper (parameterless) + `Monsoon.hpp` decl — parallel to
   `switchMelodyMode`/`switchRhythmMode` (Monsoon.cpp:528/531, Monsoon.hpp:1758/1759).
3. **The Monsoon context-menu "Q-Mix Mode" submenu** (MonsoonWidget.cpp) — a `QMI` struct parallel to
   `RMI`/`MMI` (MonsoonWidget.cpp:1260-1275): `onAction` → `module->switchQmixMode()`, `step()`
   checkmarks `qmixMode==value`, with "Dice (static)" (value 0) + "Realtime" (value 1) items under a
   "Q-Mix Mode" label + separator. This is the piece Rodney flagged as missing.

## OPEN BEHAVIOUR QUESTION (verify with Rodney BEFORE wiring — do not guess)
The two toggle SOURCES currently diverge, for ALL THREE streams:
- The CONTEXT MENU calls `switch*Mode()` → the LOSSLESS cache/restore.
- `fireDieAction` (Gate 3 assignable + Raffles gates) flips the flag DIRECTLY
  (`qmixMode = 1 - qmixMode`), BYPASSING `switch*Mode()` → reseed-on-return, no lossless restore.
So today a menu toggle and a gate toggle of the SAME mode behave differently. Decide the intended
Q-MIX behaviour: should `DA_LIVESTATIC_Q` route through `switchQmixMode()` (lossless parity across
both sources), or is the die-action intentionally a reseed? Whatever is chosen very likely should
align melody/rhythm too (`DA_LIVESTATIC_M`/`_R` have the identical split). Rodney's call — this is
a musical-behaviour decision, not a mechanical one.

## ORDERING
Do AFTER `CC_PROMPT_POSITION_RESOLVER_STEP4_FINAL_RETIRE_STEPINDEX.md`. Then `switchQmixMode` mirrors
the clean pattern-only `switchMelodyMode`/`switchRhythmMode` and there is no position-cache twin to
accidentally copy. If built earlier, follow PATTERN-ONLY explicitly (the WHAT'S MISSING #1 note).

## ACCEPTANCE
- `make RACK_DIR=…` clean; the "Q-Mix Mode" submenu appears and toggles; the checkmark tracks.
- `bash test/run_all.sh` green; march goldens byte-identical (mode toggles are UI-driven, not in
  goldens). If `switchMelodyMode`'s lossless restore has a headless test, mirror it for Q-MIX
  (enter realtime, mutate, return to dice → `cachedQmix`/effective pattern restored bit-identically).

## FEEL-TEST (Rodney)
- Q-MIX menu toggle dice↔realtime; Gate 3 assignable; Raffles gate — all flip the mode; the checkmark
  and any mode LED track. Returning to dice restores the exact pre-switch q-mix pattern (if lossless
  is the chosen behaviour for that source).

## DELIVERABLE
On its own branch (e.g. `feat/switch-qmix-mode`), PR into `lane-expander-refactor`. Local `make` +
headless green; Rodney feel-tests. Note the OPEN BEHAVIOUR QUESTION resolution in the PR.
