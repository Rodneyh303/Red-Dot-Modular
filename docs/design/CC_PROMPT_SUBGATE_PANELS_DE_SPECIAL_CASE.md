# CC PROMPT — subgate per-voice panels: KEEP StoreKnob (de-param), fix only VISUAL + POLY-DIM divergence

CORRECTION (Rodney caught it): an earlier version of this prompt told CC to replace StoreKnob with real
configParams. THAT WAS WRONG — it would UNDO the de-paramming migration (NUM_PARAMS_MIGRATION.md /
PARAM_CLASSIFICATION.md) and reintroduce the DAW automation overflow (five modules sized to NUM_PARAMS
export NOTHING; per-voice state MUST stay off params[]). The subgate per-voice knobs are per-voice state →
store-backed (StoreKnob) is architecturally CORRECT. KEEP StoreKnob.

## THE REAL PROBLEM — de-paramming is SEPARABLE from knob-widget + poly-dim (Rodney's distinction)
De-paramming is about STORAGE (store field vs params[]). It says NOTHING about which WIDGET renders the
knob or whether it dims with the poly count. The subgate panels' divergence is NOT caused by de-paramming —
it is caused by CC not wiring the presentation/behaviour that StoreKnob CAN do:
- StoreKnob HAS a lockWhen/dim hook (ui/StoreBound.hpp:54-62) — a store-backed knob CAN poly-dim.
- StoreKnob can be styled to match the themed cog look.
So FIX THE PRESENTATION, keep the storage.

## THE THREE FIXES (keep StoreKnob throughout)
1. KNOB LOOK: make the subgate StoreKnobs render like the lane panels' Themed_Compact_Cog_Dim — same cog
   face, same size, same ring. (Match the visual; the binding stays StoreKnob. If StoreKnob's draw can
   reuse the Themed_Compact_Cog_Dim art/draw path, do that — one look, two storage models.)
2. POLY-COUNT DIM: wire StoreKnob's lockWhen/dim to the poly count exactly like the lane panel's
   dimIfInactive (StraitsLaneExpander.cpp:214-221, `voiceNum > activeVoices_`). Carry the activeVoices_
   tracking onto the subgate panel and feed it to StoreKnob's dim hook so inactive voices dim like
   REST/LEGATO. (StoreKnob already supports this — it just was not wired.)
3. THE CONFIG CRASH (bd09335): re-examine. The config()-once crash came from the subgate INPUT jacks
   (now stripped — modulation is Causeway). With the mod jacks GONE and StoreKnob being a plain Widget (NO
   configParam, NO configInput — it does not touch the Rack registry, per PARAM_CLASSIFICATION), the panel
   should config like the distribution expander's StoreKnobs do (config(0,0,0,0) de-param law). Verify the
   bd09335 hack is still needed AFTER the mod jacks are removed — it may now be unnecessary (the jacks were
   the config-slot problem, not the StoreKnobs). Remove it if the jack removal already resolves it.

## THE ONE THING FOR RODNEY TO DECIDE (do not guess)
The MAIN lane panels (StraitsLaneExpander) are NOT de-parammed — they still use real configParam
(StraitsLaneExpander.hpp:52/56). The subgate panels ARE de-parammed (StoreKnob). So the two per-voice
families use DIFFERENT storage models. PARAM_CLASSIFICATION.md has BOTH a "de-param per-voice state" rule
AND a carve-out ("per-voice REST/ACC KEEP param — fixed addressing, no selection dependence", line 66).
So which is right for subgates is a POLICY question:
- If the lane panels' keep-param carve-out applies to subgates too → subgates could use real params like
  the lanes (but that re-inflates NUM_PARAMS — check the budget).
- If de-param is the direction → the LANE panels are a PENDING migration and the subgates are AHEAD.
This prompt assumes KEEP StoreKnob (de-param, the safer/forward direction) and fixes only look + dim. If
Rodney wants both families on the SAME storage model, that is a separate larger decision (either de-param
the lanes too, or confirm the carve-out covers subgates). FLAG this; do not unify storage without his call.

## DONE =
- Subgate StoreKnobs render like the lane cog (same look); poly-count dim wired via StoreKnob lockWhen/dim
  (inactive voices dim like REST/LEGATO); the bd09335 config hack removed IF the mod-jack removal resolved
  it (else documented why still needed). StoreKnob KEPT (de-param intact — NUM_PARAMS not re-inflated).
- Visually + behaviourally identical to the lane subpanels except tint AND storage model (store-backed,
  which is invisible to the user). No DAW-overflow regression.
- Storage-model unification (lanes vs subgates) FLAGGED for Rodney, not done unilaterally.

## WHY (the corrected lesson)
De-paramming (NUM_PARAMS_MIGRATION) is REQUIRED for per-voice state (DAW export). KEEP StoreKnob. The
divergence Rodney saw (different knobs, no poly-dim) is PRESENTATION not storage — StoreKnob supports both,
it just was not wired. Fix the look + the dim hook; keep the storage. (The earlier 'use real params' fix
was itself a failure to check the settled de-param architecture — exactly the error we keep correcting.)

════════════════════════════════════════════════════════════════════════════════
## CONTEXT — the de-param family + why poly-dim applies to subgates but NOT dist/send (Rodney)
Verified de-param status across recent per-voice panels:
- Distribution expander: configParam 0, StoreKnob — DE-PARAMMED.
- Macro send subpanel: configParam 0, StoreKnob — DE-PARAMMED.
- Subgate panels: configParam 0, StoreKnob — DE-PARAMMED (CONSISTENT with the above, NOT special-cased).
- StraitsLaneExpander: real configParam — NOT de-parammed (the OUTLIER; predates the migration, pending).
So the subgate StoreKnob choice MATCHES the recent de-param convention (dist + send). CC was CONSISTENT, not
special-casing. KEEP StoreKnob (confirmed correct, re-confirmed by the family).

### Poly-dim is a FORMAT property, not a storage property — applies to GRID panels only
None of the StoreKnob panels wire poly-dim today (dist ~1 incidental, send 0, subgate 0). But poly-dim is
only APPROPRIATE for the 16-VOICES-VISIBLE GRID format (the lane panels + the subgate panels: a 6/6/4 grid
showing all 16 voices at once — dimming the inactive ones is meaningful). It is NOT appropriate for the
ONE-VOICE-SELECTED format (distribution + send: you pick a voice via the shared selection and edit its
row — there is no 16-voice grid to dim). So:
- SUBGATE panels (grid format, like the lanes) → ADD poly-dim. Correct.
- DISTRIBUTION + SEND (selection format) → do NOT add poly-dim; leave them. Their lack of poly-dim is
  CORRECT for their format, not a gap to fix. Do NOT "unify" them to match.
The format (grid vs selection), not the storage (param vs store), decides whether poly-dim applies. Subgate
and lane are both grid → both poly-dim. Dist and send are both selection → neither poly-dims.

### Net for the subgate fix
KEEP StoreKnob (de-param, matches the family). ADD poly-dim (subgate is grid format). MATCH the lane cog
look. The subgate panels should end up looking + behaving like the LANE panels (same grid format) while
sharing the STORAGE model of dist/send (StoreKnob). No contradiction — look/behaviour follow the FORMAT,
storage follows the de-param migration.
