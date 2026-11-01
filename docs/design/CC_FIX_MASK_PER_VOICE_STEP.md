# CC FIX — output mask uses the GLOBAL step, must use the PER-VOICE phrase step (Rodney)

SYMPTOM (Rack): the output mask does nothing useful — all 4 menu options (Cycle/Range/Painted/follow-R)
behave identically (the RANGE is always followed), and the behaviour is the SAME for all voices. (East-can't-
paint-a-follow-Macro-voice is CORRECT — line ~353 bails for follow-Macro; paint it on Macro or set the voice
Independent.)

## ROOT CAUSE — MonsoonOutputGenerator.cpp:45
    const int maskStep = engine.stepIndex;   // the GLOBAL phrase step
outputMaskPasses(vs, maskStep) is then called for EVERY voice with the GLOBAL step. BUT outputMaskPasses
(SequencerEngine.hpp:630) resolves the mask in PER-VOICE WINDOW space — it calls effectiveVoiceLor(v,…) and
maskWindowContains(O, L, phraseStep), expecting phraseStep = THAT VOICE'S phrase step, not the global one.
CONSEQUENCE:
- Checking a per-voice-space mask at the GLOBAL step makes it per-voice-BLIND → SAME behaviour all voices
  (symptom 3).
- For MASK_DYNAMIC (range-as-mask) at the global step, it just RE-CHECKS the range the LOR already applied →
  no visible difference → "range always followed", all 4 options look identical (symptoms 1 & 2).
- CYCLE/PAINTED misalign (global-step lookup into a per-voice-space pattern).

## THE FIX — pass each voice its OWN phrase step
maskStep must be PER-VOICE: the voice's current phrase position (0-15) in ITS OWN window — the same position
the voice READS at (from laneTickV_[v] / voiceRead_ / the voice's phrase step incl advanceOffset_[v]), NOT
engine.stepIndex.
- MONO: outputMaskPasses(mono(), <mono's phrase step>) — mono's own step (likely stepIndex is correct for
  mono, but use the mono phrase-step accessor for consistency).
- POLY: outputMaskPasses(vs, <voice vs's phrase step>) — the per-voice phrase step, so the mask is checked
  at WHERE THAT VOICE IS. This is what makes it per-voice AND makes the mask kinds differ from the raw LOR.
- Find/confirm the per-voice phrase-step accessor (the 0-15 phrase position the voice reads at — the same
  value the voice's gate/CV already use). outputMaskPasses needs THAT per voice, not the shared global step.
VERIFY the delegation: outputMaskPasses already resolves local-East vs Macro (maskLocalEast_[v]); the only
bug is the STEP argument. Do NOT change outputMaskPasses — only fix the maskStep passed in (per voice).

## VERIFY (Rack, after the fix)
- 4 menu options now DIFFER: Cycle = plays everywhere; Range = only the window; Painted = the painted cells;
  follow-R rotates the painted pattern with R.
- Per voice: voices with different windows/painted masks gate DIFFERENTLY (not all the same).
- Goldens: at defaults (Cycle) outputMaskPasses is true every step → bit-identical. The per-voice-step fix
  only changes behaviour for non-Cycle masks (which the goldens don't exercise unless pinned). GOLDEN_A
  untouched.

## NOTE
Classic per-voice-blindness bug: the mask was wired to the OUTPUT correctly (OutputGenerator calls
outputMaskPasses) but fed the GLOBAL step, so it couldn't be per-voice and degenerated to the LOR. One line
(maskStep) → per-voice.
