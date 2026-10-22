# ⚠️ ROOT CAUSE CONFIRMED (upgraded from diagnostic to FIX) — voice mismatch, not delegation
Pinned in code: the two sides read DIFFERENT VOICES.
- PROB-OUT / scope: finalRandomByStrand(strand, step) (PatternEngine.hpp:503) returns
  warpRow(0, s) — HARDCODED VOICE 0 (mono/V1). The prob CV out always shows V1's warp.
- MACRO BARS: StraitsSandsMacroVisual.cpp:590 draws warpDisplayMix(mono ? 0 : polyVoice, ...)
  — the DISPLAYED voice, which is a POLY voice (V2/V3/…) whenever you've selected one on the tab.
So when viewing a poly voice: BARS show that poly voice's warp; SCOPE shows V1's warp. They diverge
because they read DIFFERENT VOICES — exactly Rodney's symptom:
  edit East on the displayed poly voice (own-slot) → BARS move, scope (V1) doesn't;
  edit Macro global → V1 (delegated) picks it up → SCOPE moves, but the displayed poly voice's bars
  show its own-slot East curve → bars don't.
This is NOT delegation polarity (that's clean) — it is the mono/voice-0-special residue AGAIN:
finalRandomByStrand is hardcoded warpRow(0,...). Same bug class as the melodyPitchV double-writer.

THE FIX — a DESIGN CALL for Rodney, then CC implements:
The prob CV OUT is genuinely the MONO (V1) output — so V1's warp on the scope is arguably CORRECT for
the mono out. The inconsistency is the BARS showing a DIFFERENT (poly) voice than the mono out. Two
coherent fixes:
  (A) If the Macro prob-out/scope is meant to be the MONO voice: the Macro BARS should also show V1
      (voice 0) — i.e. warpDisplayMix(0, ...) — so bars == the mono out they represent. (Bars follow
      the out.) Simplest; makes Macro bars always the mono/V1 view.
  (B) If the Macro bars are meant to show the SELECTED poly voice: then there must be a per-voice prob
      OUT for that voice, and the scope should be taken from THAT voice's out — i.e. finalRandomByStrand
      needs a voice arg (not hardcoded 0) for the poly prob-outs. (Out follows the bars.) Bigger.
RODNEY DECIDES A vs B. Likely (A) for the Macro mono-out case (bars show the voice the out emits), with
(B) reserved for if/when per-voice poly prob-outs are wanted. Until Rodney picks, DO NOT patch — the
diagnostic below confirms the mechanism; the fix is gated on the A/B design call.

────────────────────────────────────────────────────────────────────────────────
# CC PROMPT — DIAGNOSE: warp bars vs scope disagree (display bug, NOT ownership polarity)

CONTEXT: Rodney's REFRAMED repro (this is the real symptom — supersedes "East controls Macro"):
  - Take the probability OUT from MACRO to a VCV scope.
  - Hand-edit warp on EAST → Macro's BARS move, but the SCOPE (actual prob out) does NOT.
  - Hand-edit warp on MACRO → the SCOPE changes, but Macro's BARS do NOT.
So: SCOPE follows MACRO edits (CORRECT — Macro drives output). BARS follow EAST edits (WRONG — bars
should show what the engine outputs = the effective/Macro-delegated warp). This is a DISPLAY bug. The
engine/ownership chain is CORRECT (that's why the scope is right) — do NOT re-chase polarity/voiceSlot;
both were verified clean.

## WHAT'S VERIFIED (build on this)
- warpDisplayMix (bars, PatternEngine.cpp:141) and warpMixCell (engine mirror, :131) apply delegation
  IDENTICALLY — both call warpBank_->warpedValue(v, lane, u, warpDelegated_[v][lane]). So the two do NOT
  diverge on the delegation COMPUTE. The divergence is elsewhere.
- Bars compute warpDisplayMix LIVE each frame (StraitsSandsMacroVisual.cpp:590) — reflects setWarpF/
  setGlobalWarpF immediately. Scope reads the MIRROR warped_ (warpRow/finalRandomByStrand), updated only
  on rebuildWarpMirror*.
- Edit rebuild scopes DIFFER: Macro edit → setGlobalWarpF + requestWarpRebuildLaneDelegating (delegating
  voices only); East edit → setWarpF + requestWarpRebuildVoiceLane (one voice-lane). (WarpCurvePopup.hpp:126-127)

## THE HYPOTHESIS TO CONFIRM (diagnostic-first — verify, don't patch blind)
The bars and the scope are showing DIFFERENT (voice,lane) DELEGATION STATES:
- The MACRO bars are drawn for a voice/lane where warpDelegated_ is FALSE (own-slot) → warpDisplayMix
  shows the EAST own curve → bars move on an EAST edit.
- The scope output being monitored is a voice/lane where warpDelegated_ is TRUE → routes to Macro's
  globalWarpF → scope moves on a MACRO edit, not East.
i.e. the voice the BARS represent has a different delegation state than the voice feeding the SCOPE OUT.
TASKS:
1. Pin WHICH voice/slot the Macro bars draw (the v passed to warpDisplayMix at StraitsSandsMacroVisual.cpp:590
   — mono?0 : polyVoice) vs WHICH strand/voice feeds the probability-OUT the scope reads (trace the prob
   CV out back to its finalRandomByStrand / warpRow voice index).
2. Compare warpDelegated_[v][lane] for those two voice indices on the SAME lane. The bug is confirmed if
   they DIFFER (bars' voice own-slot, scope's voice delegated — or the reverse).
3. Check WHEN warpDelegated_ is populated vs read: is it possible the bars read a STALE warpDelegated_
   (set once, not refreshed) while the engine uses a current one — OR the displayed voice simply isn't
   the voice whose output is on the scope.

## LIKELY FIX (confirm the hypothesis first)
The Macro bars should display the EFFECTIVE warp for the lane AS THE ENGINE OUTPUTS IT — i.e. for a
Macro-delegated lane, show Macro's global curve (delegated=true), matching the scope. If the bars are
drawing a voice whose delegation state differs from the monitored output, align them: the Macro panel's
bars should represent the delegated/effective result (what's heard), not an own-slot East curve. This is
the same class as the earlier melodyPitchV double-writer / per-lane-gate bug — two views of "the warp
for this lane" that must agree. The fix is to make the bars' (voice, delegation) match the output's.

## CONTRACT TEST
Assert: for a Macro-delegated (voice,lane), warpDisplayMix(that voice, lane, u) == the engine's
warped_ value == what finalRandomByStrand returns for the monitored strand. i.e. BARS == SCOPE for the
same effective lane. Editing the Macro global curve moves BOTH; editing an own-slot East curve on a
DELEGATED lane moves NEITHER (it's overridden by Macro). RED on current (they disagree), GREEN when the
bars show the effective/delegated value the output uses.

## DONE =
- The voice/delegation mismatch between bars and scope identified (task 1-2).
- Bars show the EFFECTIVE (delegated) warp matching the engine output for the monitored lane.
- Contract test: bars == scope for the same effective (voice,lane); RED→GREEN.
- bash test/run_all.sh green. PR: "needs Rack re-test — edit East on a delegated lane: bars AND scope
  both unchanged; edit Macro: bars AND scope both move together."

## NOT THIS PROMPT
- Ownership polarity / voiceSlot (verified clean — the engine/scope is correct).
- The perf rebuild fix, resolver, tilt (separate).
