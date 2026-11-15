# CC PROMPT — Position Resolver STEP 3b: decouple the UI/widget reads from the stepIndex field (public playedColumn())

**For the LOCAL CC build (has the Rack SDK).** This pass touches widget/module code that
`#include <rack.hpp>` — it CANNOT be built or verified in the headless cloud harness, so it was
left out of slice 3. It builds with `make RACK_DIR=…` and is feel-tested in Rack by Rodney.

Goal: every EXTERNAL reader of `engine.stepIndex` reads it through a public accessor
(`playedColumn()`), so no widget depends on the field directly. This does NOT delete the field —
that is step 4-final (see NON-SCOPE: it has a write-path dependency that rides the holder model).

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS. PURE REFACTOR — BIT-IDENTICAL. `playedColumn() == stepIndex` by the
identity harness, so every repoint is value-identical. Headless `run_all.sh` + the FULL
`make RACK_DIR=…` build must both be green; Rodney feel-tests the playheads.

## BRANCH
Off `lane-expander-refactor`. Branch `fix/position-resolver-step3b-ui-playedcolumn`. PR into it.

## THE ACCESSORS (add to SequencerEngine, public)
```cpp
int  playedColumn() const { return pos_.playedColumn(); }   // the played column (-1 = not started)
bool started()      const { return pos_.playedColumn() >= 0; }
```
(Value-identical to `stepIndex` by the identity harness. `pos_` is already on the engine.)

## THE READ SITES (repoint ALL — verified inventory; `grep -rn "\.stepIndex" src/`)
VALUE reads `… eng.stepIndex …` → `eng.playedColumn()`:
- `src/Intertropical.cpp:117`  `const int si = eng.stepIndex;`
- `src/StraitsSandsMacroVisual.cpp:1353, :1378`  `calcPlayhead(eng.stepIndex, …)`
- `src/dsp/engines/SequencerEngine.cpp:1354`-caller side — see Monsoon.cpp:1354 below.

GUARD reads `eng.stepIndex >= 0 ? X : -1` → `eng.started() ? X : -1`:
- `src/StraitsSandsMacroVisual.cpp:1063, :1107`
- `src/StraitsEastSandsVisual.cpp:1049, :1794, :1885, :1920, :1957`
- `src/ui/VisualExpanderHelpers.hpp:114, :126`  (`eng.stepIndex < 0` → `!eng.started()`)
- `src/ui/LanternRecordCore.hpp:151`  `const int step = eng.stepIndex;` → `eng.playedColumn()`

SENTINEL `engine.stepIndex == -1` → `!engine.started()`:
- `src/Monsoon.cpp:1196`
- `src/dsp/managers/MonsoonModeController.cpp:379`

DRIFT CHECK read (Monsoon.cpp:1354) `engine.phaseStepFor(absStep) != engine.stepIndex`
  → `… != engine.playedColumn()`.

## LEAVE ALONE (NOT this pass)
- **The engine-internal `stepIndex`** — the field, the slice-4 mirror writes (`stepIndex =
  pos_.playedColumn()` at the three authority sites), the internal `== -1` sentinels (the gate
  bodies ~:1937/:1979/:2098), the `prevStep` capture, the reset `stepIndex = -1`. The field STAYS.
- **The Monsoon.hpp alias** `int& stepIndex = engine.stepIndex;` (:1649) and `lastStepIndex` (:1650)
  — the mode-switch cache uses them by reference; leave them.
- **The mode-switch cache** `switchMelodyMode/switchRhythmMode(int& stepIndex, int& lastStepIndex)`
  (PatternEngine) and the Monsoon.cpp bare-`stepIndex` writes (:528/:531 mode switch, :609 reset)
  — WRITE paths, step 4-final.
- **`src/deprecated/StraitsWestSandsVisual.cpp:179`** — deprecated; only touch if it is in the
  build.

## NON-SCOPE (step 4-final — the next pass, NOT this one)
Deleting the `stepIndex`/`lastStepIndex` FIELDS is step 4-final
(`CC_PROMPT_POSITION_RESOLVER_STEP4_FINAL_RETIRE_STEPINDEX.md`). It is gated ONLY on this pass
(the UI must read `playedColumn()` first) — NOT on the holder model. The mode-switch position
cache (`switchMelodyMode` doing `stepIndex = cachedMelodyStepIndex`) turned out to be VESTIGIAL:
`recomputeEffectiveMelody` restores the pattern from the draw counter, the `cached*StepIndex`
values are read only inside the switch, the engine `lastStepIndex` field is never read elsewhere,
and post-slice-4 the restored `stepIndex` is a mirror the next advance overwrites from `pos_`
(which the switch never restores). So the playhead already flows with the transport across a mode
switch — no `pos_` save/restore surgery is needed; step 4-final DELETES the dead cache. This pass
just removes the widgets' dependency on the field NAME so that deletion is smaller.

## ACCEPTANCE
1. `make RACK_DIR=/path/to/Rack-SDK` — the FULL plugin compiles clean (this is the point of the
   local build — the headless harness can't see these files).
2. `bash test/run_all.sh` — all green; march goldens byte-identical.
3. `grep -rn "\.stepIndex\|engine->stepIndex" src/ --include=*.cpp --include=*.hpp` shows ONLY the
   leave-alone sites (the alias, the mode-switch, the engine-internal field) — no widget/visual
   reader of the raw field remains.

## FEEL-TEST (Rodney, in Rack — the part no harness covers)
The playhead must track EXACTLY as before in every surface that moved:
- Sands East + Macro visuals: the per-lane playhead dots sit on the right step, forward AND
  reverse, at non-default length/offset/rotation, and blank (`-1`) when stopped.
- Lantern record display: the disc/record paints the correct bar/cell as the playhead moves.
- Visual expander + Intertropical: playhead-derived readouts unchanged.
- Across modes A/B/C/D and gate/phase, and a melody/rhythm mode switch (the cache path is
  untouched, but confirm the playhead still resumes correctly).

## DELIVERABLE
The accessor + the repointed read sites, on `fix/position-resolver-step3b-ui-playedcolumn`,
PR into `lane-expander-refactor`. Local `make` green + headless green; Rodney feel-tests before merge.
