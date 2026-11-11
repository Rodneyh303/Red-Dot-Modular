# CC DIAGNOSTIC  Sands Macro send subpanel shows the same send on every voice (regression from the sends move)

Build on feat/phase-duals-gaps. Diagnostic FIRST. Do not change behaviour until the cause is shown by test.

## SYMPTOM (Rodney, in Rack)
Was per-voice on the Sands Macro strip. After moving sends to SandsMacroSendSubpanel, changing a send and switching
the voice shows the SAME send on every voice.

## WHAT IS ALREADY VERIFIED (code)
- Storage is per voice: Monsoon.hpp:910, editor.macroSend[448] = 16 voices x 7 lanes x 4 items; index
  (v*7 + lane)*4 + item (Monsoon.hpp:1495-1496). The store is NOT global.
- The subpanel binds every send through bindStoreKnob with get/set lambdas that resolve the voice AT CALL TIME:
  slotLiveFrom(&m) = m->engine.pe.displayedVoice().v (SandsMacroSendSubpanel.cpp:90, 141-142, 153-154, 168-170).
- displayedVoice is written every frame by the East visual from its own tab selection
  (StraitsEastSandsVisual.cpp:1621, in step). So the voice selection DOES reach the subpanel. The Macro visual
  has no voice-tab state and does not write displayedVoice; that is not the cause.
- StoreKnob has no change detection: it draws getValue() each time it is drawn.

So the store is per voice, the subpanel reads and writes the selected voice, and the selection reaches it. Either
the round-trip is wrong, or the displayed value is not refreshed on a voice change. The test below separates them.

## TASK 1  Round-trip test (test/test_send_subpanel_per_voice.cpp)
Using a Monsoon (or the store/accessor layer) and the subpanel's own slot resolution:
1. Set displayedVoice to voice A. Write send (lane L, item I) = 0.3 through the SAME setter the subpanel uses.
2. Set displayedVoice to voice B (different). Write the same send = 0.8.
3. Set displayedVoice back to A. Read the send through the SAME getter the subpanel uses. Expect 0.3.
4. Read at B. Expect 0.8.
5. Repeat for two lanes and a tap and a dist-send.
Outcomes:
- Reads differ per voice as expected: the STORE is correct. Go to Task 2.
- Reads equal across voices: the round-trip is wrong (slot resolution or the accessor). Report the exact
  index used on each write and read, and STOP. Do not change the accessor without showing the index math.

## TASK 2  Display refresh (only if Task 1 passes)
Log, in a debug build, each subpanel knob draw: the voice slot it resolved and the value it drew. Switch voice in
Rack. If the slot changes and the drawn value tracks the store, display is fine. If the slot changes but the
drawn value does not, the knob is not redrawn or its getValue is cached, and the fix is in the redraw path.
Report which.

## GATES (diagnostic)
- Task 1 result is recorded (per-voice round-trip pass or fail, with indices).
- Task 2 result is recorded if reached.
- No behaviour change in this diagnostic commit beyond the test and debug log.

## DO NOT
- Change the store layout, the accessor index math, or the subpanel binding before Task 1 shows where it fails.
- Re-add the sends to the Macro strip.
