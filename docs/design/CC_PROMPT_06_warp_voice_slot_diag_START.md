# CC PROMPT 06 — DIAGNOSE the warp voice/slot + Macro→East leak (symptom fingerprint)

CONTEXT: feat/warp-ui, warp wired but WRONG per-voice. This is a DIAGNOSTIC-FIRST task: the three
Rack symptoms encode the bug; find the exact index mismatch by READING + a targeted test, then fix.
Do NOT guess-and-patch (an earlier prompt misdiagnosed polyRandomSrc as raw — verify against code).

## THE SYMPTOMS (Rodney, in Rack — this fingerprint IS the diagnosis)
1. EAST, delegated lanes, voice 2+: warp has NO effect.
2. MACRO: warp has NO effect on voice 1; DOES affect voice 2+.
3. MACRO warp changes EAST's probabilities for voice 2+ — on lanes delegated OR NOT delegated.
Fact 3 (happens even on NON-delegated lanes) RULES OUT warpDelegated_ as the leak path — the leak
is an INDEX mismatch, not a delegation-flag error. Facts 2+3 say: slot 0 (voice 1 / mono) is
resolved DIFFERENTLY from slots 1+ (voices 2+), and Macro's write reaches slots that East voices read.

## THE PRIME SUSPECT (found by reading — VERIFY, don't assume)
There is an N→N SLOT/VOICE remap: MonsoonSandsManager.cpp:272 comment —
  "slot v = voice v+1. Poly voices read slot v+1 ... so they no longer collide with this mono slot."
So: SLOT 0 = voice 1 (mono/VoiceResolver::kMonoSlot); SLOT 1 = voice 2; ... SLOT n = voice n+1.
Three places must AGREE on this map, and the bug is where they DON'T:
  (a) WHERE MACRO WRITES: setGlobalWarpF writes globalWarpF[lane] (per-LANE, shared across voices
      — WarpF.hpp:341). Delegation decides who READS it. Where does the Macro UI write warpF vs
      globalWarpF, and by which slot/voice index? (StraitsSandsMacroVisual.cpp openWarpPopup + the
      w-level knob setGlobalWarpW/setWarpW.)
  (b) WHERE EAST WRITES: East's popup setWarpF(slot, lane) + w-knob setWarpW(currentSlot(), lane).
      What is currentSlot() — does it return the SLOT (0-based, slot0=voice1) or the VOICE NUMBER?
      (StraitsEastSandsVisual.cpp — find currentSlot()/the tab→slot mapping.)
  (c) WHERE THE ENGINE READS: warpRow(v, lane) → warped_[v] → rebuilt via
      warpedValue(v, lane, u, warpDelegated_[v][lane]). The consumer passes voiceIdx
      (executePolyVoice) and 0 (executeStep/mono). Is executePolyVoice's voiceIdx the SLOT or the
      VOICE NUMBER? And does warpDelegated_[v] get populated with v = SLOT or v = VOICE?
      (MonsoonSandsManager.cpp:146-150 sets warpDelegated_[v] from topo.owner(v,l) — is topo.owner's
      v the SLOT or the VOICE? vs the consumer's warpRow(voiceIdx) v.)

THE BUG IS AN OFF-BY-ONE / SLOT-vs-VOICE MISMATCH among (a)(b)(c). Symptom 2 (voice 1 dead on Macro)
= slot 0 (mono) delegation resolved opposite to slots 1+. Symptom 3 (Macro→East leak on non-deleg
lanes) = Macro's write index and East's read index overlap for voices 2+ (a shift of 1 makes
Macro's slot-k write land where East voice k+1 reads).

## METHOD (diagnostic-first)
1. TABULATE the index convention at (a)(b)(c): for each, write down "does this use SLOT (0=mono/voice1)
   or VOICE NUMBER (1=voice1)?" — by reading currentSlot(), executePolyVoice's voiceIdx origin,
   topo.owner's arg, warpDelegated_'s index, VoiceResolver::kMonoSlot and the N→N map. Put the table
   in the PR.
2. The mismatch is where two of them disagree. The fix is to make all three use ONE convention
   (prefer: the VoiceResolver slot convention, slot0=mono=voice1, since the banks are slot-indexed).
3. Write a DIAGNOSTIC TEST that pins it without Rack: set a distinct WarpF+w on Macro-global for lane
   L; set a DIFFERENT distinct WarpF+w on East slot-for-voice-2 for the SAME lane L (non-delegated);
   assert warpRow(voice-2-index, L) returns VOICE-2's East curve, NOT Macro's, AND warpRow(voice-1-
   index, L) returns the right thing. Assert Macro's global curve does NOT appear in any East-owned
   voice's warpRow. This test RED-exposes the leak; GREEN when indices align.
4. Fix the mismatch (one convention), re-run — the all-112-cell contract test from CC_PROMPT_05 (if
   present) should also go green.

## DONE =
- The index-convention table (a)(b)(c) in the PR, mismatch identified.
- One slot/voice convention across write(Macro), write(East), read(engine), warpDelegated_.
- Diagnostic test: Macro curve does NOT leak into East-owned voices; each voice's warpRow returns
  ITS OWN curve; voice 1/mono and voices 2+ both correct. RED on HEAD → GREEN after.
- bash test/run_all.sh green; needs Rack feel-test: all three symptoms gone.

## NOTE
If CC_PROMPT_05 (all-lanes warpRow routing + 112-cell contract test) is being done, fold this into
it — the 112-cell "each (voice,lane) returns ITS OWN curve, no leak" assertion catches this exact
bug. This prompt is the targeted version if 05 isn't the path.
