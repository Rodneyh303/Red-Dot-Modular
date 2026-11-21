# CC PROMPT — CADENCE C4: the Sands range display reads the effective (latched) LOR — see = play

Follow-on to C1b (merged: base/CV split — `effectiveVoiceLor`/`effectiveLaneLor` now return
`base_live + committed_CV_delta`, with CV committed at the phrase boundary and suspended during an
active LOR gesture). C4 makes the Sands range DISPLAY read that same effective value, so what the user
SEES matches what the engine PLAYS, at the phrase cadence. **This is a WIDGET change — build with
`make RACK_DIR=…` and feel-test in Rack (Rodney's leg).**

## WHY THIS IS MOSTLY A REPOINT (not new logic)
C1b already encodes the entire cadence in `effectiveVoiceLor`/`effectiveLaneLor`:
- **base is live** → a knob/drag edit shows immediately.
- **CV commits at the phrase boundary** → modulation steps the window at the phrase, not per sample.
- **CV is suspended during an active edit** (the edited scope holds its pre-gesture CV) → the window
  tracks only the live base while dragging — a stable target.
So the display does NOT need its own drag/latch logic. It just needs to read the effective accessor.
Reading the raw store (as several sites still do) is what makes it smear per-step and ignore the
cadence.

## CARDINAL RULE
VERIFY AGAINST CODE. The engine behaviour is already pinned (C1b's P1–P8); this is display wiring.
Headless `run_all.sh` must stay green; the FULL `make RACK_DIR=…` build must compile; Rodney
feel-tests the visual cadence.

## BRANCH
Off `lane-expander-refactor` (after C1b merges there). `feat/cadence-c4-sands-display`.

## THE CHANGE — repoint every Sands RANGE read from raw to the effective accessor
Enumerate yourself (grep `macroVoiceLor_`, `voiceLorStore_`, `lor(`, `macroLOR_` in the Sands/visual
widgets and `pushMaskEditor`); the known raw reads to repoint:
- `StraitsSandsMacroVisual.cpp` ~:1093-1095 — the Macro strip range reads `se->macroVoiceLor_[LEN/OFF/ROT]`
  raw → `effectiveMacroRowLor(...)` (the latched Macro row).
- `StraitsEastSandsVisual.cpp` ~:1037 — `m->engine.lor(el, LOR_LEN)` raw → the effective mono-lane
  accessor (`effectiveMonoLaneLor`/`effectiveLaneLor`), matching how `:1953` already reads
  `effectiveVoiceLor`.
- `pushMaskEditor` (`SequencerEngine.cpp` ~:378/:395) — the mask-editor rotation anchor reads
  `macroVoiceLor_[LOR_ROT]`/`voiceLorStore_[v][LOR_ROT]` raw. Repoint to the SAME source the displayed
  range uses, so the editor overlay and the range agree. (Confirm which cadence the anchor should
  follow — see FEEL-TEST.)
Leave the WRITE paths (the gesture → store, the param reads) alone — only the DISPLAY reads move.

## THE ONE FEEL DECISION (Rodney, in Rack) — base-pure vs effective while dragging with CV connected
With no CV patched, base == effective, so there is nothing to decide — the repoint is the whole job.
With CV connected, during a drag the effective window = `base_live + CV_frozen` (the suspend holds CV).
Two readings:
- **Show effective (recommended default):** the range you see IS what plays (base moving under your
  drag, CV frozen). No jump on release. The draggable handles sit at the effective edge; the gesture
  maps to base exactly as today (the engine applies base). Simplest — it's what the repoint gives.
- **Show base-pure while dragging:** during the gesture, read the raw store (base alone, CV excluded)
  so you edit a CV-free window; on release it snaps to base+CV. Needs a drag-active special-case in
  the read. Only build this if the effective-during-drag view feel-tests as confusing.
Default to the first (plain repoint). The handle-vs-fill question (do the draggable handles track base
while the fill shows effective, when CV offsets them) is a refinement to judge in Rack — flag it, do
not pre-build it.

## NON-SCOPE
- No ENGINE change — C1b owns the cadence; this is display only. (If a feel-test demands base-pure
  while dragging, that read-time branch is still widget-side.)
- The Macro LANE store (`macroLOR_`, the Macro strip's lanes) stays LIVE per C1/C1b (`effectiveMacroLaneLor`
  is un-latched) — its display is already live; don't latch it here unless Rodney rules otherwise
  (it's the open "Macro-lane latch?" flag from C1).
- C3 direction; the draw-cache; 32-step UI — unrelated.

## ACCEPTANCE
1. `make RACK_DIR=…` — the full plugin compiles.
2. `bash test/run_all.sh` — green (no engine change; the anchor/bind audit covers the widgets).
3. Grep: no Sands RANGE display read of raw `macroVoiceLor_`/`voiceLorStore_`/`lor()` for the window
   remains (only the gesture/write paths and the effective accessors).

## FEEL-TEST (Rodney, in Rack)
- Drag a Sands range (no CV): it follows your drag immediately (base live), lands on release.
- With CV patched: the window steps at the phrase (not per sample); during a drag the CV is frozen
  (stable target); decide the base-pure-vs-effective view above.
- Macro strip vs East voice strip: both now move at the phrase cadence, consistently.
- The mask-editor overlay rotation agrees with the displayed range.

## DELIVERABLE
On `feat/cadence-c4-sands-display`, PR into `lane-expander-refactor`. Local `make` + headless green;
Rodney feel-tests and makes the base-pure-vs-effective call. This completes the cadence arc
(C0→C1→C1b→C4); C3 direction + reverse-all remain as the separate direction track.
