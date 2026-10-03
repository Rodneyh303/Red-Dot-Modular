# Step 2 Commit Message

```
Sands consolidation Step 2: promote per-voice VAR/LEG draw + fix input ID layouts

Engine changes (per-voice VAR/LEG draw promoted to unconditional):
- PatternEngine.hpp: revert dead polyVariation/polyLegato RhythmDraw additions
  (the existing per-voice draw uses shared mono arrays at per-voice steps, not
  separate per-voice random arrays — the new fields were never consumed)
- SequencerEngine.cpp: remove 4 perVoiceArticulation gates (nvIdxForVoice,
  resting voice, Rule 2 LEAD, Rule 2 CONSUME) — the delegation system
  (varlegLocalEast_) is now the sole control; delegated voices are bit-identical
  to mono, Local-East voices diverge
- SequencerEngine.hpp: perVoiceArticulation default false→true (keeps Lantern
  correct until Step 5 removes the flag entirely)
- test_per_voice_articulation.cpp: section 1 updated to prove the flag is inert

Input ID layout fixes (Step 1 leftover — POLY_LANES 5→7 but layouts had +5):
- MonsoonSandsVisualExpander.hpp: DIR_MOD_START and NUM_INPUTS used +5
  (should be +POLY_LANES=7) → caused configInput OOB → abort on preview
- StraitsSandsMacroVisual.hpp: CV_START count and NUM_INPUTS same +5 issue
- StraitsSandsMacroVisual.cpp: 3 arrays sized [POLY_LANES] had 5 initializers
  (EDN, editorDirCol, laneName) — added VAR/LEG entries
- StraitsEastSandsVisual.cpp: same 5-initializer issue in 3 arrays

KNOWN ISSUE: Rack still crashes on module browser preview (Sands Visual).
The input ID fix resolved the OOB configInput but the crash persists.
Stack trace (LTO, unreliable): ModelBox::draw -> static_init -> GateState::markSemi -> abort.
Needs debugger investigation — LTO makes the stack trace misleading.
```
