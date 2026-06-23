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
STATUS: bugs 1+2 FIXED (N→N re-index, commit b44b292). Bug 3 still open — needs runtime
confirmation to pin the exact cause; static analysis narrowed it but couldn't settle it.

RULED OUT by code reading:
- The display IS synced when Macro owns: syncPatternEngineToEditor runs whenever
  selectedVoice>=1 (East step ~L466), independent of owner.
- No continuous clobber: syncEditorToPatternEngine (editor→engine write) is only called on tab
  switch (onVoiceTabChanged ~L328), not per-step.
- The draw exists: slewedPolyRhythm[v] is regenerated in redrawRhythm from the seed regardless
  of owner; combineLOR only sets the LOR window, not the draw. So polySlewed has data.

REMAINING CANDIDATES (need the running module to disambiguate):
  (a) The inert gate (East step ~L415): the visual goes fully inert/blank unless
      cachedPolyVoiceExpander != nullptr AND numPolyVoices>=1. If the test rig has Macro +
      Sands-East-visual but NOT the Straits East CV expander attached, the lanes are blank by
      this gate — which would look like "blank when Macro owns" if Macro is what's attached.
      → If this is it, the fix is to also treat "Macro attached + owns lanes" as a data-present
        condition for the inert gate (show the ceded pattern even without the CV expander).
  (b) getInterpolatedValue reads polySlewed(startVoiceIdx+voiceIdx). For East start=0,
      voiceIdx=polyVoice() (0-based). If a ceded lane's spread path leaves getSpread()/target
      degenerate, the interpolate could collapse — but data should still show as the raw draw.

RECOMMENDED FIX (owner-agnostic, resolver-sourced — try if (a) isn't the cause): populate the
East editor probabilities for a ceded lane from VoiceResolver::laneProbabilityAtStep(voice,
lane, step) — the post-everything engine probability, exactly what plays and what the prob-out
display already uses — instead of the spreadMgr interpolate that may be degenerate when Macro
owns. This is the same resolver path that fixed the prob-out mono channel.

NOT YET IMPLEMENTED: holding for Rodney to confirm which candidate (likely a) matches the
runtime, so the fix targets the real cause rather than guessing. Bugs 1+2 are independently
shippable.
