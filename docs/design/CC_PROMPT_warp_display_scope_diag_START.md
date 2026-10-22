# CC PROMPT — fix warp bars/scope SOURCE mismatch (voice-0 residue); KEEP bars continuous

CONTEXT: Rodney's repro — probability OUT from MACRO to a VCV scope:
  - Hand-edit warp on EAST → Macro's BARS move, but the SCOPE (prob out) does NOT.
  - Hand-edit warp on MACRO → the SCOPE changes, but Macro's BARS do NOT.
The bars and the scope respond to a DIFFERENT SOURCE. This is a DISPLAY/voice bug, NOT ownership
polarity and NOT delegation (both verified clean — that's why the scope/engine is correct). Work on
lane-expander-refactor.

## ROOT CAUSE (pinned in code — this is confirmed, not a hypothesis)
The bars and the prob-out read DIFFERENT VOICES:
  - PROB-OUT / scope: finalRandomByStrand(strand, step) (PatternEngine.hpp:503) returns
    warpRow(0, s) — HARDCODED VOICE 0 (mono/V1). The prob CV out always reflects V1's warp.
  - MACRO BARS: StraitsSandsMacroVisual.cpp:590 draws warpDisplayMix(mono ? 0 : polyVoice, ...) —
    the DISPLAYED voice, a POLY voice (V2/V3/…) whenever one is selected on the tab.
So when viewing a poly voice: BARS show that poly voice's warp, SCOPE shows V1's. Different voices →
Rodney's exact symptom (edit East on the displayed poly voice → bars move, V1 scope doesn't; edit
Macro global → V1 delegated picks it up → scope moves, poly voice's own-slot bars don't).
This is the mono/voice-0-special residue AGAIN — finalRandomByStrand hardcoded warpRow(0,...), same
class as the melodyPitchV double-writer.

## CRITICAL CONSTRAINT — the bars MUST stay CONTINUOUS (do NOT make bars == scope by value)
The bars DELIBERATELY show the CONTINUOUS live warp (immediate response to knob/CV); the engine/scope
shows the LATCHED per-step value (warp read at draw time). This is the intended "sight leads sound by
one step" design, and it is how the code is built: warpDisplayMix computes LIVE each frame from
warpBank_ (continuous); finalRandomByStrand reads warpRow/warped_ (latched mirror). PRESERVE THIS.
=> The bars' value WILL differ from the scope's at any instant (continuous vs latched) — that is
CORRECT. Do NOT assert bars == scope. Do NOT flatten the bars to the latched value. The ONLY thing
wrong is the VOICE/SOURCE the bars read, NOT their continuity.

## THE FIX — align the SOURCE (voice + delegation), keep bars continuous + scope latched
The bars must read the SAME (voice, owner, delegation) that the prob-out represents — while staying
continuous. Same source → editing the owning source moves BOTH (bars immediately/continuously, scope
at the next step); editing a non-owning source moves NEITHER. Only the VOICE the bars read changes.

DESIGN CALL (Rodney picks A or B — about the OUTPUT, not the display continuity):
  (A) The Macro prob-out IS the mono (V1) output → the Macro bars should read warpDisplayMix(0, ...)
      = V1's CONTINUOUS warp, matching the V1 out's source. Bars stay continuous, just show V1.
      SIMPLEST; likely correct if the Macro scope is the mono out.
  (B) The Macro bars should track the SELECTED poly voice → then the prob-out needs a per-voice form:
      give finalRandomByStrand (and the prob-out wiring) a VOICE ARG instead of hardcoded 0, and take
      the scope from THAT voice's out. Bars keep the displayed voice (still continuous). BIGGER.
Rodney: which? (Lean A unless per-voice poly prob-outs are wanted.) Do NOT patch until Rodney picks.

## CONTRACT TEST — assert same-SOURCE-responsiveness, NOT value-equality
  - Editing the OWNING source (Macro global on a delegated lane, or V1's own curve for the mono out)
    changes the bars' continuous value AND, at the next step, the scope's latched value — BOTH respond.
  - Editing a NON-owning source (own-slot East curve on a Macro-delegated lane, or a voice other than
    the one the out represents) changes NEITHER.
  - Do NOT assert bars == scope in value (the latch makes them legitimately differ). Assert they
    respond to the SAME source.
RED on current HEAD (bars and scope respond to different sources/voices), GREEN after the source aligns.

## DONE =
- Bars read the same (voice, delegation) as the prob-out (per Rodney's A/B pick), STILL via the
  continuous live warpDisplayMix (bars never flattened to the latched value).
- Contract test: same-source-responsiveness (owning→both, non-owning→neither; NOT value-equality);
  RED→GREEN.
- bash test/run_all.sh green. PR: "needs Rack re-test — edit East on a delegated lane: bars AND scope
  unchanged; edit Macro: bars move continuously AND scope moves at next step (both respond, values
  differ by the latch — that's correct)."

## NOT THIS PROMPT
- Ownership polarity / voiceSlot derivation (verified clean — engine/scope is correct).
- Changing the continuous-vs-latched timing (that is the intended design — preserve it).
- The perf rebuild fix, the resolver/seam work, the MO tilt (all separate).
