# CC FIX — output mask step must come through per-voice LOR ADDRESSING, not voiceRead_ / stepIndex (Rodney)

PRIOR FIXES FAILED (global stepIndex → voiceReadPosition both no-ops). Root cause is DEEPER: the mask step
is being sourced from the WRONG position space entirely.

## WHY BOTH FAILED
outputMaskPasses(vs, phraseStep) checks the mask WINDOW (maskWindowContains(O, L, phraseStep)) where (O,L)
is the voice's LOR window — so phraseStep MUST be the voice's position IN ITS OWN LOR WINDOW SPACE. But:
- engine.stepIndex = the GLOBAL step (not per-voice, wrong space).
- voiceReadPosition(vs) = voiceRead_[v], which is (SequencerEngine.hpp:532) "the buffer cell the voice's
  FRAME reads — the Stage-3 range-strip + tests" — a UI/frame cache, NOT the gating phrase step. AND
  voiceRead_[0] ≡ 0 ALWAYS (hpp:460) → mono mask is checked at step 0 forever (mono broken too).
So neither is the voice's LOR-space phrase position → outputMaskPasses either checks the wrong space or (if
voiceRead is out of 0-15 / always 0) trivially passes → NO EFFECT, all kinds identical.

## THE REAL ISSUE (Rodney's instinct — bring it through the addressing machinery)
There is NO single "voice phrase step" field to read — a voice's position is PER-LANE, computed by the
SAME addressing the gate/CV lanes use: getStrandIdx(polyLaneTick(voiceIdx, lane), polyLenE, polyOffE,
polyRotE) (SequencerEngine.cpp:1776-1862). The mask window (O,L) lives in that per-voice LOR space, so the
mask step MUST be computed the SAME way — through polyLaneTick + the voice's LOR — not a frame cache, not
the global step.

## THE FIX — compute the mask step via the per-voice LOR addressing
- The mask gates the WHOLE voice output, so it needs ONE canonical per-voice phrase position. Use the voice's
  own LOR window walk computed the SAME way the output lanes are: the voice's phrase step =
  getStrandIdx(polyLaneTick(vs, <reference lane>), polyLenE(vs,…), polyOffE(vs,…), polyRotE(vs,…)) — the
  voice's position in ITS window, 0..(L-1), the exact space maskWindowContains expects.
- Pick the reference consistently: the mask is a VOICE-level gate, so use the voice's FRAME position (the
  voiceLOR window walk), NOT a specific lane's — i.e. the voice-window step, computed through the same
  getStrandIdx/voice-LOR path, not voiceRead_. If the voice-frame position is only available as the
  voiceRead_ cache (which is wrong/zero), COMPUTE it properly here via the voice LOR (polyLaneTick-style at
  the voice-frame level), through the addressing, as Rodney asks — do NOT read the frame cache.
- MONO: the mono voice's phrase step must be its REAL step (mono's LOR window position), NOT voiceRead_[0]≡0.
  Use the same addressing for the mono slot (its window walk), or engine.stepIndex IF mono's window == global
  (confirm; mono may legitimately be the global step, but verify it's not 0-pinned).
- outputMaskPasses itself is UNCHANGED (its window/kind/delegation logic is correct). ONLY the phraseStep
  argument changes — to the properly-addressed per-voice (and real mono) LOR-space position.

## VERIFY
- Mono mask now gates (was stuck at step 0): Range on mono → only the window plays.
- Each poly voice gates at ITS window position → different voices with different windows/painted masks gate
  differently; the 4 kinds differ (Cycle all-on, Range window, Painted cells, follow-R rotates).
- Goldens bit-identical at Cycle default (outputMaskPasses true every step regardless of phraseStep when
  kind==CYCLE).

## NOTE (the architecture point — Rodney)
The mask must resolve through the SAME per-voice LOR addressing as every other per-voice read (the resolver/
getStrandIdx/polyLaneTick path). Sourcing it from a frame UI-cache (voiceRead_) or the global step was the
bug — a position read that bypassed the addressing machinery. Bring it through the addressing, like all
per-voice probability/position reads.
