# CC PROMPT — DE-SPECIAL-CASE the subgate per-voice panels: make them StraitsLaneExpander clones

Rodney: the subgate per-voice panels are STILL special-cased — different knobs, no poly-count lighting,
and they caused a crash the main subpanels never needed. We do NOT want special cases. Make them IDENTICAL
to the StraitsLaneExpander panels in EVERY way except TINT.

## THE ROOT CAUSE (one unrequested decision cascaded into all three complaints)
The subgate panels use `StoreKnob` (ui/StoreBound.hpp, "the de-param law") bound to the Monsoon bank store,
instead of real Rack PARAMS like the lane panels (StraitsLaneExpander.cpp:202
`bindParam<redDot::Themed_Compact_Cog_Dim>`). That ONE choice — never asked for — produced ALL THREE:
1. DIFFERENT KNOBS: StoreKnob is a different widget than Themed_Compact_Cog_Dim → the subgate knobs render
   differently. Not styling — a different widget class.
2. NO POLY-COUNT LIGHTING: the lane panel has activeVoices_ + dimIfInactive (StraitsLaneExpander.cpp:220-221
   `voiceNum > activeVoices_`) dimming/locking inactive voices; StoreKnob carries NONE of that machinery →
   subgate voices don't dim with the poly count.
3. THE CRASH (bd09335, "Rack config()-once law — subgate inputs join StraitsIds"): the custom input/config
   handling of the de-param path hit a config-slot problem the lane panels NEVER have (same class as the
   distribution expander's config(0,0,42,0) mis-slot — the de-param/custom-config path keeps crashing where
   real params don't).

## THE DECISION (Rodney): StoreKnob was never requested and the cons outweigh any pro — USE REAL PARAMS
The StoreKnob "benefit" (no param registry) is not worth it: the lane panels prove real params work for a
16-voice per-decision bank; the de-param path has now caused TWO config crashes; it breaks visual +
behavioural consistency. REJECTED. The subgate per-voice panels become StraitsLaneExpander clones.

## THE FIX — make each subgate per-voice panel a StraitsLaneExpander, tint the only difference
1. KNOBS: bind real `Themed_Compact_Cog_Dim` params via the SAME bindParam path StraitsLaneExpander uses —
   NOT StoreKnob. Delete the StoreKnob usage from the subgate panels. (The subgate values still live in the
   Monsoon bank; wire the real params to that bank the way a lane expander's params reach their store — if
   the lane expander uses real Rack params with IDs, the subgate panels do the SAME, registering their
   param IDs properly so config() is correct and no crash-fix hack is needed.)
2. POLY-COUNT LIGHTING: carry activeVoices_ + dimIfInactive VERBATIM from StraitsLaneExpander so subgate
   voices dim/lock with the poly count exactly like REST/LEGATO do. Same machinery, not a reimplementation.
3. CONFIG / NO CRASH: with real params registered correctly (the lane pattern), the config()-once crash and
   its bd09335 workaround are UNNECESSARY — remove the hack; the panels config like a lane expander does.
4. EVERYTHING ELSE from StraitsLaneExpander VERBATIM: the knob grid binding loop, the mod-arc overlay, the
   theme following, the seam fillers, the dimensions (165-wide 6/6/4) — all the same. The ONLY permitted
   difference is TINT (per level×decision, already done, fine).
5. The 8 panels / option-a structure, the gen_straits_lane.py generation, the snapping — all KEEP (those
   are correct). This fix is specifically: StoreKnob → real params, + the poly-dim machinery, + drop the
   config hack.

## DONE =
- Subgate per-voice panels use real Themed_Compact_Cog_Dim params (StoreKnob GONE), bound like a lane
  expander; param IDs registered so config() is correct; the bd09335 config hack REMOVED (not needed).
- activeVoices_/dimIfInactive carried verbatim → subgate voices dim with poly count like REST/LEGATO.
- Visually + behaviourally IDENTICAL to the lane subpanels except tint. No special case remains.
- run_all + anchor audit green; no browser-add crash (verify in a real-SDK/asserts build — the rack-free
  gates can't catch config crashes, per b001216).

## WHY (so it doesn't recur)
Subpanels are StraitsLaneExpander-shaped — real params, the lane's dim/lock machinery, the lane's config.
The "de-param / StoreKnob" path was never requested and keeps causing config crashes + visual/behavioural
divergence. One pattern (the lane expander), tint-parameterised. No special cases. Reuse, don't re-shape.
