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

## UNIFY THE LIVE/STATIC SWITCH PATH — DRY/ODR (DECIDED, Rodney 2026-11-15)
There are THREE routes that switch live↔static mode, and they diverge: the CONTEXT MENU calls
`switch*Mode()` (the LOSSLESS cache/restore), while the two gate routes flip the flag DIRECTLY in
`fireDieAction` (`xMode = 1 - xMode`, BYPASSING `switch*Mode()` → NO lossless restore on return). That
is the ODR/DRY defect. FIX: route EVERY live/static switch through the ONE main path `switch*Mode()`
— all three routes, nothing excluded:
- CONTEXT MENU (`RMI`/`MMI`/new `QMI`) → `switch*Mode()` — already correct, keep.
- GATE 3 assignable + RAFFLES gate, via `fireDieAction`:
  - `DA_LIVESTATIC_R` → `switchRhythmMode()` (was `rhythmMode = 1 - rhythmMode`).
  - `DA_LIVESTATIC_M` → `switchMelodyMode()` (was `melodyMode = 1 - melodyMode`).
  - `DA_LIVESTATIC_Q` → `switchQmixMode()` (new).
  (`switch*Mode()` already flips internally — `next = 1 - prev` — so this is a direct substitution,
  now WITH the lossless cache/restore on both arms. Both gate routes share `fireDieAction`, so this
  is one edit per stream, covering Gate 3 AND Raffles.)

NOT MODE SWITCHES — out of scope, leave UNCHANGED (these are DICE ROLLS, not live/static switches):
- `DA_REDICE_*` / `dice*()` (`xMode = 0; setPending*Roll()`) — a FRESH roll (advance the RNG stream,
  no reseed).
- `DA_LASTDICE_*` (`xMode = 0; setPending*LastRoll()`) — replay the LAST roll.
- the UI dice / last-dice buttons (`processLastDiceButtons`).
These are DRAWS on the current seed (PatternEngine.cpp:1065 — "ROLL pending = advance RNG, no
reseed"), NOT reseeds and NOT mode switches, so they were never candidates for this unification —
they just go to dice and roll. (A true RESEED = a new Philox seed, the separate seed-pending /
reseed-on-restart path — also unrelated here.) Do not touch them.

THREAD NOTE (be aware, not a blocker): `fireDieAction` runs on the AUDIO thread (process() — Gate 3
`Monsoon.cpp:1113`, Raffles `:1781`); the menu runs on the UI thread. After this change the gate
switch does the buffer copy + `recomputeEffective*` on the audio thread — bounded and rare (a mode
switch), and arguably SAFER for that source (no cross-thread write of the buffers process() reads).
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
