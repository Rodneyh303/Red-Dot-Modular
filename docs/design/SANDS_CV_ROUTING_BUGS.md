# Sands East/Macro CV-routing bugs — diagnosis + fix plan

Three bugs observed in the running module (reported by Rodney). All live in the Sands
East/Macro "Interpretation Y" voice-mix-in layer. Diagnosed below; fixes staged by certainty.

## Bug 1 — Macro v2 tab sends CV to voice 1 (should target v2)
ROOT CAUSE (confirmed): the Macro send banks are 0-indexed poly banks, `sendId(v,lane,item)`
with v=0..14 → poly voices 2..16. The v2 tab maps `pv = polyBankIndex(2) = 0`, so it edits
`sendId(0,...)`. BUT Interpretation Y (MonsoonSandsManager.cpp ~L128) reuses `sendId(0,...)`
as "voice 0's slice" to fold Macro CV onto **voice 1 / mono**. So bank 0 is shared between
the v2-tab edits and the mono mix-in → editing on v2 modulates voice 1.

The collision is the voice-0/voice-1 conflation: there is no dedicated mono send bank (15
banks for 15 poly voices), and Interp Y borrowed bank 0.

FIX OPTIONS (needs Rodney's intent):
  (A) Give the mono mix-in its OWN send storage (a dedicated mono slice, not bank 0); bank 0
      returns to being purely the v2 tab's. Clean, costs a small param block.
  (B) Edit the mono mix-in ON the v1 tab (currently v1 edits nothing) + reserve bank 0 for
      mono, shifting poly v2→bank1.. (needs 16 banks). Matches "should be v1 tab" phrasing
      but breaks the 15-bank↔15-voice mapping.
RECOMMEND (A): dedicated mono mix-in slot, edited on the v1 tab. Implement once confirmed.

## Bug 2 — voice-1 mix-in CV is attenuated by the LEFT attenuverters as well as the mixers
ROOT CAUSE (confirmed): in MonsoonSandsManager.cpp `eastMix` (~L114-119), the East CV folded
onto voice 1 is scaled by `East::attenId(0, r, c)` — voice 0's LEFT per-lane attenuverter.
Per Rodney the voice-1 mix-in should be governed by the MIXER send only, not the left
attenuverters. (The Macro path `macroMix` already uses only the mixer send × macroCVDelta —
correct; macroCVDelta carries Macro's own atten, which is expected.)
FIX (unambiguous): the East voice-1 mix-in should use the MIX-IN SEND as its depth, not the
left attenId. i.e. mirror macroMix: depth = the East mix-in send for voice 0, applied to the
East CV. (Confirm there is an East mix-in send param analogous to Macro's sendId.)

## Bug 3 — Sands East lanes show no pattern when control is ceded to Macro
ROOT CAUSE (likely): the editor's per-step probabilities come from
`spreadMgr.getInterpolatedValue(voiceIdx, lane, i)` (PolyVoiceSandsParameterManager::
syncPatternEngineToEditor). When Macro OWNS the lane, the per-voice spreadMgr mirror for that
voice/lane isn't populated (spread is fed on the East-owned path), so getInterpolatedValue
returns empty → blank lane. The engine DOES have the pattern (polyRhythmRandom[pv]); the
display just reads the wrong (unfed) source when ceded.
FIX (needs verification): when Macro owns the lane, populate the editor probs from the engine
draw the resolver exposes — VoiceResolver::laneProbabilityAtStep(voice, lane, step) for the
displayed voice — instead of the un-fed spreadMgr mirror. (Good resolver use: it already
reads the post-everything engine prob uniformly.)

## Order
- Bug 2 first (unambiguous, isolated to eastMix).
- Bug 3 next (display-only; resolver-sourced prob fill when ceded — verify the blank is the
  spreadMgr mirror, not an `inert`/alpha gate).
- Bug 1 last (needs the A-vs-B design decision; do not guess — it touches the mono mix-in
  storage model).
