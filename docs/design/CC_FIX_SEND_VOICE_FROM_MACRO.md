# CC FIX  sends take their voice from the MACRO strip's selection (a send-voice, owned by Macro)

Build on feat/phase-duals-gaps. Supersedes CC_DIAG_SEND_SUBPANEL_PER_VOICE.md (the cause is now known).

## CAUSE (verified)
- The Macro strip has its own voice selection: tabGroup + viewVoice (StraitsSandsMacroVisual.cpp:83-84, 928).
- The send subpanel reads PatternEngine::displayedVoice (SandsMacroSendSubpanel.cpp:90, slotLiveFrom).
- displayedVoice is written ONLY by East's tabs (StraitsEastSandsVisual.cpp:1621). So Macro's selection never reaches
  the sends, and every send resolves to whichever voice East last set. This is a migration artefact: the on-panel
  Macro strip used its own selection; moving sends to the subpanel moved the reader onto East's state.

## THE MODEL (Rodney)
- EAST keeps displayedVoice for its own purposes (its probability display / recompute). UNCHANGED.
- SENDS take their voice from the MACRO strip. Macro's selection decides which channel the sends are set on.
- The send subpanel reads the SEND VOICE, not displayedVoice.

## THE FIX
1. Add a SEND VOICE to PatternEngine beside displayedVoice_: `VoiceSlot sendVoice_ = kMonoVoiceSlot;` with
   `sendVoice()` (read) and `setSendVoice(VoiceSlot)` (write). Document it as the SENDS selection, owned by the Macro
   strip.
2. The Macro strip writes it. In StraitsSandsMacroVisual, whenever viewVoice changes (and once on construction/load),
   call engine.pe.setSendVoice(VoiceResolver::slot(viewVoice + 1)). Mirror the mapping East uses, but DO NOT touch
   displayedVoice from Macro.
3. The send subpanel reads it. In SandsMacroSendSubpanel.cpp, slotLiveFrom returns
   m->engine.pe.sendVoice().v (NOT displayedVoice). The live reads and writes resolve at call time, as now.
4. Relabel for the user: the subpanel's "VIEWED VOICE: V%d" and the "(viewed voice)" captions become
   "SEND VOICE: V%d" and "(send voice)", so the user can see which voice the sends are on.
5. East is UNCHANGED. Its tabs keep writing displayedVoice, its recompute keeps reading it.

## PERSISTENCE (decided: Macro reopens on the same voice)
The Macro strip's tab selection is NOT saved today (verified: no save/load of viewVoice or the tab in
Monsoon.cpp / MonsoonPersistenceManager.cpp; a widget's state is not persisted, only the module's). Add it:
1. The selection lives in the MODULE, not the widget: a Monsoon field (macroViewVoice, stored as a voice number 1..16).
   The Macro strip WRITES it on tab change and READS it on init (StraitsSandsMacroVisual init block ~line 896).
2. SAVE it in Monsoon's dataToJson under a new key (e.g. "macroViewVoice"). LOAD it in dataFromJson, defaulting to V1
   when the key is absent.
3. On init the Macro strip sets its tabGroup to the stored voice AND calls engine.pe.setSendVoice(slot(stored)), so the
   sends open on the same voice the strip was left on.
4. The send store itself already persists per voice (MonsoonPersistenceManager.cpp:202, 553; MacroTapBank; WarpBank), so
   no change to the send data is needed.
Gate: save a patch with Macro on V5, reload: Macro opens on V5, the send subpanel shows V5's sends, and East is unaffected.

## GATES
- Switching the MACRO strip's voice tab changes the subpanel's SEND VOICE label AND the values shown (per voice).
- Switching the EAST strip's voice tab does NOT change the send voice or the subpanel.
- Save/reload: Macro reopens on the saved voice; a patch with no saved key opens on V1.
- Round-trip per voice: write a send on voice A (Macro tab A), switch Macro to voice B, the send reads the B value
  (default or as written); switch back to A and the A value returns. Test with the Macro selection, not displayedVoice.
- East's probability display and recompute behaviour unchanged (no re-pins; East goldens untouched).
- ≤16 audio: sends still applied per voice as before (no change to the send maths or the fold).
- run_all green on both SDK legs.

## DO NOT
- Set displayedVoice from the Macro strip.
- Change East's selection, its recompute, or the send store layout / index math.
- Re-add sends to the Macro strip.
