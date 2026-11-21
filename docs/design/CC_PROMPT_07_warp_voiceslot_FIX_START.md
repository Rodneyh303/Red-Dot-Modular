# CC PROMPT 07 — warp voice/slot bug = the DOCUMENTED voiceSlot-vs-polyBank lie (the V16-flicker class)

STATUS CORRECTION: CC_PROMPT_05 is NOT implemented — only the design doc (48fab37) exists; there is
no test_warp_all_lanes and no all-lanes routing on feat/warp-ui (HEAD fdc6a03 = the 04 UI work).
So Rack is running the PRE-05 code, which has this bug and no test guarding it.

## THIS IS A KNOWN, DOCUMENTED BUG CLASS IN THIS REPO — do NOT treat it as new
Git history: ba3d4a6 "VoiceResolver::polyBankIndex LIES (name says 0=V2, code returns v-1=voiceSlot)";
aeb7a8b/26fa2a3 "V16 flicker — read by POLY-BANK not VOICESLOT"; 5767557 "truth-ify VoiceResolver →
polyVoiceSlot". There are/were TWO voice-index conventions and MIXING them is the recurring bug:
  - voiceSlot (AUTHORITATIVE, VoiceResolver.hpp:18-19,39-40): V1 = mono = slot 0 (kMonoSlot);
    V2..V16 = poly, voiceSlot = V-1 (so V2→1, V3→2). "0-based poly bank is DEAD since the COLLAPSE."
  - polyBank (DEAD): 0 = V2. Any code still using this is the lie.
The warp Macro→East leak + voice-1-vs-2+ asymmetry IS this bug resurfacing in the warp wiring.

## THE SYMPTOMS → the index mismatch (Rodney, Rack)
1. EAST delegated lanes, V2+: warp no effect.
2. MACRO: no effect on V1; works V2+.
3. MACRO warp changes EAST probs for V2+, on delegated OR non-delegated lanes.
Fact 3 on NON-delegated lanes ⇒ not a warpDelegated_ logic error — it's an INDEX mismatch: Macro's
write and the consumer's read disagree by one (voiceSlot vs polyBank / an off-by-one in slot↔voice).
Sym 2 (V1 dead on Macro) = slot 0 (mono) handled on a different convention than slots 1+.

## THE FIX: make EVERY warp site use voiceSlot (VoiceResolver), full stop
VoiceResolver is THE authority (V1=slot0=mono, V2..16=slot1..15). Audit and conform EVERY warp
index to it. Check each site for "is this voiceSlot or polyBank/off-by-one?":
  (a) MACRO write — setGlobalWarpF/setGlobalWarpW (per-LANE, shared) + setWarpF/setWarpW: which slot?
      (StraitsSandsMacroVisual.cpp openWarpPopup + w-knob.) The Macro tab's selected voice → slot via
      VoiceResolver, NOT a raw tab index or v+1.
  (b) EAST write — setWarpF(slot,lane)/setWarpW(currentSlot(),lane): what does currentSlot() return?
      It MUST be the VoiceResolver voiceSlot (V1→0,V2→1). If it returns a polyBank or tab-index, that
      is the bug. (StraitsEastSandsVisual.cpp.)
  (c) ENGINE read — warpRow(v,lane), warpDelegated_[v][l] from topo.owner(v,l), executePolyVoice's
      voiceIdx, executeStep's 0. Is topo.owner(v)'s v and warpDelegated_'s v the SAME voiceSlot as
      the consumer's warpRow(v)? (MonsoonSandsManager.cpp:146-150 + the :272 "slot v = voice v+1"
      comment — RECONCILE that comment with VoiceResolver; if the code there uses v+1 anywhere for
      the warp bank, that's the off-by-one.)
  (d) DELEGATION population: warpDelegated_[v][l] = (topo.owner(v,l)==MACRO) for v=0..15 — confirm
      topo.owner's v is voiceSlot (0=mono/V1), matching the consumer.

Pick voiceSlot everywhere. Remove/replace any v+1, v-1, polyBank, or raw-tab-index in a warp path.
Where a tab index or voice number must become a slot, go through VoiceResolver, not arithmetic.

## THE CONTRACT TEST (the thing that ends this bug class — build it even though 05's didn't land)
test/test_warp_voice_isolation.cpp (header-only where possible; model on test_warp_mirror):
For EACH voiceSlot s in 0..15 and EACH lane l:
  1. Set a DISTINCT WarpF+w on EAST slot s, lane l (non-delegated). Assert warpRow(s,l) returns
     THAT curve's mix — and warpRow(s',l) for ALL s'≠s is UNCHANGED (no leak to other voices).
  2. Set a distinct WarpF+w on MACRO-global lane l, mark slot s delegated. Assert warpRow(s,l)
     returns the MACRO curve; non-delegated slots on lane l return THEIR OWN (or identity). Assert
     the Macro curve NEVER appears in a non-delegated slot's warpRow (the leak test).
  3. Explicitly include s=0 (V1/mono) AND s=1 (V2) — the two the symptoms distinguish — so an
     off-by-one between them FAILS the test.
RED on HEAD (the leak + V1 asymmetry), GREEN when all warp indices are voiceSlot-consistent.

## DONE =
- Every warp index (Macro write, East write, engine read, delegation) uses VoiceResolver voiceSlot;
  no v±1 / polyBank / raw-tab-index in any warp path; the :272 comment reconciled with the code.
- test_warp_voice_isolation: per-slot no-leak + delegation-correct + V1/V2 distinguished; RED→GREEN.
- bash test/run_all.sh green; needs Rack feel-test: all 3 symptoms gone (warp per-voice on East
  and Macro, no Macro→East leak, V1 works).

## NOTE
This supersedes CC_PROMPT_06 (diagnostic) — the diagnosis is DONE: it's the voiceSlot-vs-polyBank
mix, the documented V16-flicker class. And it's the per-voice-isolation core of CC_PROMPT_05 (which
was never implemented). Do THIS.
