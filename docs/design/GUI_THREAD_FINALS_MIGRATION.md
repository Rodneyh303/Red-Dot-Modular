# GUI-thread finals migration — retire the last race class

## The term: "finals"
PatternEngine's data flows through three storage stages:
1. **Sources** (`*Source[]`) — raw Philox draws; written on dice/reseed. The material.
2. **Intermediates** — A/B candidates (`*LockedA`/`*CandB`) + slewed buffers. The derivation.
3. **Finals** (`random_[16][6][16]`, 16 voices × 6 strands × 16 steps) — the post-pipeline,
   ready-to-consume values every reader uses: articulation thresholds, East probability
   bars, probability CV outs, Change Alley's remap. "Writes finals" = writes `random_`.

## The flaw (measured 2026-07-21, during the Change Alley write-side revert)
`random_` has 73 write sites in 7 files. 64 are AUDIO-thread (PatternEngine 27,
SandsManager 31, SequencerEngine 5, Persistence 1) — single-threaded within process(),
safe. **18 are GUI-thread**: the parameter managers' `syncEditorToPatternEngine` paths,
called from widget `step()` per frame:
- MonoSandsParameterManager.hpp — 12 sites
- PolySandsParameterManager.hpp — 3 sites
- PolyVoiceSandsParameterManager.hpp — 3 sites
Callers (non-deprecated): StraitsEastSandsVisual.cpp:577 (East editor drag path),
plus the display-direction syncs in StraitsSandsMacroVisual.cpp / MonsoonSandsVisualExpander.cpp
(read direction — verify write-direction callers per manager during migration).

A widget writing the engine's read surface while the audio thread is mid-cycle is a
data race (UB per the C++ memory model; benign-looking on x86 for aligned floats, but
LOGICALLY racy — it caused the Change Alley off-diagonal display flash, and it defeats
any architecture that assumes `random_` has one coherent owner).

## The fix: store→sync (the pattern Change Alley pins already use)
GUI writes INPUTS only (params / store fields / dirty flags). The AUDIO thread owns the
entire pipeline from there to finals: expanderManager.sync / processDNA applies staged
edits each control cycle. One owner per stage: GUI upstream, audio downstream.

Migration per manager (~18 sites, mechanical):
1. Replace each direct `patternEngine->xRandom[i] = v` with a write to a staging buffer
   on the manager (or the existing param where one exists) + a dirty flag.
2. Audio-side: in the manager's existing audio-called sync (or processDNA), apply staged
   values to finals when dirty, clear flag.
3. Verify with the editor-drag → engine round-trip cases in the suite; add a replica
   test asserting no GUI-path method touches `random_`/named final refs (grep-level guard
   in run_all.sh is acceptable).

## Sequencing
NOT a gate for panel/widget work (cannot touch finals). IS a gate before any future
engine feature that writes finals or assumes single-owner `random_` (e.g. any revisit of
write-side mapping, tape/history features, lock snapshotting that copies finals).
