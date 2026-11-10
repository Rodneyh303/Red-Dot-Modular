# CC FIX  Macro module: remove the voice-1 / mono special case from the Macro strip

Build on feat/phase-duals-gaps. Display-only. Rodney: "all voices should follow the global per-voice range. Nothing
special about voice 1 should creep in. That is an anti-pattern."

## THE SPECIAL CASE (verified, StraitsSandsMacroVisual.cpp)
`onMonoTab = VoiceResolver::isMono(viewVoiceNum)` (line 970) is true for V1. It branches three ways:
1. Line 990: barVoice = onMonoTab ? mono() : slot(pv+1). V1 draws its bars from the mono slot, others from their slot.
2. Line 1063: the playhead = onMonoTab ? variationLanePlayhead(eng) : variationLanePlayheadForVoice(...). V1 draws the
   GLOBAL mono walk (16 steps), others draw their window.
3. Line 1096: the read = onMonoTab ? se->macroFrameRead()/macroFrameSign() : eng.voiceReadPosition(vslot). V1 reads the
   macro frame, others read the voice cell.
Result: V1 on the Macro strip does not follow the shared Macro per-voice range the way V2..V16 do.

## FIX
1. Remove onMonoTab from the Macro strip. Every viewed voice (V1..V16) takes the SAME path:
   - barVoice = the viewed voice's slot (one expression for all voices);
   - playhead = variationLanePlayheadForVoice(eng, vslot) for all voices;
   - read = eng.voiceReadPosition(vslot) and voiceReadSign(vslot) for all voices.
   Use the slot expression the poly voices already use (VoiceResolver::slot(...)), applied to viewVoice for every voice.
2. Delete the macroFrameRead / macroFrameSign use on the Macro strip if nothing else needs them. If they are still needed
   elsewhere, leave them, but the Macro strip must not use them.
3. Do NOT change the engine here. The mono slot's transport role (spread, step advance, mono lanes) is the design and is
   a separate question (see below).

## CHECK THE ENGINE CONSEQUENCE (verify, do not change in this fix)
After step 1 every viewed voice, V1 included, reads its own voice cell. Confirm that the voice cell for V1 (the mono slot)
follows the shared Macro window the same way V2..V16 do: effectiveVoiceLor(mono()) must resolve to macroVoiceLor_ for a
follow-Macro voice, exactly as for poly voices. If it does not, that is an engine special case to report, not to patch here.

## SEPARATE QUESTION (flag, do not fix here)
The ENGINE has voice-1 / mono special cases that are part of the mono-as-transport design: spread interpolation
(SequencerEngine.hpp:1457-1474, v==0 uses monoPub), the step-advance mono decisions, and the poly loops that start at
v=1. These are not Macro-module behaviour. Whether V1 should stop being the transport is a larger decision; record it,
do not change it in this fix.

## GATES
- Macro strip: V1, V2 and V16 each show a playhead in the SHARED Macro window, cycling within it, with the same
  playhead rule. Changing the Macro L/O/R moves V1 exactly as it moves V2.
- No onMonoTab (and no isMono) branch remains in the Macro strip's playhead, read or bar path.
- The East strip and the audio are unchanged. run_all green on both SDK legs.

## DO NOT
- Special-case V1 anywhere in the Macro module.
- Change the engine's mono transport, the spread interpolation, or the step advance in this fix.
