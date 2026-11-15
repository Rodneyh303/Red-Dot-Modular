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

## UNIFY THE TOGGLE PATH — DRY/ODR (DECIDED, Rodney 2026-11-15)
Today the two toggle SOURCES diverge, for ALL THREE streams: the CONTEXT MENU calls `switch*Mode()`
(LOSSLESS cache/restore), while `fireDieAction` (Gate 3 assignable + Raffles gates) flips the flag
DIRECTLY (`xMode = 1 - xMode`, BYPASSING `switch*Mode()` → reseed-on-return). That is the ODR/DRY
defect. FIX: every LIVE/STATIC TOGGLE routes through the ONE main path `switch*Mode()`:
- `fireDieAction` `DA_LIVESTATIC_R` → `switchRhythmMode()` (was `rhythmMode = 1 - rhythmMode`).
- `fireDieAction` `DA_LIVESTATIC_M` → `switchMelodyMode()` (was `melodyMode = 1 - melodyMode`).
- `fireDieAction` `DA_LIVESTATIC_Q` → `switchQmixMode()` (new).
  (`switch*Mode()` already flips the mode internally — `next = 1 - prev` — so this is a direct
  substitution, now WITH the lossless cache/restore on both arms.)

CARVE-OUT — do NOT route these through `switch*Mode()` (different intent, NOT a toggle):
- `DA_LASTDICE_*` (`xMode = 0; setPending*LastRoll()`) — go to dice and re-apply the LAST roll.
- `DA_REDICE_*` / `dice*()` (`xMode = 0; setPending*Roll()`) — go to dice with a FRESH roll.
These SET to dice with a reseed; the `switch*Mode()` return-arm would restore the cached pattern and
fight the pending roll. Leave them exactly as they are.

THREAD NOTE (be aware, not a blocker): `fireDieAction` runs on the AUDIO thread (process() — Gate 3
`Monsoon.cpp:1113`, Raffles `:1781`); the menu runs on the UI thread. After this change the gate
toggle does the buffer copy + `recomputeEffective*` on the audio thread — bounded and rare (a mode
toggle), and arguably SAFER for that source (no cross-thread write of the buffers process() reads).
The menu path is unchanged. No new locking needed; just know the work moved threads for the gate
source.

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
headless green; Rodney feel-tests. The PR adds `switchQmixMode` (all three parts) AND the toggle-path
unification (the three `DA_LIVESTATIC_*` → `switch*Mode()` substitutions), so a Gate 3 / Raffles
toggle is now lossless, identical to the menu.
