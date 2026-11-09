# CC FIX  Macro strip draws the GLOBAL variation walk for the voice playhead; it must draw the voice's own read

Build on feat/phase-duals-gaps. Display-only. The engine cycle path is already correct (voiceFrameTickAt cycle branch
folds the unwrapped tick through the voice's L; rho = R).

## SYMPTOM (Rodney, in Rack)
Macro in cycle mode: the voice strip's playhead runs over the GLOBAL 16 steps, not the voice's window. East's strip
cycles within its per-voice range correctly.

## CAUSE (verified)
- StraitsEastSandsVisual.cpp:1945: the voice playhead is drawn from eng.voiceReadPosition(vslot) (the voice's own
  read cell), and for poly voices the variation playhead uses variationLanePlayheadForVoice(eng, slot) (line 1914).
- StraitsSandsMacroVisual.cpp:1060: the Macro strip draws variationLanePlayhead(eng), the MONO / GLOBAL variation walk,
  for the displayed voice. That walk is the global 16-step pattern, so the Macro playhead runs over 16 regardless of
  the voice's L.
The engine is not at fault: the audio voice (voiceFrameTickAt cycle branch) cycles within L.

## FIX
In StraitsSandsMacroVisual, mirror East's logic for the displayed voice:
- When the viewed voice is a poly voice (viewVoice != mono), draw the playhead from
  variationLanePlayheadForVoice(eng, VoiceResolver::slot(viewVoice + 1)) (the same accessor East uses at line 1914),
  or from eng.voiceReadPosition(slot) where East uses that for the voice playhead. Use the SAME accessor East uses for
  the same purpose; do not invent a third.
- When the viewed voice is mono (V1 on the mono path), keep variationLanePlayhead(eng), as now.
- Do not change the engine, the frame, the store, or East.

## GATES
- Macro strip, cycle mode, voice with L=8 under a 16-step global: the playhead on the strip repeats every 8 steps and
  stays inside the voice window (matches the audio).
- Macro strip and East strip show the same playhead for the same voice and window (the two strips agree).
- Mono (V1) playhead unchanged.
- The rotation marker: after this fix, verify the strip's rotation/chevron shows R within 0..L-1. If the chevron still
  shows R outside the window, that is a second display fix, to be specced separately.
- Display-only: no audio, golden or store change. run_all green on both SDK legs.

## DO NOT
- Change voiceFrameTickAt, effectiveVoiceLor, the clamps, or the store.
- Change East's playhead logic.
